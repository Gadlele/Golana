/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sequences: eight per track (FELUCCA_PATBANK). A track plays one of them at a time: trk[].step is the one playing, the
 * other seven live in a RAM mirror (pb_pat, packed as a project packs steps: 9 bytes). Each sequence has its own steps, its
 * own LEN (P_SLEN) and its own automation (the track's motion records, at most PB_MO_MAX = 64 of them while it is not the
 * one playing). DIV, SWING, GATE, QUANTIZE and the sound stay the track's.
 * Switching (pb_select): the playing sequence is packed into the mirror, the chosen one is put into the track, at once,
 * in the middle of a pass (the playhead keeps its step). Main loop only.
 * Persistence (patbank_store.c): the project record keeps the sequence that plays; the others are in flash, five stores
 * (project A..D, the live music) of six objects: object o < 5 holds sequences 6 o .. 6 o + 5 (q = track * 8 + slot), object
 * 5 the last two and the rest (the sequence each track plays, the LENs, the automation of the sequences not playing). */
#if FELUCCA_PATBANK
#define PB_N 8u                          /* sequences per track */
#define PB_NSEQ (NTRK * PB_N)
#define PB_STEP_B 9u                     /* a step packed: project.c proj_pack's nine bytes */
#define PB_PAT_B (NSTEP * PB_STEP_B)
#define PB_MO_MAX 64u                    /* automation records of a sequence that is not playing */
#define PB_PER 6u                        /* sequences an object holds */
#define PB_NOBJ 6u
#define PB_NSTORE 5u
#define PB_LIVE 4u                       /* the live music's store */
#define PB_HDR 8u
#define PB_KEY0 19u                      /* SEQ held: C5 .. G5, the eight rightmost keys, are sequences 1..8 */
#define PB_META_O 5u
#define PB_OBJ_MAX ST_PAYLOAD_MAX
typedef struct { uint8_t count, on; motion_event_t ev[PB_MO_MAX]; } pb_mo_t;

static uint8_t pb_cur[NTRK];             /* the sequence each track plays, 0..7 */
static uint8_t pb_len[PB_NSEQ];          /* LEN of each (the playing one's is the track's P_SLEN) */
static uint8_t pb_pat[PB_NSEQ][PB_PAT_B] __attribute__((section(".pool")));
static pb_mo_t pb_mo[PB_NSEQ] __attribute__((section(".pool")));   /* (the playing sequence's: motion) */
static uint8_t pb_next[NTRK];            /* a switch waiting for the end of the sequence playing: the slot + 1, 0 none (SEQ SWITCH END) */
static uint8_t pb_last[NTRK];            /* the step each track was on when pb_poll last looked */
static uint8_t pb_ready;
static uint8_t pb_dirty;                 /* bit o: the live store's object o is older than the mirror */
static uint32_t pb_gen;                  /* + 1 per change (autosave_sig) */

