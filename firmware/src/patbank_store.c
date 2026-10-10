/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The sequence banks in flash (patbank.c): store s (0..3 the projects A..D, PB_LIVE the music as it is now), six objects
 * OBJ_PB0 + 6 s + o, each an A/B pair of storage.c. Payload: 8 bytes {'P', 'B', 1, o, 0 x 4}, then
 *   o < 5   six sequences (576 bytes each), 3464 bytes
 *   o = 5   the last two sequences, the sequence each track plays (4), the 32 LENs, per sequence its automation record
 *           count and ON (2 bytes), then the records of the sequences with some (3 bytes each, in order); the playing
 *           sequence's are in the project record, not here. At most PB_META_RECS of them: more is refused (pb_meta_ok) */
#if FELUCCA_PATBANK && FELUCCA_FLASH
#define PB_META_FIX (PB_HDR + 2u * PB_PAT_B + NTRK + PB_NSEQ + 2u * PB_NSEQ)
#define PB_META_RECS ((PB_OBJ_MAX - PB_META_FIX) / 3u)
static uint8_t pb_wire[PB_OBJ_MAX] __attribute__((aligned(4), section(".pool")));

static uint32_t pb_obj_pats(uint32_t o) { return o < PB_META_O ? PB_PER : PB_NSEQ - PB_PER * PB_META_O; }
static uint32_t pb_records(void)            /* automation records the mirror holds outside the playing sequences */
{
    uint32_t t, n = 0;
    for (t = 0; t < PB_NSEQ; t++)
        if (t % PB_N != pb_cur[t / PB_N])
            n += pb_mo[t].count;
    return n;
}
static int pb_meta_ok(void) { return pb_records() <= PB_META_RECS; }

/* object o -> pb_wire; its length */
static uint32_t pb_build(uint32_t o)
{
    uint32_t n = pb_obj_pats(o), pos = PB_HDR, q;
    memset(pb_wire, 0, PB_HDR);
    pb_wire[0] = 'P'; pb_wire[1] = 'B'; pb_wire[2] = 1; pb_wire[3] = (uint8_t)o;
    memcpy(pb_wire + pos, pb_pat[o * PB_PER], n * PB_PAT_B);
    pos += n * PB_PAT_B;
    if (o == PB_META_O) {
        memcpy(pb_wire + pos, pb_cur, NTRK); pos += NTRK;
        memcpy(pb_wire + pos, pb_len, PB_NSEQ); pos += PB_NSEQ;
        for (q = 0; q < PB_NSEQ; q++) {
            int on = q % PB_N != pb_cur[q / PB_N];
            pb_wire[pos++] = on ? pb_mo[q].count : 0;
            pb_wire[pos++] = on ? pb_mo[q].on : 0;
        }
        for (q = 0; q < PB_NSEQ; q++)
            if (q % PB_N != pb_cur[q / PB_N]) {
                memcpy(pb_wire + pos, pb_mo[q].ev, pb_mo[q].count * sizeof(motion_event_t));
                pos += pb_mo[q].count * (uint32_t)sizeof(motion_event_t);
            }
    }
    return pos;
}

/* the objects of `mask` of store s -> flash; 0 ok. Syncs the playing sequences first */
static int pb_store_save(uint32_t s, uint32_t mask)
{
    uint32_t o;
    pb_need();
    pb_sync_all();
    if (!pb_meta_ok())
        return 3;
    for (o = 0; o < PB_NOBJ; o++)
        if ((mask >> o) & 1u) {
            uint32_t n = pb_build(o);
            if (st_save(OBJ_PB0 + s * PB_NOBJ + o, pb_wire, n))
                return 2;
        }
    return 0;
}

/* store s -> the mirror, after the music (steps, LEN, automation: the playing sequences) was loaded; 0: no such store
 * (the mirror holds just the loaded music, pb_reset_to_live) */
static int pb_store_load(uint32_t s)
{
    uint32_t o, q, t, pos, n;
    uint8_t cur[NTRK], len[PB_NSEQ], cnt[PB_NSEQ], on[PB_NSEQ];
    pb_reset_to_live();
    n = (uint32_t)st_load(OBJ_PB0 + s * PB_NOBJ + PB_META_O, pb_wire, sizeof pb_wire);
    if ((int)n < 0 || n < PB_META_FIX || pb_wire[0] != 'P' || pb_wire[1] != 'B' || pb_wire[2] != 1 || pb_wire[3] != PB_META_O)
        return 0;
    pos = PB_HDR + 2u * PB_PAT_B;
    memcpy(cur, pb_wire + pos, NTRK); pos += NTRK;
    memcpy(len, pb_wire + pos, PB_NSEQ); pos += PB_NSEQ;
    for (q = 0; q < PB_NSEQ; q++) {
        cnt[q] = pb_wire[pos++];
        on[q] = pb_wire[pos++];
    }
    for (t = 0; t < NTRK; t++)
        if (cur[t] >= PB_N)
            return 0;
    {   /* the records, checked against the length before any of it is kept */
        uint32_t tot = 0;
        for (q = 0; q < PB_NSEQ; q++) {
            if (cnt[q] > PB_MO_MAX) return 0;
            tot += cnt[q];
        }
        if (pos + tot * sizeof(motion_event_t) > n) return 0;
    }
    memcpy(pb_pat[PB_META_O * PB_PER], pb_wire + PB_HDR, 2u * PB_PAT_B);
    for (q = 0; q < PB_NSEQ; q++) {
        uint32_t k;
        pb_mo[q].count = 0;
        if (!cnt[q])
            continue;
        pb_mo[q].on = on[q] != 0u;
        for (k = 0; k < cnt[q]; k++, pos += sizeof(motion_event_t))
            memcpy(&pb_mo[q].ev[k], pb_wire + pos, sizeof(motion_event_t));
        pb_mo[q].count = cnt[q];
    }
    for (q = 0; q < PB_NSEQ; q++)
        pb_len[q] = len[q] >= (uint32_t)TP[P_SLEN].min && len[q] <= (uint32_t)TP[P_SLEN].max ? len[q] : (uint8_t)TP[P_SLEN].def;
    for (o = 0; o < PB_META_O; o++) {
        n = (uint32_t)st_load(OBJ_PB0 + s * PB_NOBJ + o, pb_wire, sizeof pb_wire);
        if ((int)n == (int)(PB_HDR + PB_PER * PB_PAT_B) && pb_wire[0] == 'P' && pb_wire[1] == 'B' && pb_wire[2] == 1 &&
            pb_wire[3] == o)
            memcpy(pb_pat[o * PB_PER], pb_wire + PB_HDR, PB_PER * PB_PAT_B);
    }
    for (t = 0; t < NTRK; t++) {            /* the sequence a track plays is the loaded music's (the record) */
        pb_cur[t] = cur[t];
        pb_mo[t * PB_N + cur[t]].count = 0;
        pb_sync(t);
    }
    pb_dirty = 0x3Fu;
    pb_gen++;
    return 1;
}

/* the autosave's companion (project.c autosave_poll): the live store, the objects that changed; 0 ok */
static int pb_flush_live(void)
{
    uint32_t o, m;
    pb_sync_all();
    m = pb_dirty;
    if (!m)
        return 0;
    for (o = 0; o < PB_NOBJ; o++)
        if ((m >> o) & 1u) {
            if (pb_store_save(PB_LIVE, 1u << o))
                return 1;
            pb_dirty &= (uint8_t)~(1u << o);
        }
    return 0;
}
#endif
