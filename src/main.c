#define SDL_MAIN_HANDLED

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

#define WINDOW_W 900
#define WINDOW_H 360

/* vibrato */
#define VIB_RATE   5.0f
#define VIB_DEPTH  0.003f

/* tremolo */
#define TREM_RATE  0.8f
#define TREM_DEPTH 0.35f

/* chorus */
#define CHORUS_RATE  0.35f
#define CHORUS_DEPTH 0.0025f
#define CHORUS_DELAY 0.025f

/* Jetsons envelopes */
#define AMP_ATTACK   0.004f
#define AMP_RELEASE  0.002f

#define PITCH_DECAY  0.0018f
#define PITCH_SWEEP  2.0f      /* up to +2 octaves */
#define GLIDE_RATE   0.0025f

/* =========================
   JETSONS ENGINE FLUTTER
========================= */
#define ENGINE_RATE   28.0f   /* fast mechanical wobble */
#define ENGINE_DEPTH  0.12f   /* pitch modulation depth */
#define ENGINE_AM     0.35f   /* amplitude flutter depth */

static float engine_phase = 0.0f;


/* =========================
   VOICE STRUCT
========================= */
typedef struct {
    float phase[6];

    float base_freq;
    float current_freq;
    float target_freq;

    float amp;
    float amp_target;
    int sustaining;
    float pitch_env;   /* 1.0 → 0.0 */
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
static int chorus_on  = 1;

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
static const char *note_names[NUM_NOTES] = {"C","D","E","F","G","A","B","C"};

static int note_active[NUM_NOTES] = {0};

/* per-oscillator pan (-1 left .. +1 right) */
static const float osc_pan[6] = {-0.7f,-0.3f,0.0f,0.2f,0.5f,0.8f};

/* =========================
   OSCILLATORS
========================= */
static float square(float p) {
    return (fmodf(p, 1.0f) < 0.5f) ? 1.0f : -1.0f;
}
static float triangle(float p) {
    float x = fmodf(p, 1.0f);
    return 4.0f * fabsf(x - 0.5f) - 1.0f;
}

/* =========================
   VOICE CONTROL
========================= */
static void note_on(float freq) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!voices[i].active) {
            Voice *v = &voices[i];
            memset(v->phase, 0, sizeof(v->phase));

            v->base_freq   = freq;
            v->current_freq = freq * 0.5f;   /* start low */
            v->target_freq  = freq;

            v->amp = 0.0f;
            v->amp_target = 0.35f;
            v->sustaining = 1;
            v->pitch_env = 1.0f;  /* start with sweep */

            v->vib_offset = (float)i * 1.31f;
            v->active = 1;
            break;
        }
    }
}

static void note_off(float freq)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active &&
            fabsf(voices[i].base_freq - freq) < 0.1f) {
            voices[i].sustaining = 0;
            voices[i].amp_target = 0.0f;
        }
    }
}