/* ---- steps <-> bytes: the bytes of project.c proj_pack / proj_unpack ---- */
static void pb_pack_step(uint8_t *b, const step_t *s)
{
    uint32_t r = step_ratchet(s) - 1u, nd = (uint32_t)step_nudge(s) & 15u, j;
    uint32_t n = s->n > 4u ? 4u : s->n, tm = s->time > ST_REST ? ST_REST : s->time;
    for (j = 0; j < 4u; j++)
        b[j] = (uint8_t)((s->note[j] > 127u ? 127u : s->note[j]) | (nd >> j & 1u) << 7);
    b[4] = (uint8_t)(n | tm << 3 | (s->flags & 3u) << 5);
    b[5] = (uint8_t)((s->vel > 127u ? 127u : s->vel) | (r & 1u) << 7);
    b[6] = s->hit;
    b[7] = s->acc & s->hit;
    b[8] = (uint8_t)((s->probability > 101u ? 101u : s->probability) | (r >> 1) << 7);
}
static int pb_unpack_step(step_t *s, const uint8_t *b)
{
    uint32_t meta = b[4], nd = 0, j;
    for (j = 0; j < 4u; j++) {
        nd |= (uint32_t)(b[j] >> 7) << j;
        s->note[j] = b[j] & 127u;
    }
    s->n = meta & 7u;
    s->time = (meta >> 3) & 3u;
    s->flags = (meta >> 5) & 3u;
    s->vel = b[5] & 127u;
    meta |= (uint32_t)(b[5] & 128u) << 1;
    s->hit = b[6];
    s->acc = b[7];
    s->probability = b[8] & 127u;
    meta |= (uint32_t)(b[8] & 128u) << 2;
    if ((meta & 255u) > 127u || s->n > 4u || s->time > ST_REST || s->probability > 101u || (s->acc & ~s->hit))
        return 0;
    step_set_ratchet(s, (meta >> 8) + 1u);
    step_set_nudge(s, (int32_t)(nd ^ 8u) - 8);
    return 1;
}
static void pb_pack_pat(uint8_t *d, const step_t *st)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        pb_pack_step(d + i * PB_STEP_B, &st[i]);
}
static int pb_unpack_pat(step_t *st, const uint8_t *d)   /* 0: a step is no step: every one empty */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (!pb_unpack_step(&st[i], d + i * PB_STEP_B)) {
            for (i = 0; i < NSTEP; i++)
                step_clear(&st[i]);
            return 0;
        }
    return 1;
}

static void pb_clear_all(void)
{
    uint32_t q;
    step_t e[NSTEP];
    for (q = 0; q < NSTEP; q++)
        step_clear(&e[q]);
    pb_pack_pat(pb_pat[0], e);
    for (q = 1; q < PB_NSEQ; q++)
        memcpy(pb_pat[q], pb_pat[0], PB_PAT_B);
    memset(pb_mo, 0, sizeof pb_mo);
    memset(pb_cur, 0, sizeof pb_cur);
    memset(pb_next, 0, sizeof pb_next);
    for (q = 0; q < PB_NSEQ; q++)
        pb_len[q] = (uint8_t)TP[P_SLEN].def;
    pb_ready = 1;
}
static void pb_need(void) { if (!pb_ready) pb_clear_all(); }

/* the playing sequence of track t into the mirror (its steps, its LEN); marks the live store's objects that changed */
static void pb_sync(uint32_t t)
{
    uint8_t b[PB_PAT_B];
    uint32_t q;
    pb_need();
    q = t * PB_N + pb_cur[t];
    pb_pack_pat(b, trk[t].step);
    if (memcmp(pb_pat[q], b, PB_PAT_B)) {
        memcpy(pb_pat[q], b, PB_PAT_B);
        pb_dirty |= (uint8_t)(1u << (q / PB_PER));
        pb_gen++;
    }
    if (pb_len[q] != (uint8_t)trk[t].p[P_SLEN]) {
        pb_len[q] = (uint8_t)trk[t].p[P_SLEN];
        pb_dirty |= 1u << PB_META_O;
        pb_gen++;
    }
}
static void pb_sync_all(void)
{
    uint32_t t;
    for (t = 0; t < NTRK; t++)
        pb_sync(t);
}

/* a sequence with a note or a hit in it (the playing one: the track's steps) */
static int pb_filled(uint32_t t, uint32_t slot)
{
    uint32_t i;
    pb_need();
    if (slot == pb_cur[t])
        return !seq_is_empty(&trk[t]);
    for (i = 0; i < NSTEP; i++) {
        const uint8_t *b = pb_pat[t * PB_N + slot] + i * PB_STEP_B;
        if ((b[4] & 7u) || b[6])
            return 1;
    }
    return 0;
}

/* track t plays sequence `slot`. 0 done; 1 no such; 2 the playing sequence has more than PB_MO_MAX automation records
 * (it could not be kept); 3 the automation of the sequence does not fit with the other tracks' */
