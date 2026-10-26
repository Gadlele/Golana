/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele */
/* Golana's 25 factory sounds (golana_prog.c gl_factory): each one's values that differ from INIT. 1..8 the
 * classic analog palette (basses, leads, brass, an arpeggio, an organ), 9..16 warm and ambient (pads, slow arps,
 * soft bells, a drone, a sweet lead, textures), 17..24 (1.2) made with the SUB oscillator and DRIFT, 25 (1.3) a
 * supersaw.
 * Time values: 40 ~ 20 ms, 60 ~ 80 ms, 80 ~ 0.3 s, 90 ~ 0.7 s, 100 ~ 1.4 s, 110 ~ 3 s, 120 ~ 6 s (gen_tables.py).
 * LFO FREQ: 20 ~ 0.15 Hz, 40 ~ 0.4 Hz, 60 ~ 1.2 Hz, 85 ~ 4.4 Hz, 90 ~ 5.6 Hz.
 * PATCH k (0..3): SRC FEG AEG LFO1 LFO2 VELO KBD, DST PITCH OSC2 CTRL1 NOISE CUTOFF AMP LFO2F, INT +-63 */
#define S(i, v) {(uint8_t)(i), (int16_t)(v)}
#define PROG(n, a) {n, a, (uint8_t)NELEM(a)}
#define PSRC(k) (GL_P1SRC + 3 * (k))
#define PDST(k) (GL_P1DST + 3 * (k))
#define PINT(k) (GL_P1INT + 3 * (k))
enum { W_SAW, W_PULSE, W_TRI, W_SINE, W_MORPH };
enum { X_SAW, X_SQR, X_TRI, X_MIX };
enum { L_SAW, L_SQR, L_TRI, L_SIN, L_SH };
enum { F_LPF24, F_LPF12, F_BPF, F_HPF };
enum { Q_FEG, Q_AEG, Q_LFO1, Q_LFO2, Q_VELO, Q_KBD };
enum { D_PITCH, D_OSC2, D_CTRL1, D_NOISE, D_CUT, D_AMP, D_LFO2F };
enum { U_SQR1, U_SQR2, U_SIN1, U_SIN2, U_SAW1, U_SAW2, U_MIX1, U_MIX2 };   /* SUB TYPE: -1 SQR .. -2 MIX */

