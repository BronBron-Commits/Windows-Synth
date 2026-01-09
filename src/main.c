#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  8
#define STEPS       8
#define BPM         120

/* =========================
   VOICE TYPES
========================= */
typedef enum {
    VOICE_SQUARE,
    VOICE_NOISE
} VoiceType;

/* =========================
   VOICE STRUCT
========================= */
typedef struct {
    float phase;
    float pitch;
    float pitch_decay;
    float amp;
    float amp_decay;
    VoiceType type;
    int active;
} Voice;

/* =========================
   GLOBAL STATE
========================= */
static Voice voices[MAX_VOICES];
static int running = 1;

/* sequencer */
static int seq_steps[STEPS] = {0};
static int seq_pos = 0;
static int seq_running = 0;
static Uint32 last_tick = 0;

/* =========================
   WAVEFORMS
========================= */
static float square(float phase)
{
    return (fmodf(phase, 1.0f) < 0.25f) ? 1.0f : -1.0f;
}

static float noise(void)
{
    return ((rand() & 0x7FFF) / 16384.0f) - 1.0f;
}

/* =========================
   TRIGGERS
========================= */
static void trigger_square(float base_freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].phase = 0.0f;
            voices[i].pitch = base_freq * 8.0f;
            voices[i].pitch_decay = 0.92f;
            voices[i].amp = 1.0f;
            voices[i].amp_decay = 0.88f;
            voices[i].type = VOICE_SQUARE;
            voices[i].active = 1;
            break;
        }
    }
}

static void trigger_noise(void)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].amp = 1.0f;
            voices[i].amp_decay = 0.80f;
            voices[i].type = VOICE_NOISE;
            voices[i].active = 1;
            break;
        }
    }
}

/* =========================
   AUDIO CALLBACK
========================= */
void audio_cb(void *userdata, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        float mix = 0.0f;

        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *voice = &voices[v];
            if (!voice->active)
                continue;

            float s = 0.0f;

            if (voice->type == VOICE_SQUARE) {
                s = square(voice->phase);
                voice->phase += voice->pitch / SAMPLE_RATE;
                voice->pitch *= voice->pitch_decay;
            } else {
                s = noise();
            }

            mix += s * voice->amp;
            voice->amp *= voice->amp_decay;

            if (voice->amp < 0.001f)
                voice->active = 0;
        }

        if (mix > 1.0f) mix = 1.0f;
        if (mix < -1.0f) mix = -1.0f;

        out[i] = (int16_t)(mix * 32767);
    }
}

/* =========================
   MAIN
========================= */
int main(int argc, char **argv)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Windows Synth - Step Sequencer",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        520,
        240,
        SDL_WINDOW_SHOWN
    );

    SDL_AudioSpec want = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!dev) {
        printf("Audio device failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_PauseAudioDevice(dev, 0);

    Uint32 step_ms = (60000 / BPM) / 2;

    SDL_Event e;
    while (running) {
        Uint32 now = SDL_GetTicks();

        if (seq_running && now - last_tick >= step_ms) {
            last_tick = now;

            if (seq_steps[seq_pos])
                trigger_square(440.0f);

            seq_pos = (seq_pos + 1) % STEPS;
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE)
                    running = 0;

                if (k == SDLK_SPACE) {
                    seq_running = !seq_running;
                    printf("Sequencer %s\n", seq_running ? "ON" : "OFF");
                }

                if (k >= SDLK_1 && k <= SDLK_8) {
                    int idx = k - SDLK_1;
                    seq_steps[idx] = !seq_steps[idx];
                    printf("Step %d: %s\n", idx + 1,
                           seq_steps[idx] ? "ON" : "OFF");
                }

                if (k == SDLK_b)
                    trigger_noise();
            }
        }

        SDL_Delay(1);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
