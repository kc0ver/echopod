#include "sim_opus.h"

#include <stddef.h>

bool sim_opus_open(SimOpus *opus, const char *path)
{
    (void)opus;
    (void)path;
    return false;
}

uint64_t sim_opus_read(SimOpus *opus, float *frames, uint64_t count, uint32_t channels)
{
    (void)opus;
    (void)frames;
    (void)count;
    (void)channels;
    return 0;
}

bool sim_opus_seek(SimOpus *opus, uint64_t frame)
{
    (void)opus;
    (void)frame;
    return false;
}

void sim_opus_close(SimOpus *opus)
{
    (void)opus;
}

uint32_t sim_opus_playable(const SimOpus *opus)
{
    (void)opus;
    return 0;
}
