#include "screens/equalizer_screen.h"

#include <stddef.h>

Screen *equalizer_screen_init(EqualizerScreen *equalizer, EqualizerConfig config)
{
    (void)equalizer;
    (void)config;
    return NULL;
}

ScreenAction equalizer_screen_open(void *context)
{
    (void)context;
    return screen_action_none();
}

const char *equalizer_screen_summary(const void *context)
{
    (void)context;
    return "";
}

const char *equalizer_screen_preset_name(EqualizerPreset preset)
{
    (void)preset;
    return "";
}
