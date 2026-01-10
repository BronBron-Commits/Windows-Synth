#include <SDL.h>
#include <math.h>
#include <stdio.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  16

#define WINDOW_W 640
#define WINDOW_H 240

#define VIB_RATE   5.0f
#define VIB_DEPTH  0.003f

#define NUM_NOTES 8

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

/* vibrato */
static float vibrato_phase = 0.0f;

/* diatonic C major scale (1–8) */
static float note_freqs[NUM_NOTES] = {
    261.63f, /* C4 */
    293.66f, /* D4 */
    329.63f, /* E4 */
    349.23f, /* F4 */
    392.00f, /* G4 */
    440.00f, /* A4 */
    493.88f, /* B4 */
    523.25f  /* C5 */
};

static int note_active[NUM_NOTES] = {0};

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

            for (int o = 0; o < 6; o++)
                v->phase[o] = 0.0f;

            v->freq = freq;
            v->amp = 0.0f;
            v->target_amp = 0.35f;
            v->lp = 0.0f;
            v->vib_offset = (float)i * 1.37f;
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
    int samples = len / 2;

    for (int i = 0; i < samples; i++) {
        float mix = 0.0f;

        /* global vibrato */
        vibrato_phase += (2.0f * M_PI * VIB_RATE) / SAMPLE_RATE;
        if (vibrato_phase > 2.0f * M_PI)
            vibrato_phase -= 2.0f * M_PI;

        float vib = sinf(vibrato_phase);

        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *vc = &voices[v];
            if (!vc->active)
                continue;

            float s = 0.0f;

            float vib_mod =
                1.0f +
                vib * VIB_DEPTH +
                sinf(vibrato_phase + vc->vib_offset) * (VIB_DEPTH * 0.5f);

            float f = vc->freq * vib_mod;

            /* layered ensemble */
            s += square(vc->phase[0]);
            s += square(vc->phase[1] * 1.002f);
            s += square(vc->phase[2] * 0.998f);
            s += triangle(vc->phase[3]);
            s += triangle(vc->phase[4] * 1.003f);
            s += triangle(vc->phase[5] * 0.997f);

            s *= (1.0f / 6.0f);

            for (int o = 0; o < 6; o++)
                vc->phase[o] += f / SAMPLE_RATE;

            /* envelope */
            vc->amp += (vc->target_amp - vc->amp) * 0.0015f;

            /* low-pass smoothing */
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
int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "Polyphonic Ensemble – Diatonic Scale",
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
                    for (int i = 0; i < NUM_NOTES; i++)
                        note_active[i] = 0;
                }

                if (k >= SDLK_1 && k <= SDLK_8) {
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
