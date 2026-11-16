#include "screens/reader_screen.h"

#include "app/reader_page.h"
#include "gfx/text.h"
#include "ui/header.h"
#include "ui/navigation.h"

enum {
    READER_MARGIN = 6,
    READER_LINE_GAP = 2,
    READER_TOP_MARGIN = 4
};

static const Font *reader_font(const ReaderScreen *reader)
{
    return reader->large ? reader->theme->fonts.body : reader->theme->fonts.small;
}

static ReaderLayout reader_layout(const ReaderScreen *reader, Rect content)
{
    const Font *font = reader_font(reader);
    int lines = content.height / (font_line_height(font) + READER_LINE_GAP);
    return (ReaderLayout){
        .font = font, .width = content.width - 2 * READER_MARGIN, .lines = lines > 0 ? lines : 1};
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    ReaderScreen *reader = context;
    (void)now_ms;
    if (event.gesture != KEY_GESTURE_PRESS && event.gesture != KEY_GESTURE_REPEAT) {
        return navigation_exit_action(event);
    }
    ReaderLayout layout = reader_layout(reader, theme_content_area(reader->theme));
    if (event.key == KEY_VOLUME_DOWN) {
        ReaderPage page;
        if (reader_page_next(reader->text, reader->length, reader->offset, layout, &page)) {
            reader->offset += page.length;
        }
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_UP) {
        reader->offset = reader_page_previous(reader->text, reader->length, reader->offset, layout);
        return screen_action_none();
    }
    if (event.key == KEY_PLAY) {
        reader->large = !reader->large;
        return screen_action_none();
    }
    return navigation_exit_action(event);
}

static void draw_line(Canvas *canvas, TextStyle style, Point pen, const ReaderScreen *reader,
                      uint32_t offset, size_t bytes)
{
    while (bytes > 0 &&
           (reader->text[offset + bytes - 1] == '\n' || reader->text[offset + bytes - 1] == '\r')) {
        bytes--;
    }
    text_draw_span(canvas, style, pen, (TextSpan){reader->text + offset, bytes});
}

static void draw(void *context, Canvas *canvas)
{
    ReaderScreen *reader = context;
    const Theme *theme = reader->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, reader->screen.title);
    ReaderLayout layout = reader_layout(reader, content);
    TextStyle style = {.font = layout.font, .color = theme->colors.text};
    int line_height = font_line_height(layout.font) + READER_LINE_GAP;
    uint32_t offset = reader->offset;
    int y = content.y + READER_TOP_MARGIN;
    for (int line = 0; line < layout.lines && offset < reader->length; line++) {
        size_t bytes = reader_line_bytes(layout.font, reader->text + offset,
                                         reader->length - offset, layout.width);
        if (bytes == 0) {
            break;
        }
        draw_line(canvas, style, (Point){content.x + READER_MARGIN, y}, reader, offset, bytes);
        offset += (uint32_t)bytes;
        y += line_height;
    }
}

Screen *reader_screen_init(ReaderScreen *reader, const Theme *theme, const char *text,
                           uint32_t length)
{
    *reader = (ReaderScreen){.theme = theme, .text = text, .length = length, .large = true};
    reader->screen = (Screen){
        .title = "Reader",
        .stays_on_wake = true,
        .context = reader,
        .handle_key = handle_key,
        .draw = draw,
    };
    return &reader->screen;
}
