#include "screens/reader_screen.h"

#include "app/reader_book.h"
#include "app/reader_page.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "gfx/text.h"
#include "hal/storage.h"
#include "hal/system.h"
#include "ui/header.h"
#include "ui/label.h"
#include "ui/marquee.h"
#include "ui/navigation.h"

enum {
    READER_MARGIN = 8,
    READER_LINE_GAP = 2,
    READER_TOP_MARGIN = 6,
    READER_INDENT_CHARS = 2,
    READER_INDENT_CODEPOINT = 0x4E00,
    READER_FLIP_MS = 200,
    READER_FLIP_FRAME_MS = 16
};

enum {
    READER_WINDOW_BYTES = 3072,
    READER_VIEW_GUARD = 1024,
    READER_BOM_BYTES = 3
};

static const char READER_BOOK_FALLBACK_PATH[] = "/Books/reader.txt";
static const char READER_BOOKS_FILE[] = "/.echo/books.bin";
static const char READER_PROGRESS_FILE[] = "/.echo/reader.bin";
static const char READER_INDEX_FILE[] = "/.echo/reader.idx";

enum {
    READER_PROGRESS_HEADER_BYTES = 8,
    READER_PROGRESS_ENTRY_BYTES = 12,
    READER_PROGRESS_MAX = 128
};

enum {
    READER_INDEX_HEADER_BYTES = 24,
    READER_INDEX_ENTRY_BYTES = 4,
    READER_INDEX_BATCH = 32,
    READER_INDEX_VERSION = 1,
    READER_INDEX_PAGE_MAX = (READER_INDEX_FILE_BYTES - READER_INDEX_HEADER_BYTES) /
        READER_INDEX_ENTRY_BYTES,
    READER_ROW_FOLDER = 0x80
};

static HalFile books_file;
static bool books_open;
static HalFile book_file;
static bool book_open;
static HalFile index_file;
static bool index_open;
static bool index_ready;
static uint32_t index_count;
static uint32_t index_layout;
static uint32_t book_text_start;
static uint32_t window_base;
static uint32_t window_bytes;
static char window[READER_WINDOW_BYTES];
static char current_path[READER_BOOKS_PATH_CAPACITY];
static char slot_path[READER_BOOKS_PATH_CAPACITY];
static char browse_path[READER_BOOKS_PATH_CAPACITY];
static uint8_t row_record[READER_BOOKS_MAX];
static int row_count;

static const Font *reader_font(const ReaderScreen *reader)
{
    return reader->large ? reader->theme->fonts.body : reader->theme->fonts.small;
}

static ReaderLayout reader_layout(const ReaderScreen *reader, Rect content)
{
    const Font *font = reader_font(reader);
    int line_height = font_line_height(font) + READER_LINE_GAP;
    int usable = content.height - READER_TOP_MARGIN - font->ascent - font->descent;
    int lines = usable > 0 ? usable / line_height + 1 : 1;
    int indent = READER_INDENT_CHARS * font_find_glyph(font, READER_INDENT_CODEPOINT)->advance;
    return (ReaderLayout){
        .font = font, .width = content.width - 2 * READER_MARGIN, .lines = lines, .indent = indent};
}

static void update_counter(ReaderScreen *reader)
{
    StringBuilder builder = string_builder_make(reader->counter, sizeof reader->counter);
    string_builder_append_unsigned(&builder, (uint32_t)reader->page_index);
    string_builder_append_char(&builder, '/');
    string_builder_append_unsigned(&builder, (uint32_t)reader->page_total);
}

static void close_books(void)
{
    if (books_open) {
        hal_file_close(&books_file);
    }
    books_open = false;
    books_file.handle = -1;
    books_file.size = 0;
}

static void close_index(void)
{
    if (index_open) {
        hal_file_close(&index_file);
    }
    index_open = false;
    index_ready = false;
    index_count = 0;
    index_file.handle = -1;
    index_file.size = 0;
}

static void close_book(void)
{
    if (book_open) {
        hal_file_close(&book_file);
    }
    book_open = false;
    book_file.handle = -1;
    book_file.size = 0;
    book_text_start = 0;
    window_base = 0;
    window_bytes = 0;
    close_index();
}

static void close_reader(void)
{
    close_book();
    close_books();
}

static bool window_load(uint32_t base)
{
    if (!book_open) {
        return false;
    }
    uint32_t offset = book_text_start + base;
    if (offset >= book_file.size) {
        return false;
    }
    uint32_t available = book_file.size - offset;
    uint32_t bytes = available < READER_WINDOW_BYTES ? available : READER_WINDOW_BYTES;
    if (!hal_file_read(book_file, offset, window, bytes)) {
        return false;
    }
    window_base = base;
    window_bytes = bytes;
    return true;
}