/* ------------------------------------------------------------- classic --- */
static const gl_set_t F_FATBASS[] = {             /* two oscillators an octave apart, a round 24 dB filter */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, -12), S(GL_MIX2, 100),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 42), S(GL_RESO, 30), S(GL_FEGINT, 34), S(GL_KTRK, 20),
    S(GL_FA, 0), S(GL_FD, 75), S(GL_FS, 25), S(GL_FR, 50),
    S(P_ATK, 0), S(P_DEC, 80), S(P_SUS, 115), S(P_REL, 45), S(P_GLIDE, 15), S(P_LEVEL, 108), S(P_REV, 0),
};
static const gl_set_t F_ACID[] = {                /* a squelchy resonant line: legato notes glide, DIST on */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_MIX1, 127),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 30), S(GL_RESO, 108), S(GL_FEGINT, 46), S(GL_KTRK, 30),
    S(GL_FA, 0), S(GL_FD, 68), S(GL_FS, 0), S(GL_FR, 55), S(GL_EGRESET, 1), S(GL_DIST, 1),
    S(P_ATK, 0), S(P_DEC, 90), S(P_SUS, 127), S(P_REL, 40), S(P_GLIDE, 25), S(P_LEVEL, 100),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 30),
};
static const gl_set_t F_GLIDELEAD[] = {           /* PWM pulse and a fifth above, portamento, ping-pong echoes */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 35), S(GL_O1C2, 40),
    S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 7), S(GL_O2TUNE, 6), S(GL_MIX2, 70),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 82), S(GL_RESO, 30), S(GL_FEGINT, 18), S(GL_FD, 80), S(GL_FS, 60),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 45), S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 86), S(GL_L2KEY, 2),
    S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 10), S(P_SUS, 120), S(P_REL, 55), S(P_GLIDE, 55), S(P_LEVEL, 100),
    S(GL_DTYPE, 1), S(GL_DTIME, 1), S(GL_DFDBK, 55), S(P_DLY, 50), S(P_REV, 25),
};
static const gl_set_t F_UNISAW[] = {              /* every voice on one note, spread: a wall of saws */
    S(P_VOICE, V_UNISON), S(P_DETUNE, 70), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, -12),
    S(GL_MIX2, 80), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 92), S(GL_RESO, 18), S(GL_FEGINT, 10), S(GL_FD, 85),
    S(GL_FS, 80), S(P_ATK, 5), S(P_SUS, 120), S(P_REL, 60), S(P_GLIDE, 10), S(P_LEVEL, 94), S(P_REV, 30),
};
static const gl_set_t F_SYNCPAD[] = {             /* OSC 2 synced, its pitch swept by the filter EG */
    S(GL_O1WAVE, W_SAW), S(GL_MIX1, 30), S(GL_O2WAVE, X_SAW), S(GL_O2MOD, 2), S(GL_O2SEMI, 5), S(GL_MIX2, 127),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 100), S(GL_RESO, 15),
    S(GL_FA, 85), S(GL_FD, 100), S(GL_FS, 40), S(GL_FR, 95),
    S(PSRC(1), Q_FEG), S(PDST(1), D_OSC2), S(PINT(1), 40),
    S(P_ATK, 45), S(P_DEC, 90), S(P_SUS, 105), S(P_REL, 85), S(P_LEVEL, 96),
    S(GL_MFX, 1), S(P_IA, 35), S(P_IB, 60), S(P_IMIX, 90), S(P_REV, 45),
};
static const gl_set_t F_BRIGHTARP[] = {           /* a plucked pulse, 16ths over two octaves */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 25), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 12), S(GL_MIX2, 45),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 62), S(GL_RESO, 55), S(GL_FEGINT, 32), S(GL_FD, 70), S(GL_FS, 15),
    S(P_ATK, 0), S(P_DEC, 78), S(P_SUS, 0), S(P_REL, 60), S(P_LEVEL, 110),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 0), S(P_AOCT, 2), S(P_ARATE, 2), S(P_AGATE, 70),
    S(GL_DTYPE, 0), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 45), S(P_REV, 25),
};
static const gl_set_t F_BRASS[] = {               /* two detuned saws, a slow filter swell */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 9), S(GL_MIX2, 110),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 48), S(GL_RESO, 12), S(GL_FEGINT, 40), S(GL_KTRK, 40),
    S(GL_FA, 62), S(GL_FD, 85), S(GL_FS, 70), S(GL_FR, 70),
    S(P_ATK, 50), S(P_DEC, 90), S(P_SUS, 112), S(P_REL, 65), S(P_LEVEL, 98),
    S(GL_MFX, 3), S(P_IA, 30), S(P_IB, 50), S(P_IMIX, 70), S(P_REV, 30),
};
static const gl_set_t F_ORGAN[] = {               /* square and its octave, a rotating chorus */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 0), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 12), S(GL_MIX2, 90),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 105), S(GL_RESO, 5), S(GL_KTRK, 40),
    S(P_ATK, 2), S(P_DEC, 60), S(P_SUS, 127), S(P_REL, 30), S(P_LEVEL, 98),
    S(GL_MFX, 1), S(P_IA, 80), S(P_IB, 55), S(P_IMIX, 100), S(P_REV, 30),
};

