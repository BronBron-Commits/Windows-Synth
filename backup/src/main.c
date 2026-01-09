#include <SDL.h>
#include <math.h>
#include <stdio.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  8

#define WINDOW_W 640
#define WINDOW_H 240

/* =========================
   VOICE STRUCT
========================= */
typedef struct {
    float phase[6];
    float freq;
    float amp;
    float target_amp;
    float lp;
    int active;
} Voice;

/* =========================
   GLOBAL STATE
========================= */
static Voice voices[MAX_VOICES];
static int running = 1;

/* note toggle table */
static float note_freqs[5] = {
    261.63f, /* C */
    293.66f, /* D */
    329.63f, /* E */
    392.00f, /* G */
    440.00f  /* A */
};

static int note_active[5] = { 0 };

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
            Voice* v = &voices[i];

            for (int o = 0; o < 6; o++)
                v->phase[o] = 0.0f;

            v->freq = freq;
            v->amp = 0.0f;
            v->target_amp = 0.35f;
            v->lp = 0.0f;
            v->active = 1;
            break;
        }
    }
}

static void note_off(float freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active && fabsf(voices[i].freq - freq) < 0.1f) {
            voices[i].target_amp = 0.0f;
        }
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
void audio_cb(void* ud, Uint8* stream, int len)
{
    int16_t* out = (int16_t*)stream;
    int samples = len / 2;

    for (int i = 0; i < samples; i++) {
        float mix = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            Voice* vc = &voices[v];
            if (!vc->active)
                continue;

            float s = 0.0f;

            /* layered ensemble */
            s += square(vc->phase[0]);
            s += square(vc->phase[1] * 1.002f);
            s += square(vc->phase[2] * 0.998f);
            s += triangle(vc->phase[3]);
            s += triangle(vc->phase[4] * 1.003f);
            s += triangle(vc->phase[5] * 0.997f);

            s *= (1.0f / 6.0f);

            for (int o = 0; o < 6; o++)
                vc->phase[o] += vc->freq / SAMPLE_RATE;

            /* smooth envelope */
            vc->amp += (vc->target_amp - vc->amp) * 0.0015f;

            /* gentle low-pass */
            vc->lp += 0.04f * (s - vc->lp);
            s = vc->lp;

            mix += s * vc->amp;

            if (vc->amp < 0.0005f && vc->target_amp == 0.0f)
                vc->active = 0;
        }

        if (mix > 1.0f) mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;

        out[i] = (int16_t)(mix * 32767);
    }
}

/* =========================
   MAIN
========================= */
int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window* win = SDL_CreateWindow(
        "Polyphonic Ensemble Instrument",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN
    );

    SDL_Renderer* ren =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_AudioSpec want = { 0 };
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev =
        SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);

    SDL_PauseAudioDevice(dev, 0);

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE)
                    running = 0;

                if (k == SDLK_SPACE) {
                    all_notes_off();
                    for (int i = 0; i < 5; i++)
                        note_active[i] = 0;
                }

                if (k >= SDLK_1 && k <= SDLK_5) {
                    int idx = k - SDLK_1;
                    note_active[idx] ^= 1;

                    if (note_active[idx])
                        note_on(note_freqs[idx]);
                    else
                        note_off(note_freqs[idx]);
                }
            }
        }

        SDL_SetRenderDrawColor(ren, 15, 15, 15, 255);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
