/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele, based on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Golana's interface: the MATRIX. In place of Felucca's UI (main.c and the browser build call gu_input, gu_leds and
 * gu_draw instead of ui_input, ui_leds and ui_draw; the rest of Felucca's UI code is never called).
 *
 * The screen (240 x 240): a header (the program, its name, PLAY / the octave, the battery), the tabs, four cards and a
 * service strip at the bottom (the arpeggiator left, the tempo right).
 *   SELECT     the section (and its pages: FILTER 1-2, PATCH 1-4, ARP 1-3), one turn a page
 *   KNOB 1..4  the four parameters on the cards
 *   ALGORITHM  CUTOFF, whatever the page (as the original's cutoff knob)
 *   PRESETS    the program, 1..32: it loads as the knob turns (edits not written are lost, as on the original)
 *   HOME       the PERFORM page; EDIT OSC 1 / OSC 2 / MIXER; SCL FILTER; ENV FILTER EG / AMP EG; LFO LFO 1 / LFO 2 /
 *              PATCH 1..4; GLO AMP / VOICE / GLOBAL / ROUTE; SEQ the ARP pages; REC the sequencer; FX MOD FX / DELAY / REVERB
 *              (a second press: the next one of the group)
 *   ARP        the arpeggiator on / off;  PLAY  the step sequencer: play / stop;  OCT- OCT+  the keys' octave
 *   REC        the step sequencer: PATTERN (a key writes its note into the step and moves on), SETUP
 *   SAVE       WRITE: PRESETS picks the slot, KNOB 1 the letter, KNOB 2 changes it, SAVE writes, HOME cancels
 * Programs: 32 slots in the user preset store (upreset.c); slots 1..16 play the factory sounds until written over,
 * 17..32 INIT. MIDI Program Change 0..31 loads a program. */

/* ---------------------------------------------------------------- colours --- */
/* the palettes (GLO SETUP PALETTE): the grounds, the text, and one colour per page button (its tabs, its meters, its
 * drawing), in the order HOME EDIT SCL ENV LFO GLO REC SEQ FX */
typedef struct {
    uint16_t bg, surf, raise, line, text, mid, dim, ink;
    uint8_t light;
    uint16_t btn[9];
} gu_pal_t;
static const char *const N_GPAL[] = {"NIGHT", "CRT", "PAPER", "NEON"};
static const char *const N_GVIEW[] = {"TRAIL", "ORBIT", "RIDGE"};   /* (1.3: TRAIL in RING's place) */
static const gu_pal_t GU_PALS[4] = {
    {RGB(14, 17, 24), RGB(26, 30, 40), RGB(38, 44, 58), RGB(48, 54, 70), RGB(232, 230, 225), RGB(150, 156, 172),
     RGB(88, 94, 110), RGB(14, 17, 24), 0,
     {RGB(255, 181, 71), RGB(255, 128, 92), RGB(236, 214, 96), RGB(240, 122, 156), RGB(176, 146, 255),
      RGB(92, 206, 170), RGB(150, 214, 104), RGB(102, 170, 255), RGB(112, 214, 238)}},
    {RGB(4, 10, 6), RGB(10, 22, 13), RGB(18, 36, 24), RGB(26, 52, 34), RGB(186, 245, 200), RGB(96, 164, 116),
     RGB(46, 90, 61), RGB(4, 10, 6), 0,                 /* CRT: phosphors, green the main one */
     {RGB(255, 190, 80), RGB(255, 150, 70), RGB(220, 240, 120), RGB(120, 255, 170), RGB(90, 230, 220),
      RGB(70, 240, 120), RGB(170, 255, 110), RGB(110, 220, 255), RGB(140, 255, 210)}},
    {RGB(230, 227, 220), RGB(244, 242, 238), RGB(216, 212, 204), RGB(196, 191, 181), RGB(34, 32, 28),
     RGB(107, 102, 93), RGB(163, 158, 148), RGB(250, 249, 246), 1,   /* PAPER: light, the colours deeper */
     {RGB(206, 120, 0), RGB(214, 76, 40), RGB(160, 130, 0), RGB(200, 50, 100), RGB(110, 70, 210),
      RGB(0, 140, 110), RGB(70, 140, 30), RGB(30, 100, 210), RGB(0, 130, 170)}},
    {RGB(18, 11, 30), RGB(29, 20, 48), RGB(42, 30, 68), RGB(58, 43, 88), RGB(242, 236, 255), RGB(169, 155, 200),
     RGB(94, 81, 128), RGB(18, 11, 30), 0,              /* NEON */
     {RGB(255, 225, 77), RGB(255, 154, 60), RGB(182, 255, 59), RGB(255, 79, 160), RGB(176, 124, 255),
      RGB(60, 255, 194), RGB(255, 92, 240), RGB(92, 140, 255), RGB(63, 232, 255)}},
};
static uint8_t gu_pi, gu_view;                  /* the palette, HOME's drawing (kept with the settings: gu_prefs) */
#define GU_P (&GU_PALS[gu_pi & 3u])
#define GC_BG (GU_P->bg)
#define GC_SURF (GU_P->surf)
#define GC_RAISE (GU_P->raise)
#define GC_LINE (GU_P->line)
#define GC_TEXT (GU_P->text)
#define GC_MID (GU_P->mid)
#define GC_DIM (GU_P->dim)
#define GC_INK (GU_P->ink)
#define GC_AMBER (GU_P->btn[0])
#define GC_TEAL (GU_P->btn[5])
#define GC_PINK (GU_P->btn[3])

/* ------------------------------------------------------------ the matrix --- */
/* virtual parameters (not a track parameter of part 1) */
enum { GV_NONE = 0xFF, GV_RES = 200, GV_BPM, GV_TUNE, GV_CLOCK, GV_STEP, GV_STEPON, GV_MIDI, GV_PAL, GV_VIEW,
       GV_SSTEP, GV_SNOTE, GV_STYPE, GV_SFLAG, GV_SLEN };   /* (the step sequencer: REC) */
typedef struct { uint8_t id; const char *label; const char *const *names; uint8_t nn; } gu_par_t;   /* nn: names' count */
#define GN(a) a, (uint8_t)NELEM(a)                    /* (the lists have no 0 at their end: GOL_TP's GPE counts them) */
typedef struct { uint8_t btn; const char *tab, *tshort; gu_par_t k[4]; } gu_page_t;
static const char *const N_GVOICE[] = {"POLY", "MONO", "LEGATO", "UNISON"};
static const char *const N_GRES[] = {"1/32", "1/24", "1/16", "1/12", "1/8", "1/4"};
static const uint8_t GU_RES_DIV[6] = {3, 5, 2, 4, 1, 0};    /* N_GRES -> P_ARATE (params.c N_DIV) */
static const char *const N_GCLOCK[] = {"INT", "USB", "TRS"};
static const char *const N_GMIDI[] = {"ALL", "CH 1"};
static const char *const N_GSTYPE[] = {"REST", "NOTE", "TIE"};
static const char *const N_GSFLAG[] = {"OFF", "ACCENT", "SLIDE", "BOTH"};
static const char *const N_GLOCK[] = {"OFF", "ON", "WHITE"};   /* (1.3) P_QUANT: OFF, SNAP, WHITE (seq.c QN_*) */
static const char *const N_GKEY[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* the pages, grouped by the button that opens them (its pages side by side: the tabs). tab: the page's name on its
 * tab when it is open (after the button's name), tshort: on its tab while another page of the button is open */
static const gu_page_t GU_PAGES[] = {
    {B_HOME, "PERFORM", "PERFORM",
     {{GL_CUTOFF, "CUTOFF", 0}, {GL_RESO, "RESO", 0}, {GL_FEGINT, "EG INT", 0}, {GL_L1FREQ, "LFO 1", 0}}},
    {B_EDIT, "OSC 1", "OSC 1", {{GL_O1WAVE, "WAVE", 0}, {GL_O1C1, "CTRL 1", 0}, {GL_O1C2, "CTRL 2", 0}, {GL_L1FREQ, "LFO 1", 0}}},
    {B_EDIT, "OSC 2", "OSC 2",
     {{GL_O2WAVE, "WAVE", 0}, {GL_O2MOD, "MOD", 0}, {GL_O2SEMI, "SEMI", 0}, {GL_O2TUNE, "TUNE", 0}}},
    {B_EDIT, "MIXER", "MIXER", {{GL_MIX1, "OSC 1", 0}, {GL_MIX2, "OSC 2", 0}, {GL_NOISE, "NOISE", 0}, {GL_FOLD, "FOLD", 0}}},
    {B_EDIT, "SUB/DRIFT", "SUB",                     /* (1.2) */
     {{GL_SUBTYPE, "SUB TYPE", 0}, {GL_SUB, "SUB LEVEL", 0}, {GL_DRIFT, "DRIFT", 0}, {GV_NONE, 0, 0}}},
    {B_SCL, "FILTER", "FILTER",
     {{GL_FTYPE, "TYPE", 0}, {GL_CUTOFF, "CUTOFF", 0}, {GL_RESO, "RESO", 0}, {GL_FEGINT, "EG INT", 0}}},
    {B_SCL, "KEY TRACK", "KEY TRK", {{GL_KTRK, "KEY TRACK", 0}, {GV_NONE, 0, 0}, {GV_NONE, 0, 0}, {GV_NONE, 0, 0}}},
    /* (1.3) SCALE LOCK: every key (and MIDI note) in the scale; CHORD: a key plays a chord (chord.c) */
    {B_SCL, "SCALE", "SCALE",
     {{P_QUANT, "LOCK", GN(N_GLOCK)}, {P_ROOT, "KEY", GN(N_GKEY)}, {P_SCALE, "SCALE", 0}, {GV_NONE, 0, 0}}},
    {B_SCL, "CHORD", "CHORD", {{P_CHRD, "CHORD", 0}, {P_VOIC, "VOICING", 0}, {GV_NONE, 0, 0}, {GV_NONE, 0, 0}}},
    {B_ENV, "FILTER EG", "FILTER EG",
     {{GL_FA, "ATTACK", 0}, {GL_FD, "DECAY", 0}, {GL_FS, "SUSTAIN", 0}, {GL_FR, "RELEASE", 0}}},
    {B_ENV, "AMP EG", "AMP EG", {{P_ATK, "ATTACK", 0}, {P_DEC, "DECAY", 0}, {P_SUS, "SUSTAIN", 0}, {P_REL, "RELEASE", 0}}},
    {B_LFO, "LFO 1", "L1",
     {{GL_L1WAVE, "WAVE", 0}, {GL_L1FREQ, "FREQ", 0}, {GL_L1KEY, "KEY SYNC", 0}, {GL_L1SYNC, "TEMPO", 0}}},
    {B_LFO, "LFO 2", "L2",
     {{GL_L2WAVE, "WAVE", 0}, {GL_L2FREQ, "FREQ", 0}, {GL_L2KEY, "KEY SYNC", 0}, {GL_L2SYNC, "TEMPO", 0}}},
#define GU_PATCH(k, n, sh) {B_LFO, n, sh, {{GL_P1SRC + 3 * (k), "SOURCE", 0}, {GL_P1DST + 3 * (k), "DEST", 0}, \
                                          {GL_P1INT + 3 * (k), "INTENSITY", 0}, {GV_NONE, 0, 0}}}
    GU_PATCH(0, "PATCH 1", "P1"), GU_PATCH(1, "PATCH 2", "P2"), GU_PATCH(2, "PATCH 3", "P3"),
    GU_PATCH(3, "PATCH 4", "P4"),
#undef GU_PATCH
    {B_GLO, "AMP", "AMP", {{P_LEVEL, "LEVEL", 0}, {P_PAN, "PANPOT", 0}, {GL_DIST, "DIST", 0}, {P_SPRD, "SPREAD", 0}}},
    {B_GLO, "VOICE", "VOICE",
     {{P_VOICE, "ASSIGN", GN(N_GVOICE)}, {P_DETUNE, "DETUNE", 0}, {P_GLIDE, "PORTA", 0}, {GL_EGRESET, "EG RESET", 0}}},
    {B_GLO, "GLOBAL", "GLOB",
     {{GV_BPM, "TEMPO", 0}, {GV_TUNE, "TUNE", 0}, {GV_CLOCK, "CLOCK", GN(N_GCLOCK)}, {GV_MIDI, "MIDI IN", GN(N_GMIDI)}}},
    {B_GLO, "SETUP", "SETUP", {{GL_ROUTE, "FX ORDER", 0}, {GV_PAL, "PALETTE", 0}, {GV_VIEW, "HOME", 0}, {GV_NONE, 0, 0}}},
    {B_SEQ, "ARP", "ARP", {{GL_ARPON, "ARP", 0}, {GL_ARPTYPE, "TYPE", 0}, {P_AOCT, "RANGE", 0}, {GV_RES, "RES", GN(N_GRES)}}},
    {B_SEQ, "TIMING", "TIMING",
     {{P_AGATE, "GATE", 0}, {P_ASWING, "SWING", 0}, {P_AHOLD, "LATCH", GN(N_GONOFF)}, {GL_ARPKSYNC, "KEY SYNC", 0}}},
    {B_SEQ, "STEPS", "STEPS",
     {{GL_ARPLAST, "LAST STEP", 0}, {GV_STEP, "STEP", 0}, {GV_STEPON, "ON / OFF", GN(N_GONOFF)}, {GV_NONE, 0, 0}}},
    {B_REC, "PATTERN", "PATTERN",
     {{GV_SSTEP, "STEP", 0}, {GV_SNOTE, "NOTE", 0}, {GV_STYPE, "TYPE", GN(N_GSTYPE)}, {GV_SFLAG, "ACC / SLIDE", GN(N_GSFLAG)}}},
    {B_REC, "SETUP", "SETUP",
     {{GV_SLEN, "LENGTH", 0}, {P_SDIV, "RES", 0}, {P_SGATE, "GATE", 0}, {P_SSWING, "SWING", 0}}},
    {B_FX, "MOD FX", "MOD FX", {{GL_MFX, "TYPE", 0}, {P_IA, "SPEED", 0}, {P_IB, "DEPTH", 0}, {P_IMIX, "MIX", 0}}},
    {B_FX, "DELAY", "DELAY",
     {{GL_DTYPE, "TYPE", 0}, {GL_DTIME, "TIME", 0}, {GL_DFDBK, "FEEDBACK", 0}, {P_DLY, "LEVEL", 0}}},
    {B_FX, "REVERB", "REVERB",
     {{GL_RTYPE, "TYPE", 0}, {GL_RSIZE, "SIZE", 0}, {GL_RDAMP, "DAMP", 0}, {P_REV, "LEVEL", 0}}},
};
#define GU_NPAGE NELEM(GU_PAGES)
/* each page button's own colour: its tabs and its cards' meters, every time its pages are open */
static uint16_t gu_btn_col(uint32_t b)
{
    static const uint8_t IX[NB] = {[B_HOME] = 0, [B_EDIT] = 1, [B_SCL] = 2, [B_ENV] = 3, [B_LFO] = 4, [B_GLO] = 5,
                                   [B_REC] = 6, [B_SEQ] = 7, [B_FX] = 8};
    return GU_P->btn[b < NB ? IX[b] : 8u];
}

static struct {
    uint8_t page, prog, dirty, ready;
    uint8_t save, save_slot, cur;                /* WRITE: open, its slot, the letter KNOB 2 changes */
    uint8_t step;                                /* ARP 3: the step KNOB 2 points at (0..7) */
    uint8_t sstep;                               /* REC STEPS: the sequencer step being edited (0..15) */
    char name[13], sname[13];
    char toast[24];
    uint32_t toast_t, hot_t;
    int8_t hot;                                  /* the card last turned (lit for a moment), -1 none */
    uint32_t sig_head, sig_list, sig_card[4], sig_body, sig_foot;
    uint8_t shown_save, shown_uboot;
} gu;

static track_t *gu_t(void) { return &trk[0]; }

/* --------------------------------------------------------------- programs --- */
/* 1.2: the values a record keeps past its P_COUNT (packed[GU_XTRA_AT + k], the value itself: nothing before read or
 * wrote there, so a program of 1.0 / 1.1 has 0, SUB and DRIFT off). Their slots in the record hold what they held
 * before (the SLICER's defaults), so firmware before loads a 1.2 program as it would have, without them */
static const uint8_t GU_XTRA[3] = {GL_SUBTYPE, GL_SUB, GL_DRIFT};
#define GU_XTRA_AT P_COUNT
_Static_assert(GU_XTRA_AT + NELEM(GU_XTRA) <= UP_CAT_AT, "Golana's extra values: before the category byte");

static void gu_prog_values(uint32_t k, int16_t *v, char *name)
{
    if (up_used(k) && up_rec(k)->engine == ENGI_GOLANA) {
        const up_rec_t *r = up_rec(k);
        uint32_t i;
        up_values(r, v);
        for (i = 0; i < NELEM(GU_XTRA); i++)
            v[GU_XTRA[i]] = (int16_t)param_fit(param_desc_of(ENGI_GOLANA, GU_XTRA[i]),
                                               r->ver >= 4u && r->np <= GU_XTRA_AT ? r->packed[GU_XTRA_AT + i] : 0);
        up_name(k, name);
    } else {
        gl_factory(k < GL_NFACT ? k : 0xFFu, v, name);
    }
}
static int gu_user(uint32_t k) { return up_used(k) && up_rec(k)->engine == ENGI_GOLANA; }

static void gu_load(uint32_t k)
{
    int16_t v[P_COUNT];
    char nm[16];
    k %= GL_NPROG;
    gu_prog_values(k, v, nm);
    panic_req |= 1u;                              /* what part 1 sounds ends (the voice mode may change) */
    fm1_irq_off();
    gl_apply(gu_t(), v);
    if (gu_user(k)) {                                 /* its sequencer pattern (16 steps), its LENGTH */
        load_pat16(gu_t(), up_rec(k)->note, up_rec(k)->flags);
    } else {
        static const uint8_t NONE[16];
        const uint8_t *nt = NONE, *fl = NONE;
        if (k == 1u) {                                /* ACID LINE: Felucca's ACID pattern */
            nt = PATTERNS[0].note;
            fl = PATTERNS[0].flags;
        }
        load_pat16(gu_t(), nt, fl);
    }
    gu_t()->p[P_SLEN] = (int16_t)clamp(v[P_SLEN], 1, 16);
    fm1_irq_on();
    gu.prog = (uint8_t)k;
    str_cpy(gu.name, nm, 13);
    gu.dirty = 0;
}

/* WRITE: part 1's sound -> slot k, named name; 0 ok */
static int gu_store(uint32_t k, const char *name)
{
    up_rec_t r;
    uint32_t i;
    int rc;
    memset(&r, 0, sizeof r);
    r.used = UP_USED;
    r.ver = UP_VER;
    r.engine = ENGI_GOLANA;
    r.np = P_COUNT;
    up_set_name(&r, k, name);
    for (i = 0; i < P_COUNT; i++)
        up_set_value(&r, i, (int16_t)clamp(gu_t()->p[i], -64, 191));
    for (i = 0; i < NELEM(GU_XTRA); i++) {            /* (1.2: SUB, DRIFT) */
        r.packed[GU_XTRA_AT + i] = (uint8_t)clamp(gu_t()->p[GU_XTRA[i]], 0, 127);
        up_set_value(&r, GU_XTRA[i], TP[GU_XTRA[i]].def);
    }
    up_pat_from(&r, gu_t()->step);                    /* the sequencer's 16 steps with the sound */
    rc = up_put(k, &r);
    return rc == 0 || rc == 3 ? 0 : rc;
}

/* the palette and HOME's drawing, with the settings (flash): in two of Felucca's settings this interface does not
 * use, HOLD (the layers' hold time) and LEDS (the idle glow) */
static void gu_prefs_load(void)
{
    gu_pi = (uint8_t)((settings_hold + 3u) & 3u);       /* (HOLD's default, 1: NIGHT) */
    /* LEDS 0 (its default): TRAIL, 1: RIDGE (1.2's RIDGE was LEDS 0, its RING LEDS 1), 2: ORBIT */
    gu_view = (uint8_t)(settings_leds == 2u ? 1u : settings_leds == 1u ? 2u : 0u);
}
static void gu_prefs_save(void)
{
    settings_hold = (uint8_t)((gu_pi + 1u) & 3u);
    settings_leds = (uint8_t)(gu_view == 1u ? 2u : gu_view == 2u ? 1u : 0u);
    settings_save();                                     /* (written when the sequencer is stopped: settings_poll) */
}

/* ------------------------------------------------------------- parameters --- */
static const param_desc_t *gu_desc(uint32_t id) { return param_desc_of(ENGI_GOLANA, id); }

/* the step sequencer (seq.c plays part 1's steps): 16 steps, saved with the program (upreset.c's pattern) */
static uint32_t gu_slen(void) { return (uint32_t)clamp(gu_t()->p[P_SLEN], 1, 16); }
static step_t *gu_sst(void) { return &gu_t()->step[gu.sstep % 16u]; }
static int32_t gu_stype(const step_t *st) { return st->time == ST_TIE ? 2 : st->time == ST_NOTE && st->n ? 1 : 0; }
static void gu_snote_set(step_t *st, uint32_t note)   /* the step a NOTE of note (velocity 96, as a pattern's) */
{
    st->note[0] = (uint8_t)note;
    st->n = 1;
    st->vel = 96;
    st->time = ST_NOTE;
}
static void gu_range(const gu_par_t *q, int32_t *lo, int32_t *hi)
{
    switch (q->id) {
    case GV_RES: *lo = 0; *hi = 5; return;
    case GV_BPM: *lo = GP[G_BPM].min; *hi = GP[G_BPM].max; return;
    case GV_TUNE: *lo = GP[G_TUNE].min; *hi = GP[G_TUNE].max; return;
    case GV_CLOCK: *lo = 0; *hi = 2; return;
    case GV_STEP: *lo = 1; *hi = 8; return;
    case GV_STEPON: *lo = 0; *hi = 1; return;
    case GV_MIDI: *lo = 0; *hi = 1; return;
    case GV_PAL: *lo = 0; *hi = 3; return;
    case GV_VIEW: *lo = 0; *hi = 2; return;
    case GV_SSTEP: *lo = 1; *hi = gu_slen(); return;
    case GV_SNOTE: *lo = 0; *hi = 127; return;
    case GV_STYPE: *lo = 0; *hi = 2; return;
    case GV_SFLAG: *lo = 0; *hi = 3; return;
    case GV_SLEN: *lo = 1; *hi = 16; return;
    default:
        if (q->names) {
            *lo = 0;
            *hi = (int32_t)q->nn - 1;
            if (q->id < P_COUNT && gu_desc(q->id)->max < *hi)
                *hi = gu_desc(q->id)->max;
            return;
        }
        *lo = gu_desc(q->id)->min;
        *hi = gu_desc(q->id)->max;
    }
}

static uint32_t gu_steps(void)
{
    const int16_t *p = gu_t()->p;
    return (uint32_t)(p[GL_ARPST_A] & 15) | (uint32_t)(p[GL_ARPST_B] & 15) << 4;
}

static int32_t gu_get(const gu_par_t *q)
{
    const int16_t *p = gu_t()->p;
    uint32_t i;
    switch (q->id) {
    case GV_RES:
        for (i = 0; i < 6u && GU_RES_DIV[i] != p[P_ARATE]; i++)
            ;
        return i < 6u ? (int32_t)i : 2;
    case GV_BPM: return song.g[G_BPM];
    case GV_TUNE: return song.g[G_TUNE];
    case GV_CLOCK: return clamp(song.g[G_CLOCK], 0, 2);
    case GV_STEP: return gu.step + 1;
    case GV_STEPON: return (int32_t)((gu_steps() >> gu.step) & 1u);
    case GV_MIDI: return song.g[G_ROUTE] == 1 ? 0 : 1;
    case GV_PAL: return gu_pi;
    case GV_VIEW: return gu_view;
    case GV_SSTEP: return gu.sstep + 1;
    case GV_SNOTE: return gu_sst()->n ? gu_sst()->note[0] : 60;
    case GV_STYPE: return gu_stype(gu_sst());
    case GV_SFLAG: return gu_sst()->flags & 3;
    case GV_SLEN: return (int32_t)gu_slen();
    case GV_NONE: return 0;
    default: return p[q->id];
    }
}

static void gu_set(const gu_par_t *q, int32_t v)
{
    track_t *t = gu_t();
    int32_t lo, hi;
    gu_range(q, &lo, &hi);
    v = clamp(v, lo, hi);
    switch (q->id) {
    case GV_RES: t->p[P_ARATE] = GU_RES_DIV[v]; break;
    case GV_BPM: song.g[G_BPM] = (int16_t)v; return;
    case GV_TUNE: song.g[G_TUNE] = (int16_t)v; return;
    case GV_CLOCK: song.g[G_CLOCK] = (int16_t)v; return;
    case GV_STEP: gu.step = (uint8_t)(v - 1); return;
    case GV_MIDI: song.g[G_ROUTE] = (int16_t)(v ? 0 : 1); return;
    case GV_PAL:
    case GV_VIEW:
        if (q->id == GV_PAL)
            gu_pi = (uint8_t)v;
        else
            gu_view = (uint8_t)v;
        gu_prefs_save();
        ui.force = 1;                                   /* (everything drawn again in the new colours) */
        return;
    case GV_SSTEP: gu.sstep = (uint8_t)(v - 1); return;
    case GV_SNOTE: gu_snote_set(gu_sst(), (uint32_t)v); gu.dirty = 1; return;
    case GV_STYPE: {
        step_t *st = gu_sst();
        if (v == 1)
            gu_snote_set(st, st->n ? st->note[0] : 60);
        else {
            st->time = v == 2 ? ST_TIE : ST_REST;
            st->n = 0;
            st->flags &= (uint8_t)~3u;
        }
        gu.dirty = 1;
        return;
    }
    case GV_SFLAG: {
        step_t *st = gu_sst();
        if (gu_stype(st) == 1)
            st->flags = (uint8_t)((st->flags & ~3u) | (uint32_t)v);
        gu.dirty = 1;
        return;
    }
    case GV_SLEN: t->p[P_SLEN] = (int16_t)v; gu.sstep = (uint8_t)(gu.sstep % (uint32_t)v); gu.dirty = 1; return;
    case GV_STEPON: {
        uint32_t m = (gu_steps() & ~(1u << gu.step)) | (uint32_t)(v != 0) << gu.step;
        t->p[GL_ARPST_A] = (int16_t)(m & 15u);
        t->p[GL_ARPST_B] = (int16_t)(m >> 4);
        break;
    }
    case GV_NONE: return;
    default:
        if (q->id == P_VOICE && t->p[P_VOICE] != v)
            panic_req |= 1u;                        /* the voices restart in the new mode */
        t->p[q->id] = (int16_t)v;
        break;
    }
    gl_sync(t);
    gu.dirty = 1;
}

/* the value as shown: a name, or the number (signed when the range is) */
static void gu_text(const gu_par_t *q, char *b)
{
    int32_t v = gu_get(q), lo, hi;
    const char *const *names = q->names;
    if (q->id == GV_NONE) {
        b[0] = 0;
        return;
    }
    if (q->id == GV_PAL || q->id == GV_VIEW) {
        str_cpy(b, q->id == GV_PAL ? N_GPAL[v & 3] : N_GVIEW[v % 3], 8);
        return;
    }
    if (q->id == GV_SNOTE) {                           /* a note name (C4 = 60), "--" on a rest or a tie */
        static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        if (gu_stype(gu_sst()) != 1) {
            str_cpy(b, "--", 4);
            return;
        }
        str_cpy(b, NN[v % 12], 4);
        fmt_int(b + str_len(b), v / 12 - 1);
        return;
    }
    if (q->id == GL_O1C1 && gu_t()->p[GL_O1WAVE] == 1) {   /* PULSE's WIDTH: its duty, 50 % .. ~4 % (eng_golana.c pw) */
        fmt_int(b, 50 - (v * 366 + 500) / 1000);
        str_cpy(b + str_len(b), "%", 2);
        return;
    }
    if (!names && q->id < P_COUNT && gu_desc(q->id)->fmt == F_ENUM)
        names = gu_desc(q->id)->names;
    gu_range(q, &lo, &hi);
    if (names && v >= 0 && v <= hi) {
        str_cpy(b, names[v], 12);
        return;
    }
    if (lo < 0 && v > 0) {
        b[0] = '+';
        fmt_int(b + 1, v);
    } else {
        fmt_int(b, v);
    }
}

/* -------------------------------------------------------------- drawing --- */
static uint32_t gu_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h * 16777619u;
}
static uint32_t gu_hashv(uint32_t h, int32_t v) { return (h ^ (uint32_t)v) * 16777619u; }

static int gu_fits_l(const char *s)               /* every character in AF_L (gen_aa_font.py L_CHARS) */
{
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'Y') || *s == '+' || *s == '-' || *s == '.' ||
              *s == '/' || *s == '#' || *s == ' '))
            return 0;
    return 1;
}

