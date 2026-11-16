#include "app_screens.h"

#include <stdbool.h>
#include <stddef.h>

#include "app/reader_overlay.h"
#include "core/i18n.h"
#include "diagnostics_values.h"
#include "hal/power.h"
#include "hal/storage.h"
#include "hal/system.h"

typedef enum MusicSource {
    MUSIC_COVER_FLOW,
    MUSIC_LIBRARY,
    MUSIC_FAVORITES
} MusicSource;

typedef struct MusicEntry {
    StringId label;
    MusicSource source;
    BrowseKind kind;
} MusicEntry;

enum {
    START_HOME_ONLY = -1
};

#ifdef ECHO_READER_SIM
static const char READER_DEMO_TEXT[] =
    "第一章 山间来信\n\n"
    "清晨的山谷还笼着一层薄雾，石板路上只有脚步声。林间的空气带着潮湿的草木气息，"
    "远处的溪水声若隐若现，像是在提醒赶路的人：慢一点，再慢一点。\n\n"
    "老邮差在石桥边停下，把肩上的邮包放在桥栏上。他从里面取出一封已经有些发黄的信，"
    "信封上的字迹娟秀而陌生，落款处只写着一个名字——“阿遥”。\n\n"
    "“这封信等了很久。”他自言自语，随后又笑了笑，把它重新放回包里。"
    "桥下的水映着天色，泛出一点浅浅的青；几只麻雀落在电线上，排成一行小小的逗号。\n\n"
    "山谷尽头是一座不大的村庄。炊烟从瓦房顶上慢慢升起，绕过树梢，散进云里。"
    "孩子们在巷口追逐，笑声撞在石墙上，又弹回来，落在每个人的耳朵里。\n\n"
    "阿遥站在院门口，手里握着一把旧钥匙。她已经等了三年，等到门前的桂花开了又谢，"
    "谢了又开；等到信纸的边缘起了毛，字迹也淡了。她始终相信，那个人会回来。\n\n"
    "第二章 灯下\n\n"
    "夜里，雨点敲着窗户。她点起一盏灯，把信摊在桌上，一字一句地读："
    "“若你看到这封信，说明山外的路已经通了。等春天再来的时候，我会沿着溪水走回来，"
    "带着你最喜欢的糖炒栗子。”\n\n"
    "她读到这里，忽然笑了，眼睛却有些湿。窗外的雨声渐渐小了，灯光把她的影子拉得很长，"
    "一直延伸到墙角那口旧木箱上。箱子里放着两张车票、一本相册，还有一枚生了锈的徽章。\n\n"
    "第二天清早，雾散了。她锁好院门，沿着石板路往山下走。路过石桥时，她停下脚步，"
    "朝桥下看了一眼——水面平静，倒映着初升的太阳，明亮得让人睁不开眼。\n\n"
    "第三章 归途\n\n"
    "山路很长，弯弯曲曲，像一句没有写完的句子。她走得不快，偶尔停下来，"
    "看看路边的野花，听听远处的鸟鸣。风从山谷里吹来，带着熟悉的味道。\n\n"
    "傍晚时分，她终于看见了那座熟悉的村庄。炊烟依旧，瓦房依旧，巷口的孩子却长大了许多。"
    "她停在村口，深深地吸了一口气，然后慢慢地、一步一步地走了进去。\n\n"
    "屋里亮着灯。门没有关，桌上摆着热腾腾的饭菜，还有一包用油纸包好的糖炒栗子。"
    "她停在门口，许久没有说话。灯下的人抬起头，笑着说：“回来了？”\n\n"
    "她说：“回来了。”\n\n"
    "排版测试段落：中文标点应当紧贴文字，例如逗号、句号、顿号、分号与冒号；"
    "括号（圆括号）、方括号、书名号《山海经》、引号「春晓」、【备注】以及省略号……"
    "都不应该出现在行首或行尾。英文与数字混排：Chapter 3, page 128, 2026-11-16; "
    "a hyphenated word well-known, and a long word "
    "pneumonoultramicroscopicsilicovolcanoconiosis.\n\n"
    "操作提示：音量下键翻到下一页，音量上键回到上一页，播放键切换字号，返回键退出阅读器。";
