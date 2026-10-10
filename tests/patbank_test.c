/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Eight sequences per track (patbank.c, patbank_store.c): the packed steps are a project's, a switch keeps each sequence's
 * steps, LEN and automation (64 records), the project slots and the live music keep them through flash, and a damaged or
 * missing store leaves the loaded music alone. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int erase_error, fail_after = -1;
static uint32_t erases, as_erases;               /* (as_erases: the autosave's two sectors, 0xE5000 / 0xE6000) */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases++;
    as_erases += off >= 0xE5000u && off < 0xE7000u;
    if (erase_error) return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/patbank_store.c"
#include "../firmware/src/project.c"

#define PB_BASE_END (OBJ_PB_BASE + OBJ_PB_N * 2u * ST_SECTOR)
_Static_assert(PB_BASE_END <= 0xDC000u, "the sequence banks overrun the user preset banks");
_Static_assert(PB_NOBJ * PB_NSTORE == OBJ_PB_N, "objects");
_Static_assert(PB_HDR + PB_PER * PB_PAT_B <= ST_PAYLOAD_MAX, "an object does not fit a sector");
_Static_assert(PB_META_FIX + 3u * 800u <= ST_PAYLOAD_MAX, "fewer than 800 automation records fit the last object");

static int check(const char *what, int ok)
{
    printf("patbank: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&motion, 0, sizeof motion);
    memset(motion_active, 0, sizeof motion_active);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(&as, 0, sizeof as);
    memset(&ui, 0, sizeof ui);
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    flash_ok = 1;
    erase_error = 0;
    fail_after = -1;
    pb_ready = 0;
    pb_dirty = 0;
    pb_clear_all();
}
static void note(uint32_t t, uint32_t i, uint32_t n)       /* step i of track t: note n */
{
    step_t *s = &trk[t].step[i];
    s->time = ST_NOTE; s->n = 1; s->note[0] = (uint8_t)n; s->vel = 100;
}
static int has(uint32_t t, uint32_t i, uint32_t n) { return trk[t].step[i].n == 1 && trk[t].step[i].note[0] == n; }
static void as_run(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(end - fm1_ms) > 0) {
        fm1_ms += 50u;
        autosave_poll();
    }
}
static void power_cycle(void)                              /* the RAM lost, the flash kept, the autosave restored */
{
    uint32_t k;
    memset(trk, 0, sizeof trk);
    memset(&motion, 0, sizeof motion);
    memset(motion_active, 0, sizeof motion_active);
    memset(&as, 0, sizeof as);
    memset(&ui, 0, sizeof ui);
    proj_name[0] = 0;
    chain_defaults(&chain_config);
    host_tracks_init();
    for (k = 0; k < NTRK; k++)
        trk[k].eng_req = trk[k].engine = (uint8_t)TRK_DEF[k][0];
    pb_ready = 0;
    pb_dirty = 0;
    pb_clear_all();
    (void)autosave_boot(1);
}