static int gu_toast_on(void) { return gu.toast[0] && fm1_ms - gu.toast_t < 1400u; }
static void gu_say(const char *a, const char *b)
{
    str_cpy(gu.toast, a, sizeof gu.toast);
    if (b)
        str_cpy(gu.toast + str_len(gu.toast), b, sizeof gu.toast - str_len(gu.toast));
    gu.toast_t = fm1_ms;
}

static void gu_chip(int32_t xr, int32_t y, const char *s, uint16_t bg, uint16_t fg, int32_t *xo)
{
    int32_t w = text_w(&AF_S, s) + 10;
    cv_rrect(xr - w, y, w, 17, -5, bg, GC_BG);
    cv_text_on(xr - w + 5, y + 1, &AF_S, s, fg, bg);
    *xo = xr - w - 4;
}

static void gu_draw_head(void)
{
    const track_t *t = gu_t();
    char b[32], n[4];
    uint32_t h = 0x811C9DC5u;
    int32_t x;
    int toast = gu_toast_on();
    n[0] = 'P';
    n[1] = (char)('0' + (gu.prog + 1u) / 10u);
    n[2] = (char)('0' + (gu.prog + 1u) % 10u);
    n[3] = 0;
    h = gu_hash(h, toast ? gu.toast : gu.name);
    h = gu_hashv(h, (int32_t)(gu.prog | gu.dirty << 8 | toast << 9 | (t->p[GL_ARPON] != 0) << 10 |
                              (uint32_t)(song.octave + 8) << 12 | (uint32_t)(song.playing != 0) << 16 |
                              (uint32_t)batt_shown() << 20));
    if (h == gu.sig_head)
        return;
    gu.sig_head = h;
    cv_begin(240, 32, GC_BG);
    {   /* the battery (or the bolt on USB power), right */
        int32_t lvl = batt_shown();
        cv_icon_mid(240 - 30, 16, 24, batt_icon(lvl), lvl == 1 ? GC_PINK : lvl == 0 ? GC_MID : GC_TEXT, GC_BG);
    }
    x = 240 - 44;
    if (song.octave) {
        b[0] = 'O';
        b[1] = 'C';
        b[2] = 'T';
        b[3] = song.octave > 0 ? '+' : '-';
        b[4] = (char)('0' + (song.octave > 0 ? song.octave : -song.octave));
        b[5] = 0;
        gu_chip(x, 8, b, GC_RAISE, GC_TEXT, &x);
    }
    if (song.playing)
        gu_chip(x, 8, "PLAY", gu_btn_col(B_REC), GC_INK, &x);
    if (toast) {
        cv_text(8, 7, &AF_M, gu.toast, GC_AMBER);
    } else {
        int32_t xn = cv_text(8, 7, &AF_M, n, GC_AMBER) + 8;
        xn = cv_text(xn, 7, &AF_M, gu.name, GC_TEXT);
        if (gu.dirty)
            cv_rrect(xn + 4, 13, 6, 6, -3, GC_PINK, GC_BG);
    }
    cv_rect(0, 31, 240, 1, GC_LINE);
    cv_blit(0, 0);
}

