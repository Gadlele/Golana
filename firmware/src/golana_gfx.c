/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Gadlele */
/* Golana's drawings (golana_ui.c): above the cards of the pages that have one, a picture of what the page sets, from
 * the values themselves, redrawn as they change:
 *   HOME       the sound playing, from the scope buffer (fx.c scope_buf: what goes out), three ways (GLO SETUP HOME),
 *              every frame (1.3): TRAIL the wave with the last ones behind it in a second colour, ORBIT the wave
 *              against itself a moment later (a phase portrait: every timbre draws its own figure), RIDGE the last
 *              waves stacked, the newest in front
 *   OSC 1/2    a cycle and a half of the oscillator as its WAVE, CTRL 1, MOD and SEMITONE make it
 *   FILTER     the response, dB over a log frequency axis (30 Hz .. 16 kHz), the cutoff marked
 *   FILTER EG, AMP EG   the four stages, their lengths as the times, the sustain as its level
 *   LFO 1/2    the wave, as many cycles as its rate, running
 * Integer only (no FPU); HOME redraws every frame, the LFOs every GU_LIVE_FRAMES frames, the others on a change. */

enum { GG_NONE, GG_HOME, GG_OSC1, GG_OSC2, GG_FILT, GG_FEG, GG_AEG, GG_LFO1, GG_LFO2 };
#define GU_LIVE_FRAMES 3u

static uint32_t gu_graph_of(const gu_page_t *pg)
{
    switch (pg->k[0].id) {
    case GL_CUTOFF: return pg->btn == B_HOME ? GG_HOME : GG_NONE;
    case GL_O1WAVE: return GG_OSC1;
    case GL_O2WAVE: return GG_OSC2;
    case GL_FTYPE: return GG_FILT;
    case GL_FA: return GG_FEG;
    case P_ATK: return GG_AEG;
    case GL_L1WAVE: return GG_LFO1;
    case GL_L2WAVE: return GG_LFO2;
    default: return GG_NONE;
    }
}
static int32_t gu_graph_h(uint32_t g) { return g == GG_HOME ? 84 : g ? 72 : 0; }

/* a polyline of n points (x[], y[]) 2 px thick */
static void gu_poly(const int16_t *x, const int16_t *y, uint32_t n, uint16_t c)
{
    uint32_t i;
    for (i = 1; i < n; i++) {
        cv_line(x[i - 1], y[i - 1], x[i], y[i], c);
        cv_line(x[i - 1] + 1, y[i - 1], x[i] + 1, y[i], c);
    }
}
/* a curve over the columns x0..x0+n-1 (y[] per column), with the area under it down to yb tinted */
static void gu_curve(int32_t x0, const int16_t *y, uint32_t n, int32_t yb, uint16_t c, uint16_t tint)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (y[i] < yb)
            cv_rect(x0 + (int32_t)i, y[i], 1, yb - y[i], tint);
    for (i = 1; i < n; i++) {
        cv_line(x0 + (int32_t)i - 1, y[i - 1], x0 + (int32_t)i, y[i], c);
        cv_line(x0 + (int32_t)i - 1, y[i - 1] + 1, x0 + (int32_t)i, y[i] + 1, c);
    }
}

/* ------------------------------------------------------------------ HOME --- */
/* 1.3: every view drawn every frame, lines only (as Felucca's scope), the last ones behind as a trail */
#define GH_TRAIL 5u               /* TRAIL: the waves behind the one now */
#define GH_ORB 4u                 /* ORBIT: the figures behind */
static struct {
    int32_t gain;                 /* the auto gain, Q8 (the scope's quiet sounds made visible) */
    uint8_t tr[GH_TRAIL][224];    /* TRAIL: the last waves' rows, [0] the oldest */
    uint8_t ox[GH_ORB][192], oy[GH_ORB][192];   /* ORBIT: the last figures */
    int16_t ridge[7][56];         /* RIDGE: the last waves, [0] the oldest */
    uint8_t ntr, norb, nridge;
} gh;

/* n scope samples, every stride-th, from a rising zero crossing (a still picture of a steady wave) -> s[]; their
 * peak */
