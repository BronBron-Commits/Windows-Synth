#include <SDL.h>
#include <math.h>
#include <stdio.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  8
#define STEPS       8
#define BPM         90

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

static int seq_steps[STEPS] = {0};
static int seq_pos = 0;
static int seq_running = 0;
static Uint32 last_tick = 0;

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
            v->active = 1;
            break;
        }
    }
}

static void note_off(int idx)
{
    voices[idx].target_amp = 0.0f;
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

        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *vc = &voices[v];
            if (!vc->active)
                continue;

            float s = 0.0f;

            /* detuned ensemble */
            s += square(vc->phase[0]);
            s += square(vc->phase[1] * 1.002f);
            s += square(vc->phase[2] * 0.998f);
            s += triangle(vc->phase[3]);
            s += triangle(vc->phase[4] * 1.003f);
            s += triangle(vc->phase[5] * 0.997f);

            s *= (1.0f / 6.0f);

            /* phase advance */
            for (int o = 0; o < 6; o++)
                vc->phase[o] += vc->freq / SAMPLE_RATE;

            /* smooth amp envelope */
            vc->amp += (vc->target_amp - vc->amp) * 0.002f;

            /* low-pass smoothing */
            vc->lp += 0.05f * (s - vc->lp);
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
   DRAW GRID
========================= */
static void draw_grid(SDL_Renderer *r)
{
    int margin = 40;
    int cell_w = (WINDOW_W - margin * 2) / STEPS;
    int cell_h = 60;
    int y = WINDOW_H / 2 - cell_h / 2;

    for (int i = 0; i < STEPS; i++) {
        SDL_Rect rect = {
            margin + i * cell_w,
            y,
            cell_w - 4,
            cell_h
        };

        SDL_SetRenderDrawColor(r, 40, 40, 40, 255);
        SDL_RenderFillRect(r, &rect);

        if (seq_steps[i]) {
            SDL_SetRenderDrawColor(r, 60, 140, 200, 255);
            SDL_RenderFillRect(r, &rect);
        }

        if (i == seq_pos && seq_running) {
            SDL_SetRenderDrawColor(r, 255, 220, 0, 255);
            SDL_RenderFillRect(r, &rect);
        }

        SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
        SDL_RenderDrawRect(r, &rect);
    }
}

/* =========================
   MAIN
========================= */
int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "Layered String Ensemble",
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

    Uint32 step_ms = (60000 / BPM) / 2;

    SDL_Event e;
    while (running) {
        Uint32 now = SDL_GetTicks();

        if (seq_running && now - last_tick >= step_ms) {
            last_tick = now;

            /* release previous */
            for (int v = 0; v < MAX_VOICES; v++)
                note_off(v);

            if (seq_steps[seq_pos])
                note_on(220.0f);

            seq_pos = (seq_pos + 1) % STEPS;
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;
                if (e.key.keysym.sym == SDLK_SPACE)
                    seq_running = !seq_running;
                if (e.key.keysym.sym >= SDLK_1 &&
                    e.key.keysym.sym <= SDLK_8)
                    seq_steps[e.key.keysym.sym - SDLK_1] ^= 1;
            }
        }

        SDL_SetRenderDrawColor(ren, 10, 10, 10, 255);
        SDL_RenderClear(ren);
        draw_grid(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