/* the service strip at the bottom: the arpeggiator (a light, ARP, LATCH when held) left, the tempo right */
#define GU_FOOT_Y 224
#define GU_FOOT_H 16
static void gu_draw_foot(void)
{
    const track_t *t = gu_t();
    int on = t->p[GL_ARPON] != 0, ext = song.g[G_CLOCK] != 0;
    uint32_t h = gu_hashv(0x811C9DC5u, (int32_t)(on | (t->p[P_AHOLD] != 0) << 1 | ext << 2 |
                                                   (uint32_t)song.g[G_BPM] << 4));
    char b[16];
    int32_t x;
    if (h == gu.sig_foot)
        return;
    gu.sig_foot = h;
    cv_begin(240, GU_FOOT_H, GC_BG);
    cv_rect(0, 0, 240, 1, GC_LINE);
    cv_rrect(8, 5, 8, 8, -4, on ? GC_AMBER : GC_RAISE, GC_BG);
    x = cv_text(21, 1, &AF_S, "ARP", on ? GC_TEXT : GC_DIM);
    if (t->p[P_AHOLD])
        cv_text(x + 6, 1, &AF_S, "LATCH", GC_TEAL);
    fmt_int(b, song.g[G_BPM]);
    str_cpy(b + str_len(b), ext ? " BPM EXT" : " BPM", sizeof b - str_len(b));
    cv_text(240 - 8 - text_w(&AF_S, b), 1, &AF_S, b, GC_TEXT);
    cv_blit(0, GU_FOOT_Y);
}

