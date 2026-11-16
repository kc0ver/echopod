#include "app/reader_overlay.h"

#include <stddef.h>

#include "hal/system.h"

Screen *reader_overlay_screen(ReaderScreen *reader)
{
    const ReaderOverlayEntry *entry = hal_system_load_reader();
    if (entry == NULL || entry->magic != READER_OVERLAY_MAGIC) {
        return NULL;
    }
    entry->init(reader, reader->theme);
    return &reader->screen;
}
