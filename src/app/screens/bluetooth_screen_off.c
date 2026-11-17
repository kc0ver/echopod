#include "screens/bluetooth_screen.h"

Screen *bluetooth_screen_init(BluetoothScreen *bluetooth, BluetoothLink *link, const Theme *theme)
{
    (void)bluetooth;
    (void)link;
    (void)theme;
    return NULL;
}

ScreenAction bluetooth_screen_open(void *bluetooth)
{
    (void)bluetooth;
    return screen_action_none();
}

ScreenAction bluetooth_screen_open_details(void *bluetooth)
{
    (void)bluetooth;
    return screen_action_none();
}

const char *bluetooth_screen_summary(const void *bluetooth)
{
    (void)bluetooth;
    return "";
}

int bluetooth_screen_row_count(const BluetoothScreen *bluetooth)
{
    (void)bluetooth;
    return 0;
}

const char *bluetooth_screen_row(const BluetoothScreen *bluetooth, int index)
{
    (void)bluetooth;
    (void)index;
    return "";
}

const char *bluetooth_screen_row_value(const BluetoothScreen *bluetooth, int index)
{
    (void)bluetooth;
    (void)index;
    return "";
}

const char *bluetooth_screen_line(const BluetoothScreen *bluetooth, int index)
{
    (void)bluetooth;
    (void)index;
    return "";
}