/* the tabs, as on a card index: the pages of the button whose page is open. The open one in the button's colour,
 * "BUTTON · PAGE", joined to the line under the tabs; the button's other pages beside it, dark, by short names */
/* card k of page pg as it works now: MOD FX's METAL turns SPEED DEPTH MIX into GAIN MID MID FREQ; OSC 1's CTRL 1 and
 * CTRL 2 are named for what they do with its WAVE (CTRL 2: LFO 1 on CTRL 1, the page's fourth card LFO 1's rate) */
static const gu_par_t *gu_pk(const gu_page_t *pg, uint32_t k)
{
    static const gu_par_t METAL[3] = {{P_IA, "GAIN", 0}, {P_IB, "MID", 0}, {P_IC, "MID FREQ", 0}};
    static const gu_par_t CTRL[5][2] = {
        {{GL_O1C1, "OCTAVE", 0}, {GL_O1C2, "OCT LFO", 0}},      /* SAW: the octave-up saw blended in */
        {{GL_O1C1, "WIDTH", 0}, {GL_O1C2, "PWM", 0}},          /* PULSE: 50 % .. ~6 % */
        {{GL_O1C1, "FOLD", 0}, {GL_O1C2, "FOLD LFO", 0}},      /* TRI: folded */
        {{GL_O1C1, "X-MOD", 0}, {GL_O1C2, "XMOD LFO", 0}},     /* SINE: OSC 2 moves its phase */
        {{GL_O1C1, "MORPH", 0}, {GL_O1C2, "MRPH LFO", 0}},     /* (1.3) MORPH: the saw to the square */
    };
    if (pg->k[0].id == GL_MFX && k && gu_t()->p[GL_MFX] == 5)
        return &METAL[k - 1u];
    if (pg->k[0].id == GL_O1WAVE && (k == 1u || k == 2u))
        return &CTRL[clamp(gu_t()->p[GL_O1WAVE], 0, 4)][k - 1u];
    return &pg->k[k];
}
#define GU_TAB_Y 32
#define GU_TAB_H 26
static void gu_draw_tabs(void)
{
    uint32_t btn = GU_PAGES[gu.page].btn, pi, h = 0x811C9DC5u;
    uint16_t col = gu_btn_col(btn);
    int32_t x = 3, ax = 3, aw = 0;
    char lab[32];
    h = gu_hashv(h, (int32_t)gu.page);
    if (h == gu.sig_list)
        return;
    gu.sig_list = h;
    cv_begin(240, GU_TAB_H, GC_BG);
    str_cpy(lab, btn < NB ? B_NAME[btn] : "", sizeof lab);
    str_cpy(lab + str_len(lab), " \xB7 ", sizeof lab - str_len(lab));
    str_cpy(lab + str_len(lab), GU_PAGES[gu.page].tab, sizeof lab - str_len(lab));
    for (pi = 0; pi < GU_NPAGE; pi++) {
        int32_t w;
        if (GU_PAGES[pi].btn != btn)
            continue;
        if (pi == gu.page) {
            ax = x;
            aw = text_w(&AF_S, lab) + 14;
            x += aw + 2;
            continue;
        }
        w = text_w(&AF_S, GU_PAGES[pi].tshort) + 10;
        cv_rrect(x, 6, w, GU_TAB_H, -6, GC_RAISE, GC_BG);
        cv_text_on(x + 5, 8, &AF_S, GU_PAGES[pi].tshort, GC_MID, GC_RAISE);
        x += w + 2;
    }
    cv_rect(0, GU_TAB_H - 2, 240, 2, col);
    cv_rrect(ax, 1, aw, GU_TAB_H + 6, -6, col, GC_BG);
    cv_text_on(ax + 7, 5, &AF_S, lab, GC_INK, col);
    cv_blit(0, GU_TAB_Y);
}

