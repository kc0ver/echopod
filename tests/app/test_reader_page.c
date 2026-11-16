#include "app/reader_page.h"
#include "check.h"

static const Glyph TEST_GLYPHS[] = {
    {0x20, 0, 5, 1, 0, 0, 5}, {0x2D, 0, 5, 1, 0, 0, 5},     {0x3F, 0, 5, 1, 0, 0, 5},
    {0x41, 0, 5, 1, 0, 0, 5}, {0x4E00, 0, 10, 1, 0, 0, 10},
};

static const Font TEST_FONT = {
    .glyphs = TEST_GLYPHS,
    .bitmap = NULL,
    .glyph_count = sizeof TEST_GLYPHS / sizeof TEST_GLYPHS[0],
    .bitmap_size = 0,
    .ascent = 8,
    .descent = 2,
    .pack_faces = {0, 0},
};

static void ascii_wraps_at_a_space(void)
{
    CHECK_EQUAL(4, reader_line_bytes(&TEST_FONT, "AAAA BBBB", 9, 20, 0));
}

static void a_word_longer_than_the_line_is_split(void)
{
    CHECK_EQUAL(4, reader_line_bytes(&TEST_FONT, "AAAAAA", 6, 20, 0));
}

static void cjk_wraps_between_characters(void)
{
    CHECK_EQUAL(6, reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80\xE4\xB8\x80\xE4\xB8\x80", 9, 20, 0));
}

static void a_newline_ends_the_line(void)
{
    CHECK_EQUAL(3, reader_line_bytes(&TEST_FONT, "AB\nCD", 5, 100, 0));
}

static void a_carriage_return_joins_a_newline(void)
{
    CHECK_EQUAL(4, reader_line_bytes(&TEST_FONT, "AB\r\nCD", 6, 100, 0));
}

static void a_character_wider_than_the_line_still_advances(void)
{
    CHECK_EQUAL(3, reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80", 3, 5, 0));
}

static void a_closing_punctuation_hangs_on_the_line(void)
{
    CHECK_EQUAL(9, reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80\xE4\xB8\x80\xE3\x80\x82", 9, 20, 0));
}

static void an_opening_punctuation_moves_to_the_next_line(void)
{
    CHECK_EQUAL(3, reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80\xEF\xBC\x88\xE4\xB8\x80", 9, 15, 0));
}

static void a_paragraph_starts_at_the_text_or_after_a_newline(void)
{
    CHECK(reader_starts_paragraph("A\nB", 0));
    CHECK(!reader_starts_paragraph("A\nB", 1));
    CHECK(reader_starts_paragraph("A\nB", 2));
}

static void an_indented_line_holds_fewer_characters(void)
{
    CHECK_EQUAL(6, reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80\xE4\xB8\x80\xE4\xB8\x80", 9, 20, 0));
    CHECK_EQUAL(3,
                reader_line_bytes(&TEST_FONT, "\xE4\xB8\x80\xE4\xB8\x80\xE4\xB8\x80", 9, 20, 10));
}

static void a_page_holds_the_requested_lines(void)
{
    const char *text = "A\nB\nC\nD\n";
    ReaderLayout layout = {&TEST_FONT, 100, 2, 0};
    ReaderPage page;
    CHECK(reader_page_next(text, 8, 0, layout, &page));
    CHECK_EQUAL(0, page.start);
    CHECK_EQUAL(4, page.length);
    CHECK_EQUAL(2, page.lines);
    CHECK(reader_page_next(text, 8, 4, layout, &page));
    CHECK_EQUAL(4, page.start);
    CHECK_EQUAL(4, page.length);
}

static void a_new_paragraph_gets_the_indent_again(void)
{
    const char *text =
        "\xE4\xB8\x80\xE4\xB8\x80\xE4\xB8\x80\n\xE4\xB8\x80\xE4\xB8\x80\xE4\xB8\x80\n";
    ReaderLayout layout = {&TEST_FONT, 30, 2, 20};
    ReaderPage page;
    CHECK(reader_page_next(text, 20, 0, layout, &page));
    CHECK_EQUAL(0, page.start);
    CHECK_EQUAL(10, page.length);
    CHECK_EQUAL(2, page.lines);
    CHECK(reader_page_next(text, 20, 10, layout, &page));
    CHECK_EQUAL(10, page.start);
    CHECK_EQUAL(10, page.length);
    CHECK_EQUAL(2, page.lines);
}

static void the_last_page_stops_at_the_end(void)
{
    const char *text = "A\nB\n";
    ReaderLayout layout = {&TEST_FONT, 100, 5, 0};
    ReaderPage page;
    CHECK(reader_page_next(text, 4, 0, layout, &page));
    CHECK_EQUAL(4, page.length);
    CHECK_EQUAL(2, page.lines);
    CHECK(!reader_page_next(text, 4, 4, layout, &page));
}

static void an_offset_past_the_end_is_rejected(void)
{
    ReaderLayout layout = {&TEST_FONT, 100, 3, 0};
    ReaderPage page;
    CHECK(!reader_page_next("A", 1, 2, layout, &page));
}

static void the_previous_page_walks_back_one_page(void)
{
    const char *text = "A\nB\nC\nD\nE\nF\n";
    ReaderLayout layout = {&TEST_FONT, 100, 2, 0};
    CHECK_EQUAL(0, reader_page_previous(text, 12, 4, layout));
    CHECK_EQUAL(4, reader_page_previous(text, 12, 8, layout));
    CHECK_EQUAL(0, reader_page_previous(text, 12, 0, layout));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(ascii_wraps_at_a_space),
        TEST_CASE(a_word_longer_than_the_line_is_split),
        TEST_CASE(cjk_wraps_between_characters),
        TEST_CASE(a_newline_ends_the_line),
        TEST_CASE(a_carriage_return_joins_a_newline),
        TEST_CASE(a_character_wider_than_the_line_still_advances),
        TEST_CASE(a_closing_punctuation_hangs_on_the_line),
        TEST_CASE(an_opening_punctuation_moves_to_the_next_line),
        TEST_CASE(a_paragraph_starts_at_the_text_or_after_a_newline),
        TEST_CASE(an_indented_line_holds_fewer_characters),
        TEST_CASE(a_page_holds_the_requested_lines),
        TEST_CASE(a_new_paragraph_gets_the_indent_again),
        TEST_CASE(the_last_page_stops_at_the_end),
        TEST_CASE(an_offset_past_the_end_is_rejected),
        TEST_CASE(the_previous_page_walks_back_one_page),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
