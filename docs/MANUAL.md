# Golana manual

Golana is a personal fork of [Felucca](https://github.com/hugelton/Felucca) for the M-VAVE FM-1 by Gadlele:
a single analog-style synth laid out like the classic microKORG, with its own interface (the MATRIX).
Golana is free software under the GPL-3.0, like Felucca. Felucca's own README, for the parts Golana shares (the hardware, the credits), is [FELUCCA.md](../FELUCCA.md).

## The synth

| Section | Parameters |
| --- | --- |
| OSC 1 | WAVE (SAW, PULSE, TRI, SINE), CTRL 1 and CTRL 2, named on the screen for what they do with the WAVE: SAW OCTAVE (an octave-up saw blended in) and OCT LFO; PULSE WIDTH (the duty, 50 % to 4 %) and PWM; TRI FOLD (wavefold) and FOLD LFO; SINE X-MOD (cross modulation from OSC 2) and XMOD LFO. CTRL 2 is LFO 1 moving CTRL 1: the drawing shows, faint, the two ends it reaches. The fourth card: LFO 1's rate |
| OSC 2 | WAVE (SAW, SQR, TRI), MOD (OFF, RING, SYNC, R+S), SEMITONE, TUNE |
| MIXER | OSC 1, OSC 2, NOISE, FOLD (a wavefolder on the mix, before the filter) |
| SUB/DRIFT | (1.2) SUB TYPE (-1 SQR, -2 SQR, -1 SINE, -2 SINE: a square or a sine one or two octaves below OSC 1, in step with it), SUB LEVEL (into the mixer, 0 off), DRIFT (analog age: each note's two oscillators a little out of tune, each its own way, slowly wandering, and its cutoff a little off; 0 off) |
| FILTER | TYPE (LPF24, LPF12, BPF, HPF, MS20: a driven 12 dB low-pass whose resonance screams), CUTOFF, RESO, EG INT, KEY TRACK |
| FILTER EG / AMP EG | ATTACK, DECAY, SUSTAIN, RELEASE |
| AMP | LEVEL, PANPOT, DIST, SPREAD (the voices across the stereo field) |
| VOICE | ASSIGN (POLY, MONO, LEGATO, UNISON; 4 voices; in UNISON each voice's OSC 2 is detuned the other way: 8 detuned oscillators), DETUNE, PORTA, EG RESET |
| LFO 1 / LFO 2 | WAVE (SAW, SQR, TRI, SIN, S&H), FREQ, KEY SYNC (OFF, TIMBRE, VOICE), TEMPO (sync to a note value) |
| PATCH 1-4 | SOURCE (FEG, AEG, LFO1, LFO2, VELO, KBD), DEST (PITCH, OSC2, CTRL1, NOISE, CUTOFF, AMP, LFO2F), INTENSITY |
| SEQUENCER | 16 steps saved with the program. PATTERN: STEP, NOTE, TYPE (REST, NOTE, TIE), ACC / SLIDE; a key writes its note into the step and moves on. SETUP: LENGTH 1-16, RES, GATE, SWING |
| ARP | ON, TYPE (UP, DOWN, ALT1, ALT2, RANDOM, TRIGGER; 1.1: ORDER as pressed, CONVERGE from the ends to the middle, PEDAL the lowest note between the others, DOUBLE each note twice and its octave twice), RANGE 1-4, RES, GATE, SWING, LATCH, KEY SYNC, LAST STEP and steps on/off |
| MOD FX | TYPE (CHORUS, FLANGER, ENSEMBLE, PHASER), SPEED, DEPTH, MIX; TYPE METAL (a high-gain distortion with a strong mid hump): GAIN, MID, MID FREQ (200 Hz - 5 kHz) |
| DELAY | TYPE (STEREO, CROSS, L/R), TIME (tempo), FEEDBACK, LEVEL |
| REVERB | TYPE (ROOM, SPRING, HALL), SIZE, DAMP, LEVEL |
| GLOBAL | TEMPO, TUNE, CLOCK (INT, USB, TRS), MIDI IN (ALL, CH 1) |
| SETUP | FX ORDER, saved with the program: MFX>DLY>REV (the default), DLY>MFX>REV (the MOD FX after the delay, the reverb fed from it), DLY>REV>MFX (the MOD FX last) |

32 programs: 1-24 hold the factory sounds until written over (FAT BASS, ACID LINE, GLIDE LEAD, UNISON SAW,
SYNC PAD, BRIGHT ARP, 80S BRASS, ORGAN, SOFT PAD, PHASE PAD, SLOW ARP, SOFT BELLS, DRONE, WARM LEAD, DARK UNISON,
S&H TEXTURE; 1.2: SUB BASS, REESE, POLY CHORUS, OLD STRINGS, WORN TAPE, DRIFT LEAD, SUB ARP, AGED PAD), 25-32 start
as INIT. A program you wrote keeps its slot: one written in 17-24 before 1.2 is still there. MIDI Program Change 0-31
picks a program.

## The controls

| Control | What it does |
| --- | --- |
| SELECT | every page, one after another |
| KNOB 1-4 | the four parameters on the cards |
| ALGORITHM | CUTOFF, from any page |
| PRESETS | the program (it loads as you turn; edits not written are lost, as on the original) |
| HOME | PERFORM: cutoff, resonance, EG int, LFO 1 rate |
| EDIT / SCL / ENV / LFO | OSC 1, OSC 2, MIXER, SUB/DRIFT / FILTER, KEY TRACK / FILTER EG, AMP EG / LFO 1, LFO 2, PATCH 1-4 (press again for the next) |
| GLO / REC / SEQ / FX | AMP, VOICE, GLOBAL, SETUP (FX ORDER, PALETTE, HOME) / the sequencer: PATTERN, SETUP / the arpeggiator: ARP, TIMING, STEPS / MOD FX, DELAY, REVERB |

The pages that set a shape draw it above their four knobs, from the values themselves: OSC 1 and OSC 2 their wave,
FILTER its response curve, FILTER EG and AMP EG the envelope, LFO 1 and LFO 2 the running wave. HOME draws the sound
playing, every frame, three ways (GLO · SETUP · HOME): TRAIL, the default (the wave, and the last ones behind it in a
second colour of the palette: NIGHT's green), ORBIT (the wave against itself a moment later) and RIDGE (the last waves
stacked); ORBIT and RIDGE leave a trail too. GLO · SETUP · PALETTE: NIGHT, CRT, PAPER, NEON (kept with the settings).

The screen shows the open page as a tab, "BUTTON · PAGE" in that button's own colour, with the button's other pages as dark tabs beside it.
The header shows the program, its name and the battery (a bolt on USB power); a thin strip at the bottom shows the arpeggiator (a light, ARP, LATCH) on the left and the tempo (BPM, EXT on an external clock) on the right.

| ARP | the arpeggiator on / off |
| PLAY | the step sequencer: play / stop (LATCH is on SEQ · TIMING) |
| SAVE | WRITE: PRESETS picks the slot, KNOB 1 the letter, KNOB 2 changes it, SAVE writes, HOME cancels |
| OCT- / OCT+ | the keys' octave (both held 5 s: update mode, as in Felucca) |

## When something goes wrong

- **USB rescue** (after [dinghy](https://github.com/zvenson/dinghy)'s): hold **OCT-** alone while switching on. The FM-1
  starts with no sound and no interface, only the updater: install a working build from the web installer or the
  FM-1 Firmware Hub. If Golana crashes twice in a row while starting, the FM-1 goes there by itself; if the rescue
  itself fails, the next start goes to the chip's own update mode (UBOOT).
- A crash shows GOLANA CRASH with where it happened, then the FM-1 restarts.

## Building and testing

The firmware builds exactly as Felucca does (`./build.sh`, see BUILDING.md); the package is still called
`felucca.fwsc`. Host tests of Golana:

```
cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/golana_sound tests/golana_sound.c -lm
build/host/golana_sound build/golana_wav        # every factory program to a WAV, with levels
cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/golana_sim tests/golana_sim.c -lm
build/host/golana_sim build/golana_sim           # the whole firmware on a simulated FM-1, screenshots
web/emu/build_wasm.sh                            # the browser version: build/emu/golana.html
```

---