/* card k of the page: the label, the value (big when it fits), a meter (a bar, a centred bar, or a dot per name) */
#include "golana_gfx.c"                         /* the pages' drawings */
/* the cards: two by two (114 x 86), or on a page with a drawing four in a row under it (57 wide) */
static void gu_draw_card(uint32_t k)
{
    const gu_page_t *pg = &GU_PAGES[gu.page];
    const gu_par_t *q = gu_pk(pg, k);
    char b[16];
    int32_t gh = gu_graph_h(gu_graph_of(pg)), GU_CW = gh ? 57 : 114, GU_CH = gh ? GU_FOOT_Y - 2 - 60 - gh - 4 : 78;
    int32_t v = gu_get(q), lo = 0, hi = 0, x = gh ? 3 + (int32_t)k * 59 : 4 + (int32_t)(k & 1u) * (GU_CW + 4);
    int32_t y = gh ? 60 + gh + 4 : GU_TAB_Y + GU_TAB_H + 4 + (int32_t)(k >> 1) * (GU_CH + 4), dy = GU_CH - 86;
    uint32_t h = 0x811C9DC5u;
    int hot = gu.hot == (int8_t)k && fm1_ms - gu.hot_t < 700u;
    uint16_t acc = gu_btn_col(pg->btn), surf = hot ? GC_RAISE : GC_SURF;
    gu_text(q, b);
    h = gu_hash(h, b);
    h = gu_hash(h, q->label ? q->label : "");
    h = gu_hashv(h, v * 3 + hot);
    h = gu_hashv(h, (int32_t)(gu.page | k << 8));
    if (q->id == GV_STEP || q->id == GV_STEPON || q->id == GL_ARPLAST)
        h = gu_hashv(h, (int32_t)(gu_steps() | (uint32_t)gu_t()->p[GL_ARPLAST] << 8 | (uint32_t)gu.step << 12));
    if (q->id == GV_SSTEP || q->id == GV_SLEN) {        /* the 16 steps, the one edited, the one playing */
        uint32_t i;
        for (i = 0; i < 16u; i++)
            h = gu_hashv(h, gu_stype(&gu_t()->step[i]) | (gu_t()->step[i].flags & 3) << 2);
        h = gu_hashv(h, (int32_t)(gu.sstep | gu_slen() << 8 | (song.playing ? gu_t()->seq_idx % gu_slen() : 31u) << 16));
    }
    if (h == gu.sig_card[k])
        return;
    gu.sig_card[k] = h;
    cv_begin(GU_CW, GU_CH, GC_BG);
    if (q->id == GV_NONE) {
        cv_rrect(0, 0, GU_CW, GU_CH, -8, ux_mix(GC_BG, GC_SURF, 50), GC_BG);
        cv_blit((uint32_t)x, (uint32_t)y);
        return;
    }
    cv_rrect(0, 0, GU_CW, GU_CH, -8, surf, GC_BG);
    if (hot)
        cv_rect(10, 0, GU_CW - 20, 2, acc);
    if (gh)                                             /* (narrow: the label centred) */
        cv_text_on((GU_CW - text_w(&AF_S, q->label)) / 2, 6, &AF_S, q->label, GC_MID, surf);
    else
        cv_text_on(10, 6, &AF_S, q->label, GC_MID, surf);
    gu_range(q, &lo, &hi);
    {
        int big = gu_fits_l(b) && text_w(&AF_L, b) <= GU_CW - (gh ? 6 : 12);
        const aafont_t *f = big ? &AF_L : &AF_M;
        int32_t w = text_w(f, b);
        if (w > GU_CW - (gh ? 4 : 8)) {
            f = &AF_S;
            w = text_w(f, b);
        }
        cv_text_on((GU_CW - w) / 2, (big ? 22 : f == &AF_M ? 32 : 36) + dy / 2, f, b, GC_TEXT, surf);
    }
    if (q->id == GV_SSTEP || q->id == GV_SLEN) {        /* the sequencer's 16 steps */
        uint32_t st, len = gu_slen(), play = song.playing ? gu_t()->seq_idx % len : 99u;
        for (st = 0; st < 16u; st++) {
            int32_t sx = 7 + (int32_t)st * 6 + (int32_t)(st / 4u) * 2;
            int32_t ty = gu_stype(&gu_t()->step[st]);
            uint16_t c = st >= len ? GC_BG : ty == 1 ? acc : ty == 2 ? ux_mix(acc, surf, 55) : GC_LINE;
            cv_rect(sx, 64, 5, 9, c);
            if (st == play)
                cv_rect(sx, 60, 5, 2, GC_TEXT);
            if (st == gu.sstep && q->id == GV_SSTEP)
                cv_rect(sx, 76, 5, 2, GC_TEXT);
        }
    } else if (q->id == GV_STEP || q->id == GV_STEPON || q->id == GL_ARPLAST) {   /* the 8 steps: on, off, past LAST */
        uint32_t st, m = gu_steps(), last = (uint32_t)clamp(gu_t()->p[GL_ARPLAST], 1, 8);
        for (st = 0; st < 8u; st++) {
            int32_t sx = 13 + (int32_t)st * 11;
            uint16_t c = st >= last ? GC_LINE : (m >> st) & 1u ? acc : GC_DIM;
            cv_rect(sx, 64, 8, 9, c);
            if (st == gu.step && q->id != GL_ARPLAST)
                cv_rect(sx, 76, 8, 2, GC_TEXT);
        }
    } else if (q->names || (q->id < P_COUNT && gu_desc(q->id)->fmt == F_ENUM) || hi - lo < 2) {
        int32_t n = hi - lo + 1, i, dx = n > 1 ? (GU_CW - 14) / (n - 1) : 0, ds = dx < 8 ? 4 : 6;
        if (dx > 14)
            dx = 14;
        for (i = 0; i < n && n <= 12; i++) {
            int32_t cx = GU_CW / 2 - (dx * (n - 1)) / 2 + i * dx;
            cv_rrect(cx - ds / 2, 68 + dy, ds, ds, -3, i == v - lo ? acc : GC_LINE, surf);
        }
    } else if (lo < 0) {                             /* bipolar: from the middle */
        int32_t m0 = gh ? 7 : 12, mid = GU_CW / 2, len = (v * (GU_CW / 2 - m0)) / (hi > 0 ? hi : 1);
        cv_rect(m0, 70 + dy, GU_CW - 2 * m0, 3, GC_LINE);
        cv_rect(mid - 1, 67 + dy, 2, 9, GC_MID);
        if (len > 0)
            cv_rect(mid, 70 + dy, len, 3, acc);
        else if (len < 0)
            cv_rect(mid + len, 70 + dy, -len, 3, acc);
    } else {
        int32_t m0 = gh ? 7 : 12, len = ((v - lo) * (GU_CW - 2 * m0)) / (hi - lo > 0 ? hi - lo : 1);
        cv_rect(m0, 70 + dy, GU_CW - 2 * m0, 3, GC_LINE);
        cv_rect(m0, 70 + dy, len, 3, acc);
    }
    cv_blit((uint32_t)x, (uint32_t)y);
}

