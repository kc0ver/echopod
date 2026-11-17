#include "dsp/equalizer.h"

#include <stddef.h>

void equalizer_init(Equalizer *equalizer)
{
    *equalizer = (Equalizer){0};
}

void equalizer_set_curve(Equalizer *equalizer, const EqualizerCurve *curve)
{
    (void)equalizer;
    (void)curve;
}

void equalizer_set_rate(Equalizer *equalizer, uint32_t rate)
{
    (void)equalizer;
    (void)rate;
}

void equalizer_apply_pending(Equalizer *equalizer)
{
    (void)equalizer;
}

bool equalizer_active(const Equalizer *equalizer)
{
    (void)equalizer;
    return false;
}

bool equalizer_valid(const Equalizer *equalizer)
{
    (void)equalizer;
    return false;
}

void equalizer_process(Equalizer *equalizer, int32_t *frames, uint32_t count)
{
    (void)equalizer;
    (void)frames;
    (void)count;
}

void equalizer_process_block(Equalizer *equalizer, EqualizerBlock block)
{
    (void)equalizer;
    (void)block;
}

int equalizer_preamp_tenths(const EqualizerCurve *curve, uint32_t rate)
{
    (void)curve;
    (void)rate;
    return 0;
}

bool equalizer_curve_valid(const EqualizerCurve *curve, uint32_t rate)
{
    (void)curve;
    (void)rate;
    return false;
}

bool equalizer_band_audible(EqualizerBand band, uint32_t rate)
{
    (void)band;
    (void)rate;
    return false;
}
