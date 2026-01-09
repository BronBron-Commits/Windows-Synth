#include <SDL.h>
#include <stdio.h>
#include "synth.h"

static Synth synth;

static float bitcrush(float s) {
    int v = (int)(s * 127);
    return v / 127.0f;
}

void audio_cb(void *ud, Uint8 *stream, int len) {
    int16_t *out = (int16_t*)stream;
    int samples = len / 2;

    for (int i = 0; i < samples; i++) {
        float v = bitcrush(synth_sample(&synth));
        out[i] = (int16_t)(v * 32767);
    }
}

int main(int argc, char **argv) {
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "SNES-Style Synth",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        480, 240,
        SDL_WINDOW_SHOWN
    );

    if (!win) {
        printf("Window failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want = {0};
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 1;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev =
        SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);

    if (!dev) {
        printf("Audio failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_PauseAudioDevice(dev, 0);

    synth_init(&synth);

    int running = 1;
    SDL_Event e;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;

            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_z: synth_trigger(&synth, 220, 0); break;
                    case SDLK_x: synth_trigger(&synth, 330, 0); break;
                    case SDLK_c: synth_trigger(&synth, 440, 1); break;
                    case SDLK_v: synth_trigger(&synth, 660, 2); break;
                    case SDLK_ESCAPE: running = 0; break;
                }
            }
        }
        SDL_Delay(1);
    }

    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
