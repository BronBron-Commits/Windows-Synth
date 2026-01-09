#include <SDL.h>
#include <math.h>
#include <stdio.h>

/* =========================
   CONFIG
========================= */
#define SAMPLE_RATE 44100
#define MAX_VOICES  8

/* =========================
   VOICE STRUCT
========================= */
typedef struct {
    float phase;
    float pitch;
    float pitch_decay;
    float amp;
    float amp_decay;
    int active;
} Voice;

/* =========================
   GLOBAL STATE
========================= */
static Voice voices[MAX_VOICES];
static int running = 1;

/* =========================
   WAVEFORM
========================= */
static float square(float phase)
{
    return (fmodf(phase, 1.0f) < 0.25f) ? 1.0f : -1.0f;
}

/* =========================
   TRIGGER VOICE
========================= */
static void trigger_voice(float base_freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            voices[i].phase = 0.0f;
            voices[i].pitch = base_freq * 8.0f;   /* big pitch snap */
            voices[i].pitch_decay = 0.92f;
            voices[i].amp = 1.0f;
            voices[i].amp_decay = 0.88f;
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

            float s = square(voice->phase);
            mix += s * voice->amp;

            voice->phase += voice->pitch / SAMPLE_RATE;
            voice->pitch *= voice->pitch_decay;
            voice->amp *= voice->amp_decay;

            if (voice->amp < 0.001f)
                voice->active = 0;
        }

        /* soft limit */
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
        "Windows Synth - SNES Voice Test",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        480,
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

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = 0;
                        break;

                    case SDLK_z:
                        trigger_voice(220.0f);
                        break;

                    case SDLK_x:
                        trigger_voice(330.0f);
                        break;

                    case SDLK_c:
                        trigger_voice(440.0f);
                        break;

                    case SDLK_v:
                        trigger_voice(660.0f);
                        break;
                }
            }
        }
        SDL_Delay(1);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