static int pb_select(uint32_t t, uint32_t slot)
{
    track_t *tr;
    motion_store_t mo, in;
    step_t ns[NSTEP];
    uint32_t q, old, i, f;
    if (t >= NTRK || slot >= PB_N)
        return 1;
    pb_need();
    if (slot == pb_cur[t])
        return 0;
    tr = &trk[t];
    motion_snapshot_track(tr, &mo);
    if (mo.count > PB_MO_MAX)
        return 2;
    q = t * PB_N + slot;
    old = t * PB_N + pb_cur[t];
    memset(&in, 0, sizeof in);
    in.count = pb_mo[q].count;
    in.on = pb_mo[q].on ? (uint8_t)(1u << t) : 0u;
    for (i = 0; i < in.count && i < PB_MO_MAX; i++)
        in.event[i] = pb_mo[q].ev[i];
    if (in.count > PB_MO_MAX || !motion_valid(&in))
        memset(&in, 0, sizeof in);
    for (i = 0; i < in.count; i++)
        if ((in.event[i].place >> 6) != t)
            memset(&in, 0, sizeof in);
    pb_sync(t);
    (void)pb_unpack_pat(ns, pb_pat[q]);
    if (motion_replace_track(tr, &in))
        return 3;
    pb_mo[old].count = mo.count;
    pb_mo[old].on = (uint8_t)(mo.on != 0u);
    for (i = 0; i < mo.count; i++)
        pb_mo[old].ev[i] = mo.event[i];
    pb_mo[q].count = 0;
    f = motion_guard();
    memcpy(tr->step, ns, sizeof ns);
    tr->p[P_SLEN] = (int16_t)clamp((int32_t)pb_len[q], TP[P_SLEN].min, TP[P_SLEN].max);
    tr->rh_n = tr->rskip_n = 0;
    motion_unguard(f);
    pb_cur[t] = (uint8_t)slot;
    pb_next[t] = 0;
    pb_dirty |= (uint8_t)(1u << (q / PB_PER) | 1u << (old / PB_PER) | 1u << PB_META_O);
    pb_gen++;
    return 0;
}

/* a sequence chosen for track t (SEQ > SEQUENCE's knobs, SEQ held + C5..G5). MENU > CONTROL > SEQ SWITCH: NOW at once; END,
 * while the music plays, at the end of the sequence playing (pb_due). 0 switched, 1 waits, 2 it is the one playing (a
 * waiting switch is called off), < 0 pb_select's rc, negated */
static int pb_request(uint32_t t, uint32_t s)
{
    int rc;
    t %= NTRK;
    pb_need();
    if (s == pb_cur[t]) {
        pb_next[t] = 0;
        return 2;
    }
    if ((ui_prefs2 & (PREF_SEQ_END >> 16)) && song.playing && s < PB_N) {
        pb_next[t] = (uint8_t)(s + 1u);
        return 1;
    }
    rc = pb_select(t, s);
    return rc ? -rc : 0;
}
/* main loop, every pass, per track: its waiting switch happens on the last step of the sequence playing (the next step is
 * the new one's first), at once when the music is stopped, and late (a wrap seen) when a pass missed the last step.
 * 1 switched, 0 nothing (yet), < 0 pb_select's rc, negated (the switch is dropped) */
static int pb_due(uint32_t t)
{
    const track_t *tr = &trk[t];
    uint32_t idx = tr->seq_idx, run = song.playing && tr->seq_pos < 0x7FFFFFFFu, last = pb_last[t];
    int rc;
    pb_last[t] = (uint8_t)idx;
    if (!pb_next[t])
        return 0;
    if (run && idx + 1u < (uint32_t)tr->p[P_SLEN] && idx >= last)
        return 0;
    rc = pb_select(t, (uint32_t)pb_next[t] - 1u);
    pb_next[t] = 0;
    return rc ? -rc : 1;
}

/* the music was loaded (project.c project_restore_runtime): every track plays sequence 1 of an empty bank, the loaded
 * steps and LEN in it. A store (patbank_store.c) fills the rest */
static void pb_reset_to_live(void)
{
    uint32_t t;
    pb_clear_all();
    for (t = 0; t < NTRK; t++)
        pb_sync(t);
    pb_dirty = 0x3Fu;
    pb_gen++;
}
#else
#define pb_gen 0u
#endif