/* WRITE: the slot, its name with the letter being changed, what each control does */
static void gu_draw_save(void)
{
    uint32_t h = gu_hash(0x811C9DC5u, gu.sname), i;
    char n[8];
    h = gu_hashv(h, (int32_t)(gu.save_slot | gu.cur << 8 | gu_user(gu.save_slot) << 16));
    if (h == gu.sig_body && gu.shown_save)
        return;
    gu.sig_body = h;
    gu.shown_save = 1;
    gu.sig_list = gu.sig_card[0] = gu.sig_card[1] = gu.sig_card[2] = gu.sig_card[3] = 0;
    cv_begin(240, 104, GC_BG);
    cv_text(12, 6, &AF_M, "WRITE PROGRAM", GC_AMBER);
    n[0] = 'P';
    n[1] = (char)('0' + (gu.save_slot + 1u) / 10u);
    n[2] = (char)('0' + (gu.save_slot + 1u) % 10u);
    n[3] = 0;
    cv_text(12, 34, &AF_L, n, GC_TEXT);
    cv_text(90, 46, &AF_S, gu_user(gu.save_slot) ? "OVERWRITES ITS SOUND" :
                           gu.save_slot < GL_NFACT ? "OVER A FACTORY SOUND" : "EMPTY SLOT", GC_MID);
    {   /* the name, one box per letter, the one KNOB 2 changes lit */
        int32_t x = 12;
        for (i = 0; i < 12u; i++) {
            char c[2] = {gu.sname[i] ? gu.sname[i] : ' ', 0};
            uint16_t bg = i == gu.cur ? GC_AMBER : GC_SURF;
            cv_rrect(x, 74, 17, 24, -4, bg, GC_BG);
            if (c[0] != ' ')
                cv_text_on(x + (17 - text_w(&AF_M, c)) / 2, 77, &AF_M, c, i == gu.cur ? GC_INK : GC_TEXT, bg);
            x += 18;
        }
    }
    cv_blit(0, 32);
    cv_begin(240, 104, GC_BG);
    cv_text(12, 8, &AF_S, "PRESETS   the slot", GC_MID);
    cv_text(12, 28, &AF_S, "KNOB 1   the letter", GC_MID);
    cv_text(12, 48, &AF_S, "KNOB 2   change it", GC_MID);
    cv_text(12, 68, &AF_S, "SAVE   write", GC_TEXT);
    cv_text(130, 68, &AF_S, "HOME   cancel", GC_TEXT);
    cv_blit(0, 136);
}

