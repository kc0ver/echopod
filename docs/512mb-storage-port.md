# EchoPod 512MB 适配记录

> 机型：Snowsky ECHO MINI 512MB（RKnanoD，FiiO 4.0.0 / MINIV400）
> 状态：**已解决**。存储、内部卷、更新器和关机时序已在真机验证。上游 PR #93 仍为 OPEN，本 fork 的 `main` 已包含全部提交。

## 结论

512MB 机型的内部存储是 SD NAND（NAND + SD 协议桥），挂在 eMMC 控制器 `0x60030000` 上，必须用 SD 协议驱动。开机时 `emmc_start` 先按 eMMC 发 CMD1，无应答就切到 SD NAND 路径。

早期“数据全 FF”的根因不是数据线或初始化失败，而是地址基准不同：

```text
SD 命令 LBA = Rockchip 工具 LBA + 0x2000
```

这颗 SD NAND 在 SD 命令空间里有一段 4 MiB（`0x2000` 扇区）保留前缀。`rkdeveloptool` 和 BootROM 都把工具 LBA 0 映射到 SD LBA `0x2000`，而按 SD LBA 0 去读固件槽和 FAT 卷，读到的是擦除区，所以每个字都是 `0xFF`。`CMD13 = 0x00000900`、`TCBCNT = 0x200` 和数据 CRC 通过，说明传输本身是成功的。

修复在 `b4ed32f fix: address the 512 MB slots from the tool base`。

## 实测布局

SD 命令 LBA：

| 范围 | 大小 | 内容 |
|---|---|---|
| `0x000000`–`0x001FFF` | 4 MiB | 保留前缀，擦除状态 |
| `0x002000`–`0x011FFF` | 32 MiB | 固件槽 1，`RKnanoFW` 头 |
| `0x012000`–`0x021FFF` | 32 MiB | 固件槽 2 |
| `0x088000` | 1 扇区 | MBR，分区类型 `0x0C`，起点 `+0x40` |
| `0x088040` 起 | 约 208 MiB | FAT32 卷，卷标 `ECHO MINI` |

设备报告 `0xF0000` 扇区（480 MiB）。Rockchip 工具能看到的是 `0xEE000` 扇区（974,848 扇区，499,122,176 字节），差值正好是 `0x2000`。整盘备份 `backup/echo-mini-512mb-full.bin` 就是 `0xEE000` 扇区。

`emmc_layout.c` 的对应常量：槽 1 `0x2000`、槽 2 `0x12000`、每槽 `0x10000` 扇区（32 MiB）、固件区结束 `0x22000`。工具侧槽地址则是 0 和 `0x10000`。

## 代码改动

- `platform/device/board/emmc.c`、`emmc.h`：eMMC 探测失败后走 SD NAND 初始化；读、写、状态按当前后端分派；`emmc_is_sd_nand()` 供关机路径使用。
- `platform/device/logic/emmc_layout.c`、`emmc_layout.h`：512MB 固定布局、`0x2000` 基址、从 MBR 第一分区找 FAT 卷。
- `platform/device/board/startup.c`：启动时自行设置 MSP 并清 CONTROL（该机型的交接没有应用镜像头里的 `0x03004000` 栈）。
- `platform/device/board/update.c`、`platform/device/logic/updater.c`：512MB 扫描 `MINIV300`–`MINIV898`，8GB 扫描 `HIFIEC00`–`HIFIEC97`；按镜像内的机型标记接受同型号 FiiO 原厂镜像、拒绝另一型号；构建同时输出 `MINIV898.IMG`。
- `platform/device/board/power.c`：512MB 关机时先停音频和 core 1、强制 DAC 关闭、让屏幕睡眠，再等 2 秒才拉低 hold 引脚；立即断电会锁住供电，电源键无法再开机。

## 验证

已在真机确认：

- MaskROM 写入 device 构建后能进入 UI，字库和语言包正常加载（PR #93 测试记录）。
- 512MB 关机后短按 Power 能正常开机；关机 3 小时 02 分，电量保持 88%，无掉电（`674160b` 电源修复）。
- release 构建通过播放器自身的更新器安装；Diagnostics → Firmware 显示 `7036f6b`。
- 内部卷可以挂载，更新器能按文件名读取其中的 `MINIV898.IMG`。

主机测试（当前 `main`，2026-11-18）：`test_emmc_layout`、`test_updater`、`test_image`、`device_image_passes_the_updater_check` 全部通过；release 构建通过 `size_report --check`。

尚未验证：

- 内部卷上的库扫描与播放。
- 8GB 机型在包含本适配的构建上的真机回归（8GB 的存储路径没有改动）。

## 早期失败的排查记录

`b4ed32f` 之前的所有尝试都建立在“SD LBA 0 就是固件槽”这个错误前提上，因此 mux、上电时序、传输时钟、1-bit、CMD16、DMA/FIFO 都试过，结果始终全 `FF`。这些实验性改动保留在 `wip/512mb-debug` 分支（原 stash），**没有进入 `main`**；它们不是适配代码的一部分，也不再代表当前结论。

排查中仍然有效的硬件事实：

- 控制器 `0x60030000`（eMMC 控制器），不是 `0x60020000`；协议是 SD。
- `CMD0/8/55/41/2/3/9/7` 全部成功；CID 含 ASCII `PPSD`；容量 `0x000F0000` 扇区。
- 官方 eMMC 分支的 GRF mux 为 `0xFFFF5555` / `0x003F0015` / `0x00C00040`；传输时钟 `0x9F008200` + CLKDIV 1。
- 官方读块函数使用 CMD17 `0xA0002351`、CMD18 `0xA0002352`、FIFO `0x60030200`、BLKSIZ 512、FIFOTH `0x300F0010`。

## 安装与恢复

首次安装可以先试 FiiO 的 512MB updater（`MINIV898.IMG`）；开发中验证过的路径是 MaskROM（`docs/device-install.md`）。安装完成后，EchoPod 的更新器会扫描 `MINIV300`–`MINIV898`，用 `MINIV898.IMG` 更新。FiiO updater 是否接受 EchoPod 的镜像没有单独复现记录，被拒绝时按 MaskROM 路径写槽。

恢复原厂优先使用整盘备份（MaskROM）：

```sh
sudo ./community/rkdeveloptool/rkdeveloptool ld
sudo ./community/rkdeveloptool/rkdeveloptool wl 0 ~/ReChord/backup/echo-mini-512mb-full.bin
sudo ./community/rkdeveloptool/rkdeveloptool rd
```

备份文件应为 `0xEE000` 扇区（499,122,176 字节）；如果手里的备份大小不同，先停下确认，不要直接写。

也可以把 FiiO 512 的原厂 `MINIVnnn.IMG` 放到内部存储根目录，由 EchoPod 的更新器安装回原厂。更新器按镜像内的机型标记校验同型号镜像；这是主机测试覆盖的路径，真机回刷未单独记录。

## 关键文件

- `platform/device/board/emmc.c` / `emmc.h`
- `platform/device/logic/emmc_layout.c` / `include/device/emmc_layout.h`
- `platform/device/board/startup.c`
- `platform/device/board/update.c` / `platform/device/logic/updater.c`
- `platform/device/board/power.c`
- `tests/device/test_emmc_layout.c` / `tests/device/test_updater.c`
- 上游 PR #93