static void all_notes_off(void) {
    for (int i = 0; i < MAX_VOICES; i++)
        voices[i].amp_target = 0.0f;
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
        vibrato_phase += (2.0f * (float)M_PI * VIB_RATE) / SAMPLE_RATE;
        if (vibrato_phase > 2.0f * (float)M_PI)
            vibrato_phase -= 2.0f * (float)M_PI;
        float vib = sinf(vibrato_phase);

        /* engine flutter LFO (FAST, mechanical) */
        engine_phase += (2.0f * (float)M_PI * ENGINE_RATE) / SAMPLE_RATE;
        if (engine_phase > 2.0f * (float)M_PI)
            engine_phase -= 2.0f * (float)M_PI;

        /* square-like flutter */
        float engine = sinf(engine_phase);
        engine = (engine > 0.0f) ? 1.0f : -1.0f;


        for (int v = 0; v < MAX_VOICES; v++) {
            Voice *vc = &voices[v];
            if (!vc->active) continue;

            /* pitch envelope decay */
            vc->pitch_env -= PITCH_DECAY;
            if (vc->pitch_env < 0.0f) vc->pitch_env = 0.0f;

            float pitch_mul = 1.0f + vc->pitch_env * PITCH_SWEEP;

            /* glide */
            vc->current_freq += (vc->target_freq - vc->current_freq) * GLIDE_RATE;

            float f = vc->current_freq * pitch_mul;

            /* subtle slow vibrato */
            f *= (1.0f + vib * 0.001f);

            /* FAST Jetsons engine flutter */
            f *= (1.0f + engine * ENGINE_DEPTH);


            float voiceL = 0.0f;
            float voiceR = 0.0f;

            float osc[6];
            osc[0] = square(vc->phase[0]);
            osc[1] = square(vc->phase[1] * 1.002f);
            osc[2] = square(vc->phase[2] * 0.998f);
            osc[3] = triangle(vc->phase[3]);
            osc[4] = triangle(vc->phase[4] * 1.003f);
            osc[5] = triangle(vc->phase[5] * 0.997f);

            for (int o = 0; o < 6; o++) {
                float p = osc_pan[o];
                voiceL += osc[o] * (1.0f - p) * 0.5f;
                voiceR += osc[o] * (1.0f + p) * 0.5f;
                vc->phase[o] += f / SAMPLE_RATE;
            }

            voiceL *= (1.0f / 6.0f);
            voiceR *= (1.0f / 6.0f);

            if (vc->sustaining) {
                /* attack / sustain */
                vc->amp += (vc->amp_target - vc->amp) * AMP_ATTACK;
            }
            else {
                /* release */
                vc->amp += (0.0f - vc->amp) * AMP_RELEASE;
            }


/* engine amplitude flutter */
float amp_flutter = 1.0f - ENGINE_AM + ENGINE_AM * fabsf(engine);

mixL += voiceL * vc->amp * amp_flutter;
mixR += voiceR * vc->amp * amp_flutter;


            if (vc->amp < 0.0005f && vc->amp_target == 0.0f)
                vc->active = 0;
        }

        /* tremolo */
        if (tremolo_on) {
            tremolo_phase += (2.0f * (float)M_PI * TREM_RATE) / SAMPLE_RATE;
            if (tremolo_phase > 2.0f * (float)M_PI)
                tremolo_phase -= 2.0f * (float)M_PI;
            float t = (1.0f - TREM_DEPTH) + TREM_DEPTH * (0.5f + 0.5f * sinf(tremolo_phase));
            mixL *= t;
            mixR *= t;
        }

        /* chorus */
        if (chorus_on) {
            chorus_phase += (2.0f * (float)M_PI * CHORUS_RATE) / SAMPLE_RATE;
            if (chorus_phase > 2.0f * (float)M_PI)
                chorus_phase -= 2.0f * (float)M_PI;

            float mod = sinf(chorus_phase) * CHORUS_DEPTH;
            int delay_samples = (int)((CHORUS_DELAY + mod) * SAMPLE_RATE);
            if (delay_samples < 1) delay_samples = 1;
            if (delay_samples > DELAY_BUF_SIZE - 1) delay_samples = DELAY_BUF_SIZE - 1;

            int read = (delay_idx - delay_samples + DELAY_BUF_SIZE) % DELAY_BUF_SIZE;

            float dl = delayL[read];
            float dr = delayR[read];

            delayL[delay_idx] = mixL;
            delayR[delay_idx] = mixR;

            mixL = mixL * 0.7f + dl * 0.3f;
            mixR = mixR * 0.7f + dr * 0.3f;

            delay_idx = (delay_idx + 1) % DELAY_BUF_SIZE;
        }

        /* clamp */
        if (mixL > 1.0f) mixL = 1.0f;
        if (mixL < -1.0f) mixL = -1.0f;
        if (mixR > 1.0f) mixR = 1.0f;
        if (mixR < -1.0f) mixR = -1.0f;

        out[i * 2 + 0] = (int16_t)(mixL * 32767);
        out[i * 2 + 1] = (int16_t)(mixR * 32767);
    }
}

/* =========================
   (UI CODE UNCHANGED)
========================= */
/* — Everything below this line is identical to your original UI code — */
/* (Kept unchanged for brevity; paste your existing UI code here) */