static int32_t gu_scope_take(int16_t *s, uint32_t n, uint32_t stride)
{
    uint32_t w = scope_w, i, start = w - n * stride - 1u, best = start;
    int32_t pk = 0;
    for (i = 0; i < 160u; i++) {                       /* a rising crossing in the 160 samples before */
        uint32_t a = start - i;
        if (scope_buf[(a - 1u) & (SCOPE_N - 1u)] < 0 && scope_buf[a & (SCOPE_N - 1u)] >= 0) {
            best = a;
            break;
        }
    }
    for (i = 0; i < n; i++) {
        s[i] = scope_buf[(best + i * stride) & (SCOPE_N - 1u)];
        pk = s[i] > pk ? s[i] : -s[i] > pk ? -s[i] : pk;
    }
    return pk;
}
/* the scale for peak pk, Q8, glided (louder at once, quieter slowly): a full picture whatever the level */
static int32_t gu_scope_gain(int32_t pk)
{
    int32_t want = pk > 400 ? (24000 << 8) / pk : (24000 << 8) / 400;
    if (want > 16 << 8)
        want = 16 << 8;
    gh.gain = !gh.gain || want < gh.gain ? want : gh.gain + ((want - gh.gain) >> 4);
    return gh.gain;
}
/* the trail's colour age frames back (1 .. 5): the palette's second colour (NIGHT: green), fading into the surface */
static uint16_t gu_trail_col(uint32_t age, uint32_t n, uint16_t surf)
{
    static const uint8_t PCT[6] = {100, 78, 59, 42, 26, 13};
    return ux_mix(surf, GU_P->btn[6], PCT[age * 5u / n]);
}

/* TRAIL: ~30 ms of what goes out (2-3 cycles of a bass), the wave now in the page's colour, the last ones behind it
 * in the trail's: a steady sound one sharp wave, a moving one (the filter, PWM, DRIFT) a glowing wake */
static void gu_home_trail(int32_t w, int32_t h, uint16_t acc, uint16_t surf)
{
    int16_t s[224];
    int32_t g = gu_scope_gain(gu_scope_take(s, 224, 3)), cy = h / 2, k = h / 2 - 8, i;
    uint32_t j, a;
    (void)w;
    if (gh.ntr == GH_TRAIL)
        for (j = 0; j + 1u < GH_TRAIL; j++)
            memcpy(gh.tr[j], gh.tr[j + 1u], sizeof gh.tr[0]);
    else
        gh.ntr++;
    for (i = 0; i < 224; i++)
        gh.tr[gh.ntr - 1u][i] = (uint8_t)clamp(cy - (clamp((s[i] * g) >> 8, -32767, 32767) * k >> 15), 1, h - 3);
    cv_rect(8, cy, 224, 1, ux_mix(surf, acc, 20));
    for (j = 0; j < gh.ntr; j++) {                     /* the oldest first: the newer over it */
        const uint8_t *y = gh.tr[j];
        uint16_t c = (a = gh.ntr - 1u - j) ? gu_trail_col(a, GH_TRAIL + 1u, surf) : acc;
        for (i = 1; i < 224; i++) {
            cv_line(8 + i - 1, y[i - 1], 8 + i, y[i], c);
            if (!a)
                cv_line(8 + i - 1, y[i - 1] + 1, 8 + i, y[i] + 1, c);
        }
    }
}

/* ORBIT: the wave against itself 8 samples (0.36 ms) later, a phase portrait: every timbre its own figure; the last
 * figures behind in the trail's colour */
static void gu_home_orbit(int32_t w, int32_t h, uint16_t acc, uint16_t surf)
{
    int16_t s[200];
    int32_t g = gu_scope_gain(gu_scope_take(s, 200, 1)), cx = w / 2, cy = h / 2, k = h / 2 - 6, i;
    uint32_t j, a;
    cv_rect(cx - k, cy, 2 * k, 1, ux_mix(surf, acc, 25));
    cv_rect(cx, cy - k, 1, 2 * k, ux_mix(surf, acc, 25));
    if (gh.norb == GH_ORB)
        for (j = 0; j + 1u < GH_ORB; j++) {
            memcpy(gh.ox[j], gh.ox[j + 1u], sizeof gh.ox[0]);
            memcpy(gh.oy[j], gh.oy[j + 1u], sizeof gh.oy[0]);
        }
    else
        gh.norb++;
    for (i = 0; i < 192; i++) {
        int32_t p = clamp((s[i] * g) >> 8, -32767, 32767), q = clamp((s[i + 8] * g) >> 8, -32767, 32767);
        gh.ox[gh.norb - 1u][i] = (uint8_t)clamp(cx + (p * (k + 20) >> 15), 1, w - 3);
        gh.oy[gh.norb - 1u][i] = (uint8_t)clamp(cy - (q * k >> 15), 1, h - 3);
    }
    for (j = 0; j < gh.norb; j++) {
        const uint8_t *x = gh.ox[j], *y = gh.oy[j];
        uint16_t c = (a = gh.norb - 1u - j) ? gu_trail_col(a, GH_ORB + 1u, surf) : acc;
        for (i = 1; i < 192; i++) {
            cv_line(x[i - 1], y[i - 1], x[i], y[i], c);
            if (!a)
                cv_line(x[i - 1] + 1, y[i - 1], x[i] + 1, y[i], c);
        }
    }
}

