#include "board.h"

#include <stddef.h>

#include "hal/bluetooth.h"

static HalBluetoothStatus status;

void board_bluetooth_off(void)
{
}

void board_bluetooth_tick(void)
{
}

bool board_bluetooth_routed(void)
{
    return false;
}

bool board_bluetooth_settling(void)
{
    return false;
}

bool board_bluetooth_pin(bool mute_pin, bool level)
{
    (void)mute_pin;
    return level;
}

void hal_bluetooth_set_power(bool on)
{
    (void)on;
}

void hal_bluetooth_scan(void)
{
}

void hal_bluetooth_stop_scan(void)
{
}

void hal_bluetooth_connect(const HalBluetoothDevice *device)
{
    (void)device;
}

void hal_bluetooth_disconnect(void)
{
}

void hal_bluetooth_set_stream(bool on)
{
    (void)on;
}

void hal_bluetooth_service(void)
{
}

const HalBluetoothStatus *hal_bluetooth_status(void)
{
    return &status;
}