static void gu_draw_uboot(void)
{
    char d[2] = {(char)('0' + ui.uboot % 10u), 0};
    if (gu.shown_uboot == ui.uboot)
        return;
    gu.shown_uboot = ui.uboot;
    cv_begin(240, 104, GC_BG);
    cv_text((240 - text_w(&AF_M, "UPDATE MODE IN")) / 2, 20, &AF_M, "UPDATE MODE IN", GC_TEXT);
    cv_text((240 - text_w(&AF_L, d)) / 2, 48, &AF_L, d, GC_AMBER);
    cv_blit(0, 32);
    cv_begin(240, 104, GC_BG);
    cv_text((240 - text_w(&AF_S, "LET GO OF OCT- OCT+ TO CANCEL")) / 2, 10, &AF_S, "LET GO OF OCT- OCT+ TO CANCEL",
            GC_MID);
    cv_blit(0, 136);
}

static void gu_redraw_all(void)
{
    uint32_t k;
    gu.sig_head = gu.sig_list = gu.sig_body = gu.sig_foot = 0;
    for (k = 0; k < 4u; k++)
        gu.sig_card[k] = 0;
    gu.shown_save = 0;
    gu.shown_uboot = 0xFF;
    gu_sig_graph = 0;
    lcd_fill(0, 0, 240, 240, GC_BG);
}

static void gu_draw(void)
{
    uint32_t k;
    if (!gu.ready)
        return;
    if (ux.mono || ux.light != GU_P->light || ux.style != ST_FLAT) {   /* Golana's own colours (a stored Felucca */
        ux.mono = 0;                                     /* palette, GREY or PAPER, would grey or invert them) */
        ux.light = GU_P->light;
        ux.style = ST_FLAT;
        ux.gen++;
        ui.force = 1;
    }
    if (ui.force) {                                  /* (main.c: after the splash, an update, a cancelled UBOOT) */
        ui.force = 0;
        gu_redraw_all();
    }
    if (ui.uboot) {
        gu_draw_uboot();
        gu.sig_list = gu.sig_card[0] = gu.sig_card[1] = gu.sig_card[2] = gu.sig_card[3] = gu.sig_foot = 0;
        gu.shown_save = 0;
        return;
    }
    if (gu.shown_uboot != 0xFF) {
        gu.shown_uboot = 0xFF;
        gu_redraw_all();
    }
    gu_draw_head();
    if (gu.save) {
        gu_draw_save();
        return;
    }
    if (gu.shown_save) {
        gu.shown_save = 0;
        lcd_fill(0, 32, 240, 208, GC_BG);
        gu.sig_foot = 0;
    }
    {   /* a page of another layout (with a drawing or not, or of another height): the body cleared */
        static int32_t shown_gh = -1;
        int32_t gh = gu_graph_h(gu_graph_of(&GU_PAGES[gu.page]));
        if (gh != shown_gh) {
            shown_gh = gh;
            lcd_fill(0, 58, 240, GU_FOOT_Y - 58, GC_BG);
            gu.sig_card[0] = gu.sig_card[1] = gu.sig_card[2] = gu.sig_card[3] = 0;
            gu_sig_graph = 0;
        }
        if (gh) {
            if (gu.sig_list == 0)
                gu_sig_graph = 0;
            gu_draw_graph(gu_graph_of(&GU_PAGES[gu.page]));
        }
    }
    gu_draw_tabs();
    for (k = 0; k < 4u; k++)
        gu_draw_card(k);
    gu_draw_foot();
}

/* ---------------------------------------------------------------- LEDs --- */
static uint32_t gu_group_btn(void) { return GU_PAGES[gu.page].btn; }