/* RIDGE: the last waves stacked, the newest in front, a new one every frame; lines only */
static void gu_home_ridge(int32_t w, int32_t h, uint16_t acc, uint16_t surf)
{
    int16_t s[56], y[56];
    int32_t g = gu_scope_gain(gu_scope_take(s, 56, 12)), j, i, step = (h - 24) / 6, amp = step + 6;
    if (gh.nridge == 7u)
        for (j = 0; j < 6; j++)                         /* (no memmove in the firmware's libc) */
            memcpy(gh.ridge[j], gh.ridge[j + 1], sizeof gh.ridge[0]);
    else
        gh.nridge++;
    for (i = 0; i < 56; i++)
        gh.ridge[gh.nridge - 1u][i] = (int16_t)clamp((s[i] * g) >> 8, -32767, 32767);
    for (j = 0; j < (int32_t)gh.nridge; j++) {         /* the oldest at the top, faint */
        int32_t base = 14 + (j + 7 - (int32_t)gh.nridge) * step, row = j + 7 - (int32_t)gh.nridge;
        uint16_t c = row == 6 ? acc : gu_trail_col((uint32_t)(6 - row), 7u, surf);
        for (i = 0; i < 56; i++) {
            int32_t v = gh.ridge[j][i], env = i < 8 ? i : i > 47 ? 55 - i : 8;   /* the ends calm, as the cover's */
            y[i] = (int16_t)clamp(base - ((v < 0 ? -v : v) * amp * env >> 18) - (v * amp >> 17), 1, h - 3);
        }
        for (i = 0; i < 55; i++) {
            int32_t x0 = 8 + i * (w - 16) / 55, x1 = 8 + (i + 1) * (w - 16) / 55;
            cv_line(x0, y[i], x1, y[i + 1], c);
            if (row == 6)
                cv_line(x0, y[i] + 1, x1, y[i + 1] + 1, c);
        }
    }
}