#endif

static const int8_t START_ENTRIES[START_SCREEN_COUNT] = {
    [START_MAIN_MENU] = START_HOME_ONLY,
    [START_NOW_PLAYING] = START_HOME_ONLY,
    [START_ALBUMS] = 3,
    [START_ARTISTS] = 1,
    [START_FAVORITES] = 8,
    [START_PLAYLISTS] = 7,
    [START_COVER_FLOW] = 0,
    [START_ALBUM_ARTISTS] = 2,
    [START_GENRES] = 4,
    [START_FOLDERS] = 5,
    [START_ALL_SONGS] = 6,
};

static const MusicEntry MUSIC_ENTRIES[MUSIC_ITEM_COUNT] = {
    {STR_MENU_COVER_FLOW, MUSIC_COVER_FLOW, BROWSE_ALBUMS},
    {STR_MENU_ARTISTS, MUSIC_LIBRARY, BROWSE_ARTISTS},
    {STR_MENU_ALBUM_ARTISTS, MUSIC_LIBRARY, BROWSE_ALBUM_ARTISTS},
    {STR_MENU_ALBUMS, MUSIC_LIBRARY, BROWSE_ALBUMS},
    {STR_MENU_GENRES, MUSIC_LIBRARY, BROWSE_GENRES},
    {STR_MENU_FOLDERS, MUSIC_LIBRARY, BROWSE_FOLDER},
    {STR_MENU_ALL_SONGS, MUSIC_LIBRARY, BROWSE_ALL_SONGS},
    {STR_MENU_PLAYLISTS, MUSIC_LIBRARY, BROWSE_PLAYLISTS},
    {STR_MENU_FAVORITES, MUSIC_FAVORITES, BROWSE_ALBUMS},
};

static bool player_has_track(const void *context)
{
    return player_status(context).has_track;
}

MenuItem app_screens_open_item(const char *label, Screen *target)
{
    return (MenuItem){.label = label,
                      .select = menu_open_screen,
                      .context = target,
                      .value = NULL,
                      .opens = true};
}

#ifdef ECHO_READER_OVERLAY
static ScreenAction open_reader(void *context)
{
    Screen *screen = reader_overlay_screen(context);
    return screen != NULL ? screen_action_push(screen) : screen_action_none();
}
#endif

Screen *app_screens_build_menu(MenuScreen *menu, const char *title, const MenuItem *items,
                               int count, const Theme *theme)
{
    return menu_screen_init(
        menu, (MenuConfig){.title = title, .items = items, .item_count = count, .theme = theme});
}

static Screen *browse_screen_for(BrowseEntry *entry)
{
    if (!entry->store->available) {
        return entry->unavailable;
    }
    if (entry->flow != NULL && entry->query.kind != BROWSE_FOLDER) {
        return cover_flow_screen_open_all(entry->flow);
    }
    const uint8_t *options = entry->pool->config.options;
    if (entry->flow != NULL && options != NULL && options[OPTION_FOLDER_VIEW] != 0) {
        Screen *folders = cover_flow_screen_open_group(entry->flow, LIBRARY_FOLDERS, 0, 0);
        if (folders != NULL) {
            return folders;
        }
    }
    if (entry->favorites) {
        return browse_pool_open_favorites(entry->pool, 0);
    }
    if (entry->shelf != SPOKEN_NONE) {
        return browse_pool_open_shelf(entry->pool, 0, entry->shelf);
    }
    return browse_pool_open(entry->pool, 0, entry->query);
}

static ScreenAction open_browse(void *context)
{
    return screen_action_push(browse_screen_for(context));
}

static MenuItem music_item(AppScreens *screens, const LibraryStore *store, int index)
{
    const MusicEntry *entry = &MUSIC_ENTRIES[index];
    screens->browse_entries[index] = (BrowseEntry){
        .pool = &screens->browse,
        .store = store,
        .unavailable = &screens->no_library.screen,
        .query = browse_root_query(entry->kind),
        .favorites = entry->source == MUSIC_FAVORITES,
        .flow = entry->source == MUSIC_COVER_FLOW || entry->kind == BROWSE_FOLDER
                    ? &screens->cover_flow
                    : NULL,
        .shelf = SPOKEN_NONE,
        .present = true,
    };
    return (MenuItem){.label = i18n_text(entry->label),
                      .select = open_browse,
                      .context = &screens->browse_entries[index],
                      .value = NULL,
                      .opens = true};
}

