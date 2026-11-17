#include "bluetooth_link.h"

#include <stddef.h>

const char BLUETOOTH_STORE_PATH[] = "/.echo/bluetooth.bin";

void bluetooth_link_start(BluetoothLink *link, Toast *toast, uint32_t now_ms)
{
    (void)link;
    (void)toast;
    (void)now_ms;
}

void bluetooth_link_set_enabled(BluetoothLink *link, bool enabled, uint32_t now_ms)
{
    (void)link;
    (void)enabled;
    (void)now_ms;
}

void bluetooth_link_browse(BluetoothLink *link, bool browsing, uint32_t now_ms)
{
    (void)link;
    (void)browsing;
    (void)now_ms;
}

void bluetooth_link_choose(BluetoothLink *link, int index, uint32_t now_ms)
{
    (void)link;
    (void)index;
    (void)now_ms;
}

void bluetooth_link_pair(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms)
{
    (void)link;
    (void)device;
    (void)now_ms;
}

void bluetooth_link_search(BluetoothLink *link, uint32_t now_ms)
{
    (void)link;
    (void)now_ms;
}

void bluetooth_link_forget(BluetoothLink *link, int index)
{
    (void)link;
    (void)index;
}

void bluetooth_link_sleep(BluetoothLink *link)
{
    (void)link;
}

void bluetooth_link_step(BluetoothLink *link, Player *player, uint32_t now_ms)
{
    (void)link;
    (void)player;
    (void)now_ms;
}

BluetoothDeviceState bluetooth_link_device_state(const BluetoothLink *link, int index)
{
    (void)link;
    (void)index;
    return BLUETOOTH_DEVICE_IDLE;
}

bool bluetooth_link_knows(const BluetoothLink *link, const HalBluetoothDevice *device)
{
    (void)link;
    (void)device;
    return false;
}

bool bluetooth_link_nearby(const BluetoothLink *link, int index)
{
    (void)link;
    (void)index;
    return false;
}

bool bluetooth_link_searching(const BluetoothLink *link)
{
    (void)link;
    return false;
}

const char *bluetooth_link_summary(const BluetoothLink *link)
{
    (void)link;
    return "";
}