/* =========================
   TINY BLOCK FONT (no SDL_ttf)
   - draws only needed characters: A-G, C,H,O,R,U,S,T,E,M,L,0-9,space,#
========================= */
static void draw_glyph(SDL_Renderer *r, int x, int y, int s, char c)
{
    /* 3x5 bitmap per glyph, stored as 5 rows of 3 bits (MSB->LSB) */
    unsigned rows[5] = {0,0,0,0,0};

    switch (c) {
        case 'C': rows[0]=0b111; rows[1]=0b100; rows[2]=0b100; rows[3]=0b100; rows[4]=0b111; break;
        case 'D': rows[0]=0b110; rows[1]=0b101; rows[2]=0b101; rows[3]=0b101; rows[4]=0b110; break;
        case 'E': rows[0]=0b111; rows[1]=0b100; rows[2]=0b110; rows[3]=0b100; rows[4]=0b111; break;
        case 'F': rows[0]=0b111; rows[1]=0b100; rows[2]=0b110; rows[3]=0b100; rows[4]=0b100; break;
        case 'G': rows[0]=0b111; rows[1]=0b100; rows[2]=0b101; rows[3]=0b101; rows[4]=0b111; break;
        case 'A': rows[0]=0b010; rows[1]=0b101; rows[2]=0b111; rows[3]=0b101; rows[4]=0b101; break;
        case 'B': rows[0]=0b110; rows[1]=0b101; rows[2]=0b110; rows[3]=0b101; rows[4]=0b110; break;

        case 'H': rows[0]=0b101; rows[1]=0b101; rows[2]=0b111; rows[3]=0b101; rows[4]=0b101; break;
        case 'O': rows[0]=0b111; rows[1]=0b101; rows[2]=0b101; rows[3]=0b101; rows[4]=0b111; break;
        case 'R': rows[0]=0b110; rows[1]=0b101; rows[2]=0b110; rows[3]=0b101; rows[4]=0b101; break;
        case 'U': rows[0]=0b101; rows[1]=0b101; rows[2]=0b101; rows[3]=0b101; rows[4]=0b111; break;
        case 'S': rows[0]=0b111; rows[1]=0b100; rows[2]=0b111; rows[3]=0b001; rows[4]=0b111; break;
        case 'T': rows[0]=0b111; rows[1]=0b010; rows[2]=0b010; rows[3]=0b010; rows[4]=0b010; break;
        case 'M': rows[0]=0b101; rows[1]=0b111; rows[2]=0b111; rows[3]=0b101; rows[4]=0b101; break;
        case 'L': rows[0]=0b100; rows[1]=0b100; rows[2]=0b100; rows[3]=0b100; rows[4]=0b111; break;

        case '#': rows[0]=0b101; rows[1]=0b111; rows[2]=0b101; rows[3]=0b111; rows[4]=0b101; break;

        case '1': rows[0]=0b010; rows[1]=0b110; rows[2]=0b010; rows[3]=0b010; rows[4]=0b111; break;
        case '2': rows[0]=0b111; rows[1]=0b001; rows[2]=0b111; rows[3]=0b100; rows[4]=0b111; break;
        case '3': rows[0]=0b111; rows[1]=0b001; rows[2]=0b111; rows[3]=0b001; rows[4]=0b111; break;
        case '4': rows[0]=0b101; rows[1]=0b101; rows[2]=0b111; rows[3]=0b001; rows[4]=0b001; break;
        case '5': rows[0]=0b111; rows[1]=0b100; rows[2]=0b111; rows[3]=0b001; rows[4]=0b111; break;
        case '6': rows[0]=0b111; rows[1]=0b100; rows[2]=0b111; rows[3]=0b101; rows[4]=0b111; break;
        case '7': rows[0]=0b111; rows[1]=0b001; rows[2]=0b001; rows[3]=0b001; rows[4]=0b001; break;
        case '8': rows[0]=0b111; rows[1]=0b101; rows[2]=0b111; rows[3]=0b101; rows[4]=0b111; break;
        case '9': rows[0]=0b111; rows[1]=0b101; rows[2]=0b111; rows[3]=0b001; rows[4]=0b111; break;
        case '0': rows[0]=0b111; rows[1]=0b101; rows[2]=0b101; rows[3]=0b101; rows[4]=0b111; break;

        case ' ': default: return;
    }

    for (int ry = 0; ry < 5; ry++) {
        for (int rx = 0; rx < 3; rx++) {
            if (rows[ry] & (1u << (2 - rx))) {
                SDL_Rect px = { x + rx * s, y + ry * s, s, s };
                SDL_RenderFillRect(r, &px);
            }
        }
    }
}

static void draw_text(SDL_Renderer *r, int x, int y, int s, const char *t)
{
    int cx = x;
    for (const char *p = t; *p; p++) {
        if (*p == '\n') { y += (6 * s); cx = x; continue; }
        draw_glyph(r, cx, y, s, *p);
        cx += (4 * s);
    }
}

/* =========================
   UI RENDERING
========================= */
static void draw_button(SDL_Renderer *r, SDL_Rect rc, int on)
{
    if (on) SDL_SetRenderDrawColor(r, 60, 180, 160, 255);
    else    SDL_SetRenderDrawColor(r, 40, 40, 40, 255);

    SDL_RenderFillRect(r, &rc);
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_RenderDrawRect(r, &rc);
}