static const char *reader_view(const ReaderScreen *reader, uint32_t start, uint32_t *length,
                               uint32_t *skip)
{
    uint32_t base = start > 0 ? start - 1 : 0;
    if (reader->text != NULL) {
        *length = reader->length - base;
        *skip = start - base;
        return reader->text + base;
    }
    uint32_t room = 0;
    if (base >= window_base && start >= window_base && start - window_base < window_bytes) {
        room = window_bytes - (start - window_base);
    }
    uint32_t left = reader->length - start;
    uint32_t guard = left < READER_VIEW_GUARD ? left : READER_VIEW_GUARD;
    if (room < guard && !window_load(base)) {
        return NULL;
    }
    *length = window_bytes;
    *skip = start - window_base;
    return window;
}

static bool page_span(const ReaderScreen *reader, ReaderLayout layout, uint32_t start,
                      uint32_t *next, int *lines)
{
    uint32_t length = 0;
    uint32_t skip = 0;
    const char *text = reader_view(reader, start, &length, &skip);
    if (text == NULL) {
        return false;
    }
    ReaderPage page;
    if (!reader_page_next(text, length, skip, layout, &page) || page.length == 0) {
        return false;
    }
    *next = start + (uint32_t)page.length;
    if (lines != NULL) {
        *lines = page.lines;
    }
    return true;
}

