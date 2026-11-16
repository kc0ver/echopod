#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/screen.h"
#include "ui/slide.h"
#include "ui/theme.h"

typedef struct ReaderScreen {
    Screen screen;
    const Theme *theme;
    const char *text;
    uint32_t length;
    uint32_t offset;
    uint32_t from_offset;
    int flip_y;
    int page_index;
    int page_total;
    int book_index;
    int book_count;
    bool picker_open;
    bool file_loaded;
    char counter[16];
    Slide slide;
    bool large;
} ReaderScreen;

Screen *reader_screen_init(ReaderScreen *reader, const Theme *theme, const char *text,
                           uint32_t length);