/* -------------------------------------------------------------- OSC 1 / 2 --- */
static int32_t gu_osc1_at(const int16_t *p, int32_t c1, uint32_t ph, uint32_t ph2)
{
    switch (clamp(p[GL_O1WAVE], 0, 4)) {
    case 4: {                                          /* MORPH: the saw to the square */
        int32_t a = (int32_t)(ph >> 16) - 32768, b = ph < 0x80000000u ? -26000 : 26000;
        return a + (((b - a) * c1) >> 7);
    }
    case 1:
        return ph < 0x80000000u + (uint32_t)c1 * 0xF00000u ? 26000 : -26000;
    case 2: {
        int32_t u = (osc_tri(ph) * ((32768 + c1 * 768) >> 4)) >> 11;
        u = (int32_t)((uint32_t)(u + 32768) & 0x1FFFFu);
        return u < 65536 ? u - 32768 : 98303 - u;
    }
    case 3:
        return osc_sine(ph + (((uint32_t)gl_osc2(0, ph2, 0) * (uint32_t)(c1 * 380)) << 1));
    default: {
        int32_t a = (int32_t)(ph >> 16) - 32768, b = (int32_t)((ph << 1) >> 16) - 32768;
        return a + (((b - a) * c1) >> 8);
    }
    }
}
static void gu_osc(int32_t w, int32_t h, uint32_t two, uint16_t acc, uint16_t surf)
{
    const int16_t *p = gu_t()->p;
    int16_t y[224];
    int32_t i, cy = h / 2, k = h / 2 - 8, pass, c1 = clamp(p[GL_O1C1], 0, 127), c2 = clamp(p[GL_O1C2], 0, 127);
    int32_t semi = clamp(p[GL_O2SEMI], -24, 24), md = clamp(p[GL_O2MOD], 0, 3);
    uint32_t r2 = pitch_inc((uint32_t)(60 * 16 + semi * 16)), r1 = pitch_inc(60u * 16u);
    cv_rect(8, cy, w - 16, 1, ux_mix(surf, acc, 25));
    /* OSC 1 with CTRL 2: first, faint, the wave at the two ends LFO 1 moves CTRL 1 to (CTRL 1 -+ CTRL 2); then the wave */
    for (pass = !two && c2 ? 0 : 2; pass < 3; pass++) {
        int32_t cc = pass == 0 ? clamp(c1 - c2, 0, 127) : pass == 1 ? clamp(c1 + c2, 0, 127) : c1;
        uint16_t col = pass < 2 ? ux_mix(surf, acc, 40) : acc;
        uint32_t ratio = (r2 >> 8) * 256u / ((r1 >> 16) | 1u);   /* OSC 2 / OSC 1, Q16 (32-bit: no 64-bit divide) */
        for (i = 0; i < 224; i++) {                    /* 1.5 cycles of OSC 1 */
            uint64_t tot = (uint64_t)(uint32_t)i * (0xFFFFFFFFu / 149u);
            uint32_t ph = (uint32_t)tot, ph2;
            int32_t v;
            ph2 = md >= 2 ? (uint32_t)(((uint64_t)ph * ratio) >> 16)     /* SYNC: from OSC 1's start, every cycle */
                          : (uint32_t)((tot * ratio) >> 16);
            if (!two) {
                v = gu_osc1_at(p, cc, ph, ph2);
            } else {
                v = gl_osc2((uint32_t)clamp(p[GL_O2WAVE], 0, 3), ph2, 0);
                if (md == 1 || md == 3)
                    v = mulq15(v, gu_osc1_at(p, c1, ph, ph2));
            }
            y[i] = (int16_t)(cy - (clamp(v, -32767, 32767) * k >> 15));
        }
        for (i = 1; i < 224; i++) {
            cv_line(8 + i - 1, y[i - 1], 8 + i, y[i], col);
            if (pass == 2)
                cv_line(8 + i - 1, y[i - 1] + 1, 8 + i, y[i] + 1, col);
        }
    }
}

/* ----------------------------------------------------------------- FILTER --- */
static void gu_filter(int32_t w, int32_t h, uint16_t acc, uint16_t surf)
{
    const int16_t *p = gu_t()->p;
    int16_t y[224];
    int32_t ft = clamp(p[GL_FTYPE], 0, 4), cut = clamp(p[GL_CUTOFF], 0, 127), res = clamp(p[GL_RESO], 0, 127);
    int32_t xc = cut * 223 / 127, i, top = 8, zero = top + 18;   /* 0 dB at zero; 1 px = 1 dB */
    int32_t peak = ft == 4 ? res * res / 420 : res * res / 640;  /* the resonance, dB */
    int32_t slope = ft == 0 ? 24 : 12;                  /* dB per octave; 26.6 px an octave */
    for (i = 0; i < 6; i++)                             /* octave lines */
        cv_rect(8 + i * 40, top, 1, h - top - 6, ux_mix(surf, acc, 12));
    cv_rect(8, zero, w - 16, 1, ux_mix(surf, acc, 25));
    for (i = 0; i < 224; i++) {
        int32_t d = i - xc, db, bump;
        bump = d > -14 && d < 14 ? peak * (196 - d * d) / 196 : 0;   /* the resonance: a hump round the cutoff */
        if (ft == 3)                                    /* HPF */
            db = (d < 0 ? d * slope * 10 / 266 : 0) + bump;
        else if (ft == 2)                               /* BPF: 6 dB / octave each side, its peak the resonance */
            db = -(d < 0 ? -d : d) * 60 / 266 + bump - 6 + res / 12;
        else
            db = (d > 0 ? -d * slope * 10 / 266 : 0) + bump;
        y[i] = (int16_t)clamp(zero - db, top, h - 6);
    }
    gu_curve(8, y, 224, h - 6, acc, ux_mix(surf, acc, 16));
    cv_rect(8 + xc, top, 1, h - top - 6, ux_mix(surf, acc, 60));
}