static uint32_t path_hash(const char *path)
{
    uint32_t hash = 2166136261u;
    for (; *path != '\0'; path++) {
        hash ^= (uint32_t)(uint8_t)*path;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t read_word(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static void write_word(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static uint32_t index_layout_id(const ReaderScreen *reader)
{
    return reader->large ? 1u : 0u;
}

static void index_header(const ReaderScreen *reader, uint32_t count, uint8_t *header)
{
    header[0] = 'E';
    header[1] = 'I';
    header[2] = 'D';
    header[3] = 'X';
    header[4] = READER_INDEX_VERSION;
    header[5] = (uint8_t)index_layout_id(reader);
    header[6] = 0;
    header[7] = 0;
    write_word(header + 8, reader->length);
    write_word(header + 12, path_hash(current_path));
    write_word(header + 16, count);
    write_word(header + 20, 0);
}

static bool index_load(const ReaderScreen *reader)
{
    close_index();
    if (!hal_file_open(&index_file, READER_INDEX_FILE)) {
        return false;
    }
    index_open = true;
    uint8_t header[READER_INDEX_HEADER_BYTES];
    bool valid = index_file.size >= READER_INDEX_HEADER_BYTES &&
                 hal_file_read(index_file, 0, header, sizeof header) && header[0] == 'E' &&
                 header[1] == 'I' && header[2] == 'D' && header[3] == 'X' &&
                 header[4] == READER_INDEX_VERSION &&
                 header[5] == (uint8_t)index_layout_id(reader) &&
                 read_word(header + 8) == reader->length &&
                 read_word(header + 12) == path_hash(current_path);
    uint32_t count = valid ? read_word(header + 16) : 0;
    if (valid && (count == 0 || count > READER_INDEX_PAGE_MAX ||
                  index_file.size < READER_INDEX_HEADER_BYTES + count * READER_INDEX_ENTRY_BYTES)) {
        valid = false;
    }
    if (!valid) {
        close_index();
        return false;
    }
    index_count = count;
    index_layout = (uint32_t)header[5];
    index_ready = true;
    return true;
}

static bool index_start(uint32_t page, uint32_t *start)
{
    if (!index_ready || page >= index_count) {
        return false;
    }
    uint8_t bytes[READER_INDEX_ENTRY_BYTES];
    if (!hal_file_read(index_file, READER_INDEX_HEADER_BYTES + page * READER_INDEX_ENTRY_BYTES,
                       bytes, sizeof bytes)) {
        return false;
    }
    *start = read_word(bytes);
    return true;
}

static uint32_t index_page_of(uint32_t offset)
{
    uint32_t low = 0;
    uint32_t high = index_count;
    while (low + 1 < high) {
        uint32_t middle = low + (high - low) / 2;
        uint32_t start = 0;
        if (!index_start(middle, &start)) {
            break;
        }
        if (start <= offset) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

static bool index_build(const ReaderScreen *reader, ReaderLayout layout)
{
    close_index();
    HalFile file;
    if (!hal_file_open(&file, READER_INDEX_FILE)) {
        return false;
    }
    uint32_t capacity = file.size > READER_INDEX_HEADER_BYTES
                            ? (file.size - READER_INDEX_HEADER_BYTES) / READER_INDEX_ENTRY_BYTES
                            : 0;
    hal_file_close(&file);
    uint8_t batch[READER_INDEX_BATCH * READER_INDEX_ENTRY_BYTES];
    uint8_t header[READER_INDEX_HEADER_BYTES];
    index_header(reader, 0, header);
    if (!hal_file_write_at(READER_INDEX_FILE, 0, header, sizeof header)) {
        return false;
    }
    uint32_t batch_first = 0;
    uint32_t batched = 0;
    uint32_t count = 0;
    uint32_t cursor = 0;
    bool storing = capacity > 0;
    while (cursor < reader->length) {
        uint32_t next = 0;
        if (!page_span(reader, layout, cursor, &next, NULL)) {
            break;
        }
        if (storing) {
            if (count >= capacity) {
                storing = false;
            } else {
                if (batched == 0) {
                    batch_first = count;
                }
                write_word(batch + batched * READER_INDEX_ENTRY_BYTES, cursor);
                batched++;
                if (batched == READER_INDEX_BATCH) {
                    storing = hal_file_write_at(READER_INDEX_FILE,
                                                READER_INDEX_HEADER_BYTES +
                                                    batch_first * READER_INDEX_ENTRY_BYTES,
                                                batch, batched * READER_INDEX_ENTRY_BYTES);
                    batched = 0;
                }
            }
        }
        count++;
        cursor = next;
        if ((count & 15u) == 0) {
            hal_system_keep_alive();
        }
    }
    if (storing && batched > 0) {
        storing = hal_file_write_at(
            READER_INDEX_FILE, READER_INDEX_HEADER_BYTES + batch_first * READER_INDEX_ENTRY_BYTES,
            batch, batched * READER_INDEX_ENTRY_BYTES);
    }
    if (!storing || count == 0 || count > capacity || cursor < reader->length) {
        index_header(reader, 0, header);
        (void)hal_file_write_at(READER_INDEX_FILE, 0, header, sizeof header);
        return false;
    }
    index_header(reader, count, header);
    if (!hal_file_write_at(READER_INDEX_FILE, 0, header, sizeof header)) {
        return false;
    }
    return index_load(reader);
}

static void recount_for(ReaderScreen *reader)
{
    ReaderLayout layout = reader_layout(reader, theme_content_area(reader->theme));
    if (reader->length == 0) {
        reader->page_total = 1;
        reader->page_index = 1;
        update_counter(reader);
        return;
    }
    if (reader->text == NULL) {
        if (index_ready && index_layout != index_layout_id(reader)) {
            close_index();
        }
        if (!index_ready && !index_load(reader)) {
            (void)index_build(reader, layout);
        }
        if (index_ready) {
            uint32_t page = index_page_of(reader->offset);
            uint32_t start = 0;
            if (index_start(page, &start) && start <= reader->offset) {
                reader->offset = start;
            }
            reader->page_total = (int)index_count;
            reader->page_index = (int)page + 1;
            update_counter(reader);
            return;
        }
    }
    uint32_t cursor = 0;
    int total = 0;
    int index = 1;
    while (cursor < reader->length) {
        uint32_t next = 0;
        if (!page_span(reader, layout, cursor, &next, NULL)) {
            break;
        }
        total++;
        if (cursor <= reader->offset) {
            index = total;
        }
        cursor = next;
    }
    reader->page_total = total > 0 ? total : 1;
    reader->page_index = index;
    update_counter(reader);
}

static void set_current_path(const char *path)
{
    uint32_t index = 0;
    for (; path[index] != '\0' && index + 1 < (uint32_t)sizeof current_path; index++) {
        current_path[index] = path[index];
    }
    current_path[index] = '\0';
}

static void load_progress(ReaderScreen *reader)
{
    reader->offset = 0;
    if (current_path[0] == '\0' || reader->length == 0) {
        return;
    }
    HalFile file;
    if (!hal_file_open(&file, READER_PROGRESS_FILE)) {
        return;
    }
    uint32_t hash = path_hash(current_path);
    uint8_t header[READER_PROGRESS_HEADER_BYTES];
    if (file.size >= READER_PROGRESS_HEADER_BYTES &&
        hal_file_read(file, 0, header, sizeof header) && header[0] == 'E' && header[1] == 'R' &&
        header[2] == 'D' && header[3] == 'G' && header[4] == 1) {
        uint32_t count = (uint32_t)header[6] | (uint32_t)header[7] << 8;
        if (count > READER_PROGRESS_MAX) {
            count = READER_PROGRESS_MAX;
        }
        for (uint32_t index = 0; index < count; index++) {
            uint8_t entry[READER_PROGRESS_ENTRY_BYTES];
            if (!hal_file_read(file,
                               READER_PROGRESS_HEADER_BYTES + index * READER_PROGRESS_ENTRY_BYTES,
                               entry, sizeof entry)) {
                break;
            }
            if (read_word(entry) == hash && read_word(entry + 4) == reader->length) {
                uint32_t offset = read_word(entry + 8);
                if (offset < reader->length) {
                    reader->offset = offset;
                }
                break;
            }
        }
    }
    hal_file_close(&file);
}

static void save_progress(ReaderScreen *reader)
{
    if (!reader->file_loaded || current_path[0] == '\0' || reader->length == 0) {
        return;
    }
    uint32_t hash = path_hash(current_path);
    uint8_t header[READER_PROGRESS_HEADER_BYTES] = {'E', 'R', 'D', 'G', 1, 0, 0, 0};
    uint32_t count = 0;
    int slot = -1;
    bool valid = false;
    HalFile file;
    if (hal_file_open(&file, READER_PROGRESS_FILE)) {
        if (file.size >= READER_PROGRESS_HEADER_BYTES &&
            hal_file_read(file, 0, header, sizeof header) && header[0] == 'E' && header[1] == 'R' &&
            header[2] == 'D' && header[3] == 'G' && header[4] == 1) {
            valid = true;
            count = (uint32_t)header[6] | (uint32_t)header[7] << 8;
            if (count > READER_PROGRESS_MAX) {
                count = READER_PROGRESS_MAX;
            }
            for (uint32_t index = 0; index < count; index++) {
                uint8_t entry[READER_PROGRESS_ENTRY_BYTES];
                if (!hal_file_read(
                        file, READER_PROGRESS_HEADER_BYTES + index * READER_PROGRESS_ENTRY_BYTES,
                        entry, sizeof entry)) {
                    break;
                }
                if (read_word(entry) == hash && read_word(entry + 4) == reader->length) {
                    slot = (int)index;
                    break;
                }
            }
        }
        hal_file_close(&file);
    }
    if (!valid) {
        static const uint8_t fresh[READER_PROGRESS_HEADER_BYTES] = {'E', 'R', 'D', 'G', 1, 0, 0, 0};
        (void)hal_file_write_at(READER_PROGRESS_FILE, 0, fresh, sizeof fresh);
        count = 0;
    }
    if (slot < 0) {
        if (count >= READER_PROGRESS_MAX) {
            return;
        }
        slot = (int)count;
    }
    uint8_t entry[READER_PROGRESS_ENTRY_BYTES];
    write_word(entry, hash);
    write_word(entry + 4, reader->length);
    write_word(entry + 8, reader->offset);
    if (!hal_file_write_at(READER_PROGRESS_FILE,
                           READER_PROGRESS_HEADER_BYTES +
                               (uint32_t)slot * READER_PROGRESS_ENTRY_BYTES,
                           entry, sizeof entry)) {
        return;
    }
    if ((uint32_t)slot == count) {
        uint32_t next = count + 1;
        uint8_t bytes[2] = {(uint8_t)next, (uint8_t)(next >> 8)};
        (void)hal_file_write_at(READER_PROGRESS_FILE, 6, bytes, sizeof bytes);
    }
}

static bool open_book_file(ReaderScreen *reader, const char *path)
{
    HalFile file;
    if (!hal_file_open(&file, path)) {
        return false;
    }
    uint8_t bom[READER_BOM_BYTES];
    uint32_t start = 0;
    if (file.size >= READER_BOM_BYTES && hal_file_read(file, 0, bom, sizeof bom) &&
        bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) {
        start = READER_BOM_BYTES;
    }
    save_progress(reader);
    close_book();
    book_file = file;
    book_open = true;
    book_text_start = start;
    reader->text = NULL;
    reader->length = file.size - start;
    reader->offset = 0;
    reader->file_loaded = true;
    reader->slide.active = false;
    set_current_path(path);
    return true;
}

static int load_books(void)
{
    close_books();
    if (!hal_file_open(&books_file, READER_BOOKS_FILE)) {
        return 0;
    }
    books_open = true;
    uint8_t header[READER_BOOKS_HEADER_BYTES];
    if (books_file.size < READER_BOOKS_HEADER_BYTES ||
        !hal_file_read(books_file, 0, header, sizeof header) || header[0] != 'E' ||
        header[1] != 'B' || header[2] != 'K' || header[3] != 'S' || header[4] != 1) {
        close_books();
        return 0;
    }
    uint32_t stored = (uint32_t)header[6] | (uint32_t)header[7] << 8;
    uint32_t fit = (books_file.size - READER_BOOKS_HEADER_BYTES) / READER_BOOKS_PATH_CAPACITY;
    if (stored > fit) {
        stored = fit;
    }
    if (stored > READER_BOOKS_MAX) {
        stored = READER_BOOKS_MAX;
    }
    return (int)stored;
}

static bool book_path_at(const ReaderScreen *reader, int index, char *path)
{
    if (!books_open || index < 0 || index >= reader->book_count) {
        return false;
    }
    uint32_t offset =
        READER_BOOKS_HEADER_BYTES + (uint32_t)index * (uint32_t)READER_BOOKS_PATH_CAPACITY;
    if (offset >= books_file.size) {
        return false;
    }
    uint32_t available = books_file.size - offset;
    uint32_t chunk = available < (uint32_t)READER_BOOKS_PATH_CAPACITY
                         ? available
                         : (uint32_t)READER_BOOKS_PATH_CAPACITY;
    if (!hal_file_read(books_file, offset, path, chunk)) {
        return false;
    }
    path[chunk < (uint32_t)READER_BOOKS_PATH_CAPACITY ? chunk
                                                      : (uint32_t)READER_BOOKS_PATH_CAPACITY - 1] =
        '\0';
    return path[0] != '\0';
}

static uint32_t text_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static bool same_text(const char *text, int length, const char *other)
{
    for (int index = 0; index < length; index++) {
        if (text[index] != other[index]) {
            return false;
        }
    }
    return other[length] == '\0';
}

static bool text_begins(const char *text, const char *prefix)
{
    uint32_t index = 0;
    for (; prefix[index] != '\0'; index++) {
        if (text[index] != prefix[index]) {
            return false;
        }
    }
    return true;
}

static const char *child_component(const char *path, const char *folder, int *length,
                                   bool *is_folder)
{
    uint32_t prefix = text_length(folder);
    uint32_t start = 1;
    if (prefix > 0) {
        if (!text_begins(path, folder) || path[prefix] != '/') {
            return NULL;
        }
        start = prefix + 1;
    } else if (path[0] != '/') {
        return NULL;
    }
    uint32_t end = start;
    while (path[end] != '\0' && path[end] != '/') {
        end++;
    }
    if (end == start || end - start >= (uint32_t)READER_BOOKS_PATH_CAPACITY) {
        return NULL;
    }
    *length = (int)(end - start);
    *is_folder = path[end] == '/';
    return path + start;
}

static void picker_rows(ReaderScreen *reader, const char *focus)
{
    int count = 0;
    int selected = -1;
    char last_folder[READER_BOOKS_PATH_CAPACITY];
    for (int pass = 0; pass < 2; pass++) {
        bool folders = pass == 0;
        bool have_folder = false;
        for (uint32_t index = 0; index < (uint32_t)reader->book_count; index++) {
            if (!book_path_at(reader, (int)index, slot_path)) {
                continue;
            }
            int length = 0;
            bool folder = false;
            const char *name = child_component(slot_path, browse_path, &length, &folder);
            if (name == NULL || folder != folders) {
                continue;
            }
            if (folder) {
                if (have_folder && same_text(name, length, last_folder)) {
                    continue;
                }
                for (int item = 0; item < length; item++) {
                    last_folder[item] = name[item];
                }
                last_folder[length] = '\0';
                have_folder = true;
            }
            if (count >= READER_BOOKS_MAX) {
                break;
            }
            row_record[count] = (uint8_t)(index | (folder ? (uint32_t)READER_ROW_FOLDER : 0u));
            if (focus != NULL && same_text(name, length, focus)) {
                selected = count;
            }
            count++;
        }
    }
    row_count = count;
    reader->book_index = selected >= 0 ? selected : 0;
}

static void picker_folder(ReaderScreen *reader, const char *folder, const char *focus)
{
    uint32_t length = text_length(folder);
    if (length >= (uint32_t)sizeof browse_path) {
        length = (uint32_t)sizeof browse_path - 1;
    }
    for (uint32_t index = 0; index < length; index++) {
        browse_path[index] = folder[index];
    }
    browse_path[length] = '\0';
    picker_rows(reader, focus);
}

static void picker_enter(ReaderScreen *reader)
{
    if (reader->book_index < 0 || reader->book_index >= row_count) {
        return;
    }
    uint8_t row = row_record[reader->book_index];
    if ((row & READER_ROW_FOLDER) == 0 || !book_path_at(reader, row & 0x7F, slot_path)) {
        return;
    }
    int length = 0;
    bool folder = false;
    const char *name = child_component(slot_path, browse_path, &length, &folder);
    if (name == NULL || !folder) {
        return;
    }
    uint32_t used = text_length(browse_path);
    if (used + 1 + (uint32_t)length >= (uint32_t)sizeof browse_path) {
        return;
    }
    browse_path[used++] = '/';
    for (int index = 0; index < length; index++) {
        browse_path[used + (uint32_t)index] = name[index];
    }
    browse_path[used + (uint32_t)length] = '\0';
    picker_rows(reader, NULL);
}

static void picker_up(ReaderScreen *reader)
{
    uint32_t length = text_length(browse_path);
    if (length == 0) {
        return;
    }
    uint32_t cut = length;
    while (cut > 0 && browse_path[cut - 1] != '/') {
        cut--;
    }
    char focus[READER_BOOKS_PATH_CAPACITY];
    uint32_t name_length = length - cut;
    if (name_length >= (uint32_t)sizeof focus) {
        return;
    }
    for (uint32_t index = 0; index < name_length; index++) {
        focus[index] = browse_path[cut + index];
    }
    focus[name_length] = '\0';
    browse_path[cut > 0 ? cut - 1 : 0] = '\0';
    picker_rows(reader, focus);
}

static void picker_home(ReaderScreen *reader)
{
    if (current_path[0] == '\0') {
        picker_folder(reader, "", NULL);
        return;
    }
    uint32_t end = 0;
    for (uint32_t index = 0; current_path[index] != '\0'; index++) {
        if (current_path[index] == '/') {
            end = index;
        }
    }
    char focus[READER_BOOKS_PATH_CAPACITY];
    uint32_t length = text_length(current_path + end + 1);
    if (length >= (uint32_t)sizeof focus) {
        length = (uint32_t)sizeof focus - 1;
    }
    for (uint32_t index = 0; index < length; index++) {
        focus[index] = current_path[end + 1 + index];
    }
    focus[length] = '\0';
    uint32_t folder_length = end;
    for (uint32_t index = 0; index < folder_length; index++) {
        browse_path[index] = current_path[index];
    }
    browse_path[folder_length] = '\0';
    picker_rows(reader, focus);
}

static void open_book(ReaderScreen *reader, int record)
{
    if (!book_path_at(reader, record, slot_path)) {
        return;
    }
    if (!open_book_file(reader, slot_path)) {
        return;
    }
    reader->picker_open = false;
    reader->book_index = 0;
    load_progress(reader);
    recount_for(reader);
}

static void enter(void *context, uint32_t now_ms)
{
    (void)now_ms;
    ReaderScreen *reader = context;
    reader->book_count = load_books();
    reader->book_index = 0;
    reader->slide.active = false;
    if (reader->book_count > 0 && !reader->file_loaded) {
        picker_folder(reader, "", NULL);
        reader->picker_open = true;
        return;
    }
    reader->picker_open = false;
    if (reader->file_loaded && reader->text == NULL && !book_open && current_path[0] != '\0') {
        if (open_book_file(reader, current_path)) {
            load_progress(reader);
            recount_for(reader);
            return;
        }
        reader->file_loaded = false;
        reader->length = 0;
    }
    if (!reader->file_loaded && reader->text == NULL) {
        if (open_book_file(reader, READER_BOOK_FALLBACK_PATH)) {
            load_progress(reader);
        }
    }
    recount_for(reader);
}

static uint32_t previous_page(const ReaderScreen *reader, ReaderLayout layout, uint32_t from)
{
    uint32_t previous = 0;
    uint32_t cursor = 0;
    while (cursor < from) {
        uint32_t next = 0;
        if (!page_span(reader, layout, cursor, &next, NULL)) {
            break;
        }
        previous = cursor;
        if (next >= from) {
            break;
        }
        cursor = next;
    }
    return previous;
}

static void start_flip(ReaderScreen *reader, ReaderLayout layout, bool forward, uint32_t now_ms)
{
    uint32_t from = reader->offset;
    uint32_t to = from;
    int index = reader->page_index;
    if (reader->text == NULL && index_ready) {
        int target = forward ? reader->page_index + 1 : reader->page_index - 1;
        uint32_t start = 0;
        if (target < 1 || target > reader->page_total ||
            !index_start((uint32_t)(target - 1), &start)) {
            return;
        }
        to = start;
        index = target;
    } else if (forward) {
        if (reader->page_index >= reader->page_total) {
            return;
        }
        uint32_t next = 0;
        if (!page_span(reader, layout, from, &next, NULL)) {
            return;
        }
        to = next;
        index = reader->page_index + 1;
    } else {
        if (reader->page_index <= 1) {
            return;
        }
        uint32_t previous = previous_page(reader, layout, from);
        if (previous == from) {
            return;
        }
        to = previous;
        index = reader->page_index - 1;
    }
    if (to == from) {
        return;
    }
    reader->from_offset = from;
    reader->offset = to;
    reader->page_index = index;
    update_counter(reader);
    save_progress(reader);
    slide_start(&reader->slide, now_ms, forward ? SLIDE_FORWARD : SLIDE_BACK, READER_FLIP_MS);
}

static void update(void *context, uint32_t now_ms)
{
    ReaderScreen *reader = context;
    if (!reader->slide.active) {
        return;
    }
    reader->flip_y =
        slide_progress(&reader->slide, now_ms, theme_content_area(reader->theme).height);
    if (slide_finished(&reader->slide, now_ms)) {
        reader->slide.active = false;
    }
}

static ScreenAction leave_reader(ReaderScreen *reader, KeyEvent event)
{
    if (event.key == KEY_BACK) {
        save_progress(reader);
        close_reader();
    }
    return navigation_exit_action(event);
}

static ScreenAction handle_key(void *context, KeyEvent event, uint32_t now_ms)
{
    ReaderScreen *reader = context;
    if (reader->picker_open) {
        if (event.gesture == KEY_GESTURE_PRESS || event.gesture == KEY_GESTURE_REPEAT) {
            if (event.key == KEY_VOLUME_DOWN && reader->book_index + 1 < row_count) {
                reader->book_index++;
                return screen_action_none();
            }
            if (event.key == KEY_VOLUME_UP && reader->book_index > 0) {
                reader->book_index--;
                return screen_action_none();
            }
            if (event.key == KEY_PLAY && reader->book_index >= 0 &&
                reader->book_index < row_count) {
                uint8_t row = row_record[reader->book_index];
                if ((row & READER_ROW_FOLDER) != 0) {
                    picker_enter(reader);
                } else {
                    open_book(reader, row & 0x7F);
                }
                return screen_action_none();
            }
        }
        if (event.key == KEY_BACK && event.gesture == KEY_GESTURE_PRESS) {
            if (browse_path[0] != '\0') {
                picker_up(reader);
                return screen_action_none();
            }
            if (reader->length > 0) {
                reader->picker_open = false;
                return screen_action_none();
            }
        }
        return leave_reader(reader, event);
    }
    if (event.key == KEY_PLAY && event.gesture == KEY_GESTURE_LONG_PRESS) {
        if (reader->book_count > 0) {
            picker_home(reader);
            reader->picker_open = true;
        }
        return screen_action_none();
    }
    if (event.gesture != KEY_GESTURE_PRESS && event.gesture != KEY_GESTURE_REPEAT) {
        return leave_reader(reader, event);
    }
    if (reader->slide.active) {
        return leave_reader(reader, event);
    }
    ReaderLayout layout = reader_layout(reader, theme_content_area(reader->theme));
    if (event.key == KEY_VOLUME_DOWN) {
        start_flip(reader, layout, true, now_ms);
        return screen_action_none();
    }
    if (event.key == KEY_VOLUME_UP) {
        start_flip(reader, layout, false, now_ms);
        return screen_action_none();
    }
    if (event.key == KEY_PLAY) {
        reader->large = !reader->large;
        recount_for(reader);
        save_progress(reader);
        return screen_action_none();
    }
    return leave_reader(reader, event);
}

static void draw_span(Canvas *canvas, TextStyle style, Point pen, const char *text, size_t bytes)
{
    while (bytes > 0 && (text[bytes - 1] == '\n' || text[bytes - 1] == '\r')) {
        bytes--;
    }
    text_draw_span(canvas, style, pen, (TextSpan){text, bytes});
}

static Canvas canvas_shifted_vertical(const Canvas *canvas, int dy, Rect area)
{
    Canvas shifted = *canvas;
    shifted.bounds.y -= dy;
    shifted.clip.y -= dy;
    Rect visible = {area.x, area.y - dy, area.width, area.height};
    shifted.clip = rect_intersection(shifted.clip, visible);
    return shifted;
}

static void draw_page(Canvas *canvas, ReaderScreen *reader, uint32_t start, int dy)
{
    const Theme *theme = reader->theme;
    Rect content = theme_content_area(theme);
    Canvas page = canvas_shifted_vertical(canvas, dy, content);
    if (rect_is_empty(page.clip)) {
        return;
    }
    canvas_fill_rect(&page, content, theme->colors.background);
    ReaderLayout layout = reader_layout(reader, content);
    TextStyle style = {.font = layout.font, .color = theme->colors.text};
    int line_height = font_line_height(layout.font) + READER_LINE_GAP;
    uint32_t offset = start;
    int y = content.y + READER_TOP_MARGIN + layout.font->ascent;
    for (int line = 0; line < layout.lines && offset < reader->length; line++) {
        uint32_t length = 0;
        uint32_t skip = 0;
        const char *text = reader_view(reader, offset, &length, &skip);
        if (text == NULL) {
            break;
        }
        int indent = reader_starts_paragraph(text, skip) ? layout.indent : 0;
        size_t bytes =
            reader_line_bytes(layout.font, text + skip, length - skip, layout.width, indent);
        if (bytes == 0) {
            break;
        }
        draw_span(&page, style, (Point){content.x + READER_MARGIN + indent, y}, text + skip, bytes);
        offset += (uint32_t)bytes;
        y += line_height;
    }
}

static void draw_counter(Canvas *canvas, const ReaderScreen *reader)
{
    const Theme *theme = reader->theme;
    int inset = theme->metrics.header_inset > theme->metrics.padding ? theme->metrics.header_inset
                                                                     : theme->metrics.padding;
    Rect row = theme_title_area(theme);
    Rect bounds = {row.x + inset, row.y, row.width - 2 * inset, row.height - 1};
    TextStyle style = {.font = theme->fonts.small, .color = theme->colors.text_secondary};
    Rect area = label_centered_area(style.font, bounds, reader->counter);
    area.x = bounds.x + bounds.width - area.width;
    label_draw(canvas, style, (Label){.area = area, .text = reader->counter});
}

static void draw_books(Canvas *canvas, ReaderScreen *reader)
{
    const Theme *theme = reader->theme;
    Rect content = theme_content_area(theme);
    int row_height = theme->metrics.row_height;
    int visible = row_height > 0 ? content.height / row_height : 1;
    if (visible > row_count) {
        visible = row_count;
    }
    int start = reader->book_index - visible / 2;
    if (start < 0) {
        start = 0;
    }
    if (start + visible > row_count) {
        start = row_count - visible;
    }
    for (int row = 0; row < visible; row++) {
        int index = start + row;
        if (!book_path_at(reader, row_record[index] & 0x7F, slot_path)) {
            continue;
        }
        int length = 0;
        bool folder = false;
        const char *name = child_component(slot_path, browse_path, &length, &folder);
        if (name == NULL) {
            continue;
        }
        slot_path[(uint32_t)(name - slot_path) + (uint32_t)length] = '\0';
        bool selected = index == reader->book_index;
        Rect area = {content.x, content.y + row * row_height, content.width, row_height};
        if (selected) {
            canvas_fill_rect(canvas, area, theme->colors.selection_background);
        }
        TextStyle style = {.font = theme->fonts.body,
                           .color = selected ? theme->colors.selection_text : theme->colors.text};
        Rect text = {area.x + READER_MARGIN, area.y, area.width - 2 * READER_MARGIN, area.height};
        label_draw(canvas, style, (Label){.area = text, .text = name});
    }
}

static void draw_no_book(Canvas *canvas, const ReaderScreen *reader)
{
    const Theme *theme = reader->theme;
    TextStyle style = {.font = theme->fonts.body, .color = theme->colors.text_secondary};
    const char *text = i18n_text(STR_READER_NO_BOOK);
    Rect area = label_centered_area(style.font, theme_content_area(theme), text);
    label_draw(canvas, style, (Label){.area = area, .text = text});
}

static void draw(void *context, Canvas *canvas)
{
    ReaderScreen *reader = context;
    const Theme *theme = reader->theme;
    Rect content = theme_content_area(theme);
    canvas_fill_rect(canvas, content, theme->colors.background);
    header_draw(canvas, theme, reader->screen.title);
    if (reader->picker_open) {
        draw_books(canvas, reader);
        return;
    }
    draw_counter(canvas, reader);
    if (reader->length == 0) {
        draw_no_book(canvas, reader);
        return;
    }
    if (!reader->slide.active) {
        draw_page(canvas, reader, reader->offset, 0);
        return;
    }
    marquee_frame_request(READER_FLIP_FRAME_MS);
    bool forward = reader->slide.direction == SLIDE_FORWARD;
    int leaving = forward ? -reader->flip_y : reader->flip_y;
    int arriving = forward ? content.height - reader->flip_y : reader->flip_y - content.height;
    draw_page(canvas, reader, reader->from_offset, leaving);
    draw_page(canvas, reader, reader->offset, arriving);
}

Screen *reader_screen_init(ReaderScreen *reader, const Theme *theme, const char *text,
                           uint32_t length)
{
    close_reader();
    *reader = (ReaderScreen){.theme = theme,
                             .text = text,
                             .length = text != NULL ? length : 0,
                             .page_index = 1,
                             .page_total = 1,
                             .large = true};
    update_counter(reader);
    reader->screen = (Screen){
        .title = i18n_text(STR_READER),
        .stays_on_wake = true,
        .context = reader,
        .enter = enter,
        .handle_key = handle_key,
        .update = update,
        .draw = draw,
    };
    return &reader->screen;
}
