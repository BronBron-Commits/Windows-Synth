#include "synth.h"
#include <math.h>
#include <stdlib.h>

static float square(float p) {
    return fmodf(p, 1.0f) < 0.25f ? 1.0f : -1.0f;
}

static float triangle(float p) {
    float x = fmodf(p, 1.0f);
    return 4.0f * fabsf(x - 0.5f) - 1.0f;
}

static float noise() {
    return ((rand() & 0x7FFF) / 16384.0f) - 1.0f;
}

void synth_init(Synth *s) {
    for (int i = 0; i < MAX_VOICES; i++)
        s->voices[i].active = 0;
}

void synth_trigger(Synth *s, float freq, int wf) {
    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active) {
            v->phase = 0.0f;
            v->pitch = freq * 8.0f;
            v->pitch_decay = 0.92f;
            v->amp = 1.0f;
            v->amp_decay = 0.88f;
            v->waveform = wf;
            v->active = 1;
            return;
        }
    }
}

float synth_sample(Synth *s) {
    float mix = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
        Voice *v = &s->voices[i];
        if (!v->active) continue;

        float smp = 0.0f;
        if (v->waveform == 0) smp = square(v->phase);
        else if (v->waveform == 1) smp = triangle(v->phase);
        else smp = noise();

        mix += smp * v->amp;

        v->phase += v->pitch / SAMPLE_RATE;
        v->pitch *= v->pitch_decay;
        v->amp *= v->amp_decay;

        if (v->amp < 0.001f)
            v->active = 0;
    }

    return mix * 0.25f;
}