/* ---------------------------------------------------------- warm, ambient --- */
static const gl_set_t F_SOFTPAD[] = {             /* soft triangles, a slowly breathing filter, a long hall */
    S(GL_O1WAVE, W_TRI), S(GL_O1C1, 12), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 10), S(GL_MIX2, 60),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 58), S(GL_RESO, 15), S(GL_FEGINT, 14), S(GL_KTRK, 25),
    S(GL_FA, 95), S(GL_FD, 105), S(GL_FS, 90), S(GL_FR, 100),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 30), S(GL_L1KEY, 0),
    S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 8),
    S(P_ATK, 92), S(P_DEC, 100), S(P_SUS, 120), S(P_REL, 105), S(P_LEVEL, 104),
    S(GL_MFX, 3), S(P_IA, 25), S(P_IB, 85), S(P_IMIX, 100),
    S(GL_RTYPE, 2), S(GL_RSIZE, 110), S(GL_RDAMP, 70), S(P_REV, 65),
};
static const gl_set_t F_PHASEPAD[] = {            /* two saws through a slow phaser that breathes */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 7), S(GL_MIX2, 100),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 66), S(GL_RESO, 18), S(GL_FEGINT, 8), S(GL_FA, 90), S(GL_FD, 100),
    S(GL_FS, 90), S(GL_FR, 100),
    S(P_ATK, 88), S(P_DEC, 100), S(P_SUS, 118), S(P_REL, 102), S(P_LEVEL, 96),
    S(GL_MFX, 4), S(P_IA, 18), S(P_IB, 110), S(P_IMIX, 127),
    S(GL_RTYPE, 2), S(GL_RSIZE, 105), S(P_REV, 55),
};
static const gl_set_t F_SLOWARP[] = {             /* a gentle eighth-note arpeggio into long echoes */
    S(GL_O1WAVE, W_SINE), S(GL_O1C1, 18), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 12), S(GL_MIX2, 50),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 80), S(GL_RESO, 10),
    S(P_ATK, 15), S(P_DEC, 92), S(P_SUS, 40), S(P_REL, 85), S(P_LEVEL, 108),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 2), S(P_AOCT, 2), S(P_ARATE, 1), S(P_AGATE, 85),
    S(GL_DTYPE, 1), S(GL_DTIME, 12), S(GL_DFDBK, 68), S(P_DLY, 70),
    S(GL_RTYPE, 2), S(GL_RSIZE, 112), S(P_REV, 60),
};
static const gl_set_t F_SOFTBELLS[] = {           /* a sine rung by an off-ratio OSC 2, the ring fading with the EG */
    S(GL_O1WAVE, W_SINE), S(GL_O1C1, 10), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 19), S(GL_O2TUNE, -14), S(GL_MIX2, 0),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 108), S(GL_RESO, 0),
    S(GL_FA, 0), S(GL_FD, 98), S(GL_FS, 0), S(GL_FR, 98),
    S(PSRC(1), Q_FEG), S(PDST(1), D_CTRL1), S(PINT(1), 45),
    S(P_ATK, 0), S(P_DEC, 108), S(P_SUS, 0), S(P_REL, 104), S(P_LEVEL, 112),
    S(GL_DTYPE, 0), S(GL_DTIME, 0), S(GL_DFDBK, 40), S(P_DLY, 30),
    S(GL_RTYPE, 2), S(GL_RSIZE, 118), S(GL_RDAMP, 55), S(P_REV, 75),
};
static const gl_set_t F_DRONE[] = {               /* low saws and noise, a resonant filter drifting very slowly */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, -12), S(GL_O2TUNE, 5), S(GL_MIX2, 110),
    S(GL_NOISE, 40), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 38), S(GL_RESO, 70),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 15), S(GL_L1KEY, 0), S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 22),
    S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 22),
    S(PSRC(3), Q_LFO2), S(PDST(3), D_NOISE), S(PINT(3), 20),
    S(P_ATK, 100), S(P_DEC, 100), S(P_SUS, 127), S(P_REL, 108), S(P_LEVEL, 96),
    S(GL_RTYPE, 2), S(GL_RSIZE, 120), S(GL_RDAMP, 80), S(P_REV, 70),
};
static const gl_set_t F_WARMLEAD[] = {            /* a round triangle lead, a gentle vibrato, portamento */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_TRI), S(GL_O1C1, 22), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 12),
    S(GL_MIX2, 35), S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 76), S(GL_RESO, 12),
    S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 88), S(GL_L2KEY, 2),
    S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 30), S(P_SUS, 120), S(P_REL, 70), S(P_GLIDE, 40), S(P_LEVEL, 108),
    S(GL_DTYPE, 0), S(GL_DTIME, 0), S(GL_DFDBK, 40), S(P_DLY, 35), S(GL_RTYPE, 2), S(P_REV, 45),
};
static const gl_set_t F_DARKUNI[] = {             /* unison saws far down the filter, a slow swell */
    S(P_VOICE, V_UNISON), S(P_DETUNE, 55), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, -12),
    S(GL_MIX2, 70), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 34), S(GL_RESO, 22), S(GL_FEGINT, 12),
    S(GL_FA, 100), S(GL_FD, 105), S(GL_FS, 90), S(GL_FR, 105),
    S(P_ATK, 95), S(P_DEC, 100), S(P_SUS, 124), S(P_REL, 105), S(P_LEVEL, 108),
    S(GL_MFX, 3), S(P_IA, 20), S(P_IB, 90), S(P_IMIX, 110),
    S(GL_RTYPE, 2), S(GL_RSIZE, 115), S(GL_RDAMP, 75), S(P_REV, 65),
};
static const gl_set_t F_SHTEXTURE[] = {           /* a slow sample and hold on the filter, echoes left and right */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 55), S(GL_O1C2, 35), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 7), S(GL_MIX2, 40),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 58), S(GL_RESO, 72),
    S(GL_L1WAVE, L_SH), S(GL_L1SYNC, 6), S(GL_L1KEY, 0),
    S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 30),
    S(P_ATK, 40), S(P_DEC, 100), S(P_SUS, 110), S(P_REL, 95), S(P_LEVEL, 98),
    S(GL_DTYPE, 1), S(GL_DTIME, 1), S(GL_DFDBK, 62), S(P_DLY, 55),
    S(GL_RTYPE, 2), S(GL_RSIZE, 108), S(P_REV, 50),
};

