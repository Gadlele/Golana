<p align="center">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/splash.gif" width="240" alt="Golana's power-on animation">
</p>

<h1 align="center">Golana</h1>

<p align="center">
  <b>One analog-style synth for the M-VAVE FM-1</b><br>
  laid out like the classic microKORG, with a screen that draws the sound
</p>

<p align="center">
  <a href="https://gadlele.github.io/Golana/install/"><b>Install</b></a> ·
  <a href="https://gadlele.github.io/Golana/try/"><b>Try it in your browser</b></a> ·
  <a href="https://gadlele.github.io/Golana/">Website</a> ·
  <a href="https://gadlele.github.io/Golana/firmware/Golana-1.1.fwsc">Download 1.1</a>
</p>

<p align="center">
  <img alt="version 1.1" src="https://img.shields.io/badge/version-1.1-ffb547?style=flat-square&labelColor=0e1118">
  <img alt="FM-1" src="https://img.shields.io/badge/for-M--VAVE%20FM--1-f07a9c?style=flat-square&labelColor=0e1118">
  <img alt="GPL-3.0" src="https://img.shields.io/badge/license-GPL--3.0-b092ff?style=flat-square&labelColor=0e1118">
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/device.png" width="820" alt="The FM-1 running Golana">
</p>

**Golana** turns the M-VAVE FM-1 into a single, deep analog-style synthesizer with its own interface: two oscillators,
five filters, two envelopes, two LFOs, a patch bay, an arpeggiator and a step sequencer. Every page draws what it does.
It is a fork of [Felucca](https://github.com/hugelton/Felucca).

## The synth

| Section | |
| --- | --- |
| **OSC 1** | SAW (an octave-up saw blended in), PULSE (width), TRI (wavefold), SINE (cross modulation from OSC 2), MORPH (1.3: the saw to the square) |
| **OSC 2** | SAW, SQR, TRI, MIX · RING, SYNC or both · semitone, tune |
| **MIXER** | OSC 1, OSC 2, NOISE, and FOLD, a wavefolder before the filter |
| **SUB / DRIFT** | a sub oscillator, square, sine, saw or a mix, one or two octaves below OSC 1 · DRIFT, analog age: every note a little out of tune, its own way |
| **FILTER** | LPF24, LPF12, BPF, HPF and **MS20**, a driven 12 dB low-pass whose resonance screams · EG INT, KEY TRACK |
| **EG / LFO** | FILTER EG and AMP EG · two LFOs (SAW, SQR, TRI, SIN, S&H) with key sync and tempo sync |
| **PATCH 1–4** | FEG, AEG, LFO 1, LFO 2, velocity or keyboard to pitch, OSC 2, CTRL 1, noise, cutoff, amp or LFO 2's rate |
| **VOICE** | POLY, MONO, LEGATO, UNISON (eight detuned oscillators) · DETUNE, PORTA, SPREAD |
| **ARP / SEQ** | ten arp types over 1–4 octaves (1.1: ORDER, CONVERGE, PEDAL, DOUBLE) with latch and swing · a 16-step sequencer with ties, accents and slides |
| **EFFECTS** | MOD FX (chorus, flanger, ensemble, phaser, METAL distortion), delay, reverb, in three orders |
| **PLAYING** | (1.3) SCALE LOCK in 16 scales, CHORD from one key, the MIDI mod wheel as vibrato or a patch source |
| **PROGRAMS** | 32: FAT BASS, ACID LINE, SYNC PAD, 80S BRASS, SOFT BELLS, SUB BASS, WORN TAPE, AGED PAD, SUPERSAW… 25 factory sounds and seven free |

## On the screen

<p align="center">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/home_notes.png" width="200" alt="HOME">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/edit.png" width="200" alt="EDIT">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/scl.png" width="200" alt="FILTER">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/env.png" width="200" alt="ENV">
  <br>
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/lfo.png" width="200" alt="LFO">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/patch.png" width="200" alt="PATCH">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/fx.png" width="200" alt="FX">
  <img src="https://raw.githubusercontent.com/Gadlele/Golana/gh-pages/img/glo4.png" width="200" alt="SETUP">
</p>

Each page draws its shape from the values: the waves, the filter's curve, the envelopes, the LFOs running. HOME draws
the sound playing (TRAIL, ORBIT or RIDGE). Four palettes: NIGHT, CRT, PAPER and NEON.

## The controls

| Control | |
| --- | --- |
| **HOME** | PERFORM: cutoff, resonance, EG int, LFO 1 rate |
| **EDIT / SCL / ENV / LFO** | the oscillators and mixer / the filter / the envelopes / the LFOs and the patch bay |
| **GLO / REC / SEQ / FX** | amp, voice, global, setup / the sequencer / the arpeggiator / the effects |
| **KNOB 1–4** | the four cards on the screen · **ALGORITHM**: the cutoff from any page · **PRESETS**: the program |
| **ARP / PLAY / SAVE** | arpeggiator on and off / the sequencer / write a program |

## Install

1. Open the **[web installer](https://gadlele.github.io/Golana/install/)** in Chrome or Edge on a computer.
2. Connect the FM-1 with a USB **data** cable, straight into the computer.
3. Follow the five steps: the checklist, a backup, the build, connect, install. About two minutes.

From a terminal instead: `python3 tools/fm1_install.py Golana-1.1.fwsc` (needs `pip install mido python-rtmidi`).

**If something goes wrong:** switch the FM-1 on while holding **OCT−**. It starts **GOLANA USB RESCUE**, with only
USB, ready for a new install (it also starts by itself after two failed starts). The firmware never writes the FM-1's
boot code, and the installer can put the official M-VAVE firmware back at any time.

## The code

| | |
| --- | --- |
| `main` | the latest release, now **1.1** |
| `next` | the work on the next version |
| [Releases](https://github.com/Gadlele/Golana/releases) | every version: its notes and its package |
| [`gh-pages`](https://github.com/Gadlele/Golana/tree/gh-pages) | the website: project page, installer, emulator, package |

- [The manual](docs/MANUAL.md): every section, page and control
- [What changed](CHANGELOG.md), version by version · [How a version is released](RELEASING.md)
- Building: [BUILDING.md](BUILDING.md) (`./build.sh --release 1.1`); the tests: `sh tests/run_tests.sh`

## Credits

Golana is by **Gadlele**. It is a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
(@kurogedelic), Hügelton Instruments. The USB rescue follows zvenson's [dinghy](https://github.com/zvenson/dinghy).
See [LICENSING.md](LICENSING.md) for every bundled work and its licence.

Golana is free software under the [GPL-3.0](LICENSE), with no warranty. M-VAVE and FM-1 are trademarks of their
respective owners, microKORG is a trademark of Korg; Golana is independent and not affiliated with them.
