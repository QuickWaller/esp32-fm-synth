# ESP32 FM Synthesiser

A 12-button musical synthesiser built on an ESP32, generating audio in real
time and streaming it wirelessly over Bluetooth A2DP to a speaker — no
audio codec chip, no wired output, just the ESP32 doing DSP and pushing
PCM straight into the Bluetooth stack.

**This is the working instrument** — it plays. All synthesis here runs on
plain `float` math. A separate, incomplete experiment,
[`esp32-synth-fixed-point-wip`](https://github.com/QuickWaller/esp32-synth-fixed-point-wip),
is building a fixed-point (Q16.16) version of just the envelope generator,
for microcontrollers without hardware floating point — that work hasn't
been ported into this project yet.

## What it does

- 12 physical buttons, each wired to a GPIO pin, each producing one note
- Each note is generated with **two-operator FM (frequency modulation)
  synthesis**: a carrier sine oscillator whose frequency is modulated by
  a second sine oscillator, giving a richer, more harmonically complex
  tone than a plain sine or square wave
- Every keypress gets its own **ADSR envelope** (attack/decay/sustain/release)
  so notes fade in and out naturally instead of clicking on/off
- Up to **6 simultaneous voices** (polyphony), with automatic gain
  scaling so playing more notes at once doesn't clip the output
- Audio is generated as 16-bit PCM and streamed over **Bluetooth A2DP**
  (the ESP32 acts as an A2DP *source*, i.e. it pretends to be a phone
  connecting to a Bluetooth speaker)

## Hardware

- Board: ESP32 (`upesy_wroom` in `platformio.ini` — any ESP32 dev board
  using the WROOM module should work)
- 12 momentary buttons wired to GPIO pins `32, 33, 25, 26, 27, 14, 23,
  22, 21, 19, 18, 17`, each configured with the internal pull-up
  (`INPUT_PULLUP`) — so each button just needs to short the pin to
  ground, no external resistors needed
- No display, no audio output hardware on the board itself — all audio
  goes out over Bluetooth

## Button-to-note mapping

The 12 buttons play **12 consecutive chromatic semitones** (one full
octave, semitone by semitone) starting at octave 4 — button 0 is the
lowest note, button 11 is a semitone below the octave above it. This is
not currently scale-quantized: the code has scaffolding for named scales
(`majorPattern`, `minorPattern`, `pentatonicMajorPattern`,
`pentatonicMinorPattern`, `bluesPattern` in `State.cpp`, settable via
`set_scale()`), but that scale isn't actually wired into the frequency
calculation yet — it's stored but unused. Worth knowing if you're
picking this project back up.

## Bluetooth pairing

The firmware hardcodes the target speaker's name in `main.cpp`:

```cpp
bluetoothMaster.start("Tronsmart Trip");
```

**Change this to your own speaker's Bluetooth name** before flashing —
the ESP32-A2DP library will scan for and connect to a device advertising
that exact name. Watch the serial monitor at 115200 baud for connection
status; it'll print when it's searching, connecting, and connected.

## Why FM synthesis on a microcontroller

FM synthesis is comparatively cheap to compute (two oscillator lookups
and a multiply per sample) but sounds far more interesting than a plain
oscillator, which is why it was a popular choice on hardware-constrained
synths historically (the Yamaha DX7 being the canonical example). Doing
it on an ESP32 in real time means the entire signal chain — oscillators,
envelopes, mixing, soft-clipping, format conversion — has to complete
inside the time budget of one audio buffer, running alongside button
polling and the Bluetooth stack on the same core.

## Building

Requires [PlatformIO](https://platformio.org/). From the `Project Synth`
directory:

```
pio run                # build
pio run -t upload       # build and flash
pio device monitor      # serial output at 115200 baud
```

## Project layout

```
Project Synth/
├── src/
│   ├── main.cpp              — setup, button polling/debouncing, Bluetooth init, audio callback
│   ├── State.cpp             — global synth state: active voices, note frequencies, audio generation
│   ├── Voice.cpp             — one active note: FM synthesis + its ADSR envelope
│   ├── waves.cpp             — oscillator waveforms (sine via lookup table, square, triangle, sawtooth)
│   └── ADSR.cpp              — attack/decay/sustain/release envelope calculation
├── include/
│   ├── AudioDebug.h           — compile-time flag gating verbose per-sample logging (off by default)
│   └── *.h                    — headers for the above
└── platformio.ini
```

## A note on debug logging

Earlier revisions had `Serial.printf` debug logging running inside the
audio-generation hot path — including per-oscillator-sample logging
called up to tens of thousands of times per second. That's now gated
behind an `AUDIO_DEBUG` compile flag (`include/AudioDebug.h`, off by
default) so normal builds have zero logging overhead in the timing-critical
path — flip it to `1` if you need to debug the synthesis internals.