/* ------------------------------------------------------ SUB and DRIFT (1.2) --- */
static const gl_set_t F_SUBBASS[] = {             /* a saw on a square an octave down: round and deep */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 110), S(GL_DRIFT, 12),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 38), S(GL_RESO, 22), S(GL_FEGINT, 30), S(GL_KTRK, 20),
    S(GL_FA, 0), S(GL_FD, 72), S(GL_FS, 30), S(GL_FR, 50),
    S(P_ATK, 0), S(P_DEC, 80), S(P_SUS, 118), S(P_REL, 45), S(P_GLIDE, 12), S(P_LEVEL, 106), S(P_REV, 0),
};
static const gl_set_t F_REESE[] = {               /* two saws beating slowly over a sine an octave down */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 16), S(GL_MIX2, 120),
    S(GL_SUBTYPE, U_SIN1), S(GL_SUB, 100), S(GL_DRIFT, 30),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 52), S(GL_RESO, 25), S(GL_FEGINT, 12), S(GL_FD, 90), S(GL_FS, 70),
    S(P_ATK, 4), S(P_SUS, 124), S(P_REL, 55), S(P_GLIDE, 20), S(P_LEVEL, 100),
    S(GL_MFX, 4), S(P_IA, 20), S(P_IB, 70), S(P_IMIX, 70), S(P_REV, 15),
};
static const gl_set_t F_POLYCHORUS[] = {          /* a pulse with PWM on a sub square, a lush chorus: an 80s poly */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 30), S(GL_O1C2, 35), S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 42), S(GL_L1KEY, 0),
    S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 70), S(GL_DRIFT, 25),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 64), S(GL_RESO, 15), S(GL_FEGINT, 22), S(GL_KTRK, 30),
    S(GL_FA, 10), S(GL_FD, 88), S(GL_FS, 50), S(GL_FR, 80),
    S(P_ATK, 8), S(P_DEC, 90), S(P_SUS, 110), S(P_REL, 80), S(P_LEVEL, 98),
    S(GL_MFX, 1), S(P_IA, 35), S(P_IB, 75), S(P_IMIX, 110), S(GL_RTYPE, 2), S(GL_RSIZE, 100), S(P_REV, 35),
};
static const gl_set_t F_OLDSTRINGS[] = {          /* two saws an octave apart, slow and well worn, an ensemble */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 12), S(GL_O2TUNE, 4), S(GL_MIX2, 80), S(GL_DRIFT, 70),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 74), S(GL_RESO, 8), S(GL_KTRK, 30),
    S(P_ATK, 86), S(P_DEC, 100), S(P_SUS, 120), S(P_REL, 100), S(P_LEVEL, 96),
    S(GL_MFX, 3), S(P_IA, 30), S(P_IB, 100), S(P_IMIX, 120),
    S(GL_RTYPE, 2), S(GL_RSIZE, 112), S(GL_RDAMP, 70), S(P_REV, 60),
};
static const gl_set_t F_WORNTAPE[] = {            /* soft keys off a warped tape: DRIFT high, a slow wow */
    S(GL_O1WAVE, W_TRI), S(GL_O1C1, 18), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 12), S(GL_MIX2, 22), S(GL_DRIFT, 110),
    S(GL_SUBTYPE, U_SIN1), S(GL_SUB, 45),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 66), S(GL_RESO, 10), S(GL_FEGINT, 16), S(GL_FD, 95), S(GL_FS, 40),
    S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 32), S(GL_L2KEY, 0), S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 3),
    S(P_ATK, 4), S(P_DEC, 104), S(P_SUS, 70), S(P_REL, 92), S(P_LEVEL, 108),
    S(GL_MFX, 1), S(P_IA, 20), S(P_IB, 50), S(P_IMIX, 70),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 50), S(P_DLY, 35), S(GL_RTYPE, 2), S(GL_RSIZE, 108), S(P_REV, 50),
};
static const gl_set_t F_DRIFTLEAD[] = {           /* a saw and a square a fifth up, never quite in tune, gliding */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 7), S(GL_MIX2, 55), S(GL_DRIFT, 55),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 76), S(GL_RESO, 35), S(GL_FEGINT, 20), S(GL_FD, 85), S(GL_FS, 60),
    S(P_ATK, 6), S(P_SUS, 122), S(P_REL, 60), S(P_GLIDE, 45), S(P_LEVEL, 98),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 55), S(P_DLY, 45), S(GL_RTYPE, 2), S(P_REV, 30),
};
static const gl_set_t F_SUBARP[] = {              /* DOUBLE: each note twice, then its octave, over a sub square */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 40), S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 85), S(GL_DRIFT, 18),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 56), S(GL_RESO, 48), S(GL_FEGINT, 34), S(GL_FD, 66), S(GL_FS, 10),
    S(P_ATK, 0), S(P_DEC, 76), S(P_SUS, 0), S(P_REL, 58), S(P_LEVEL, 104),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 9), S(P_AOCT, 1), S(P_ARATE, 2), S(P_AGATE, 65),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 40), S(P_DLY, 35), S(P_REV, 20),
};
static const gl_set_t F_AGEDPAD[] = {             /* saws and a breath of noise, DRIFT all the way: an old machine */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 6), S(GL_MIX2, 100), S(GL_NOISE, 18), S(GL_DRIFT, 127),
    S(GL_SUBTYPE, U_SIN2), S(GL_SUB, 40),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 50), S(GL_RESO, 20), S(GL_FEGINT, 14),
    S(GL_FA, 100), S(GL_FD, 105), S(GL_FS, 90), S(GL_FR, 105),
    S(P_ATK, 98), S(P_DEC, 100), S(P_SUS, 122), S(P_REL, 108), S(P_LEVEL, 100),
    S(GL_MFX, 4), S(P_IA, 12), S(P_IB, 100), S(P_IMIX, 110),
    S(GL_RTYPE, 2), S(GL_RSIZE, 118), S(GL_RDAMP, 75), S(P_REV, 70),
};
/* ---------------------------------------------------------------- 1.3 --- */
static const gl_set_t F_SUPERSAW[] = {            /* every voice on the note, eight saws spread wide, a saw sub, an
                                                   * ensemble: the trance lead (one note at a time, UNISON) */
    S(P_VOICE, V_UNISON), S(P_DETUNE, 88), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 11), S(GL_MIX2, 120),
    S(GL_SUBTYPE, U_SAW1), S(GL_SUB, 35), S(GL_DRIFT, 20),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 108), S(GL_RESO, 10), S(GL_FEGINT, 8), S(GL_FD, 90), S(GL_FS, 100),
    S(P_ATK, 4), S(P_SUS, 124), S(P_REL, 72), S(P_GLIDE, 18), S(P_LEVEL, 92), S(P_SPRD, 127),
    S(GL_MFX, 3), S(P_IA, 28), S(P_IB, 80), S(P_IMIX, 100),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 40), S(GL_RTYPE, 2), S(GL_RSIZE, 110), S(P_REV, 50),
};