static void gu_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    static uint8_t ready;
    if (fm1_led_anim_on())
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    for (k = 0; k < 27u; k++)
        led_put(nd, 14u + k, 1);
    led_put(nl, panel.btn[gu.save ? B_SAVE : gu_group_btn()], 1);
    led_put(nl, panel.btn[B_ARP], gu_t()->p[GL_ARPON] != 0);
    if (song.playing)                                   /* PLAY: its green LED while the sequencer plays */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    led_put(nl, panel.btn[B_OCTDN], song.octave < 0);
    led_put(nl, panel.btn[B_OCTUP], song.octave > 0);
    for (k = 0; k < 27u; k++)
        led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
    if (GU_PAGES[gu.page].k[1].id == GV_STEP) {           /* SEQ STEPS: the first 8 keys show the steps */
        uint32_t m = gu_steps(), last = (uint32_t)clamp(gu_t()->p[GL_ARPLAST], 1, 8);
        for (k = 0; k < 8u; k++)
            led_put(nl, 14u + k, k < last && ((m >> k) & 1u));
    }
    fm1_led_dim_level(0);
    for (c = 0; c < FM1_NCOL; c++) {
        nd[c] &= (uint8_t)~nl[c];
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led_mid[c] = 0;
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* --------------------------------------------------------------- input --- */
static void gu_go(uint32_t page)
{
    gu.page = (uint8_t)(page % GU_NPAGE);
}

/* a page button: its first page, or (on one of its pages) the next of them, round */
static void gu_button(uint32_t b)
{
    uint32_t pi, first = GU_NPAGE;
    for (pi = 0; pi < GU_NPAGE; pi++)
        if (GU_PAGES[pi].btn == b) {
            first = pi;
            break;
        }
    if (first == GU_NPAGE)
        return;
    if (GU_PAGES[gu.page].btn == b) {
        pi = gu.page + 1u;
        gu_go(pi < GU_NPAGE && GU_PAGES[pi].btn == b ? pi : first);
    } else {
        gu_go(first);
    }
}

static int32_t gu_accel(int32_t s, int32_t range)
{
    int32_t a = s < 0 ? -s : s;
    if (range > 32 && a >= 2)
        s *= a >= 4 ? 4 : 2;
    return s;
}

static void gu_turn(uint32_t k, int32_t s)
{
    const gu_par_t *q = gu_pk(&GU_PAGES[gu.page], k);
    int32_t lo, hi;
    if (q->id == GV_NONE || !s)
        return;
    gu_range(q, &lo, &hi);
    gu_set(q, gu_get(q) + gu_accel(s, hi - lo));
    gu.hot = (int8_t)k;
    gu.hot_t = fm1_ms;
}

static void gu_save_open(void)
{
    gu.save = 1;
    gu.save_slot = gu.prog;
    gu.cur = 0;
    memset(gu.sname, 0, sizeof gu.sname);
    str_cpy(gu.sname, gu.name, 13);
}

static const char GU_ABC[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-./#+";
static void gu_letter(int32_t s)
{
    uint32_t n = sizeof GU_ABC - 1u, i, len;
    char c = gu.sname[gu.cur] ? gu.sname[gu.cur] : ' ';
    for (i = 0; i < n && GU_ABC[i] != c; i++)
        ;
    i = (uint32_t)(((int32_t)(i % n) + s % (int32_t)n + (int32_t)n) % (int32_t)n);
    for (len = str_len(gu.sname); len < gu.cur; len++)
        gu.sname[len] = ' ';                         /* (letters before the cursor: spaces) */
    gu.sname[gu.cur] = GU_ABC[i];
    for (len = 12u; len > 0u && (gu.sname[len - 1u] == ' ' || !gu.sname[len - 1u]); len--)
        gu.sname[len - 1u] = 0;                      /* (no trailing spaces) */
}

static void gu_input(void)
{
    uint32_t pressed = fm1_input_edges(0), id, b, k, notes;
    int32_t s;
    notes = fm1_input_note_edges();                  /* (the keys sound from the audio ISR: seq.c) */
    if (!gu.ready)
        return;
    for (k = 0; k < NTRK; k++)                       /* MIDI Program Change: a program */
        if (ui.mpc[k]) {
            uint32_t v;
            fm1_irq_off();
            v = ui.mpc[k];
            ui.mpc[k] = 0;
            fm1_irq_on();
            gu_load((v & 127u) % GL_NPROG);
        }
    if (ui.uboot)
        return;
    if (notes && !gu.save && GU_PAGES[gu.page].k[0].id == GV_SSTEP) {   /* REC PATTERN: a key writes its note */
        for (k = 0; k < 27u; k++)                         /* into the step and moves on (step entry) */
            if ((notes >> k) & 1u) {
                gu_snote_set(gu_sst(), kb_map(gu_t(), k));
                gu.sstep = (uint8_t)((gu.sstep + 1u) % gu_slen());
                gu.dirty = 1;
            }
    }
    s = panel_enc(EN_PRESET);
    if (s) {
        if (gu.save) {
            gu.save_slot = (uint8_t)(((int32_t)gu.save_slot + s % (int32_t)GL_NPROG + (int32_t)GL_NPROG) %
                                     (int32_t)GL_NPROG);
        } else {
            gu_load((uint32_t)(((int32_t)gu.prog + s % (int32_t)GL_NPROG + (int32_t)GL_NPROG) % (int32_t)GL_NPROG));
        }
    }
    if (gu.save) {
        s = panel_enc(EN_K1);
        if (s)
            gu.cur = (uint8_t)clamp((int32_t)gu.cur + s, 0, 11);
        s = panel_enc(EN_K2);
        if (s)
            gu_letter(s);
        (void)panel_enc(EN_K3);
        (void)panel_enc(EN_K4);
        (void)panel_enc(EN_SELECT);
        (void)panel_enc(EN_ALGO);
    } else {
        s = panel_enc(EN_SELECT);
        if (s)
            gu_go((uint32_t)(((int32_t)gu.page + s % (int32_t)GU_NPAGE + (int32_t)GU_NPAGE) % (int32_t)GU_NPAGE));
        for (k = 0; k < 4u; k++)
            gu_turn(k, panel_enc(EN_K1 + k));
        s = panel_enc(EN_ALGO);
        if (s) {                                     /* CUTOFF, from any page */
            static const gu_par_t CUT = {GL_CUTOFF, "CUTOFF", 0};
            char v[8];
            gu_set(&CUT, gu_get(&CUT) + gu_accel(s, 127));
            gu_text(&CUT, v);
            if (GU_PAGES[gu.page].k[1].id != GL_CUTOFF && GU_PAGES[gu.page].k[0].id != GL_CUTOFF)
                gu_say("CUTOFF ", v);
        }
    }
    for (id = 0; id < 32u; id++) {
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        if (b >= NB)
            continue;
        if (gu.save && b != B_PLAY) {                /* (PLAY works on WRITE: STOP TO SAVE can be done there) */
            if (b == B_SAVE) {
                int rc;
                if (transport_busy()) {                  /* (no flash erase while playing: up_put would refuse it, its */
                    gu_say("STOP TO SAVE", 0);           /* message is Felucca's UI's) the WRITE screen stays */
                    continue;
                }
                if (!gu.sname[0])
                    str_cpy(gu.sname, "PROGRAM", 13);
                rc = gu_store(gu.save_slot, gu.sname);
                if (!rc) {
                    gu.prog = gu.save_slot;
                    str_cpy(gu.name, gu.sname, 13);
                    gu.dirty = 0;
                    gu_say("WRITTEN", 0);
                } else {
                    gu_say("NOT WRITTEN", 0);            /* (a flash error: the slot holds what it held) */
                }
                gu.save = 0;
            } else if (b == B_HOME) {
                gu.save = 0;
                gu_say("CANCELLED", 0);
            }
            continue;
        }
        switch (b) {
        case B_HOME:
        case B_EDIT:
        case B_SCL:
        case B_ENV:
        case B_LFO:
        case B_GLO:
        case B_REC:
        case B_SEQ:
        case B_FX:
            gu_button(b);
            break;
        case B_ARP: {
            static const gu_par_t A = {GL_ARPON, "ARP", 0};
            gu_set(&A, !gu_get(&A));
            gu_say("ARP ", gu_get(&A) ? "ON" : "OFF");
            break;
        }
        case B_PLAY:                                     /* the sequencer: play / stop */
            transport_req = song.playing || seq_counting() ? 2u : 1u;
            gu_say(song.playing ? "STOP" : "PLAY", 0);
            break;
        case B_OCTDN:
        case B_OCTUP:
            if ((fm1_in.buttons & ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]))) ==
                ((1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP])))
                break;                               /* (both: the UPDATE MODE gesture, main.c) */
            song.octave += b == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
            {
                char o[4] = {song.octave > 0 ? '+' : song.octave < 0 ? '-' : ' ',
                             (char)('0' + (song.octave < 0 ? -song.octave : song.octave)), 0, 0};
                gu_say("OCTAVE ", o[0] == ' ' ? o + 1 : o);
            }
            break;
        case B_SAVE:
            gu_save_open();
            break;
        default:
            break;
        }
    }
}

/* power-on (main.c, after felucca_init): MIDI from every channel, program 1 */
static void gu_boot(void)
{
    song.g[G_ROUTE] = 1;                             /* SEL: every channel plays part 1 */
    song.sel = 0;
    gu.hot = -1;
    gu.shown_uboot = 0xFF;
    gu_prefs_load();
    gu_load(0);
    gu.ready = 1;
}
