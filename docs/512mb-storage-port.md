# EchoPod 512MB 移植攻坚记录

> 记录时间：2026-11-16
> 对象：Snowsky ECHO MINI 512MB（RKnanoD，FiiO 4.0.0 / MINIV400）
> 目标：把 EchoPod 自定义固件完整移植到 512MB 机型并在真机可用
> 结果：**未成功**。存储能识别、能发命令，但所有数据读取返回全 `FF`。当前设备仍停在 EchoPod 诊断屏，恢复原厂的步骤见附录 A。

---

## 1. 摘要（结论先行）

- 512MB 机型的内部存储是 **SD NAND**（NAND + SD 协议桥），挂在 **eMMC 控制器 `0x60030000`** 上，必须用 **SD 协议**驱动。
- 本机 SD 初始化完全成功：CID/CSD/RCA/容量/状态全部正确，`CMD13 = 0x00000900`（transfer + ready）。
- 唯一异常：**数据阶段返回全 `0xFF`**。控制器 `TCBCNT = 0x200`（确实搬满 512 字节），无 CRC 错误，说明卡在线路上真实返回了全 FF。
- 矛盾点：**bootloader 能读同一颗芯片的 LBA 0 并把我们的镜像跑起来**；官方固件也能正常读写；只有 EchoPod 的应用层读取失败。
- 已逐项排除：控制器选择、eMMC 协议、地址模式、命令序列、CMD16、4-bit/1-bit、mux、上电时序、传输时钟、DMA/FIFO、刷机方式。
- 回到上游 HEAD 的规范实现（issue #19 的最小复刻）后结果**完全一致**，证明失败不是本轮实验性改动造成的。
- 最终判断：缺口在“作者未公开的代码细节”或“厂商总线序列/个体差异”，在没有逻辑分析仪的条件下无法继续定位。

---

## 2. 背景

- EchoPod 上游支持 **8GB 机型**：内部为标准 eMMC，外置 microSD 为标准 SD 卡。
- 512MB 是后来新增的低价版本（279 元），硬件上把内部存储换成了 480MB 的 SD NAND。
- 上游文档明确写“不支持 512MB”；RECHO/ReChord 也只支持 8GB，并警告 512MB 可能变砖。
- 本项目分支从 `af87ccd` 起就在尝试 512MB 支持；当前基点为 `3fc8963 fix: follow the 512 MB MBR to the FAT volume`。

---

## 3. 已确认真机事实（勿重复怀疑）

### 3.1 控制器与协议

| 项 | 实测值 |
|---|---|
| 存储控制器 | `0x60030000`（eMMC 控制器），不是 `0x60020000` |
| 协议 | **SD**（不是 eMMC） |
| eMMC 探测 | CMD1 等 2.4 秒、两种 OCR 参数全程无应答（`cmd1_answered=0`） |
| SD 初始化 | CMD0/8/55/41/2/3/9/7 全部成功 |
| 容量 | `0x000F0000` = 983,040 扇区（480MB） |
| CMD13 状态 | `0x00000900`（transfer + ready），`in_transfer=1` |
| 总线宽度 | 4-bit（实验中确认可切到 1-bit） |
| CID | `00AA55FD C80002FD 50505344 C855AA41`（含 `AA55` 与 ASCII `PPSD`） |

### 3.2 数据阶段的硬证据

| 证据 | 值 | 说明 |
|---|---|---|
| `TCBCNT` | `0x200` | 控制器确实搬满了 512 字节 |
| CRC | 通过 | 空 FIFO/未传输的假设被排除 |
| 读回内容 | 全 `FF` | 卡在线路上真实返回了全 FF |

### 3.3 loader 侧证据（Ubuntu VM + rkdeveloptool）

- `rfi`：SAMSUNG，480MB，983,040 sectors，Block 512KB，Page 8KB，ECC Bits 0。
- `rl 0 10241 readback.bin` 读回 SHA256 与写入的 `MINIV898-wl.img` **完全一致** → 芯片 LBA 0 里确实是我们的镜像。
- 全盘备份 `backup/echo-mini-512mb-full.bin` 的 LBA0 头为 `26202310...`（旧 Rockchip 头）；当前镜像头为 `26201511...`。
- 已确认未插 TF 卡，排除读到外部卡。

### 3.4 官方固件的板级配置（反汇编 MINIV400 `01_app_code.bin`）

- 函数 `0x39F30` 的 mux 表：
  - index0（SD `0x60020000`）：`GRF0x20 = 0xFC005400`、`GRF0x24 = 0x003F0015`
  - index1（eMMC `0x60030000`）：`GRF0x00 = 0xFFFF5555`、`GRF0x04 = 0x003F0015`、`GRF0x24 = 0x00C00040`
