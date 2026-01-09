#include <SDL.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100
#define TONE_FREQ   440.0f

static float phase = 0.0f;
static int running = 1;
static int sound_on = 0;   /* toggled by SPACE */

/* =========================
   AUDIO CALLBACK
========================= */
void audio_cb(void *userdata, Uint8 *stream, int len)
{
    int16_t *out = (int16_t *)stream;
    int samples = len / sizeof(int16_t);

    for (int i = 0; i < samples; i++) {
        float sample = 0.0f;

        if (sound_on) {
            sample = sinf(phase) * 0.4f;

            phase += 2.0f * 3.1415926535f * TONE_FREQ / SAMPLE_RATE;
            if (phase >= 2.0f * 3.1415926535f)
                phase -= 2.0f * 3.1415926535f;
        }

        out[i] = (int16_t)(sample * 32767);
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
        "Windows Synth - SPACE = Toggle Sound",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        400,
        200,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        printf("Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec want = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!dev) {
        printf("Audio device failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_PauseAudioDevice(dev, 0);

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;

                if (e.key.keysym.sym == SDLK_SPACE) {
                    sound_on = !sound_on;
                    printf("Sound %s\n", sound_on ? "ON" : "OFF");
                }
            }
        }
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