static Screen *open_group_flow(void *context, LibrarySection group, uint16_t subject, int level)
{
    return cover_flow_screen_open_group(context, group, subject, level);
}

static void build_music(AppScreens *screens, AppScreensContext context)
{
    BrowsePoolConfig browse = {
        .library = &context.store->library,
        .theme = context.theme,
        .player = context.player,
        .now_playing = &screens->now_playing.screen,
        .favorites = context.favorites,
        .toast = context.toast,
        .spoken = context.spoken,
        .options = context.settings->settings.options,
        .open_flow = open_group_flow,
        .flow_context = &screens->cover_flow,
    };
    browse_pool_init(&screens->browse, browse);
    CoverFlowConfig cover_flow = {
        .library = &context.store->library,
        .theme = context.stage_theme != NULL ? context.stage_theme : context.theme,
        .covers = context.covers,
        .favorites = context.favorites,
        .toast = context.toast,
        .browse = &screens->browse,
        .player = context.player,
        .options = context.settings->settings.options,
    };
    cover_flow_screen_init(&screens->cover_flow, cover_flow);
    for (int index = 0; index < MUSIC_ITEM_COUNT; index++) {
        screens->music_items[index] = music_item(screens, context.store, index);
    }
    app_screens_build_menu(&screens->music, i18n_text(STR_RESCAN_MUSIC_TITLE), screens->music_items,
                           MUSIC_ITEM_COUNT, context.theme);
}

static void build_settings(AppScreens *screens, AppScreensContext context)
{
    SettingsContext settings = {
        .store = context.settings,
        .player = context.player,
        .power = context.power,
        .toast = context.toast,
        .rescan_requested = context.rescan_requested,
        .bluetooth = &screens->bluetooth,
        .equalizer = &screens->equalizer,
        .date_time = &screens->date_time,
        .folders = &screens->folders,
        .sleep_option = 0,
    };
    DateTimeConfig date_time = {.clock = context.clock,
                                .store = context.settings,
                                .toast = context.toast,
                                .theme = context.theme};
    date_time_screen_init(&screens->date_time, date_time);
    FoldersConfig folders = {.toast = context.toast,
                             .rescan_requested = context.rescan_requested,
                             .theme = context.theme};
    folders_screen_init(&screens->folders, folders);
    settings_screens_init(&screens->settings, settings, context.theme);
}

static bool shelf_present(const void *context)
{
    const BrowseEntry *entry = context;
    return entry->present;
}

static MenuItem shelf_item(AppScreens *screens, AppScreensContext context, SpokenKind shelf)
{
    uint16_t folder = 0;
    BrowseEntry *entry = &screens->shelf_entries[shelf == SPOKEN_PODCAST ? 1 : 0];
    *entry = (BrowseEntry){
        .pool = &screens->browse,
        .store = context.store,
        .unavailable = &screens->no_library.screen,
        .shelf = shelf,
        .present = context.store->available &&
                   spoken_shelf_folder(&context.store->library, shelf, &folder),
    };
    return (MenuItem){
        .label = i18n_text(shelf == SPOKEN_PODCAST ? STR_MENU_PODCASTS : STR_MENU_AUDIOBOOKS),
        .select = open_browse,
        .context = entry,
        .opens = true,
        .shown = shelf_present,
        .shown_context = entry};
}