- bootloader 留下的实测 GRF 值：`0x00=0x5555`、`0x04=0x15`、`0x20=0`、`0x24=0x40`。
- 官方读块函数：CMD17 命令字 `0xA0002351`、CMD18 `0xA0002352`、FIFO `0x60030200`、BLKSIZ 512、FIFOTH `0x300F0010`——与 EchoPod 的实现逐位一致。

---

## 4. 关键寄存器

| 名称 | 地址 |
|---|---|
| `EMMC_BASE` | `0x60030000` |
| `SD_BASE` | `0x60020000` |
| `GRF_BASE` | `0x50010000` |
| `CRU_BASE` | `0x40180000` |

DW-MMC 寄存器偏移：

| 寄存器 | 偏移 | 寄存器 | 偏移 |
|---|---|---|---|
| `MMC_CTRL` | `0x00` | `RINTSTS` | `0x44` |
| `CLKDIV` | `0x08` | `STATUS` | `0x48` |
| `CLKSRC` | `0x0C` | `FIFOTH` | `0x4C` |
| `CLKENA` | `0x10` | `TCBCNT` | `0x5C` |
| `TMOUT` | `0x14` | `TBBCNT` | `0x60` |
| `CTYPE` | `0x18` | `CMDARG` | `0x28` |
| `BLKSIZ` | `0x1C` | `CMD` | `0x2C` |
| `BYTCNT` | `0x20` | `RESP0` | `0x30` |
| `INTMASK` | `0x24` | `FIFO` | `0x200` |

---

## 5. 诊断手段

### 5.1 第一阶段：18 字段诊断屏

失败时由 `storage_failed()` 调用 `board_panic_words("STORAGE", words, 18)` 显示。字段顺序：

```
cmd1_answered / cmd1_ocr / cmd1_argument / cid0..3 / head_ok / head_0
sector_count / card_status / bus_width / host_base / mux_data / mux_command
ctrl / failed_command / magic
```

用途：一次性带出 CMD1 探测结果、CID、mux 回读、总线宽度、失败命令号等。

### 5.2 第二阶段：9 字段最小诊断

回退 HEAD 后精简为 3 行 × 3 字段，腾出 CODE_RAM：

```
started / sd_nand / sector_count
status  / lba0_0  / lba0_1
magic   / 0       / 0
```

含义：

| 字段 | 含义 | 本机实测 |
|---|---|---|
| `started` | `emmc_start` 成功（含容量与布局解析） | `00000001` |
| `sd_nand` | 走了 SD NAND 路径 | `00000001` |
| `sector_count` | 扇区数 | `000F0000` |
| `status` | CMD13 状态字 | `00000900` |
| `lba0_0/lba0_1` | LBA0 前两个字 | `FFFFFFFF` / `FFFFFFFF` |
| `magic` | 字库表首字（应为 `54504745`） | `FFFFFFFF` |

### 5.3 观测环境的限制

- 无 JTAG、无串口、无逻辑分析仪。
- 每次实验 = 一次 MaskROM `wl` 刷机 + 人眼看屏，单次只能取回几十字节信息。
- 诊断代码受 `CODE_RAM` 157,292 字节硬限制；多次因空间不足删减旧字段。

---

## 6. 尝试与结果时间线

### 6.1 更早会话（交接前）

| # | 尝试 | 结果 | 结论 |
|---|---|---|---|
| 1 | 把存储切到 SD 控制器 `0x60020000` | CMD55 失败、卡无应答 | 芯片不在 SD 控制器上，回退 |
| 2 | 按 eMMC 协议发 CMD1（两种 OCR 参数） | 2.4 秒超时，`cmd1_answered=0` | 不是 eMMC 芯片 |
| 3 | 标准 SD 初始化（CMD0/8/55/41/2/3/9/7） | 全部成功 | 走 SD 协议正确 |
| 4 | 读 CID/CSD/容量/CMD13 | CID 含 `PPSD`，容量 `0x0F0000`，状态 `0x900` | 识别层正常 |
| 5 | CMD17 / CMD18 读块 | 数据全 `FF` | 数据阶段异常 |
| 6 | 4-bit / 1-bit | 均全 `FF` | 位宽不是根因（当时其他变量未对齐） |
| 7 | SDSC 字节寻址 / 块寻址 | 均全 `FF` | 地址模式排除 |
| 8 | 慢时钟、改 CMD16、清 `MMC_CTRL` DMA 残留 | 均全 `FF` | 常规软件项排除 |
| 9 | 写 `GRF0x20 = 0x5400` | 无变化 | mux 不是唯一变量 |
| 10 | loader 侧 `rl` 回读校验 | SHA256 与镜像一致 | 写入位置正确 |
| 11 | 全盘备份 | `echo-mini-512mb-full.bin` | 恢复保障 |

