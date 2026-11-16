#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/font.h"

typedef struct ReaderLayout {
    const Font *font;
    int width;
    int lines;
} ReaderLayout;

typedef struct ReaderPage {
    uint32_t start;
    uint32_t length;
    int lines;
} ReaderPage;

size_t reader_line_bytes(const Font *font, const char *text, size_t length, int width);
bool reader_page_next(const char *text, size_t length, uint32_t offset, ReaderLayout layout,
                      ReaderPage *page);
uint32_t reader_page_previous(const char *text, size_t length, uint32_t offset,
                              ReaderLayout layout);
