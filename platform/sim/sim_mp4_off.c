#include "sim_mp4.h"

#include <stddef.h>

bool sim_mp4_open(SimMp4 *mp4, const char *path)
{
    (void)mp4;
    (void)path;
    return false;
}

uint64_t sim_mp4_read(SimMp4 *mp4, float *frames, uint64_t count, uint32_t channels)
{
    (void)mp4;
    (void)frames;
    (void)count;
    (void)channels;
    return 0;
}

bool sim_mp4_seek(SimMp4 *mp4, uint64_t frame)
{
    (void)mp4;
    (void)frame;
    return false;
}

void sim_mp4_close(SimMp4 *mp4)
{
    (void)mp4;
}

uint32_t sim_mp4_playable(const SimMp4 *mp4)
{
    (void)mp4;
    return 0;
}