### 6.2 本次会话：mux 对齐（镜像 `3E8787E6...`）

**依据**：官方 `0x39F30` 的 mux 表显示 eMMC 分支只写 `0x00/0x04/0x24`，不写 `0x20`。

**改动**：

- `sd_nand_start` 改为按主机分派：EMMC 主机走官方 eMMC 三件套（`0xFFFF5555` / `0x003F0015` / `0x00C00040`）；SD 主机保持原样。
- 删除已不显示的 lba/glyph 采集代码以腾出 CODE_RAM（最终 157,208/157,292）。

**结果**：`mux_command = 00000055`，数据仍 `FFFFFFFF`。

**分析**：`0x55` 是先前 SD 分支写入的 `0x15` 残留与 `0x40` 叠加（bit0/2/4 未清）。说明 GRF 带写掩码，写入前必须把对应位清零。

### 6.3 本次会话：上电/复位时序对齐（镜像 `A747208A...`）

**依据**：官方初始化代码 `0x38b66`、`0x38f66` 等处的延时：

| 项 | 官方 | 我方（改前） |
|---|---|---|
| PWREN 断电时长 | 10 ms | 10 µs |
| PWREN 上电后等待 | 500 ms | 600 µs |
| 400kHz 时钟后、CMD0 前 | 100 ms | 0 |
| CMD0 之后 | 100 ms | 1 ms |

**改动**：

- `mmc.c`：上电时序改为 `PWREN=0 → 10ms → 1 → 500ms`。
- `sd_card.c`：CMD0 后等待 1ms → 100ms。
- `emmc.c`：改为 **EMMC 主机优先**，该分支自行完成门控/复位/CRU/mux，`GRF0x24` 全程保持 `0x40`；初始化前再等 100ms。
- `glyphs.c`：失败诊断屏改为**再等 1.2 秒**才取快照，用于验证“等久一点是否能读到”。

**结果**：`mux_command = 00000040`（正确），额外等待 1.2 秒后数据仍 `FFFFFFFF`。

**结论**：引脚 mux 与上电时序均排除；NAND 桥“FTL 挂载慢”的假设排除。

### 6.4 本次会话：传输时钟对齐（镜像 `8484A50F...`）

**依据**：官方时钟函数 `0x386F8` + CRU 写入 `0x38DFE` + 源频获取 `0x38DC0`：

- `0x38DFE` 在分频字段为 0 时直接返回、不写 CRU；官方从不使用分频 0。
- 目标速率 24/25/48/50MHz 经 udiv 取偶后全部收敛到同一组：CRU `0x9F008200` + 控制器 CLKDIV 1（约 1/4 源频，~6MHz）。
- 我方原传输态为 CRU `0x9F008000` + CLKDIV 0（近源频直出）。

**改动**：`MmcHost` 增加 `transfer_divider`；EMMC 传输 CRU 改为 `0x9F008200`、分频 1；SD 主机保持 0。

**结果**：仍全 `FF`，`mux_command` 仍为 `0x40`。

**结论**：传输时钟排除。

### 6.5 本次会话：强制 1-bit（镜像 `1C79E3AF...`）

**依据**：官方第一个数据命令（ACMD51 读 SCR）发生在 1-bit 模式；且官方在切 4-bit 前会执行 CMD19（5A 图案）+ CMD14（回显 `0xA5`）总线测试。1-bit 路径在本机官方系统里确定可用。

**改动**：`sd_card_init` 前设 `max_bus_width = 1`，不发 ACMD6，`CTYPE` 保持复位值。

**结果**：`bus_width = 00000001`（实验生效确认），数据仍 `FFFFFFFF`。

**结论**：位宽排除。连 DAT0 单线也读不到，说明“焊盘使能”与位宽无关。

### 6.6 本次会话：回退 HEAD 最小复刻（镜像 `F21D40E2...`）

**动机**：排除“本轮实验性改动引入故障”的可能，复刻 issue #19 的最小配方。

**改动**：

- `emmc.c/.h`、`mmc.c/.h`、`sd.c`、`sd_card.c/.h`、`language.c`、`test_sd_card.c` 全部回到 HEAD。
- 本轮所有实验改动存入 `git stash@{0}`。
- 仅保留：启动栈修复（`startup.c`）、`board_panic_words`、9 字段最小诊断。

