# Windows-Synth

**Windows-Synth** is a native C / SDL2 polyphonic ensemble synthesizer for Windows, inspired by the orchestral pad and string textures of classic 16-bit era games (e.g. *The Legend of Zelda: A Link to the Past*).

The project focuses on **procedural sound synthesis**, not samples, and emphasizes a clear separation between:

* audio engine
* effect processing
* user interaction
* visual presentation

This is not a DAW, plugin, or tracker — it is a **standalone instrument**.

---

## Features

### Audio Engine

* Real-time procedural synthesis (no samples)
* Polyphonic voice allocation
* Layered oscillators per voice (square + triangle)
* Stereo output
* Deterministic voice behavior (no random jitter)

### Effects

* **Vibrato** (pitch modulation)
* **Tremolo** (amplitude modulation)
* **Chorus** (modulated short stereo delay)
* Effects can be toggled at runtime
* Effects applied in a clear, ordered signal chain

### Controls

* Keyboard-driven input only (no mouse interaction)
* Diatonic one-octave layout using number keys
* Toggle-style notes (sustained chords supported)

| Key     | Function                       |
| ------- | ------------------------------ |
| `1`–`8` | Toggle notes (C D E F G A B C) |
| `C`     | Toggle chorus                  |
| `T`     | Toggle tremolo                 |
| `SPACE` | All notes off                  |
| `ESC`   | Quit                           |

### User Interface

* Custom-rendered piano-style keyboard
* White and black keys drawn to real proportions
* Active notes visually highlighted
* Note names and key numbers labeled
* Effect panel with clear visual on/off state
* No external font libraries (custom block font rendering)

---

## Design Goals

* **Sound-first**: the audio engine drives the project, not the UI
* **Minimal dependencies**: SDL2 only
* **Deterministic behavior**: reproducible sound and visuals
* **Readable C code**: easy to modify and extend
* **Instrument-like interaction**, not sequencer or tracker behavior

---

## Architecture Overview

```
Oscillators
  → Voice Mixer
    → Vibrato (pitch)
      → Tremolo (amplitude)
        → Chorus (time / stereo)
          → Output
```

UI rendering is completely decoupled from audio processing.

---

## Project Structure

```
Windows-Synth/
├── src/
│   └── main.c        # Audio engine, effects, UI rendering
├── build/
│   └── synth.exe     # Build output (ignored by git)
├── Makefile
├── README.md
└── .gitignore
```

---

## Build Requirements

* Windows 10 / 11
* MinGW-w64 or equivalent GCC toolchain
* SDL2 development libraries
* `make`

SDL2 must be available in your compiler include and library paths.

---

## Building

From the repository root:

```cmd
make clean
make
```

This produces:

```
build\synth.exe
```

Run with:

```cmd
build\synth.exe
```

> **Note:** On Windows, the executable must be closed before rebuilding, as the OS locks running binaries.

---

## Current State

This project currently supports:

* playable polyphonic chords
* ensemble-style stereo width
* expressive motion via modulation
* clear visual feedback

It is intentionally **not** a complete synthesizer workstation.

---

## Planned / Possible Extensions

* Reverb (simple Schroeder or FDN)
* Octave shifting
* Preset save/load
* Additional chord modes
* Envelope controls (attack / release)
* Visual animation tied to audio amplitude
* MIDI input (future)

---

## License

MIT License

---

## Credits / Notes

This project is a learning-focused exploration of:

* real-time audio programming
* DSP fundamentals
* low-level UI rendering
* instrument-oriented software design

It is built without game engines, DAWs, or audio frameworks beyond SDL2.
