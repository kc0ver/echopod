ENTRY(READER_OVERLAY_ENTRY)

MEMORY
{
    READER_RAM (rwx) : ORIGIN = 0x030771D4, LENGTH = 0x2E2C
}

SECTIONS
{
    .overlay :
    {
        LONG(0) LONG(0) LONG(0) LONG(0)
        KEEP(*(.rodata.READER_OVERLAY_ENTRY))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > READER_RAM

    .data : { *(.data .data.*) } > READER_RAM

    .bss (NOLOAD) :
    {
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
    } > READER_RAM

    .comment 0 : { *(.comment) }
    .ARM.attributes 0 : { *(.ARM.attributes) }

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}

ASSERT(ADDR(.overlay) == 0x030771D4, "the reader overlay must start at 0x030771D4")
ASSERT(READER_OVERLAY_ENTRY == 0x030771E4, "the entry table must follow the 16-byte header")
ASSERT(SIZEOF(.data) == 0, "the reader overlay may not hold initialised data")
ASSERT(ADDR(.bss) >= ADDR(.overlay) + SIZEOF(.overlay), "the reader overlay's .bss must follow its code")
ASSERT(ADDR(.bss) + SIZEOF(.bss) <= 0x0307A000, "the reader overlay and its .bss must fit the gap")