static void build_home(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    screens->home_items[0] =
        app_screens_open_item(i18n_text(STR_RESCAN_MUSIC_TITLE), &screens->music.screen);
    screens->home_items[1] = shelf_item(screens, context, SPOKEN_AUDIOBOOK);
    screens->home_items[2] = shelf_item(screens, context, SPOKEN_PODCAST);
    screens->home_items[3] =
        app_screens_open_item(i18n_text(STR_MENU_SETTINGS), &screens->settings.menu.screen);
    screens->home_items[4] =
        app_screens_open_item(i18n_text(STR_DIAG_TITLE), &screens->diagnostics.screen);
    screens->home_items[5] =
        app_screens_open_item(i18n_text(STR_MENU_NOW_PLAYING), &screens->now_playing.screen);
    screens->home_items[5].shown = player_has_track;
    screens->home_items[5].shown_context = context.player;
#ifdef ECHO_READER_OVERLAY
    screens->home_items[6] = (MenuItem){.label = i18n_text(STR_READER),
                                        .select = open_reader,
                                        .context = &screens->reader,
                                        .opens = true};
#elif defined(ECHO_READER_DEMO)
    screens->home_items[6] = app_screens_open_item(i18n_text(STR_READER), &screens->reader.screen);
#endif
    home_art_init(&screens->home_art, context.covers, context.store, context.player, theme);
    screens->home_art.notice = hal_storage_card_ready() ? NULL : i18n_text(STR_MENU_NO_CARD);
    MenuConfig home = {.title = "EchoPod",
                       .items = screens->home_items,
                       .item_count = HOME_ITEM_COUNT,
                       .theme = theme,
                       .panel = {.width = HOME_ART_WIDTH,
                                 .min_height = HOME_ART_MIN_HEIGHT,
                                 .update = home_art_update,
                                 .draw = home_art_draw,
                                 .context = &screens->home_art}};
    menu_screen_init(&screens->home, home);
}

Screen *app_screens_init(AppScreens *screens, AppScreensContext context)
{
    const Theme *theme = context.theme;
    bool card = hal_storage_card_ready();
    MessageConfig no_library = {
        .title = i18n_text(card ? STR_MENU_NO_LIBRARY : STR_MENU_NO_CARD),
        .message = i18n_text(card ? STR_MENU_NO_LIBRARY_HINT : STR_MENU_NO_CARD_HINT),
        .theme = theme};
    UpNextConfig up_next = {
        .library = &context.store->library, .theme = theme, .player = context.player};
    NowPlayingConfig now_playing = {.player = context.player,
                                    .library = &context.store->library,
                                    .theme = theme,
                                    .covers = context.covers,
                                    .up_next = up_next_screen_init(&screens->up_next, up_next),
                                    .options = context.settings->settings.options};
    EqualizerConfig equalizer = {
        .store = context.equalizer, .toast = context.toast, .theme = theme};
    now_playing.equalizer = equalizer_screen_init(&screens->equalizer, equalizer);
    message_screen_init(&screens->no_library, no_library);
    bluetooth_screen_init(&screens->bluetooth, context.bluetooth, theme);
    now_playing_screen_init(&screens->now_playing, now_playing);
#ifdef ECHO_READER_SIM
    reader_screen_init(&screens->reader, theme, READER_DEMO_TEXT, sizeof READER_DEMO_TEXT - 1);
#elif defined(ECHO_READER_OVERLAY)
    screens->reader = (ReaderScreen){.theme = theme};
#endif
    build_music(screens, context);
    build_settings(screens, context);
    diagnostics_menu_build(screens, context.options, theme);
    build_home(screens, context);
    return &screens->home.screen;
}

static int music_path(AppScreens *screens, int entry, Screen **path)
{
    BrowseEntry *browse = &screens->browse_entries[entry];
    if (!browse->store->available) {
        return 0;
    }
    list_view_select(&screens->music.list, entry, 0);
    path[0] = &screens->music.screen;
    path[1] = browse_screen_for(browse);
    return path[1] != NULL ? START_PATH_CAPACITY : 1;
}

int app_screens_start_path(AppScreens *screens, StartScreen start, Screen **path)
{
    if (start == START_NOW_PLAYING) {
        list_view_select(&screens->home.list, 1, 0);
        path[0] = &screens->now_playing.screen;
        return 1;
    }
    if ((int)start >= (int)START_SCREEN_COUNT || START_ENTRIES[start] == START_HOME_ONLY) {
        return 0;
    }
    return music_path(screens, START_ENTRIES[start], path);
}
