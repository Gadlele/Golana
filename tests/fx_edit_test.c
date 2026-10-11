/* SPDX-License-Identifier: GPL-3.0-only */
/* Independent FX edges through the panel, replay, undo and fixed project store. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void edit_init(uint32_t len)
{
    ui_power_on();
    song.rec = 0;
    ui_prefs &= (uint8_t)~PREF_ACCEL;
    TSEL->p[P_SLEN] = (int16_t)len;
    TSEL->p[P_SDIV] = 2;
    go_page(GR_EVENTS);
}
static uint32_t edge(uint32_t tr, uint32_t fx, uint32_t step, uint32_t phase, int on)
{
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if (e->place == (tr << 6 | step) && MOTION_ID(e) == MOTION_FX_FIRST + fx &&
            (uint8_t)e->value == phase && !(e->param & MOTION_LOCK) == !!on) return i;
    }
    return MOTION_MAX;
}
static void pick_edge(uint32_t i) { ev_follow(EVC(EVK_REC, i)); }
static uint32_t selected(void) { uint16_t rw[EV_ROWS]; return ev_cur(rw, 0) & 0xFFu; }
static void play_at(uint32_t step, uint32_t phase)
{
    TSEL->seq_idx = (uint16_t)step;
    uint32_t length = step_samples(TSEL, div_samples(TSEL->p[P_SDIV]), step);
    TSEL->seq_pos = (length * phase + 255u) / 256u;
}
static int panel_and_undo(void)
{
    int bad = 0;
    edit_init(8);
    motion_fx_put(TSEL, 2, PF_LPF, 40, 1);
    motion_fx_put(TSEL, 6, PF_LPF, 180, 0);
    motion_set_lock(&trk[1], 3, P_REV, 80);
    motion_store_t before = motion;
    pick_edge(edge(0, PF_LPF, 2, 40, 1));
    turn(EN_K2, 2);
    bad += check("FX EDIT: KNOB2 moves ON across steps while OFF and other track stay", edge(0, PF_LPF, 4, 40, 1) < MOTION_MAX &&
        edge(0, PF_LPF, 6, 180, 0) < MOTION_MAX && motion_find(&trk[1], 3, P_REV) >= 0 && motion_valid(&motion));
    uint32_t i = selected();
    turn(EN_K2, 1);
    bad += check("FX EDIT: selection follows the same ON after list reordering", selected() < motion.count &&
        motion.event[selected()].place == 5 && !(motion.event[selected()].param & MOTION_LOCK));
    undo_swap();
    bad += check("FX EDIT: one continuous step gesture undoes both moves", edge(0, PF_LPF, 2, 40, 1) < MOTION_MAX &&
        edge(0, PF_LPF, 6, 180, 0) < MOTION_MAX && motion.count == before.count && motion_valid(&motion));
    undo_swap();
    bad += check("FX EDIT: redo restores moved ON and preserves OFF", edge(0, PF_LPF, 5, 40, 1) < MOTION_MAX &&
        edge(0, PF_LPF, 6, 180, 0) < MOTION_MAX && motion_valid(&motion));
    pick_edge(edge(0, PF_LPF, 6, 180, 0));
    turn(EN_K2, 1); turn(EN_K4, 1);
    bad += check("FX EDIT: OFF step and fine phase independently extend duration", edge(0, PF_LPF, 7, 181, 0) < MOTION_MAX &&
        edge(0, PF_LPF, 5, 40, 1) < MOTION_MAX && motion_valid(&motion));
    pick_edge(edge(0, PF_LPF, 5, 40, 1));
    turn(EN_K4, -1);
    bad += check("FX EDIT: fine ON position edits one 1/256 unit", edge(0, PF_LPF, 5, 39, 1) < MOTION_MAX);
    before = motion; ev_knob(2, 1); ev_oct();
    bad += check("FX EDIT: effect identity and ON/OFF kind remain fixed", !memcmp(&before, &motion, sizeof motion));
    i = selected();
    bad += check("FX EDIT: phase 255 keeps unsigned representation", motion_fx_move(TSEL, i, 5, 255) < MOTION_MAX &&
        edge(0, PF_LPF, 5, 255, 1) < MOTION_MAX && motion_valid(&motion));
    pick_edge(edge(0, PF_LPF, 5, 255, 1)); ev_knob(3, 1);
    bad += check("FX EDIT: fine phase clamps at 255 without rolling to another step", edge(0, PF_LPF, 5, 255, 1) < MOTION_MAX);
    ev_knob(3, -255); ev_knob(3, -1);
    bad += check("FX EDIT: fine phase clamps at zero", edge(0, PF_LPF, 5, 0, 1) < MOTION_MAX);
    return bad;
}
static int collision_and_capture(void)
{
    int bad = 0;
    edit_init(4);
    motion_fx_put(TSEL, 0, PF_LPF, 40, 1); motion_fx_put(TSEL, 2, PF_LPF, 40, 0);
    pick_edge(edge(0, PF_LPF, 0, 40, 1)); ev_knob(1, 1);
    motion_store_t before = motion;
    uint32_t tag = undo_mot_tag; uint8_t keep = undo.keep;
    ev_knob(1, 1);
    bad += check("FX EDIT: equal-time ON/OFF rejection leaves data and previous undo intact", !memcmp(&before, &motion, sizeof motion) &&
        undo_mot_tag == tag && undo.keep == keep && edge(0, PF_LPF, 1, 40, 1) < MOTION_MAX);
    undo_swap();
    bad += check("FX EDIT: failed move does not replace the preceding valid undo", edge(0, PF_LPF, 0, 40, 1) < MOTION_MAX);
    motion_fx_put(TSEL, 1, PF_LPF, 40, 1);
    before = motion;
    bad += check("FX EDIT: identical ON collision is rejected atomically", motion_fx_move(TSEL, edge(0, PF_LPF, 0, 40, 1), 1, 40) == MOTION_MAX &&
        !memcmp(&before, &motion, sizeof motion));
    bad += check("FX EDIT: invalid index, track and bounds do not alter records", motion_fx_move(TSEL, MOTION_MAX, 0, 0) == MOTION_MAX &&
        motion_fx_move(&trk[1], 0, 0, 0) == MOTION_MAX && motion_fx_move(TSEL, 0, NSTEP, 0) == MOTION_MAX &&
        motion_fx_move(TSEL, 0, 0, 256) == MOTION_MAX && !memcmp(&before, &motion, sizeof motion));
    song.rec = 1; seq_start(); play_at(0, 90); perf_press(PF_HPF, 1);
    pick_edge(edge(0, PF_LPF, 0, 40, 1)); before = motion; tag = undo_mot_tag;
    ev_knob(1, 1);
    bad += check("FX EDIT: unfinished FX take rejects edits without consuming its reserved release", !memcmp(&before, &motion, sizeof motion) &&
        undo_mot_tag == tag && motion_fx_owner[PF_HPF] == 1);
    perf_press(PF_HPF, 0); song.rec = 0; seq_stop();
    chain.running = 1; before = motion; ev_knob(1, 1);
    bad += check("FX EDIT: active SONG retains STOP TO EDIT", !memcmp(&before, &motion, sizeof motion));
    chain.running = 0;
    return bad;
}
static int live_timing(void)
{
    int bad = 0;
    edit_init(4);
    motion_fx_put(TSEL, 0, PF_LPF, 40, 1); motion_fx_put(TSEL, 0, PF_LPF, 180, 0);
    seq_start(); play_at(0, 100); motion_fx_refresh(TSEL);
    bad += check("FX EDIT: initial re-chase finds the active recorded hold", perf_motion[0] == PF_BIT(PF_LPF));
    uint32_t idx = TSEL->seq_idx, pos = TSEL->seq_pos;
    pick_edge(edge(0, PF_LPF, 0, 40, 1)); ev_knob(3, 80);
    bad += check("FX EDIT: moving ON into future releases replay and schedules new ON", !perf_motion[0] && motion_fx_next[0] == 120 &&
        TSEL->seq_idx == idx && TSEL->seq_pos == pos);
    undo_swap();
    bad += check("FX EDIT: live undo re-chases restored ON at current playhead", perf_motion[0] == PF_BIT(PF_LPF) && motion_fx_next[0] == 180);
    undo_swap(); play_at(0, 119); seq_tick(TSEL, 0);
    bad += check("FX EDIT: changed ON is not replayed early", !perf_motion[0]);
    play_at(0, 120); seq_tick(TSEL, 0);
    bad += check("FX EDIT: changed ON fires at its new fine phase", perf_motion[0] == PF_BIT(PF_LPF));
    play_at(0, 160); pick_edge(edge(0, PF_LPF, 0, 180, 0)); ev_knob(3, -30);
    bad += check("FX EDIT: moving OFF into the elapsed part ends the hold immediately", !perf_motion[0]);
    ev_knob(3, 50);
    bad += check("FX EDIT: moving OFF later extends the hold and replaces the stale deadline", perf_motion[0] == PF_BIT(PF_LPF) && motion_fx_next[0] == 200);
    play_at(0, 180); seq_tick(TSEL, 0);
    bad += check("FX EDIT: original OFF deadline no longer ends the hold", perf_motion[0] == PF_BIT(PF_LPF));
    play_at(0, 200); seq_tick(TSEL, 0);
    bad += check("FX EDIT: moved OFF releases at its new deadline", !perf_motion[0]);
    seq_stop();
    return bad;
}
static int loop_and_priority(void)
{
    int bad = 0;
    edit_init(4);
    motion_fx_put(TSEL, 3, PF_FRZ, 120, 1); motion_fx_put(TSEL, 0, PF_FRZ, 80, 0);
    pick_edge(edge(0, PF_FRZ, 3, 120, 1)); ev_knob(1, -1);
    seq_start(); play_at(0, 40); motion_fx_refresh(TSEL);
    bad += check("FX EDIT: moved ON across steps retains a cross-loop hold", perf_motion[0] == PF_BIT(PF_FRZ));
    pick_edge(edge(0, PF_FRZ, 0, 80, 0)); ev_knob(3, -20);
    play_at(0, 60); seq_tick(TSEL, 0);
    bad += check("FX EDIT: edited cross-loop OFF releases at its new phase", !perf_motion[0]);
    seq_stop();
    edit_init(4);
    motion_fx_put(TSEL, 0, PF_R16, 40, 1); motion_fx_put(TSEL, 0, PF_FRZ, 80, 1);
    motion_fx_put(TSEL, 2, PF_R16, 0, 0); motion_fx_put(TSEL, 2, PF_FRZ, 0, 0);
    seq_start(); play_at(0, 120); motion_fx_refresh(TSEL);
    bad += check("FX EDIT: re-chase chooses the last activated recorded buffer FX", perf_pick(perf_motion[0]) == PF_FRZ);
    pick_edge(edge(0, PF_R16, 0, 40, 1)); ev_knob(3, 60);
    bad += check("FX EDIT: changing ON timing updates overlapping buffer priority", perf_pick(perf_motion[0]) == PF_R16 && motion_valid(&motion));
    perf_press(PF_OUP, 1);
    pick_edge(edge(0, PF_R16, 0, 100, 1)); ev_knob(3, -1);
    bad += check("FX EDIT: manual hold remains on top during timeline edits", (perf_held & PF_BIT(PF_OUP)) &&
        perf_pick(perf_held | perf_motion[0]) == PF_OUP);
    perf_press(PF_OUP, 0); seq_stop();
    return bad;
}
static int storage_and_pool(void)
{
    int bad = 0;
    edit_init(64);
    motion_fx_put(TSEL, 63, PF_ETHR, 255, 1); motion_fx_put(TSEL, 0, PF_ETHR, 1, 0);
    pick_edge(edge(0, PF_ETHR, 63, 255, 1)); ev_knob(1, -1); ev_knob(3, -1);
    project_t q, restored; project_store_t disk;
    project_capture(&q);
    bad += check("FX EDIT: edited unsigned phase and cross-loop timing round-trip in FUN10", proj_pack(&disk, &q) &&
        proj_import(&restored, &disk, sizeof disk) && !memcmp(&q.motion, &restored.motion, sizeof q.motion) && sizeof disk == 3840);
    for (uint32_t j = 0; j < MOTION_MAX - 2u; j++)
        motion_set_event(&trk[1], j % 64u, j < 64u ? P_REV : P_DLY, 40);
    pick_edge(edge(0, PF_ETHR, 62, 254, 1)); uint32_t count = motion.count; ev_knob(1, -1);
    bad += check("FX EDIT: a full shared pool still permits an in-place edge move", count == MOTION_MAX && motion.count == MOTION_MAX &&
        edge(0, PF_ETHR, 61, 254, 1) < MOTION_MAX && motion_valid(&motion));
    undo_swap(); undo_swap();
    bad += check("FX EDIT: full-pool undo and redo preserve all other track records", motion.count == MOTION_MAX &&
        motion_count(&trk[1]) == MOTION_MAX - 2u && edge(0, PF_ETHR, 61, 254, 1) < MOTION_MAX && motion_valid(&motion));
    return bad;
}
static int raw_undo_and_other_owner(void)
{
    int bad = 0;
    edit_init(4);
    motion_fx_put(TSEL, 0, PF_LPF, 10, 1); motion_fx_put(TSEL, 1, PF_LPF, 10, 0);
    motion_fx_put(TSEL, 2, PF_LPF, 10, 0); /* redundant edge remains editable until save/load cleanup */
    pick_edge(edge(0, PF_LPF, 0, 10, 1)); ev_knob(3, 1);
    undo_swap();
    bad += check("FX EDIT: undo retains every raw edge, including a redundant OFF", motion.count == 3 &&
        edge(0, PF_LPF, 0, 10, 1) < MOTION_MAX && edge(0, PF_LPF, 2, 10, 0) < MOTION_MAX);
    undo_swap();
    bad += check("FX EDIT: redo also retains raw edge count", motion.count == 3 && edge(0, PF_LPF, 0, 11, 1) < MOTION_MAX);
    motion_fx_set_track(1); song.rec = 2; seq_start();
    trk[1].seq_idx = 0; trk[1].seq_pos = 0; perf_press(PF_HPF, 1);
    pick_edge(edge(0, PF_LPF, 0, 11, 1)); ev_knob(3, 1);
    bad += check("FX EDIT: capture on another track does not block the selected closed timeline", edge(0, PF_LPF, 0, 12, 1) < MOTION_MAX &&
        motion_fx_owner[PF_HPF] == 2 && motion_fx_reserved() == 1 && motion_valid(&motion));
    perf_press(PF_HPF, 0); seq_stop();
    return bad;
}
static int randomized_order(void)
{
    int bad = 0, ok = 1;
    uint32_t seed = 71;
    edit_init(64);
    for (uint32_t fx = 0; fx < PF_NFX; fx++) {
        motion_fx_put(TSEL, fx * 2u, fx, 40, 1);
        motion_fx_put(TSEL, fx * 2u + 1u, fx, 160, 0);
    }
    motion_set_lock(&trk[1], 3, P_REV, 70); motion_set_event(TSEL, 4, P_DLY, 25);
    /* Deliberately reorder whole-step blocks, as track replacements can do. */
    for (uint32_t j = 0; j < motion.count / 2u; j++) {
        motion_event_t tmp = motion.event[j]; motion.event[j] = motion.event[motion.count - 1u - j];
        motion.event[motion.count - 1u - j] = tmp;
    }
    ok &= motion_valid(&motion);
    for (uint32_t trial = 0; trial < 2000u; trial++) {
        seed = seed * 1664525u + 1013904223u;
        uint32_t i = (seed >> 8) % motion.count, step = (seed >> 16) % 64u, phase = seed >> 24;
        motion_store_t before = motion;
        if (!MOTION_IS_FX(&motion.event[i]) || (motion.event[i].place >> 6)) continue;
        motion_event_t moved = before.event[i];
        uint32_t to = motion_fx_move(TSEL, i, step, phase);
        ok &= motion_valid(&motion) && motion.count == before.count;
        if (to == MOTION_MAX) { ok &= !memcmp(&before, &motion, sizeof motion); continue; }
        moved.place = (uint8_t)step; moved.value = (int8_t)phase;
        ok &= !memcmp(&motion.event[to], &moved, sizeof moved);
        uint32_t a = 0, b = 0;
        while (a < motion.count && b < motion.count) {
            if (a == i) { a++; continue; } if (b == to) { b++; continue; }
            ok &= !memcmp(&before.event[a++], &motion.event[b++], sizeof moved);
        }
    }
    bad += check("FX EDIT: 2000 moves preserve valid ordering, edge identity and all other records", ok);
    return bad;
}
int main(void)
{
    int bad = panel_and_undo() + collision_and_capture() + live_timing() + loop_and_priority() + storage_and_pool() +
        raw_undo_and_other_owner() + randomized_order();
    printf("FX edge editing: %s\n", bad ? "FAILED" : "passed");
    return bad ? 1 : 0;
}