**结果**：

```
00000001 00000001 000F0000
00000900 FFFFFFFF FFFFFFFF
FFFFFFFF 00000000 00000000
```

**结论**：HEAD 规范路径同样失败。**失败不是本轮实验改动造成的**；缺口在 HEAD 未复刻到的细节（作者未公开的代码）或本机差异。

### 6.7 本次会话：官方更新流程

**计划**：恢复原厂 → 根目录放 `MINIV898.IMG`（版本 300–898，根目录无其他 `.IMG`）→ 重启由官方 updater 安装。

**结果**：最终屏幕仍为 STORAGE（9 字段同上）。

**说明**：未最终确认安装是经官方 updater 还是 MaskROM 完成，因此不能单独据此判定 updater 路径；但存储结果与 MaskROM 路径一致。

---

## 7. 镜像与构建记录

| 镜像 SHA256（前 8 位） | 核心改动 | 结果 |
|---|---|---|
| `67A557CA` | 不写 GRF0x20（保持 0）+ CLKENA 去低功耗位 | `mux_command=0x15`，数据 FF |
| `3E8787E6` | EMMC 主机用官方 mux 三件套 | `mux_command=0x55`（残留叠加），数据 FF |
| `A747208A` | EMMC 优先 + 官方上电/复位时序 + 1.2s 延迟快照 | `mux_command=0x40`，延迟后仍 FF |
| `8484A50F` | 传输时钟 `0x9F008200` + CLKDIV 1 | 仍 FF |
| `1C79E3AF` | 强制 1-bit | `bus_width=1`，仍 FF |
| `F21D40E2` | 回到 HEAD 最小复刻 | `started=1 sd_nand=1`，数据 FF |
| `MINIV898.IMG` `03C87976` | 同上（未补齐版） | 官方 updater 用 |

构建环境（Windows）：

```powershell
$env:PATH = "C:\msys64\clang64\bin;" + $env:PATH
C:\msys64\clang64\bin\cmake.exe --build --preset device
C:\msys64\clang64\bin\cmake.exe --build --preset release
```

- device：code 156,344 / 157,292（余 948）
- release：code 154,448 / 157,292（余 2,844）
- CTest：164 项中 163 通过，仅 #161 `format` 在 Windows 上 BAD_COMMAND（预期）
- `check_device_image.exe dist/MINIV898.IMG` → `updater check: valid`
- 改动文件 `clang-format --dry-run --Werror` 通过

---

## 8. 公开资料调研

| 来源 | 内容 | 价值 |
|---|---|---|
| 上游 docs | 明确不支持 512MB；docs 无 SD NAND 笔记 | 无技术答案 |
| EchoPod issue #19 | 作者 aaronbali 报告“sd_card_init 跑在 eMMC 控制器上”成功，验证过 16.5MB 读取 sha256、8MB 写入回读、64 专辑建库；**代码从未公开** | 唯一真机成功证据 |
| RECHO / ReChord | 仅支持 8GB，警告 512MB 可能变砖；issue 中无 512MB 条目 | 无 |
| RKnano/RKnanoD 相似设备 | Digma Insomnia、QUMO Boxon 等，仅刷机工具/固件，无存储逆向 | 无 |
| SD NAND 通用资料 | 协议与 SD 卡一致；CMD17 读回全 FF = 卡没理命令；排查项：片选、初始化时钟、命令序列、寻址、信号完整性 | 清单项已全部试过 |
| 逻辑分析仪 | 24MHz 8 通道（约 26 元）可看 CLK/CMD/DAT0，PulseView 自带 sd_sdio 解码 | 唯一剩余的观测手段 |

### issue #19 中与实现相关的要点

- FiiO 512 的探测路径：`r1=0` → **不掉电**，直接 CMD0 → CMD8 → CMD5 → CMD55。
- 时钟用 **PLL**（`hs`），传输上限 50/48/24 MHz。
- mux 三件套与 EchoPod 一致（`0xFFFF5555`、`0x003F0015`、`0x00C00040`）。
- 安装：官方 updater 只认 300–898 版本，前 10240 扇区同时写 slot 1 和 slot 2，不做机型校验。
- 恢复：`wl 0 974848`（设备报 983,040 扇区，可读区到 974,848）。
- 代码体积：加 SD 路径会超 `LOADED_LIMIT`，靠共用布局的一个扇区缓冲压回去。

---

## 9. 已排除项汇总

**控制器 / 协议**

