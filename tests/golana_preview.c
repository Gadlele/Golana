/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele */
/* Screenshots of Golana's drawings from the whole firmware on a simulated FM-1 (as the browser build runs it):
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/golana_preview tests/golana_preview.c -lm
 *   build/host/golana_preview OUTDIR       -> OUTDIR/*.ppm */
#include <stdio.h>
#include "../web/emu/felucca_web.c"

static const char *dir = ".";
static void render(double ms)
{
    uint32_t n = (uint32_t)(ms * 44.1 + 0.5), k;
    for (k = 0; k < n; k += 128u)
        web_render(n - k < 128u ? n - k : 128u);
}
static void shot(const char *name)
{
    char path[256];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((web_fb[i] >> 8) | (web_fb[i] << 8));
        uint8_t rgb[3] = {(uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63u) << 2), (uint8_t)((c & 31u) << 3)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
static void press(uint32_t b)
{
    web_buttons(1u << b);
    render(40);
    web_buttons(0);
    render(60);
}
static void go_page(uint32_t btn, uint32_t nth)
{
    uint32_t i;
    press(B_HOME);
    press(btn);
    for (i = 0; i < nth; i++)
        press(btn);
}
static void setv(uint32_t id, int32_t v) { trk[0].p[id] = (int16_t)v; gl_sync(&trk[0]); }

int main(int argc, char **argv)
{
    char nm[64];
    uint32_t pal, view;
    if (argc > 1)
        dir = argv[1];
    web_nor_erase();
    web_boot();
    render(700);
    /* HOME: three views, a chord held, three programs */
    {
        static const uint8_t PROGS[3] = {0, 8, 11};      /* FAT BASS, SOFT PAD, SOFT BELLS */
        uint32_t pi;
        for (pi = 0; pi < 3u; pi++) {
            gu_load(PROGS[pi]);
            web_keys((1u << 0) | (1u << 7));
            for (view = 0; view < 3u; view++) {
                gu_view = (uint8_t)view;
                ui.force = 1;
                render(view == 2u ? 900 : 400);
                snprintf(nm, sizeof nm, "home_%s_%u", N_GVIEW[view], pi);
                shot(nm);
            }
            web_keys(0);
            render(300);
        }
    }
    gu_view = 0;
    gu_load(0);
    go_page(B_EDIT, 0); render(100); shot("osc1_saw");
    setv(GL_O1WAVE, 1); setv(GL_O1C1, 80); ui.force = 1; render(100); shot("osc1_pulse");
    setv(GL_O1WAVE, 2); setv(GL_O1C1, 100); ui.force = 1; render(100); shot("osc1_trifold");
    /* 1.3: CTRL 1 / CTRL 2 named for the WAVE, CTRL 2's range drawn faint, LFO 1's rate on the fourth card */
    setv(GL_O1WAVE, 0); setv(GL_O1C1, 40); setv(GL_O1C2, 40); ui.force = 1; render(100); shot("osc1_saw_lfo");
    setv(GL_O1WAVE, 1); setv(GL_O1C1, 50); setv(GL_O1C2, 45); ui.force = 1; render(100); shot("osc1_pulse_pwm");
    setv(GL_O1WAVE, 2); setv(GL_O1C1, 40); setv(GL_O1C2, 40); ui.force = 1; render(100); shot("osc1_tri_lfo");
    setv(GL_O1WAVE, 3); setv(GL_O1C1, 40); setv(GL_O1C2, 30); ui.force = 1; render(100); shot("osc1_sine_lfo");
    setv(GL_O1WAVE, 4); setv(GL_O1C1, 64); setv(GL_O1C2, 40); ui.force = 1; render(100); shot("osc1_morph");
    setv(GL_O1WAVE, 0); setv(GL_O1C1, 0); setv(GL_O1C2, 0);
    go_page(B_EDIT, 1); setv(GL_O2MOD, 2); setv(GL_O2SEMI, 7); setv(GL_O2WAVE, 0); ui.force = 1; render(100); shot("osc2_sync");
    setv(GL_O2MOD, 0); setv(GL_O2SEMI, 0); setv(GL_O2WAVE, 3); ui.force = 1; render(100); shot("osc2_mix");
    setv(GL_O2WAVE, 0);
    go_page(B_SCL, 0);
    {
        static const char *const FT[5] = {"lpf24", "lpf12", "bpf", "hpf", "ms20"};
        uint32_t t;
        for (t = 0; t < 5u; t++) {
            setv(GL_FTYPE, (int32_t)t);
            setv(GL_CUTOFF, 70);
            setv(GL_RESO, t == 4u ? 120 : 90);
            ui.force = 1;
            render(100);
            snprintf(nm, sizeof nm, "filter_%s", FT[t]);
            shot(nm);
        }
    }
    go_page(B_ENV, 0); render(100); shot("eg_filter");
    go_page(B_ENV, 1); render(100); shot("eg_amp");
    gu_load(8);
    go_page(B_ENV, 1); render(100); shot("eg_amp_softpad");
    go_page(B_LFO, 0); render(300); shot("lfo1");
    go_page(B_LFO, 1); setv(GL_L2WAVE, 4); setv(GL_L2FREQ, 100); ui.force = 1; render(300); shot("lfo2_sh");
    go_page(B_GLO, 3); render(100); shot("glo_setup");
    {   /* (1.1) the ARP page with the four new types: the names fit their card */
        static const char *const NM[4] = {"arp_order", "arp_converge", "arp_pedal", "arp_double"};
        uint32_t k;
        go_page(B_SEQ, 0);
        for (k = 0; k < 4u; k++) { setv(GL_ARPON, 1); setv(GL_ARPTYPE, (int32_t)(6u + k)); ui.force = 1; render(100); shot(NM[k]); }
        setv(GL_ARPON, 0); setv(GL_ARPTYPE, 0);
    }
    /* the palettes: HOME (TRAIL) and FILTER in each */
    gu_load(0);
    for (pal = 0; pal < 4u; pal++) {
        gu_pi = (uint8_t)pal;
        go_page(B_HOME, 0);
        web_keys(1u << 3);
        ui.force = 1;
        render(400);
        snprintf(nm, sizeof nm, "pal%u_%s_home", pal, N_GPAL[pal]);
        shot(nm);
        web_keys(0);
        go_page(B_SCL, 0);
        render(100);
        snprintf(nm, sizeof nm, "pal%u_%s_filter", pal, N_GPAL[pal]);
        shot(nm);
    }
    /* HOME moving: 24 frames of each view, SOFT BELLS arpeggiated */
    gu_pi = 0;
    gu_load(5);                                          /* BRIGHT ARP */
    go_page(B_HOME, 0);
    web_keys((1u << 0) | (1u << 4) | (1u << 7));
    for (view = 0; view < 3u; view++) {
        uint32_t f;
        gu_view = (uint8_t)view;
        ui.force = 1;
        render(300);
        for (f = 0; f < 24u; f++) {
            render(60);
            snprintf(nm, sizeof nm, "anim_%s_%02u", N_GVIEW[view], f);
            shot(nm);
        }
    }
    web_keys(0);
    printf("done\n");
    return 0;
}