int main(void)
{
    int bad = 0, ok;
    uint32_t i, t, q;
    reset();

    {   /* the bytes of a step are a project's: a pattern's 576 bytes are found in the packed project */
        static project_store_t out;
        uint8_t b[PB_PAT_B];
        step_t back[NSTEP];
        ok = 1;
        for (i = 0; i < NSTEP; i++) {
            step_t *s = &trk[1].step[i];
            s->n = (uint8_t)(i % 5u); s->time = (uint8_t)(i % 3u); s->flags = (uint8_t)(i & 3u); s->vel = (uint8_t)(i * 2u);
            s->note[0] = (uint8_t)(i + 30u); s->note[1] = (uint8_t)(i + 31u); s->note[2] = (uint8_t)(i + 32u); s->note[3] = (uint8_t)(i + 33u);
            s->hit = (uint8_t)(i * 7u); s->acc = (uint8_t)(s->hit & (i * 3u)); s->probability = (uint8_t)(i % 102u);
        }
        project_capture(&proj_scratch);
        ok &= proj_pack(&out, &proj_scratch);
        pb_pack_pat(b, trk[1].step);
        ok &= memmem(out.raw, sizeof out.raw, b, sizeof b) != 0;
        ok &= pb_unpack_pat(back, b) && !memcmp(back, trk[1].step, sizeof back);
        bad += check("a sequence's packed bytes are the project's own, and unpack gives the steps back", ok);
        memset(trk[1].step, 0, sizeof trk[1].step);
        for (i = 0; i < NSTEP; i++) step_clear(&trk[1].step[i]);
    }

    {   /* a switch keeps each sequence: steps, LEN, automation */
        reset();
        note(0, 0, 40); trk[0].p[P_SLEN] = 12; (void)motion_set_event(&trk[0], 3, P_ATK, 10);
        ok = pb_select(0, 3) == 0 && pb_cur[0] == 3 && !has(0, 0, 40) && trk[0].p[P_SLEN] == TP[P_SLEN].def &&
             motion_count(&trk[0]) == 0;
        note(0, 5, 50); trk[0].p[P_SLEN] = 32; (void)motion_set_event(&trk[0], 9, P_ATK, 20);
        (void)motion_set_event(&trk[0], 10, P_ATK, 21);
        note(1, 0, 61);                                     /* another track: untouched by track 1's switches */
        ok &= pb_select(0, 0) == 0 && has(0, 0, 40) && !has(0, 5, 50) && trk[0].p[P_SLEN] == 12 && motion_count(&trk[0]) == 1 &&
              has(1, 0, 61) && pb_cur[1] == 0;
        ok &= pb_select(0, 3) == 0 && has(0, 5, 50) && trk[0].p[P_SLEN] == 32 && motion_count(&trk[0]) == 2;
        ok &= pb_filled(0, 0) && pb_filled(0, 3) && !pb_filled(0, 1) && pb_select(0, 8) == 1 && pb_select(4, 0) == 1;
        bad += check("a switch keeps each sequence's steps, LEN and automation; the other tracks stay", ok);
    }

    {   /* 64 automation records of a sequence that is not playing */
        reset();
        for (i = 0; i < 64u; i++) (void)motion_set_event(&trk[2], i, P_ATK, 5);
        ok = motion_count(&trk[2]) == 64u && pb_select(2, 1) == 0 && pb_select(2, 0) == 0 && motion_count(&trk[2]) == 64u;
        trk[2].step[0].time = ST_NOTE;
        for (i = 0; i < 1u; i++) (void)motion_set_event(&trk[3], i, P_ATK, 6);
        ok &= motion_count(&trk[2]) + motion_count(&trk[3]) == 65u;
        reset();
        for (i = 0; i < 64u; i++) (void)motion_set_event(&trk[2], i, P_ATK, 5);
        for (i = 0; i < 20u; i++) (void)motion_set_event(&trk[2], i, P_DEC, 5);   /* 84 records: more than it can keep */
        ok &= motion_count(&trk[2]) == 84u && pb_select(2, 1) == 2 && pb_cur[2] == 0 && motion_count(&trk[2]) == 84u;
        bad += check("64 automation records are kept; more than 64 refuses the switch and changes nothing", ok);
    }

    {   /* a project slot keeps its sequences; another slot has its own */
        reset();
        note(0, 0, 40); note(1, 2, 41);
        (void)pb_select(0, 2); note(0, 1, 42); trk[0].p[P_SLEN] = 8; (void)motion_set_event(&trk[0], 4, P_ATK, 9);
        (void)pb_select(0, 7); note(0, 7, 47);
        (void)pb_select(1, 5); note(1, 3, 51);
        ok = project_save_as(0, "BANKS") == 0;
        memset(trk, 0, sizeof trk); memset(&motion, 0, sizeof motion); memset(motion_active, 0, sizeof motion_active);
        host_tracks_init(); pb_ready = 0; pb_clear_all();       /* (the RAM only: the flash keeps project A) */
        note(0, 0, 99);
        ok &= project_save_as(1, "OTHER") == 0;
        for (t = 0; t < NTRK; t++) { memset(trk[t].step, 0, sizeof trk[t].step); for (i = 0; i < NSTEP; i++) step_clear(&trk[t].step[i]); }
        memset(proj_slot, 0, sizeof proj_slot);              /* (the RAM copies lost: from flash) */
        project_load(0);
        ok &= pb_cur[0] == 7 && pb_cur[1] == 5 && has(0, 7, 47) && has(1, 3, 51) && !has(0, 0, 99);
        ok &= pb_select(0, 0) == 0 && has(0, 0, 40) && pb_select(0, 2) == 0 && has(0, 1, 42) && trk[0].p[P_SLEN] == 8 &&
              motion_count(&trk[0]) == 1 && pb_select(1, 0) == 0 && has(1, 2, 41);
        project_load(1);
        ok &= pb_cur[0] == 0 && has(0, 0, 99) && pb_select(0, 2) == 0 && !has(0, 1, 42);
        bad += check("project SAVE and LOAD keep each track's sequences, the one that plays, LEN, automation; slots apart", ok);
    }

    {   /* the live music: the autosave brings the sequences back after a power cycle */
        reset();
        note(0, 0, 40); (void)pb_select(0, 4); note(0, 2, 44); (void)motion_set_event(&trk[0], 6, P_ATK, 12);
        trk[0].p[P_SLEN] = 24;
        (void)pb_select(3, 1); note(3, 1, 71);
        as_run(120000u);
        ok = as.writes >= 1u;
        power_cycle();
        ok &= pb_cur[0] == 4 && pb_cur[3] == 1 && has(0, 2, 44) && has(3, 1, 71) && trk[0].p[P_SLEN] == 24 && motion_count(&trk[0]) == 1;
        ok &= pb_select(0, 0) == 0 && has(0, 0, 40) && pb_select(3, 0) == 0;
        bad += check("the autosave restores each track's sequences after a power cycle (the live store)", ok);
        /* a change of only a sequence that is not playing is written too */
        note(0, 9, 90); (void)pb_select(0, 6); note(0, 11, 92);
        as_run(120000u);
        power_cycle();
        ok = pb_cur[0] == 6 && has(0, 11, 92) && pb_select(0, 0) == 0 && has(0, 9, 90);
        bad += check("a sequence edited then left (not playing now) is in the live store too", ok);
    }

    {   /* a damaged or missing store: the loaded music stays, every other sequence empty */
        reset();
        note(0, 0, 40); (void)pb_select(0, 2); note(0, 1, 42);
        ok = project_save_as(0, "X") == 0;
        for (q = 0; q < 2u; q++) {                            /* both copies of object 5 (the meta) lost */
            uint32_t base = OBJ_PB_BASE + (PB_META_O + 0u) * 2u * ST_SECTOR + q * ST_SECTOR;
            memset(nor + base, 0, ST_SECTOR);
        }
        memset(proj_slot, 0, sizeof proj_slot);
        project_load(0);
        ok &= has(0, 1, 42) && pb_cur[0] == 0 && !pb_filled(0, 2) && !pb_filled(0, 5) && pb_select(0, 1) == 0 && !has(0, 1, 42);
        bad += check("a lost store: the loaded music plays as sequence 1, the others empty", ok);
    }

    {   /* too much automation across the sequences: SAVE refused, flash as it was */
        reset();
        note(0, 0, 40);
        for (t = 0; t < NTRK; t++)
            for (q = 1; q < PB_N; q++) {
                pb_mo[t * PB_N + q].count = PB_MO_MAX;
                pb_mo[t * PB_N + q].on = 1;
                for (i = 0; i < PB_MO_MAX; i++) { pb_mo[t * PB_N + q].ev[i].place = (uint8_t)(t << 6 | i); pb_mo[t * PB_N + q].ev[i].param = P_ATK; }
            }
        ok = !pb_meta_ok() && project_save_as(0, "FULL") != 0 && !strcmp(ui.msg, "AUTOMATION FULL") && !project_used(0);
        bad += check("more automation than the store holds: SAVE says AUTOMATION FULL and writes nothing", ok);
    }

    {   /* a failing flash: SAVE ERROR, the old slot kept */
        reset();
        note(0, 0, 40);
        ok = project_save_as(0, "OLD") == 0;
        note(0, 1, 41);
        fail_after = 1;
        ok &= project_save_as(0, "NEW") != 0 && !strcmp(ui.msg, "SAVE ERROR");
        fail_after = -1;
        memset(proj_slot, 0, sizeof proj_slot);
        project_load(0);
        ok &= has(0, 0, 40) && !has(0, 1, 41);
        bad += check("a flash failing in the middle of SAVE: SAVE ERROR, the project as it was", ok);
    }

    {   /* MENU > CONTROL > SEQ SWITCH: NOW at once; END, while playing, at the end of the sequence playing */
        reset();
        ok = !(ui_prefs2 & (PREF_SEQ_END >> 16)) && menu_n(MI_SEQSW) == 2u && !strcmp(MI_NAME[MI_SEQSW], "SEQ SWITCH") &&
             !strcmp(menu_vname(MI_SEQSW, 0), "NOW") && !strcmp(menu_vname(MI_SEQSW, 1), "END") && MI_TAB[MI_SEQSW] == MTAB_CONTROL;
        song.playing = 1;
        trk[0].p[P_SLEN] = 16; trk[0].seq_idx = 3;
        (void)pb_request(0, 2);
        ok &= pb_cur[0] == 2 && !pb_next[0];                       /* NOW (the default): at once, playing or not */
        menu_put(MI_SEQSW, 1);
        ok &= menu_get(MI_SEQSW) == 1 && (ui_prefs2 & (PREF_SEQ_END >> 16));
        (void)pb_request(0, 5);
        for (t = 0; t < NTRK; t++) (void)pb_due(t);
        ok &= pb_cur[0] == 2 && pb_next[0] == 6;                   /* END, playing: waits (step 4 of 16) */
        trk[0].seq_idx = 14; for (t = 0; t < NTRK; t++) (void)pb_due(t);
        ok &= pb_cur[0] == 2;
        trk[0].seq_idx = 15; for (t = 0; t < NTRK; t++) (void)pb_due(t);                           /* the last step: the next one is the new sequence's */
        ok &= pb_cur[0] == 5 && !pb_next[0];
        (void)pb_request(0, 1); (void)pb_request(0, 5);                  /* the one playing chosen again: the waiting one called off */
        ok &= pb_cur[0] == 5 && !pb_next[0];
        (void)pb_request(0, 4);
        trk[0].seq_idx = 9; for (t = 0; t < NTRK; t++) (void)pb_due(t); trk[0].seq_idx = 2; for (t = 0; t < NTRK; t++) (void)pb_due(t);   /* a pass missed the last step: the wrap is seen */
        ok &= pb_cur[0] == 4 && !pb_next[0];
        (void)pb_request(0, 6);
        song.playing = 0; for (t = 0; t < NTRK; t++) (void)pb_due(t);                              /* stopped: at once */
        ok &= pb_cur[0] == 6 && !pb_next[0];
        (void)pb_request(1, 3);                                       /* stopped: no waiting at all */
        ok &= pb_cur[1] == 3 && !pb_next[1];
        song.playing = 1;
        (void)pb_request(2, 1); trk[2].p[P_SLEN] = 16; trk[2].seq_idx = 15;
        project_save_as(0, "Q"); pb_reset_to_live();               /* (a project load / reset forgets a waiting switch) */
        ok &= !pb_next[2];
        bad += check("SEQ SWITCH: NOW at once; END waits for the last step (or a wrap seen), stopped switches at once, same slot cancels", ok);
    }

    printf(bad ? "patbank test FAILED\n" : "patbank test passed\n");
    return bad != 0;
}