static const gl_prog_t GL_FACTORY[] = {
    PROG("FAT BASS", F_FATBASS),
    PROG("ACID LINE", F_ACID),
    PROG("GLIDE LEAD", F_GLIDELEAD),
    PROG("UNISON SAW", F_UNISAW),
    PROG("SYNC PAD", F_SYNCPAD),
    PROG("BRIGHT ARP", F_BRIGHTARP),
    PROG("80S BRASS", F_BRASS),
    PROG("ORGAN", F_ORGAN),
    PROG("SOFT PAD", F_SOFTPAD),
    PROG("PHASE PAD", F_PHASEPAD),
    PROG("SLOW ARP", F_SLOWARP),
    PROG("SOFT BELLS", F_SOFTBELLS),
    PROG("DRONE", F_DRONE),
    PROG("WARM LEAD", F_WARMLEAD),
    PROG("DARK UNISON", F_DARKUNI),
    PROG("S&H TEXTURE", F_SHTEXTURE),
    PROG("SUB BASS", F_SUBBASS),
    PROG("REESE", F_REESE),
    PROG("POLY CHORUS", F_POLYCHORUS),
    PROG("OLD STRINGS", F_OLDSTRINGS),
    PROG("WORN TAPE", F_WORNTAPE),
    PROG("DRIFT LEAD", F_DRIFTLEAD),
    PROG("SUB ARP", F_SUBARP),
    PROG("AGED PAD", F_AGEDPAD),
    PROG("SUPERSAW", F_SUPERSAW),
};
_Static_assert(NELEM(GL_FACTORY) == GL_NFACT, "25 factory sounds");
#undef S
#undef PROG
#undef PSRC
#undef PDST
#undef PINT
