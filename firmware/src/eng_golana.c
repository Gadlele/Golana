/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele, based on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* GOLANA: the one engine of Golana, an analog-style synth after the classic microKORG layout.
 *
 *   OSC 1   SAW (CTRL1: a second saw an octave up blended in, brighter), PULSE (CTRL1: the width, 50 % .. ~5 %),
 *           TRI (CTRL1: folds the wave, more harmonics), SINE (CTRL1: cross modulation, OSC 2 moves its phase),
 *           (1.3) MORPH (CTRL1: from the saw to the square). CTRL2: LFO 1 on CTRL1 (PWM, a moving fold, ...)
 *   OSC 2   SAW / SQR / TRI / (1.3) MIX (a saw and a square at once), MOD OFF / RING (OSC 1 x OSC 2) / SYNC (restarts with OSC 1) / R+S, SEMI +-24, TUNE +-63 ct
 *   SUB     (1.2) a square or a sine (1.3: a saw, a MIX) an octave or two below OSC 1, in step with it, its LEVEL into the mixer
 *   MIXER   OSC 1, OSC 2 and NOISE levels into the filter
 *   FOLD    a wavefolder on the mix before the filter (Buchla / Serge style): 0 off, up to ~8x into the folds
 *   FILTER  LPF24 (two state-variable stages, the resonance on the second), LPF12, BPF, HPF, MS20 (a 12 dB low-pass
 *           driven at its input whose resonance saturates and screams, after the Korg MS-20's); CUTOFF, RESO, the
 *           filter EG's INT (+-), KEY TRACK (+-63, 63 = the cutoff follows the keys exactly)
 *   FILTER EG  its own ADSR per voice; EG RESET ON starts both envelopes from 0 on a retrigger
 *   AMP     the part's level, pan and ADSR (voice.c), DIST ON a soft drive after the filter
 *   LFO 1/2 per voice: SAW SQR TRI SIN S&H, FREQ or a SYNC note value of the tempo, KEY SYNC OFF (free, in step
 *           with the part's clock), TIMBRE (from 0 when the first key goes down), VOICE (from 0 every note)
 *   DRIFT   (1.2) analog age: every note's two oscillators a little off pitch, each its own way, slowly wandering,
 *           its cutoff a little off too, the oscillators free running (0: none, exactly as before)
 *   PATCH   4 routes SRC -> DST by INT (+-63): sources FEG AEG LFO1 LFO2 VELO KBD, (1.3) MODW (the MIDI mod wheel,
 *           CC1), MW*LFO1 / MW*LFO2 (the LFO as deep as the wheel: MW*LFO2 -> PITCH a wheel vibrato), AFTER (channel
 *           aftertouch); with no route from the wheel, the wheel is a vibrato of its own (5.5 Hz, up to +-40 ct).
 *           Destinations PITCH, OSC2 (OSC 2's pitch), CTRL1, NOISE, CUTOFF, AMP, LFO2F (LFO 2's rate)
 * Felucca's track LFO, LFO DEST and matrix are not used by this engine (mod.c leaves it alone): their parameter
 * slots hold Golana's own values (core.h GL_*). At most 4 voices (POLY and UNISON). */

static const char *const N_GO1WAVE[] = {"SAW", "PULSE", "TRI", "SINE", "MORPH"};   /* (1.3: MORPH) */
static const char *const N_GO2WAVE[] = {"SAW", "SQR", "TRI", "MIX"};               /* (1.3: MIX) */
static const char *const N_GO2MOD[] = {"OFF", "RING", "SYNC", "R+S"};
static const char *const N_GFTYPE[] = {"LPF24", "LPF12", "BPF", "HPF", "MS20"};
static const char *const N_GLWAVE[] = {"SAW", "SQR", "TRI", "SIN", "S&H"};
static const char *const N_GLKEY[] = {"OFF", "TIMBRE", "VOICE"};
static const char *const N_GLSYNC[] = {"OFF", "4BAR", "2BAR", "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "16T", "1/32"};
static const uint8_t GL_SYNC_DIV[11] = {0, 9, 8, 7, 6, 0, 1, 4, 2, 5, 3};   /* N_GLSYNC -> fx.c div_samples */
static const char *const N_GPSRC[] = {"FEG", "AEG", "LFO1", "LFO2", "VELO", "KBD",
                                      "MODW", "MW*LFO1", "MW*LFO2", "AFTER"};   /* (1.3: the last four) */
static const char *const N_GPDST[] = {"PITCH", "OSC2", "CTRL1", "NOISE", "CUTOFF", "AMP", "LFO2F"};
static const char *const N_GARPT[] = {"UP", "DOWN", "ALT1", "ALT2", "RANDOM", "TRIGGER",
                                      "ORDER", "CONVERGE", "PEDAL", "DOUBLE"};   /* (1.1: the last four) */
static const char *const N_GDTYPE[] = {"STEREO", "CROSS", "L/R"};
static const char *const N_GRTYPE[] = {"ROOM", "SPRING", "HALL"};
static const char *const N_GMFX[] = {"OFF", "CHORUS", "FLANGER", "ENSEMBL", "PHASER", "METAL"};
static const char *const N_GROUTE[] = {"MFX>DLY>REV", "DLY>MFX>REV", "DLY>REV>MFX"};
static const char *const N_GONOFF[] = {"OFF", "ON"};
static const char *const N_GSUB[] = {"-1 SQR", "-2 SQR", "-1 SINE", "-2 SINE", "-1 SAW", "-2 SAW", "-1 MIX", "-2 MIX"};
static const char *const N_GDTIME[] = {"1/4", "1/8", "1/16", "1/32", "1/8T", "16T", "1/2", "1/1", "2BAR", "4BAR",
                                       "1/8D", "16D", "1/4D"};
enum { GS_FEG, GS_AEG, GS_LFO1, GS_LFO2, GS_VELO, GS_KBD, GS_MODW, GS_MWL1, GS_MWL2, GS_AT, GS_N };
enum { GD_PITCH, GD_OSC2, GD_CTRL1, GD_NOISE, GD_CUT, GD_AMP, GD_LFO2F, GD_N };
enum { GO_SAW, GO_PULSE, GO_TRI, GO_SINE, GO_MORPH };
enum { GM_OFF, GM_RING, GM_SYNC, GM_RSYNC };

#define GPD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define GPE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}
/* Golana's meaning of the slots it borrows (core.h GL_*): params.c param_desc_of returns these for the GOLANA
 * engine (label 0: the slot keeps Felucca's own descriptor) */
static const param_desc_t GOL_TP[P_COUNT] = {
    [GL_MIX1] = GPD("OSC1", F_PCT, 0, 127, 127),
    [GL_FOLD] = GPD("FOLD", F_PCT, 0, 127, 0),
    [GL_MIX2] = GPD("OSC2", F_PCT, 0, 127, 0),
    [GL_NOISE] = GPD("NOISE", F_PCT, 0, 127, 0),
    [GL_FTYPE] = GPE("TYPE", N_GFTYPE, 0),
    [GL_RESO] = GPD("RESO", F_PCT, 0, 127, 20),
    [GL_FEGINT] = GPD("EGINT", F_BIPCT, -63, 63, 0),
    [GL_KTRK] = GPD("KEYTR", F_BIPCT, -63, 63, 32),
    [GL_FA] = GPD("ATK", F_TIME, 0, 127, 0),
    [GL_FD] = GPD("DEC", F_TIME, 0, 127, 64),
    [GL_FS] = GPD("SUS", F_PCT, 0, 127, 64),
    [GL_FR] = GPD("REL", F_TIME, 0, 127, 50),
    [GL_DIST] = GPE("DIST", N_GONOFF, 0),
    [GL_EGRESET] = GPE("RESET", N_GONOFF, 0),
    [GL_L1WAVE] = GPE("WAVE", N_GLWAVE, 2),
    [GL_L1FREQ] = GPD("FREQ", F_LFOHZ, 0, 127, 60),
    [GL_L1KEY] = GPE("KEY", N_GLKEY, 1),
    [GL_L1SYNC] = GPE("SYNC", N_GLSYNC, 0),
    [GL_L2WAVE] = GPE("WAVE", N_GLWAVE, 3),
    [GL_L2FREQ] = GPD("FREQ", F_LFOHZ, 0, 127, 70),
    [GL_L2KEY] = GPE("KEY", N_GLKEY, 1),
    [GL_L2SYNC] = GPE("SYNC", N_GLSYNC, 0),
    [GL_ARPON] = GPE("ARP", N_GONOFF, 0),
    [GL_ARPTYPE] = GPE("TYPE", N_GARPT, 0),
    [GL_ARPKSYNC] = GPE("KSYNC", N_GONOFF, 0),
    [GL_ARPLAST] = GPD("LAST", F_INT, 1, 8, 8),
    [GL_ARPST_A] = GPD("STP14", F_INT, 0, 15, 15),
    [GL_ARPST_B] = GPD("STP58", F_INT, 0, 15, 15),
    [GL_DTYPE] = GPE("DTYPE", N_GDTYPE, 0),
    [GL_DTIME] = GPE("DTIME", N_GDTIME, 1),
    [GL_DFDBK] = GPD("DFDBK", F_PCT, 0, 120, 55),
    [GL_RTYPE] = GPE("RTYPE", N_GRTYPE, 2),
    [GL_RSIZE] = GPD("RSIZE", F_PCT, 0, 127, 95),
    [GL_RDAMP] = GPD("RDAMP", F_PCT, 0, 127, 60),
    [GL_MFX] = GPE("MFX", N_GMFX, 0),
    [GL_ROUTE] = GPE("ROUTE", N_GROUTE, 0),
    [GL_SUBTYPE] = GPE("SUBTYP", N_GSUB, 0),
    [GL_SUB] = GPD("SUB", F_PCT, 0, 127, 0),
    [GL_DRIFT] = GPD("DRIFT", F_PCT, 0, 127, 0),
#define GPATCH(k, s, d) [GL_P1SRC + 3 * (k)] = GPE("SRC", N_GPSRC, s), [GL_P1DST + 3 * (k)] = GPE("DST", N_GPDST, d), \
                        [GL_P1INT + 3 * (k)] = GPD("INT", F_BIPCT, -63, 63, 0)
    GPATCH(0, GS_LFO2, GD_PITCH), GPATCH(1, GS_FEG, GD_PITCH), GPATCH(2, GS_LFO1, GD_CUT), GPATCH(3, GS_LFO1, GD_AMP),
#undef GPATCH
};
#undef GPD
#undef GPE

/* the voice state beyond voice_t (the part's engine memory, engines.c eng_mem) */
typedef struct {
    int32_t f1, f2, f3, f4;      /* the filter: stage 1 (LPF24's first), stage 2 */
    int32_t feg;                 /* filter EG, Q24 */
    uint32_t l1, l2;             /* LFO phases */
    int32_t s1, s2;              /* their S&H values, Q15 */
    int32_t nst;                 /* noise */
    int32_t jl;                  /* OSC 2 sync: 1 + the step of a restart a sample ago (16 bits), 0 none */
    uint8_t fst;                 /* filter EG stage: 0 off, 1 attack, 2 decay / sustain, 3 release */
    uint8_t sc;                  /* SUB: OSC 1's cycles (its low 2 bits: the sub's phase above OSC 1's) */
    uint16_t dn;                 /* DRIFT: blocks until the wander's next target */
    int16_t d1, d2, dc;          /* DRIFT: this note's offsets, OSC 1, OSC 2 and the cutoff (Q15, -1..1) */
    int16_t dw, dt;              /* DRIFT: the slow wander and its target (Q15) */
    int32_t fx, fg, tx, tg;      /* (1.3) the folds' ADAA: FOLD's and TRI's last input and its antiderivative */
} glv_t;
typedef struct {
    glv_t v[NVOICE];
    uint32_t free1, free2;       /* KEY SYNC OFF: the part's free clocks */
    uint32_t tim1, tim2;         /* TIMBRE: from 0 when the first key goes down */
    uint32_t inc1, inc2;         /* this block's base increments */
    int32_t seed;
    uint32_t vph;                /* (1.3) the wheel's own vibrato, its phase */
} glp_t;
ENG_MEM_FITS("GOLANA", sizeof(glp_t));
static glp_t *gl_part(const track_t *t) { return (glp_t *)ENG_MEM((uint32_t)(t - trk) % NPART); }
static uint32_t div_samples(uint32_t div);         /* (fx.c) */

static inline int32_t gp(const track_t *t, uint32_t id, int32_t lo, int32_t hi) { return clamp(t->p[id], lo, hi); }

/* an LFO's base increment per block: FREQ, or one cycle per SYNC note value */
static __attribute__((noinline)) uint32_t gl_lfo_inc(const track_t *t, uint32_t freq_id, uint32_t sync_id, int32_t fm)
{
    int32_t s = gp(t, sync_id, 0, 10);
    if (s) {
        uint32_t cyc = div_samples(GL_SYNC_DIV[s]), q, r;
        cyc = cyc ? cyc : 1u;
        q = 0xFFFFFFFFu / cyc;
        r = 0xFFFFFFFFu % cyc;
        return q * CTL + (r * CTL + CTL) / cyc;
    }
    return LFO_INC[clamp(t->p[freq_id] + fm, 0, 127)];
}

static int32_t gl_wave(uint32_t wave, uint32_t ph, int32_t sh)
{
    switch (wave) {
    case 0:
        return (int32_t)(ph >> 16) - 32768;
    case 1:
        return ph < 0x80000000u ? 32767 : -32767;
    case 2:
        return osc_tri(ph);
    case 3:
        return osc_sine(ph);
    default:
        return sh;
    }
}

/* one LFO of a voice, a block on: phase += inc, a new S&H value each cycle; its value (Q15, bipolar) */
static int32_t gl_lfo_step(uint32_t *ph, int32_t *sh, uint32_t inc, uint32_t wave, int32_t *seed)
{
    uint32_t old = *ph;
    *ph += inc;
    if (*ph < old || !*sh)
        *sh = (int32_t)(noise32(seed) >> 16) - 32768;
    return gl_wave(wave, *ph, *sh);
}

/* (1.3) the folders without their aliasing, ADAA (antiderivative anti-aliasing, first order). The fold is the
 * triangle T(x) = a - |w|, w = x - a wrapped into [-2a, 2a) (period 4a, a = 1 << lg: T(0) = 0, rising); the output
 * is T's mean between the last input and this one, (G(x) - G(x')) / (x - x'), G its antiderivative a w - w |w| / 2
 * (periodic: T's mean is 0); inputs closer than 4 apart take T at their middle */
static inline int32_t gl_tri_w(int32_t x, uint32_t lg)
{
    return (int32_t)(((uint32_t)x + (1u << lg)) & ((4u << lg) - 1u)) - (int32_t)(2u << lg);
}
static inline int32_t gl_tri_int(int32_t x, uint32_t lg)   /* (w (2a - |w|) <= a * a = 2^30: 32 bits, exact) */
{
    int32_t w = gl_tri_w(x, lg);
    return (w * ((2 << lg) - (w < 0 ? -w : w))) >> 1;
}
static inline int32_t gl_fold_aa(int32_t x, int32_t *xp, int32_t *gp, uint32_t lg)
{
    int32_t g = gl_tri_int(x, lg), d = x - *xp, y;
    if (d > 3 || d < -3) {
        y = (g - *gp) / d;
    } else {
        int32_t w = gl_tri_w((x + *xp) >> 1, lg);
        y = (1 << lg) - (w < 0 ? -w : w);
    }
    *xp = x;
    *gp = g;
    return y;
}

static void gol_block(track_t *t)
{
    glp_t *g = gl_part(t);
    g->inc1 = gl_lfo_inc(t, GL_L1FREQ, GL_L1SYNC, 0);
    g->inc2 = gl_lfo_inc(t, GL_L2FREQ, GL_L2SYNC, 0);
    g->free1 += g->inc1;
    g->free2 += g->inc2;
    g->tim1 += g->inc1;
    g->tim2 += g->inc2;
    g->vph += (uint32_t)((11ull << 31) / (FS / CTL));   /* 5.5 Hz */
    if (!g->seed)
        g->seed = 0x5EED1234;
}

static void gol_note_on(track_t *t, voice_t *v)
{
    glp_t *g = gl_part(t);
    glv_t *s = &g->v[(uint32_t)(v - t->v) % NVOICE];
    uint32_t i, others = 0, fresh = v->env == 0 && v->env_out == 0;
    for (i = 0; i < NVOICE; i++)
        others |= t->v[i].gate && &t->v[i] != v;
    if (!others) {                                      /* TIMBRE: the first key down starts the LFOs from 0 */
        g->tim1 = 0;
        g->tim2 = 0;
    }
    if (fresh) {
        s->f1 = s->f2 = s->f3 = s->f4 = 0;
        s->feg = 0;
        s->jl = 0;
        v->ph[0] = noise32(&g->seed);                   /* (1.3) free-running oscillators: each note from its own */
        v->ph[1] = noise32(&g->seed);                   /* point of the waves, as an analog synth's (1.2: from 0) */
        s->sc = (uint8_t)(v->ph[1] >> 30);
        s->fx = s->tx = 0;
        s->fg = gl_tri_int(0, 14);
        s->tg = gl_tri_int(0, 15);
        if (t->p[GL_DRIFT] > 0) {                       /* DRIFT: each note its own offsets */
            s->d1 = (int16_t)(noise32(&g->seed) >> 16);
            s->d2 = (int16_t)(noise32(&g->seed) >> 16);
            s->dc = (int16_t)(noise32(&g->seed) >> 16);
        }
        s->l1 = gp(t, GL_L1KEY, 0, 2) == 2 ? 0u : gp(t, GL_L1KEY, 0, 2) == 1 ? g->tim1 : g->free1;
        s->l2 = gp(t, GL_L2KEY, 0, 2) == 2 ? 0u : gp(t, GL_L2KEY, 0, 2) == 1 ? g->tim2 : g->free2;
        s->s1 = s->s2 = 0;
    } else {                                            /* retrigger: LFOs run on, KEY SYNC VOICE restarts them */
        if (gp(t, GL_L1KEY, 0, 2) == 2)
            s->l1 = 0;
        if (gp(t, GL_L2KEY, 0, 2) == 2)
            s->l2 = 0;
        if (t->p[GL_EGRESET]) {                         /* EG RESET: both envelopes from 0 */
            s->feg = 0;
            v->env = 0;
        }
    }
    if (!s->nst)
        s->nst = 0x2468ACE1 + (int32_t)v->age;
    s->fst = 1;
}

/* the filter EG, one control tick (as voice.c env_tick: linear attack, exponential decay and release) */
static int32_t gl_feg_tick(const track_t *t, voice_t *v, glv_t *s)
{
    if (!v->gate && s->fst && s->fst < 3u)
        s->fst = 3;
    switch (s->fst) {
    case 1:
        s->feg += (int32_t)ENV_LIN[gp(t, GL_FA, 0, 127)];
        if (s->feg >= (1 << 24)) {
            s->feg = 1 << 24;
            s->fst = 2;
        }
        break;
    case 2:
        s->feg += mulq16((gp(t, GL_FS, 0, 127) << 17) - s->feg, ENV_EXP[gp(t, GL_FD, 0, 127)]);
        break;
    case 3:
        s->feg -= mulq16(s->feg, ENV_EXP[gp(t, GL_FR, 0, 127)]);
        if (s->feg < (1 << 10)) {
            s->feg = 0;
            s->fst = 0;
        }
        break;
    default:
        s->feg = 0;
        break;
    }
    return s->feg >> 9;
}

/* a saw and a square at once, half each (OSC 2's MIX, the SUB's): the square's level (osc_pulse: +-16384) the saw's */
static inline int32_t gl_mix(uint32_t ph, uint32_t inc) { return (osc_saw(ph, inc) >> 1) + osc_pulse(ph, inc, 0x80000000u); }
/* OSC 2's wave at phase ph (increment inc) */
static inline int32_t gl_osc2(uint32_t w, uint32_t ph, uint32_t inc)
{
    return w == 1u ? osc_pulse(ph, inc, 0x80000000u) : w == 2u ? osc_tri(ph) : w == 3u ? gl_mix(ph, inc) : osc_saw(ph, inc);
}

/* DRIFT: the note's offsets and the wander (a new target every ~0.37 s, followed over about as long), in 1/4096 of
 * the pitch (2.37 a cent: up to +-12 ct each oscillator, +-5 ct the wander) -> *df1, *df2, and in cutoff units (the
 * return); (1.3) the wheel's own vibrato (wheel: no route plays the wheel), up to +-40 ct (95 / 4096). Out of line:
 * gol_render's loop as it was */
static __attribute__((noinline)) int32_t gl_drift(const track_t *t, glp_t *g, glv_t *s, int32_t drift, int32_t wheel,
                                                  int32_t *df1, int32_t *df2)
{
    int32_t dcut = 0;
    if (drift) {
        int32_t dw;
        if (!s->dn--) {
            s->dn = 511;
            s->dt = (int16_t)(noise32(&g->seed) >> 16);
        }
        s->dw = (int16_t)(s->dw + ((s->dt - s->dw) >> 9));
        dw = (s->dw * drift) >> 15;
        *df1 = (((s->d1 * drift) >> 15) * 28 >> 7) + (dw * 12 >> 7);
        *df2 = (((s->d2 * drift) >> 15) * 28 >> 7) + (dw * 12 >> 7);
        dcut = ((s->dc * drift) >> 15) * 5 + dw;
    }
    if (wheel && t->mw) {
        int32_t vb = (((osc_sine(g->vph) * t->mw) >> 7) * 95) >> 15;
        *df1 += vb;
        *df2 += vb;
    }
    return dcut;
}

static void gol_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    glp_t *g = gl_part(t);
    glv_t *s = &g->v[(uint32_t)(v - t->v) % NVOICE];
    const int16_t *p = t->p;
    uint32_t w1 = (uint32_t)gp(t, GL_O1WAVE, 0, 4), w2 = (uint32_t)gp(t, GL_O2WAVE, 0, 3);
    uint32_t md = (uint32_t)gp(t, GL_O2MOD, 0, 3), ft = (uint32_t)gp(t, GL_FTYPE, 0, 4), k, i;
    int32_t feg, lfo1, lfo2, src[GS_N], d[GD_N], ctrl1, cut, reso, x1, x2, xn, xs, gain;
    int32_t pitch1, pitch2, fold;
    uint32_t inc1, inc2;
    tsvf_t f1c, f2c;
    int32_t k2, ring = md == GM_RING || md == GM_RSYNC, sync = md == GM_SYNC || md == GM_RSYNC;
    int32_t dist = p[GL_DIST] != 0, drift = gp(t, GL_DRIFT, 0, 127), df1 = 0, df2 = 0, dcut = 0, wheel = 1;
    uint32_t st = (uint32_t)gp(t, GL_SUBTYPE, 0, 7), so = 1u + (st & 1u);   /* SUB: its type, its octaves down */
    /* control rate: the envelopes, the LFOs, the patch */
    feg = gl_feg_tick(t, v, s);
    for (k = 0; k < GD_N; k++)
        d[k] = 0;
    src[GS_FEG] = feg;
    src[GS_AEG] = m->envq15;
    src[GS_VELO] = v->vel * 258;
    src[GS_KBD] = clamp(((int32_t)v->note - 60) * 512, -32767, 32767);
    lfo1 = gl_lfo_step(&s->l1, &s->s1, g->inc1, (uint32_t)gp(t, GL_L1WAVE, 0, 4), &g->seed);
    src[GS_LFO1] = lfo1;
    src[GS_MODW] = t->mw * 258;
    src[GS_AT] = t->at * 258;
    src[GS_MWL1] = (lfo1 * t->mw) >> 7;
    {   /* LFO 2's rate may be patched: the routes to LFO2F first, from the sources known so far */
        int32_t f2 = 0;
        for (k = 0; k < 4u; k++) {
            int32_t a = gp(t, GL_P1INT + 3u * k, -63, 63), sr = gp(t, GL_P1SRC + 3u * k, 0, GS_N - 1);
            if (a && gp(t, GL_P1DST + 3u * k, 0, GD_N - 1) == GD_LFO2F && sr != GS_LFO2 && sr != GS_MWL2)
                f2 += (src[sr] * a) >> 14;
        }
        lfo2 = gl_lfo_step(&s->l2, &s->s2, f2 ? gl_lfo_inc(t, GL_L2FREQ, GL_L2SYNC, f2) : g->inc2,
                           (uint32_t)gp(t, GL_L2WAVE, 0, 4), &g->seed);
    }
    src[GS_LFO2] = lfo2;
    src[GS_MWL2] = (lfo2 * t->mw) >> 7;
    for (k = 0; k < 4u; k++) {
        int32_t a = gp(t, GL_P1INT + 3u * k, -63, 63), sr = gp(t, GL_P1SRC + 3u * k, 0, GS_N - 1);
        uint32_t ds = (uint32_t)gp(t, GL_P1DST + 3u * k, 0, GD_N - 1);
        if (a && ds != GD_LFO2F)
            d[ds] += src[sr] * a;                       /* Q15 x 63 */
        if (a && sr >= GS_MODW && sr <= GS_MWL2)
            wheel = 0;                                  /* (a route plays the wheel: no vibrato of its own) */
    }
    if (drift || (wheel && t->mw))
        dcut = gl_drift(t, g, s, drift, wheel, &df1, &df2);
    /* pitches (1/16 semitone): the voice's (glide, unison, tune, bend in m), the patch */
    pitch1 = m->pitch16 + ((d[GD_PITCH] * 3) >> 15);
    pitch2 = pitch1 + gp(t, GL_O2SEMI, -24, 24) * 16 + ((d[GD_OSC2] * 3) >> 15);
    inc1 = d[GD_PITCH] || df1 ? cents_inc(clamp(pitch1, 0, 2047), 0, m->fine + df1) : m->inc;
    /* UNISON: OSC 2 detuned the other way from OSC 1 (each voice two detuned oscillators: 8 for 4 voices) */
    inc2 = cents_inc(clamp(pitch2, 0, 2047), gp(t, GL_O2TUNE, -63, 63),
                     (t->p[P_VOICE] == V_UNISON ? m->fine - 2 * v->fine : m->fine) + df2);
    if (inc1 > 0x40000000u)
        inc1 = 0x40000000u;
    if (inc2 > 0x40000000u)
        inc2 = 0x40000000u;
    /* OSC 1's CTRL1, moved by CTRL2 (LFO 1) and the patch */
    ctrl1 = clamp(gp(t, GL_O1C1, 0, 127) + ((lfo1 * gp(t, GL_O1C2, 0, 127)) >> 15) + (d[GD_CTRL1] >> 14), 0, 127);
    /* the mixer: levels Q15 (a full oscillator into the filter at about a quarter of the voice's scale) */
    x1 = gp(t, GL_MIX1, 0, 127) * 258;
    x2 = gp(t, GL_MIX2, 0, 127) * 258;
    xn = clamp(gp(t, GL_NOISE, 0, 127) + (d[GD_NOISE] >> 14), 0, 127) * 258;
    xs = gp(t, GL_SUB, 0, 127) * 258;
    /* the filter: CUTOFF, the EG, KEY TRACK (63: the keys exactly, ~18.7 per 1/16 semitone), the patch */
    cut = (gp(t, GL_CUTOFF, 0, 127) << 8) + ((feg * gp(t, GL_FEGINT, -63, 63)) >> 6) +
          ((gp(t, GL_KTRK, -63, 63) * (m->pitch16 - 60 * 16) * 19) >> 6) + (d[GD_CUT] >> 6) + dcut;
    reso = gp(t, GL_RESO, 0, 127);
    tsvf_coef(&f2c, cut, reso);
    k2 = 8192 - reso * 7600 / 127;
    if (ft == 0u)
        tsvf_coef_k(&f1c, cut, 6000);                   /* LPF24: the first stage without the peak */
    else if (ft == 4u)
        tsvf_coef_k(&f1c, cut, 8192 - reso * 8140 / 127);   /* MS20: up to a ringing, near self-oscillating peak */
    fold = gp(t, GL_FOLD, 0, 127) ? 4096 + gp(t, GL_FOLD, 0, 127) * 230 : 0;   /* FOLD: the gain into the folds, Q12 */
    gain = clamp(32767 + (d[GD_AMP] >> 6), 0, 49151);  /* AMP: the patch's gain, up to 1.5 */
    {
        uint32_t ph0 = v->ph[0], ph1 = v->ph[1], pw, sc = s->sc;
        int32_t a1 = s->f1, a2 = s->f2, b1 = s->f3, b2 = s->f4, nst = s->nst, jl = s->jl;
        int32_t fx = s->fx, fg = s->fg, tx = s->tx, tg = s->tg;
        int32_t xm = ctrl1 * 380;                       /* SINE: the cross modulation's depth (phase, Q16 of a cycle) */
        if (m->pitch16 > 60 * 16)                       /* (1.3) halved an octave above C4, a third two above: in */
            xm = xm * 192 / (192 + m->pitch16 - 60 * 16);   /* tune high up, without its aliasing */
        int32_t tfold = 32768 + ctrl1 * 768;            /* TRI: the fold's gain, Q15 (1 .. 4) */
        int32_t oct = ctrl1 * 129;                      /* SAW: the octave-up saw's share */
        int32_t mrph = ctrl1 * 258;                     /* MORPH: the square's share, Q15 */
        pw = 0x80000000u + (uint32_t)ctrl1 * 0xF00000u; /* PULSE: 50 % .. ~6 % */
        for (i = 0; i < n; i++) {
            int32_t o1, o2, sx, y;
            /* OSC 2 first (SINE's cross modulation reads it); its sync restart rounded as in ANALOG's */
            if (sync && w2 == 0u) {
                int32_t bl = blep(ph1, inc2);
                if (jl) {
                    bl = (bl * ((jl - 1) >> 1)) >> 15;
                    jl = 0;
                }
                o2 = (int32_t)(ph1 >> 16) - 32768 - bl;
                if (ph0 > 0xFFFFFFFFu - inc1)
                    o2 -= (blep(ph0, inc1) * (int32_t)(ph1 >> 17)) >> 15;
            } else {
                o2 = gl_osc2(w2, ph1, inc2);
            }
            switch (w1) {
            case GO_PULSE:
                o1 = osc_pulse(ph0, inc1, pw);
                break;
            case GO_TRI:                                /* reflected at the edges: a triangle of period 131072 */
                o1 = gl_fold_aa((osc_tri(ph0) * (tfold >> 4)) >> 11, &tx, &tg, 15u);
                break;
            case GO_SINE:
                o1 = osc_sine(ph0 + (((uint32_t)o2 * (uint32_t)xm) << 1));   /* (wraps: a phase) */
                break;
            case GO_MORPH: {                            /* (1.3) CTRL 1 from the saw to the square */
                int32_t sw = osc_saw(ph0, inc1);
                o1 = sw + mulq15((osc_pulse(ph0, inc1, 0x80000000u) << 1) - sw, mrph);
                break;
            }
            default:
                o1 = osc_saw(ph0, inc1);
                if (oct)
                    o1 = mulq15(o1, 32767 - (oct >> 1)) + mulq15(osc_saw(ph0 << 1, inc1 << 1), oct >> 1);
                break;
            }
            if (ring)
                o2 = mulq15(o1, o2);
            sx = mulq15(o1, x1) + mulq15(o2, x2);
            if (xs) {                                   /* SUB: OSC 1's phase an octave or two down, OSC 1's cycles above it */
                uint32_t sp = sc << (32u - so) | ph0 >> so, si = inc1 >> so;
                int32_t u = st < 2u ? osc_pulse(sp, si, 0x80000000u) : st < 4u ? osc_sine(sp)
                          : st < 6u ? osc_saw(sp, si) : gl_mix(sp, si);   /* SQR, SINE, (1.3) SAW, MIX */
                sx += mulq15(u, xs);
            }
            ph0 += inc1;
            ph1 += inc2;
            sc += ph0 < inc1;
            if (sync && ph0 < inc1) {                   /* OSC 1 wrapped: OSC 2 restarts with it */
                uint32_t f = ph0 / ((inc1 >> 16) | 1u);
                uint32_t at = ph1 - f * (inc2 >> 16);
                jl = (int32_t)(at >> 16) + 1;
                ph1 = f * (inc2 >> 16);
            }
            if (xn)
                sx += mulq15((int32_t)(noise32(&nst) >> 17) - 16384, xn);
            sx = (sx * 3) >> 3;                         /* one oscillator ~12 k into the filter, all three ~32 k */
            if (fold) {                                 /* FOLD: the wave reflected at +-16384, more folds the harder */
                int32_t u = gl_fold_aa((clamp(sx, -65535, 65535) * (fold >> 2)) >> 10, &fx, &fg, 14u);
                sx = u + (u >> 2);                      /* (x 1.25: about the level it came in at) */
            }
            switch (ft) {
            case 0:
                y = tsvf_lp(&f1c, sx, &a1, &a2);
                y = tsvf_lp(&f2c, y, &b1, &b2);
                break;
            case 2:
                y = analog_tap(&f2c, k2, sx, &b1, &b2, 1u);
                break;
            case 3:
                y = analog_tap(&f2c, k2, sx, &b1, &b2, 2u);
                break;
            case 4: {                                   /* MS20: driven in, the resonating state saturated */
                int32_t in = softclip(clamp(sx * 2, -65535, 65535)) >> 1, v1, v2, v3 = in - b2;
                v1 = (f1c.a1 * b1 + f1c.a2 * v3) >> 13;
                v2 = b2 + ((f1c.a2 * b1 + f1c.a3 * v3) >> 13);
                b1 = softclip(clamp((2 * v1 - b1) >> 1, -65535, 65535)) << 1;   /* the scream: saturated, not runaway */
                b2 = clamp(2 * v2 - b2, -150000, 150000);
                y = clamp(v2, -65535, 65535);
                break;
            }
            default:
                y = tsvf_lp(&f2c, sx, &b1, &b2);
                break;
            }
            y = soft_knee(y, 16000) << 1;
            if (dist)
                y = softclip(clamp(y, -32767, 32767) * 4);
            if (gain != 32767)
                y = mulq15(y, gain);
            out[i] += voice_amp(clamp(y, -65535, 65535), m, i) << 1;
        }
        v->ph[0] = ph0;
        v->ph[1] = ph1;
        s->sc = (uint8_t)sc;
        s->f1 = a1;
        s->f2 = a2;
        s->f3 = b1;
        s->f4 = b2;
        s->nst = nst;
        s->fx = fx;
        s->fg = fg;
        s->tx = tx;
        s->tg = tg;
        s->jl = jl;
    }
}

/* the voice ends (its amp envelope's release) on its own; the filter EG's release does not hold it */
static const preset_t GOLANA_PRESETS[] = {
    {"INIT", {0, 0, 0, 0, 0, 0, 0, 100}, {0, 64, 127, 40}, 0, 0, FX(0, 0, 0, 0), 0},
};

static const engine_t ENG_GOLANA = {
    .name = "GOLANA",
    .page_title = {"OSC", "OSC2"},
    .edit = {
        {"WAVE", F_ENUM, 0, 4, 0, N_GO1WAVE, 0},
        {"CTRL1", F_INT, 0, 127, 0, 0, 0},
        {"CTRL2", F_INT, 0, 127, 0, 0, 0},
        {"WAVE2", F_ENUM, 0, 3, 0, N_GO2WAVE, 0},
        {"MOD", F_ENUM, 0, 3, 0, N_GO2MOD, 0},
        {"SEMI", F_SEMI, -24, 24, 0, 0, 0},
        {"TUNE", F_INT, -63, 63, 0, 0, "ct"},
        {"CUT", F_CUTOFF, 0, 127, 100, 0, 0},
    },
    .presets = GOLANA_PRESETS,
    .npresets = NELEM(GOLANA_PRESETS),
    .knob = {P_E7, GL_RESO, GL_FEGINT, P_E1},
    .poly = 4,
    .keep = 0,
    .note_on = gol_note_on,
    .render = gol_render,
    .block = gol_block,
};
