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

static bool never_starts_a_line(uint32_t codepoint)
{
    switch (codepoint) {
    case ',':
    case '.':
    case ';':
    case ':':
    case '!':
    case '?':
    case ')':
    case ']':
    case '}':
    case 0x3001:
    case 0x3002:
    case 0x300B:
    case 0x300D:
    case 0x300F:
    case 0x3011:
    case 0x3015:
    case 0x3017:
    case 0xFF01:
    case 0xFF09:
    case 0xFF0C:
    case 0xFF1A:
    case 0xFF1B:
    case 0xFF1F:
    case 0x2026:
        return true;
    default:
        return false;
    }
}

static bool never_ends_a_line(uint32_t codepoint)
{
    switch (codepoint) {
    case '(':
    case '[':
    case '{':
    case 0x300A:
    case 0x300C:
    case 0x300E:
    case 0x3010:
    case 0x3014:
    case 0x3016:
    case 0xFF08:
        return true;
    default:
        return false;
    }
}

static size_t line_bytes(const Font *font, const char *text, size_t length, int width, int indent)
{
    int used = indent;
    size_t index = 0;
    size_t break_at = 0;
    size_t previous_start = 0;
    uint32_t previous = 0;
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
            if (never_starts_a_line(codepoint)) {
                return index + step;
            }
            if (never_ends_a_line(previous) && previous_start > 0) {
                return previous_start;
            }
            return break_at > 0 ? break_at : index;
        }
        used += advance;
        previous = codepoint;
        previous_start = index;
        index += step;
        if (breakable(codepoint)) {
            break_at = index;
        }
    }
    return index;
}

bool reader_starts_paragraph(const char *text, uint32_t offset)
{
    return offset == 0 || text[offset - 1] == '\n';
}

size_t reader_line_bytes(const Font *font, const char *text, size_t length, int width, int indent)
{
    return length == 0 ? 0 : line_bytes(font, text, length, width, indent);
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
        int indent = reader_starts_paragraph(text, offset) ? layout.indent : 0;
        size_t bytes =
            reader_line_bytes(layout.font, text + offset, length - offset, layout.width, indent);
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
