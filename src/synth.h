#pragma once

#define SAMPLE_RATE 44100
#define MAX_VOICES 16

typedef struct {
    float phase;
    float phase_inc;
    float amp;
    float amp_decay;
    float pitch;
    float pitch_decay;
    int waveform;
    int active;
} Voice;

typedef struct {
    Voice voices[MAX_VOICES];
} Synth;

void synth_init(Synth *s);
void synth_trigger(Synth *s, float freq, int waveform);
float synth_sample(Synth *s);