static void draw_keyboard(SDL_Renderer *r)
{
    /* Piano-like key proportions */
    const int margin = 40;
    const int kb_y = 120;
    const int kb_h = 190;
    const int white_h = 190;
    const int black_h = 120;

    const int white_w = (WINDOW_W - margin * 2) / NUM_NOTES;
    const int start_x = margin;

    /* white keys */
    for (int i = 0; i < NUM_NOTES; i++) {
        SDL_Rect wrc = { start_x + i * white_w, kb_y, white_w, white_h };

        if (note_active[i]) SDL_SetRenderDrawColor(r, 105, 165, 225, 255);
        else                SDL_SetRenderDrawColor(r, 238, 238, 238, 255);
        SDL_RenderFillRect(r, &wrc);

        SDL_SetRenderDrawColor(r, 10, 10, 10, 255);
        SDL_RenderDrawRect(r, &wrc);

        /* labels area */
        SDL_Rect lab = { wrc.x, wrc.y + wrc.h - 34, wrc.w, 34 };
        SDL_SetRenderDrawColor(r, 25, 25, 25, 255);
        SDL_RenderFillRect(r, &lab);

        SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
        char buf[16];
        snprintf(buf, sizeof(buf), "%s %d", note_names[i], i+1);
        draw_text(r, lab.x + 10, lab.y + 9, 3, buf);
    }

    /* black keys positions over the diatonic white keys:
       C# between C-D, D# between D-E, (no black between E-F),
       F# between F-G, G# between G-A, A# between A-B, (none between B-C)
    */
    const int blk_w = (int)(white_w * 0.58f);
    const int blk_y = kb_y;
    const int blk_h = black_h;

    int black_after_white[] = {0,1,3,4,5}; /* after C,D,F,G,A */
    for (int bi = 0; bi < 5; bi++) {
        int w = black_after_white[bi];
        int bx = start_x + (w + 1) * white_w - (blk_w / 2);

        SDL_Rect brc = { bx, blk_y, blk_w, blk_h };
        SDL_SetRenderDrawColor(r, 18, 18, 18, 255);
        SDL_RenderFillRect(r, &brc);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
        SDL_RenderDrawRect(r, &brc);

        /* small label */
        SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
        draw_text(r, brc.x + 8, brc.y + brc.h - 24, 2,
                  (w==0) ? "C#" :
                  (w==1) ? "D#" :
                  (w==3) ? "F#" :
                  (w==4) ? "G#" : "A#");
    }
}

static void draw_header(SDL_Renderer *r)
{
    SDL_Rect top = {0, 0, WINDOW_W, 80};
    SDL_SetRenderDrawColor(r, 18, 18, 18, 255);
    SDL_RenderFillRect(r, &top);

    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    draw_text(r, 40, 22, 4, "WINDOWS-SYNTH");
    SDL_SetRenderDrawColor(r, 170, 170, 170, 255);
    draw_text(r, 40, 52, 2, "1-8 NOTES  |  C CHORUS  |  T TREMOLO  |  SPACE ALL OFF  |  ESC QUIT");
}

static void draw_fx(SDL_Renderer *r)
{
    SDL_Rect chorus = { WINDOW_W/2 - 170, 318, 150, 32 };
    SDL_Rect trem   = { WINDOW_W/2 +  20, 318, 150, 32 };

    draw_button(r, chorus, chorus_on);
    draw_button(r, trem, tremolo_on);

    SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
    draw_text(r, chorus.x + 20, chorus.y + 9, 2, "CHORUS");
    draw_text(r, trem.x + 18,   trem.y + 9,   2, "TREMOLO");
}

/* =========================
   MAIN
========================= */
int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "Windows-Synth — Ensemble Instrument",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = 2;
    want.samples = 512;
    want.callback = audio_cb;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
    if (!dev) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(dev, 0);

    SDL_Event e;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;

                if (k == SDLK_ESCAPE) running = 0;

                if (k == SDLK_SPACE) {
                    all_notes_off();
                    for (int i = 0; i < NUM_NOTES; i++) note_active[i] = 0;
                }

                if (k >= SDLK_1 && k <= SDLK_8) {
                    int i = (int)(k - SDLK_1);
                    note_active[i] ^= 1;
                    if (note_active[i]) note_on(note_freqs[i]);
                    else                note_off(note_freqs[i]);
                }

                if (k == SDLK_c) chorus_on  ^= 1;
                if (k == SDLK_t) tremolo_on ^= 1;
            }
        }

        SDL_SetRenderDrawColor(ren, 12, 12, 12, 255);
        SDL_RenderClear(ren);

        draw_header(ren);
        draw_keyboard(ren);
        draw_fx(ren);

        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