/* ----------------------------------------------------------------- the EGs --- */
static void gu_eg(int32_t w, int32_t h, uint32_t amp, uint16_t acc, uint16_t surf)
{
    const int16_t *p = gu_t()->p;
    int16_t y[224];
    int32_t a = clamp(p[amp ? P_ATK : GL_FA], 0, 127), d = clamp(p[amp ? P_DEC : GL_FD], 0, 127);
    int32_t s = clamp(p[amp ? P_SUS : GL_FS], 0, 127), r = clamp(p[amp ? P_REL : GL_FR], 0, 127);
    int32_t wa = 2 + a * 56 / 127, wd = 3 + d * 56 / 127, wr = 3 + r * 56 / 127, ws, top = 8, bot = h - 8, i, n = 0;
    int32_t yt = bot - top, ys = bot - s * yt / 127, lv;
    ws = 224 - wa - wd - wr;
    for (i = 0; i < wa && n < 224; i++)                 /* attack: linear */
        y[n++] = (int16_t)(bot - (i + 1) * yt / wa);
    lv = 32767;                                         /* decay, release: exponential, ~99 % over their length */
    for (i = 0; i < wd && n < 224; i++) {
        lv -= lv * 150 / (wd * 32 + 1);
        y[n++] = (int16_t)(ys - (ys - top) * lv / 32767);
    }
    for (i = 0; i < ws && n < 224; i++)
        y[n++] = (int16_t)ys;
    lv = 32767;
    for (i = 0; i < wr && n < 224; i++) {
        lv -= lv * 150 / (wr * 32 + 1);
        y[n++] = (int16_t)(bot - (bot - ys) * lv / 32767);
    }
    while (n < 224)
        y[n++] = (int16_t)bot;
    gu_curve(8, y, 224, bot, acc, ux_mix(surf, acc, 16));
    cv_rect(8 + wa + wd, top, 1, bot - top, ux_mix(surf, acc, 35));            /* the key let go */
    cv_rect(8 + wa + wd + ws, top, 1, bot - top, ux_mix(surf, acc, 35));
}

/* ------------------------------------------------------------------ LFOs --- */
static uint32_t gu_lfo_ph[2], gu_lfo_n[2];
static void gu_lfo(int32_t w, int32_t h, uint32_t two, uint16_t acc, uint16_t surf)
{
    const int16_t *p = gu_t()->p;
    int16_t y[224];
    uint32_t wave = (uint32_t)clamp(p[two ? GL_L2WAVE : GL_L1WAVE], 0, 4), i;
    int32_t f = clamp(p[two ? GL_L2FREQ : GL_L1FREQ], 0, 127), sync = p[two ? GL_L2SYNC : GL_L1SYNC];
    uint32_t cyc = sync ? 2u : 1u + (uint32_t)f / 32u, inc = 0xFFFFFFFFu / 224u * cyc;
    int32_t cy = h / 2, k = h / 2 - 9;
    {
        uint32_t old = gu_lfo_ph[two];
        gu_lfo_ph[two] += LFO_INC[f] * 8u;              /* it runs (a slow LFO slowly) */
        if (gu_lfo_ph[two] < old)
            gu_lfo_n[two]++;
    }
    cv_rect(8, cy, w - 16, 1, ux_mix(surf, acc, 25));
    {
        uint32_t prev = gu_lfo_ph[two], c = gu_lfo_n[two];
        for (i = 0; i < 224u; i++) {
            uint32_t ph = gu_lfo_ph[two] + i * inc;
            if (ph < prev)
                c++;                                    /* a new cycle: S&H a new value */
            prev = ph;
            y[i] = (int16_t)(cy - (gl_wave(wave, ph, (int32_t)((c * 2654435761u) >> 16) - 32768) * k >> 15));
        }
    }
    for (i = 1; i < 224u; i++) {
        cv_line(8 + (int32_t)i - 1, y[i - 1], 8 + (int32_t)i, y[i], acc);
        cv_line(8 + (int32_t)i - 1, y[i - 1] + 1, 8 + (int32_t)i, y[i] + 1, acc);
    }
}

