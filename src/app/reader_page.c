#include "app/reader_page.h"

#include "core/utf8.h"

static int advance_of(const Font *font, uint32_t codepoint)
{
    const Glyph *glyph = font_find_glyph(font, codepoint);
    return glyph != NULL ? glyph->advance : 0;
}

static bool breakable(uint32_t codepoint)
{
    return codepoint == ' ' || codepoint == '-';
}

static size_t line_bytes(const Font *font, const char *text, size_t length, int width)
{
    int used = 0;
    size_t index = 0;
    size_t break_at = 0;
    while (index < length) {
        const char *cursor = text + index;
        uint32_t codepoint = utf8_next(&cursor);
        size_t step = (size_t)(cursor - (text + index));
        if (step == 0 || step > length - index) {
            break;
        }
        if (codepoint == '\r') {
            index += step;
            continue;
        }
        if (codepoint == '\n') {
            return index + step;
        }
        int advance = advance_of(font, codepoint);
        if (used + advance > width && index > 0) {
            return break_at > 0 ? break_at : index;
        }
        used += advance;
        index += step;
        if (breakable(codepoint)) {
            break_at = index;
        }
    }
    return index;
}

size_t reader_line_bytes(const Font *font, const char *text, size_t length, int width)
{
    return length == 0 ? 0 : line_bytes(font, text, length, width);
}

bool reader_page_next(const char *text, size_t length, uint32_t offset, ReaderLayout layout,
                      ReaderPage *page)
{
    if (offset >= length) {
        return false;
    }
    uint32_t start = offset;
    int lines = 0;
    while (lines < layout.lines) {
        size_t bytes = reader_line_bytes(layout.font, text + offset, length - offset, layout.width);
        if (bytes == 0) {
            break;
        }
        offset += (uint32_t)bytes;
        lines++;
    }
    page->start = start;
    page->length = offset - start;
    page->lines = lines;
    return true;
}

uint32_t reader_page_previous(const char *text, size_t length, uint32_t offset, ReaderLayout layout)
{
    uint32_t previous = 0;
    uint32_t cursor = 0;
    while (cursor < offset) {
        ReaderPage page;
        if (!reader_page_next(text, length, cursor, layout, &page) || page.length == 0) {
            break;
        }
        previous = cursor;
        uint32_t next = cursor + page.length;
        if (next >= offset) {
            break;
        }
        cursor = next;
    }
    return previous;
}
