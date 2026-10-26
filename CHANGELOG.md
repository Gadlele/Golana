# Changelog

Golana's versions, newest first. The numbers: **X.Y** adds something (saved programs still open), **X.Y.Z** only
fixes, a new **X** may change how things are saved.

## 1.3 — 2026-10-26

- **OSC 1's CTRL 1 and CTRL 2 say what they do**: their names change with the WAVE (SAW: OCTAVE, OCT LFO; PULSE:
  WIDTH, PWM; TRI: FOLD, FOLD LFO; SINE: X-MOD, XMOD LFO), PULSE's WIDTH shows its duty in %, the drawing shows
  faintly how far CTRL 2 (LFO 1) moves the wave, and the page's fourth card is LFO 1's rate.
- **HOME's scope, quicker and with a trail**: redrawn every frame with lines only, as Felucca's. A new view, **TRAIL**
  (the default): the wave now, the last ones behind it in the palette's second colour (NIGHT: orange and green),
  about 30 ms of sound (two or three cycles of a bass). ORBIT and RIDGE leave a trail too; TRAIL takes RING's place.
- **New waves**: OSC 1's **MORPH** (CTRL 1 from the saw to the square), OSC 2's **MIX** (a saw and a square at
  once), and four new SUB types: **-1 SAW, -2 SAW, -1 MIX, -2 MIX**.
- **Cleaner oscillators**: the wavefolders (FOLD, TRI's fold) without most of their aliasing (ADAA), SINE's cross
  modulation a little shallower high up (in tune, no whistling), and every note starts its oscillators from its own
  point of the wave, as an analog synth's (the attacks a little alive, no longer all alike).
- **Mod wheel** (MIDI CC1): a vibrato on its own, or, routed in the PATCH, the sources **MODW**, **MW*LFO1**,
  **MW*LFO2** (an LFO as deep as the wheel) and **AFTER** (aftertouch).
- **SCALE LOCK** (SCL › SCALE): every key and MIDI note in the scale you choose (16 scales, any key).
- **CHORD** (SCL › CHORD): a key or a MIDI note plays a chord: the scale's own triad or seventh, or a fixed shape
  (MAJ, MIN, DOM7, MAJ7, MIN7, SUS4, POW), in five voicings.
- **SUPERSAW**, a new factory sound in program 25 (a program you wrote there stays).

## 1.2 — 2026-10-20

- **SUB oscillator** (EDIT › SUB/DRIFT): a square or a sine one or two octaves below OSC 1, in step with it, with its
  own level into the mixer. Deep basses without giving up OSC 2.
- **DRIFT** (EDIT › SUB/DRIFT): analog age. Each note's two oscillators a little out of tune, each its own way, slowly
  wandering, and its filter a little off: from a touch of life to a worn-out machine.
- **Eight new factory sounds** in programs 17-24, made with them: SUB BASS, REESE, POLY CHORUS, OLD STRINGS,
  WORN TAPE, DRIFT LEAD, SUB ARP, AGED PAD. A program you wrote in 17-24 stays as it is.
- Programs saved with 1.0 and 1.1 load exactly as before (SUB and DRIFT off).

## 1.1 — 2026-10-19

- **Four new arpeggiator types** (SEQ › ARP › TYPE), after the six of 1.0, which play as before:
  - **ORDER**: the notes in the order you pressed them.
  - **CONVERGE**: from the outside in, the lowest, the highest, the second lowest, the second highest…
  - **PEDAL**: the lowest note between each of the others, as a pedal (C E C G C C′…).
  - **DOUBLE**: each note twice, then its octave twice (C C C′ C′, E E E′ E′, G G G′ G′).

## 1.0 — 2026-10-16

The first release. Golana is [Felucca](https://github.com/hugelton/Felucca) 1.5.1 turned into one analog-style synth,
laid out like the classic microKORG:

- **Two oscillators**: OSC 1 SAW, PULSE, TRI (wavefold) or SINE (cross modulation); OSC 2 SAW, SQR or TRI with RING
  and SYNC; noise and a wavefolder in the mixer.
- **Five filters**: LPF24, LPF12, BPF, HPF and MS20, with key tracking and its own envelope.
- **FILTER EG, AMP EG, two LFOs** with key and tempo sync, and **four patch cords**.
- **POLY, MONO, LEGATO, UNISON** (eight detuned oscillators), detune, portamento, spread.
- **Arpeggiator** and a **16-step sequencer** saved with each program.
- **Effects**: MOD FX (chorus, flanger, ensemble, phaser, METAL), delay and reverb in three orders.
- **32 programs**, 16 factory sounds; MIDI Program Change; CC74 and CC71 move cutoff and resonance.
- **Pages that draw** the sound; four palettes; a three-second power-on with the eighties' stripes.
- **USB rescue**: OCT− held at power-on, or after two failed starts.
