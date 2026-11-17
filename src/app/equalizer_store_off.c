#include "equalizer_store.h"

#include <stddef.h>

const char EQUALIZER_STORE_PATH[] = "/.echo/equalizer.bin";
const char EQUALIZER_FOLDER_PATH[] = "/EQ";

void equalizer_store_load(EqualizerStore *store)
{
    *store = (EqualizerStore){0};
}

bool equalizer_store_save(EqualizerStore *store)
{
    (void)store;
    return true;
}

bool equalizer_store_apply(EqualizerStore *store)
{
    (void)store;
    return true;
}

bool equalizer_store_select_file(EqualizerStore *store, const char *name, EqualizerTextError *error)
{
    (void)store;
    (void)name;
    if (error != NULL) {
        *error = (EqualizerTextError){0};
    }
    return false;
}

bool equalizer_store_file_curve(const char *name, EqualizerCurve *curve, EqualizerTextError *error)
{
    (void)name;
    (void)curve;
    if (error != NULL) {
        *error = (EqualizerTextError){0};
    }
    return false;
}
