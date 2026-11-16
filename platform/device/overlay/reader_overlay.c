#include "app/reader_overlay.h"
#include "screens/reader_screen.h"

#include <stddef.h>

static void init(ReaderScreen *reader, const Theme *theme)
{
    reader_screen_init(reader, theme, NULL, 0);
}

__attribute__((section(".rodata.READER_OVERLAY_ENTRY"), used))
const ReaderOverlayEntry READER_OVERLAY_ENTRY = {READER_OVERLAY_MAGIC, init};
