#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  16
#define NUM_NOTES   8

#define WINDOW_W 640
#define WINDOW_H 300

/* vibrato */
#define VIB_RATE   5.0f
#define VIB_DEPTH  0.003f

/* tremolo */
#define TREM_RATE  0.8f
#define TREM_DEPTH 0.35f

/* chorus */
#define CHORUS_RATE 0.35f
#define CHORUS_DEPTH 0.0025f
#define CHORUS_DELAY 0.025f   /* base delay seconds */

/* =========================
   VOICE STRUCT
========================= */
typedef struct {
    float phase[6];
    float freq;
    float amp;
    float target_amp;
    float lp;
    float vib_offset;
    int active;
} Voice;

/* =========================
   GLOBAL STATE
========================= */
static Voice voices[MAX_VOICES];
static int running = 1;

/* LFOs */
static float vibrato_phase = 0.0f;
static float tremolo_phase = 0.0f;
static float chorus_phase  = 0.0f;

/* effect toggles */
static int tremolo_on = 1;
static int chorus_on  = 0;

/* delay buffer for chorus */
#define DELAY_BUF_SIZE (SAMPLE_RATE / 2)
static float delayL[DELAY_BUF_SIZE];
static float delayR[DELAY_BUF_SIZE];
static int delay_idx = 0;

/* diatonic C major scale */
static float note_freqs[NUM_NOTES] = {
    261.63f, 293.66f, 329.63f, 349.23f,
    392.00f, 440.00f, 493.88f, 523.25f
};

static int note_active[NUM_NOTES] = {0};

/* per-oscillator pan */
static const float osc_pan[6] = {
    -0.7f, -0.3f, 0.0f,
     0.2f,  0.5f, 0.8f
};

/* =========================
   WAVEFORMS
========================= */
static float square(float p)
{
    return (fmodf(p, 1.0f) < 0.5f) ? 1.0f : -1.0f;
}

static float triangle(float p)
{
    float x = fmodf(p, 1.0f);
    return 4.0f * fabsf(x - 0.5f) - 1.0f;
}

/* =========================
   VOICE CONTROL
========================= */
static void note_on(float freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            Voice *v = &voices[i];
            memset(v->phase, 0, sizeof(v->phase));
            v->freq = freq;
            v->amp = 0.0f;
            v->target_amp = 0.35f;
            v->lp = 0.0f;
            v->vib_offset = (float)i * 1.31f;
            v->active = 1;
            break;
        }
    }
}

static void note_off(float freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && fabsf(voices[i].freq - freq) < 0.1f)
            voices[i].target_amp = 0.0f;
    }
}

static void all_notes_off(void)
{
    for (int i = 0; i < MAX_VOICES; i++)
        voices[i].target_amp = 0.0f;
}

/* =========================
   AUDIO CALLBACK
========================= */
void audio_cb(void *ud, Uint8 *stream, int len)
{
    int16_t *out = (int16_t*)stream;
    int frames = len / (sizeof(int16_t) * 2);

    for (int i = 0; i < frames; i++) {
        float mixL = 0.0f;
        float mixR = 0.0f;

        /* vibrato */
        vibrato_phase += (2.0f * M_PI * VIB_RATE) / SAMPLE_RATE;
        float vib = sinf(vibrato_phase);

        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *vc = &voices[v];
            if (!vc->active) continue;

            float f = vc->freq *
                (1.0f + vib * VIB_DEPTH);

            float voiceL = 0.0f;
            float voiceR = 0.0f;

            for (int o = 0; o < 6; o++) {
                float w =
                    (o < 3)
                    ? square(vc->phase[o])
                    : triangle(vc->phase[o]);

                float p = osc_pan[o];
                voiceL += w * (1.0f - p);
                voiceR += w * (1.0f + p);
                vc->phase[o] += f / SAMPLE_RATE;
            }

            voiceL *= 0.08f;
            voiceR *= 0.08f;

            vc->amp += (vc->target_amp - vc->amp) * 0.0015f;

            mixL += voiceL * vc->amp;
            mixR += voiceR * vc->amp;

            if (vc->amp < 0.0005f && vc->target_amp == 0.0f)
                vc->active = 0;
        }

        /* tremolo */
        if (tremolo_on) {
            tremolo_phase += (2.0f * M_PI * TREM_RATE) / SAMPLE_RATE;
            float t =
                (1.0f - TREM_DEPTH) +
                TREM_DEPTH * (0.5f + 0.5f * sinf(tremolo_phase));
            mixL *= t;
            mixR *= t;
        }

        /* chorus */
        if (chorus_on) {
            chorus_phase += (2.0f * M_PI * CHORUS_RATE) / SAMPLE_RATE;
            float mod = sinf(chorus_phase) * CHORUS_DEPTH;

            int delay_samples =
                (int)((CHORUS_DELAY + mod) * SAMPLE_RATE);

            int read = (delay_idx - delay_samples + DELAY_BUF_SIZE)
                       % DELAY_BUF_SIZE;

            float dl = delayL[read];
            float dr = delayR[read];

            delayL[delay_idx] = mixL;
            delayR[delay_idx] = mixR;

            mixL = mixL * 0.7f + dl * 0.3f;
            mixR = mixR * 0.7f + dr * 0.3f;

            delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;
        }

        out[i * 2]     = (int16_t)(fmaxf(-1, fminf(1, mixL)) * 32767);
        out[i * 2 + 1] = (int16_t)(fmaxf(-1, fminf(1, mixR)) * 32767);
    }
}

/* =========================
   VISUALS
========================= */
static void draw_button(SDL_Renderer *r, SDL_Rect rect, int on)
{
    if (on)
        SDL_SetRenderDrawColor(r, 80, 180, 120, 255);
    else
        SDL_SetRenderDrawColor(r, 40, 40, 40, 255);

    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &rect);
}

static void draw_ui(SDL_Renderer *r)
{
    /* note buttons */
    int margin = 40;
    int w = (WINDOW_W - margin * 2) / NUM_NOTES;
    for (int i = 0; i < NUM_NOTES; i++) {
        SDL_Rect rc = { margin + i * w, 60, w - 6, 50 };
        draw_button(r, rc, note_active[i]);
    }

    /* effect buttons */
    SDL_Rect trem = { 160, 150, 120, 40 };
    SDL_Rect chor = { 360, 150, 120, 40 };

    draw_button(r, trem, tremolo_on);
    draw_button(r, chor, chorus_on);
}

/* =========================
   MAIN
========================= */
int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "Ensemble Synth – FX Chain",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer *ren =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_AudioSpec want = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev =
        SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);

    SDL_PauseAudioDevice(dev, 0);

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE) running = 0;
                if (k == SDLK_t) tremolo_on ^= 1;
                if (k == SDLK_c) chorus_on  ^= 1;

                if (k >= SDLK_1 && k <= SDLK_8) {
                    int i = k - SDLK_1;
                    note_active[i] ^= 1;
                    if (note_active[i])
                        note_on(note_freqs[i]);
                    else
                        note_off(note_freqs[i]);
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
        SDL_RenderClear(ren);
        draw_ui(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
