/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele, based on Felucca (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Golana's programs: a whole sound in part 1's parameters (the GOLANA engine's, core.h GL_*, and the part's own:
 * level, pan, amp EG, voice, arp, sends, the INSERT as MOD FX). gl_defaults fills an INIT program; GL_FACTORY
 * (below) are the 25 factory sounds, written as the values they change from it. gl_sync derives what the rest of
 * the firmware reads from Golana's values: the arp's MODE (seq.c), the INSERT's TYPE (fx.c), and the delay and
 * reverb settings, which are global in Felucca (song.g) but belong to the program in Golana. */

#define GL_NPROG 32u                    /* program slots (upreset.c's 32 user preset slots) */
#define GL_NFACT 25u                    /* slots 1..25 hold the factory sounds until written over (1.2: 17..24, 1.3: 25) */

/* GL_ARPTYPE -> seq.c's AM_* (UP, DN, UPDN = ALT1, ALT2, RND, CHORD = TRIGGER; 1.1: ORD = ORDER, CONV = CONVERGE,
 * THUMB = PEDAL, DOUBLE) */
static const uint8_t GL_AM[10] = {1, 2, 3, 15, 4, 14, 5, 9, 12, 16};
/* GL_MFX -> fx.c's IT_* (OFF, CHORUS, FLANGER, ENSEMBLE, PHASER, METAL) */
static const uint8_t GL_IT[6] = {0, 8, 7, 9, 6, 10};

static void gl_defaults(int16_t *p)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        p[i] = param_desc_of(ENGI_GOLANA, i)->def;
    p[P_LEVEL] = 104;
    p[P_ATK] = 0;
    p[P_DEC] = 64;
    p[P_SUS] = 127;
    p[P_REL] = 40;
    p[P_ARATE] = 2;                     /* 1/16 */
    p[P_AOCT] = 1;
    p[P_AGATE] = 90;
    p[P_APROB] = 127;
    p[P_DIST] = p[P_CHOR] = p[P_DLY] = 0;
    p[P_REV] = 0;
    p[P_VOICE] = V_POLY;
    p[P_GLMODE] = 1;                    /* portamento: a time, as the original */
    p[P_GLIDE] = 0;
    p[P_DETUNE] = 30;
    p[P_IA] = 40;                       /* MOD FX SPEED */
    p[P_IB] = 70;                       /* .. DEPTH */
    p[P_IC] = 50;                       /* .. feedback (fixed) */
    p[P_IMIX] = 127;
    p[P_SPRD] = 0;
    p[GL_ROUTE] = 0;                    /* MFX>DLY>REV */
    p[P_LSYNC] = 0;                     /* (the track LFO: idle) */
}

/* what the rest of the firmware reads, from Golana's values (the main loop: after an edit or a load) */
static void gl_sync(track_t *t)
{
    int16_t *p = t->p;
    int32_t ty = clamp(p[GL_ARPTYPE], 0, (int32_t)NELEM(GL_AM) - 1), mf = clamp(p[GL_MFX], 0, 5);
    p[P_AMODE] = (int16_t)(p[GL_ARPON] ? GL_AM[ty] : 0);
    p[P_ITYPE] = (int16_t)GL_IT[mf];
    p[P_APROB] = 127;
    p[P_LD_PIT] = p[P_LD_FLT] = p[P_LD_SHP] = p[P_LD_AMP] = 0;
    p[P_ED_FLT] = p[P_ED_PIT] = p[P_ED_SHP] = 0;
    p[GL_ROUTE] = (int16_t)clamp(p[GL_ROUTE], 0, 2);
    if (t == &trk[0]) {                 /* the delay and the reverb of the program that plays */
        song.g[G_DTIME] = (int16_t)clamp(p[GL_DTIME], 0, 12);
        song.g[G_DFDBK] = (int16_t)clamp(p[GL_DFDBK], 0, 120);
        song.g[G_DMIX] = 110;
        song.g[G_DCOLOR] = 70;
        song.g[G_RTYPE] = (int16_t)clamp(p[GL_RTYPE], 0, 2);
        song.g[G_RSIZE] = (int16_t)clamp(p[GL_RSIZE], 0, 127);
        song.g[G_RDAMP] = (int16_t)clamp(p[GL_RDAMP], 0, 127);
    }
}

/* ------------------------------------------------------------------ factory --- */
typedef struct { uint8_t id; int16_t v; } gl_set_t;
typedef struct { const char *name; const gl_set_t *set; uint8_t n; } gl_prog_t;
#include "golana_factory.c"

/* program pi's values (pi < GL_NFACT: a factory sound; else INIT) -> p; its name -> name (13 bytes) */
static void gl_factory(uint32_t pi, int16_t *p, char *name)
{
    uint32_t i;
    gl_defaults(p);
    if (pi < NELEM(GL_FACTORY)) {
        const gl_prog_t *g = &GL_FACTORY[pi];
        for (i = 0; i < g->n; i++)
            if (g->set[i].id < P_COUNT)
                p[g->set[i].id] = (int16_t)param_fit(param_desc_of(ENGI_GOLANA, g->set[i].id), g->set[i].v);
        if (name)
            str_cpy(name, g->name, 13);
    } else if (name) {
        str_cpy(name, "INIT", 13);
    }
}

/* the values into track t at once (the audio side: the caller keeps the ISR out, fm1_irq_off) */
static void gl_apply(track_t *t, const int16_t *p)
{
    uint32_t i;
    t->eng_req = ENGI_GOLANA;
    for (i = 0; i < P_COUNT; i++)
        t->p[i] = (int16_t)param_fit(param_desc_of(ENGI_GOLANA, i), p[i]);
    t->preset = 0;
    gl_sync(t);
}