/* the page's drawing (y 60 .. 60 + its height), when something it shows changed or it is a live one */
static uint32_t gu_sig_graph, gu_frame;
static void gu_draw_graph(uint32_t g)
{
    const int16_t *p = gu_t()->p;
    int32_t h = gu_graph_h(g), w = 240;
    uint16_t acc = gu_btn_col(GU_PAGES[gu.page].btn), surf = GC_SURF;
    uint32_t sig = 0x811C9DC5u, live = g == GG_HOME || g == GG_LFO1 || g == GG_LFO2, i;
    static const uint8_t IDS[] = {GL_O1WAVE, GL_O1C1, GL_O1C2, GL_O2WAVE, GL_O2MOD, GL_O2SEMI, GL_FTYPE, GL_CUTOFF, GL_RESO,
                                  GL_FA, GL_FD, GL_FS, GL_FR, P_ATK, P_DEC, P_SUS, P_REL, GL_L1WAVE, GL_L1FREQ, GL_L1SYNC,
                                  GL_L2WAVE, GL_L2FREQ, GL_L2SYNC};
    gu_frame++;
    for (i = 0; i < sizeof IDS; i++)
        sig = gu_hashv(sig, p[IDS[i]]);
    sig = gu_hashv(sig, (int32_t)(g | gu_pi << 8 | gu_view << 12));
    if (live ? g != GG_HOME && gu_frame % GU_LIVE_FRAMES && sig == gu_sig_graph : sig == gu_sig_graph)
        return;
    gu_sig_graph = sig;
    cv_begin((uint32_t)w, (uint32_t)h, GC_BG);
    cv_rrect(3, 0, w - 6, h, -10, surf, GC_BG);
    switch (g) {
    case GG_HOME:
        if (gu_view == 1u)
            gu_home_orbit(w, h, acc, surf);
        else if (gu_view == 2u)
            gu_home_ridge(w, h, acc, surf);
        else
            gu_home_trail(w, h, acc, surf);
        break;
    case GG_OSC1: case GG_OSC2: gu_osc(w, h, g == GG_OSC2, acc, surf); break;
    case GG_FILT: gu_filter(w, h, acc, surf); break;
    case GG_FEG: case GG_AEG: gu_eg(w, h, g == GG_AEG, acc, surf); break;
    default: gu_lfo(w, h, g == GG_LFO2, acc, surf); break;
    }
    cv_blit(0, 60);
}

/* ---------------------------------------------------------------- SPLASH --- */
/* The power-on splash (3 s, NIGHT's colours whatever the palette): five stripes come in from the left one after the
 * other, slanted at their ends, as on the synths of the eighties; GOLANA in block letters is uncovered by a wipe and a
 * chrome glint crosses it; the stripes close into a frame, top and bottom, and the version comes up. Drawn at ~30
 * frames a second in two halves of 240 x 120 (the canvas). main.c and the browser build run it (gu_splash_step); a key
 * or a button pressed skips it (the key stays silent: seq.c kb_boot_hold) */
