#pragma once

#include <stdint.h>

#include "screens/reader_screen.h"

enum {
    READER_OVERLAY_MAGIC = 0x52445245
};

typedef struct ReaderOverlayEntry {
    uint32_t magic;
    void (*init)(ReaderScreen *reader, const Theme *theme);
} ReaderOverlayEntry;

Screen *reader_overlay_screen(ReaderScreen *reader);