- ❌ 控制器选错（`0x60020000` 无应答）
- ❌ eMMC 协议（CMD1 超时）
- ❌ 地址模式（SDSC 字节 / 块寻址都试过）
- ❌ CMD16 块长度（无条件发送也无效）
- ❌ CMD17 单块 / CMD18 多块（都试过）
- ❌ 总线宽度（4-bit / 1-bit 都全 FF）

**板级配置**

- ❌ GRF mux（`0x00/0x04/0x20/0x24` 已与官方逐位对齐）
- ❌ 上电/复位时序（10ms/500ms/100ms 全部对齐）
- ❌ 传输时钟（官方分频配置已复刻）

**数据通路**

- ❌ 空 FIFO / 未传输（`TCBCNT=0x200`、CRC 通过）
- ❌ DMA 残留（清 `MMC_CTRL` 回读 0 后仍 FF）
- ❌ 外部 TF 卡干扰（未插卡）

**软件实现**

- ❌ 本轮实验性改动（回退 HEAD 后结果一致）

---

## 10. 剩余可能性（按可信度排序）

1. **作者未公开的代码细节**：issue #19 声称成功，但 PR 从未提交。HEAD 只是复刻，无法逐行核对。
2. **厂商总线测试序列**：官方在切 4-bit 前执行 ACMD51（SCR）→ CMD6 → CMD19（5A 图案）→ CMD14（回显 `0xA5`）。EchoPod 未复刻。这是目前唯一“官方有、我们没有”的路径。
3. **本机个体差异**：SD NAND 桥的固件/批次差异，导致相同命令序列在不同机器上行为不同。
4. **观测盲区**：卡是否真的驱动 DAT0、命令是否被采样，从未被直接观测过。

---

## 11. 建议

如果只是想正常使用：

1. 恢复原厂全盘备份，继续当播放器用。
2. 备份文件再复制一份到移动硬盘/网盘，避免 VM 损坏后丢失。
3. 关注 issue #19 与上游更新；如果作者公开代码或上游支持 512MB，再尝试一次。
4. 8GB 机型才是 EchoPod 的正式支持路径。

如果未来还想继续攻坚：

1. **先拿到作者的 diff**（issue #19 留言），这是成本最低、价值最高的动作。
2. 或者准备逻辑分析仪（24MHz 8 通道即可），抓 CLK/CMD/DAT0，配合降频到 1–2MHz 的测试固件。
3. 不要再在没有新信息的情况下盲刷。

---

## 12. 经验教训

- **不要在缺少运行时证据时宣布成功**。多次靠诊断屏的硬数值扭转判断（如 `TCBCNT=0x200` 排除空 FIFO、`rl` 哈希一致排除写入位置错误）。
- **每次只改一个可区分变量**，并把回读值打进诊断屏。
- **早期结论要在其他变量对齐后重测**。例如“1-bit 已排除”是在 mux/时钟/时序均错误时得出的，不可信。
- **诊断字段与 CODE_RAM 是零和的**（上限 157,292 字节），加新字段必须删旧字段。
- **官方反汇编是唯一权威参照**，但厂商序列可能不在已读的代码段里；需要更完整的反汇编覆盖。
- **屏幕字段布局改动要同步 `glyphs.c` 的数组长度**，多/少一个会编译报错。
- **刷机次数对闪存寿命影响可忽略**（按最保守 TLC 折算仍可刷数万次）；真正的风险是刷写中途断电。
- **恢复路径必须始终可用**：全盘备份 + `wl 0` 回写是本项目最重要的保险。

---

## 附录 A：恢复原厂

```sh
sudo ./community/rkdeveloptool/rkdeveloptool ld
sudo ./community/rkdeveloptool/rkdeveloptool wl 0 ~/ReChord/backup/echo-mini-512mb-full.bin
sudo ./community/rkdeveloptool/rkdeveloptool rd
```

备份文件应为整盘大小（983,040 × 512 = 503,316,480 字节）；若大小不符，先停下确认。

## 附录 B：相关文件

- `platform/device/board/emmc.c` / `emmc.h`：双控制器、存储启动、布局、诊断
- `platform/device/board/mmc.c` / `mmc.h`：DW-MMC 控制器操作
- `platform/device/logic/sd_card.c` / `include/device/sd_card.h`：SD 协议初始化与读写
- `platform/device/board/glyphs.c`：字库与诊断屏
- `platform/device/board/sd.c`：外置 microSD 路径（8GB）
- `dist/README-512MB.md`：当前交付镜像说明
- `tests/device/test_sd_card.c`：SD 初始化序列测试
