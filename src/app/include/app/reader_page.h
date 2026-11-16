#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/font.h"

typedef struct ReaderLayout {
    const Font *font;
    int width;
    int lines;
    int indent;
} ReaderLayout;

typedef struct ReaderPage {
    uint32_t start;
    uint32_t length;
    int lines;
} ReaderPage;

bool reader_starts_paragraph(const char *text, uint32_t offset);
size_t reader_line_bytes(const Font *font, const char *text, size_t length, int width, int indent);
bool reader_page_next(const char *text, size_t length, uint32_t offset, ReaderLayout layout,
                      ReaderPage *page);
uint32_t reader_page_previous(const char *text, size_t length, uint32_t offset,
                              ReaderLayout layout);