#define GSP_MS 3000u
static const uint32_t GSP_STRIPE[5] = {0xFFB547, 0xFF805C, 0xECD660, 0xF07A9C, 0xB092FF};   /* NIGHT's buttons */
#define GSP_BG 0x0E1118u
#define GSP_TEXT 0xE8E6E1u
#define GSP_MID 0x969CACu
static const uint8_t GSP_GLYPH[6][7] = {                /* G O L A N A, 5 x 7 (bit 4 the left column) */
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}, {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x11, 0x19, 0x15, 0x15, 0x13, 0x11, 0x11}, {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
};
static uint16_t gsp_rgb(uint32_t c) { return RGB((c >> 16) & 255u, (c >> 8) & 255u, c & 255u); }
static uint16_t gsp_mix(uint32_t a, uint32_t b, int32_t k)   /* a .. b, k 0..1000 */
{
    uint32_t i, o = 0;
    k = clamp(k, 0, 1000);
    for (i = 0; i < 24u; i += 8u) {
        int32_t x = (int32_t)((a >> i) & 255u), y = (int32_t)((b >> i) & 255u);
        o |= (uint32_t)(x + (y - x) * k / 1000) << i;
    }
    return gsp_rgb(o);
}
static int32_t gsp_ease(int32_t t, int32_t t0, int32_t len)  /* smoothstep of (t - t0) / len, 0..1000 */
{
    int32_t p = clamp((t - t0) * 1000 / len, 0, 1000);
    return p * p / 1000 * (3000 - 2 * p) / 1000;
}
/* the whole picture at t ms, on the canvas (cv_oy: the half drawn) */
static void gsp_draw(int32_t t)
{
    int32_t i, r, frame = gsp_ease(t, 2000, 600), wipe = gsp_ease(t, 900, 600) * 240 / 1000;
    for (i = 0; i < 5; i++) {                           /* the stripes, 9 px, 4 apart, from y 150 */
        int32_t y = 150 + i * 13, x1 = -40 + gsp_ease(t, i * 120, 550) * 320 / 1000;
        uint16_t c = gsp_rgb(GSP_STRIPE[i]);
        if (frame <= 0) {
            for (r = 0; r < 9; r++)                     /* (the right end slanted: 18 px over the stripe) */
                cv_rect(0, y + r, x1 - 18 * r / 9, 1, c);
        } else {                                        /* each thins to a 2 px line of the frame, 3 on top, 2 below */
            int32_t th = 9 - 9 * frame / 1000, fy = i < 3 ? 18 + i * 5 : 222 - (i - 3) * 5;
            cv_rect(0, y + (fy - y) * frame / 1000, 240, th < 2 ? 2 : th, c);
        }
    }
    if (t > 900) {                                      /* GOLANA: 6 x 6 px blocks (5 lit), 36 px a letter, from 15, 75 */
        int32_t sweep = t < 1500 ? -30 : t > 2000 ? 400 : (t - 1500) * 300 / 500 - 30, k, x, y;
        for (k = 0; k < 6; k++)
            for (y = 0; y < 7; y++)
                for (x = 0; x < 5; x++) {
                    int32_t px = 15 + k * 36 + x * 6, d = px - sweep;
                    if (!((GSP_GLYPH[k][y] >> (4 - x)) & 1u) || px > wipe)
                        continue;
                    /* silver until the glint (a white band, 14 px each side) has passed over it, then the text's */
                    cv_rect(px, 75 + y * 6, 5, 5, d > -14 && d < 14 ? gsp_mix(GSP_TEXT, 0xFFFFFFu, 1000 - (d < 0 ? -d : d) * 1000 / 14)
                                                  : d >= 14 ? gsp_mix(GSP_TEXT, GSP_BG, 420) : gsp_rgb(GSP_TEXT));
                }
    }
    if (t > 2300) {                                     /* the version, faded in under the name */
        static const char V[] = FELUCCA_VERSION " \xB7 by Gadlele";
        cv_text(120 - text_w(&AF_S, V) / 2, 128, &AF_S, V, gsp_mix(GSP_BG, GSP_MID, gsp_ease(t, 2300, 400)));
    }
}
static void gu_splash_frame(uint32_t t)
{
    uint32_t h;
    for (h = 0; h < 2u; h++) {
        cv_begin(240, 120, gsp_rgb(GSP_BG));
        cv_oy = -(int32_t)(h * 120u);
        gsp_draw((int32_t)t);
        cv_oy = 0;
        cv_blit(0, h * 120u);
    }
    lcd_sync();
}
static struct { uint32_t last, notes, btn; } gsp;
static void gu_splash_start(void)
{
    gsp.notes = fm1_in.notes;                           /* (held at power-on: a press after it skips) */
    gsp.btn = fm1_in.buttons;
    gsp.last = 0xFFFFFFFFu;
}
/* t ms into the splash: the frame due drawn (one each 33 ms); 1 = over (its 3 s, or a key / button pressed) */
static int gu_splash_step(uint32_t t)
{
    if (t >= GSP_MS || (fm1_in.notes & ~gsp.notes) || (fm1_in.buttons & ~gsp.btn)) {
        uint32_t cur, ch, k;
        fm1_irq_off();                                  /* the keys down now stay silent until let go, as under the */
        cur = fm1_in.notes;                             /* splash (seq.c keyboard_block, kb_boot_hold): the key that */
        ch = cur ^ kb_prev;                             /* skipped it plays nothing */
        for (k = 0; k < 27u; k++)
            if ((ch & cur) >> k & 1u)
                kb_note[k] = KB_SILENT;
        kb_prev = cur;
        fm1_irq_on();
        return 1;
    }
    gsp.notes &= fm1_in.notes;                          /* (let go: pressed again, it skips) */
    gsp.btn &= fm1_in.buttons;
    if (gsp.last == 0xFFFFFFFFu || t - gsp.last >= 33u) {
        gsp.last = t;
        gu_splash_frame(t);
    }
    return 0;
}
