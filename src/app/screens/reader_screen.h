#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/screen.h"
#include "ui/theme.h"

typedef struct ReaderScreen {
    Screen screen;
    const Theme *theme;
    const char *text;
    uint32_t length;
    uint32_t offset;
    bool large;
} ReaderScreen;

Screen *reader_screen_init(ReaderScreen *reader, const Theme *theme, const char *text,
                           uint32_t length);
