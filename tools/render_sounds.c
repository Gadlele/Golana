/* SPDX-License-Identifier: GPL-3.0-only
 * The website's Sounds section (web/site/make_pages.py): eight short pieces, each one factory program of Golana played
 * as the FM-1 plays it (one synth, four voices, its own arpeggiator, delay and reverb; tests/hostsim.c builds the
 * firmware's engine for the computer, as tests/golana_sound.c): OUT/N.wav and OUT/sounds.json (each piece's file,
 * title, text and program). Mostly chord progressions; one line with the cutoff moving, as ALGORITHM would.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/render_sounds tools/render_sounds.c -lm
 *   build/host/render_sounds OUT          (tools/render_sounds.sh: as MP3) */
#define main hostsim_main
#include "hostsim.c"
#undef main

static FILE *w;
static uint32_t frames;
static int16_t cut_from = -1, cut_to;               /* a cutoff sweep over the piece (ALGORITHM), -1 = none */
static uint64_t nsamp;                              /* the samples rendered */
static uint32_t total;                              /* the score's length, ms */
static int wheel_sweep;                             /* the mod wheel up and back down over the piece (CC1) */
static uint32_t cur_ms(void) { return (uint32_t)(nsamp * 1000u / FS); }
static void run_ms(uint32_t ms)
{
    int32_t o[2 * CTL];
    uint32_t n = ms * (FS / 1000u) / CTL, i;
    while (n--) {
        if (cut_from >= 0 && total) {               /* up and back down: a triangle over the piece */
            uint32_t ph = cur_ms() * 2000u / total;    /* 0..2000 */
            int32_t k = ph < 1000u ? (int32_t)ph : 2000 - (int32_t)ph;
            trk[0].p[GL_CUTOFF] = (int16_t)(cut_from + (cut_to - cut_from) * (k < 0 ? 0 : k) / 1000);
        }
        if (wheel_sweep && total) {                 /* 0 for the first third, then up, held, and down at the end */
            uint32_t ph = cur_ms() * 300u / total;
            trk[0].mw = (uint8_t)(ph < 100u ? 0 : ph < 160u ? (ph - 100u) * 127u / 60u : ph < 260u ? 127u
                                  : ph < 300u ? (300u - ph) * 127u / 40u : 0u);
        }
        mix_block(o, CTL);
        nsamp += CTL;
        if (!w)
            continue;
        for (i = 0; i < CTL; i++)
            wav_put(w, o[2u * i], o[2u * i + 1u]);
        frames += CTL;
    }
}
static void midi_note(uint32_t on, uint32_t note, uint32_t vel)
{
    uint32_t st = on ? 0x90u : 0x80u;
    midi_enqueue((st >> 4) | st << 8 | (note & 127u) << 16 | (vel & 127u) << 24, 1);
}

/* ---- a score: notes in ms (a 16th of the piece's tempo: s16) ---- */
typedef struct { uint32_t t; uint8_t note, vel, on; } ev_t;
static ev_t ev[2048];
static uint32_t nev, s16;                           /* s16: a 16th, in 0.01 ms */
static uint32_t at(uint32_t bar, uint32_t six) { return (bar * 16u + six) * s16 / 100u; }
static void nt_ms(uint32_t t, uint32_t note, uint32_t dur, uint32_t vel)
{
    if (nev + 2u > NELEM(ev) || dur < 2u)
        return;
    ev[nev++] = (ev_t){t * 2u + 1u, (uint8_t)note, (uint8_t)vel, 1};
    ev[nev++] = (ev_t){(t + dur) * 2u, (uint8_t)note, 0, 0};
}
static void nt(uint32_t bar, uint32_t six, uint32_t note, uint32_t len, uint32_t vel)   /* len in 16ths, a little short */
{
    nt_ms(at(bar, six), note, len * s16 / 100u - 10u, vel);
}
static void tie(uint32_t bar, uint32_t six, uint32_t note, uint32_t len, uint32_t vel)  /* into the next note: a slide */
{
    nt_ms(at(bar, six), note, len * s16 / 100u + 20u, vel);
}
static void chord(uint32_t bar, uint32_t six, const uint8_t *c, uint32_t n, uint32_t len, uint32_t vel)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        nt(bar, six, c[i], len, vel);
}
static void line(uint32_t bar, const uint8_t (*m)[2], uint32_t n, uint32_t vel)   /* {note, 16ths}, 0 = rest */
{
    uint32_t i, six = 0;
    for (i = 0; i < n; i++) {
        if (m[i][0])
            nt(bar + six / 16u, six % 16u, m[i][0], m[i][1], vel);
        six += m[i][1];
    }
}

static const char *prog_name;
static int16_t sets[6][2];                          /* values changed from the program's (a comparison, a demo) */
static uint32_t nset;
static void setp(int32_t id, int32_t v) { sets[nset][0] = (int16_t)id; sets[nset++][1] = (int16_t)v; }
static void prog(const char *name, uint32_t bpm)
{
    int16_t v[P_COUNT];
    char nm[16];
    uint32_t k, pi;
    for (pi = 0; pi < NELEM(GL_FACTORY); pi++) {
        gl_factory(pi, v, nm);
        if (!strcmp(nm, name))
            break;
    }
    if (pi == NELEM(GL_FACTORY)) {
        fprintf(stderr, "render_sounds: no program %s\n", name);
        exit(1);
    }
    host_tracks_init();
    for (k = 0; k < NTRK; k++) {                    /* (as tests/golana_sound.c load_prog) */
        gl_factory(0xFF, v, 0);
        gl_apply(&trk[k], v);
        trk[k].engine = trk[k].eng_req;
        eng_mem_claim(&trk[k]);
    }
    gl_factory(pi, v, nm);
    for (k = 0; k < nset; k++)
        v[sets[k][0]] = sets[k][1];
    gl_apply(&trk[0], v);
    trk[0].engine = trk[0].eng_req;
    song.sel = 0;
    song.g[G_ROUTE] = 1;                            /* SEL: every channel plays the synth */
    song.g[G_BPM] = (int16_t)bpm;
    s16 = 1500000u / bpm;
    prog_name = GL_FACTORY[pi].name;
}

/* ---- the eight pieces ---- */
static void p_sync(void)                            /* SYNC PAD: Dmaj7 Bm9 Gmaj7 Aadd9 */
{
    static const uint8_t C[4][4] = {{50, 57, 61, 64}, {47, 54, 57, 61}, {43, 50, 54, 59}, {45, 52, 57, 59}};
    uint32_t b;
    prog("SYNC PAD", 84);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 4, 16, 96);
}
static void p_arp(void)                             /* BRIGHT ARP: its arpeggiator over Cm Ab Eb Bb */
{
    static const uint8_t C[4][3] = {{48, 51, 55}, {44, 48, 51}, {51, 55, 58}, {46, 50, 53}};
    uint32_t b;
    prog("BRIGHT ARP", 118);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 3, 16, 100);
}
static void p_brass(void)                           /* 80S BRASS: stabs on Eb Bb Cm Ab */
{
    static const uint8_t C[4][4] = {{51, 55, 58, 63}, {50, 53, 58, 62}, {48, 51, 55, 60}, {48, 51, 56, 60}};
    static const uint8_t HIT[5][2] = {{0, 3}, {3, 3}, {6, 2}, {10, 2}, {12, 4}};
    uint32_t b, i;
    prog("80S BRASS", 112);
    for (b = 0; b < 8u; b++)
        for (i = 0; i < (b % 2u ? 5u : 3u); i++)
            chord(b, HIT[i][0], C[b % 4u], 4, HIT[i][1], i ? 92 : 110);
}
static void p_organ(void)                           /* ORGAN: a gospel turn, C F/C C G7 */
{
    static const uint8_t C[4][4] = {{48, 52, 55, 60}, {48, 53, 57, 60}, {48, 52, 55, 64}, {47, 50, 53, 55}};
    uint32_t b, i;
    prog("ORGAN", 92);
    for (b = 0; b < 8u; b++)
        for (i = 0; i < 4u; i++)
            chord(b, i * 4u, C[b % 4u], 4, i == 3u ? 2u : 3u, 96);
}
static void p_phase(void)                           /* PHASE PAD: Am9 Fmaj7 Cmaj7 Em7, slow */
{
    static const uint8_t C[4][4] = {{45, 52, 55, 59}, {41, 52, 55, 57}, {48, 52, 55, 59}, {40, 50, 55, 59}};
    uint32_t b;
    prog("PHASE PAD", 72);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 4, 16, 92);
}
static void p_slowarp(void)                         /* SLOW ARP: Am7 Dm7 Gsus4 C, two bars each */
{
    static const uint8_t C[4][4] = {{45, 52, 55, 60}, {50, 53, 57, 60}, {43, 50, 55, 60}, {48, 52, 55, 60}};
    uint32_t b;
    prog("SLOW ARP", 90);
    for (b = 0; b < 8u; b += 2u)
        chord(b, 0, C[b / 2u], 4, 32, 96);
}
static void p_bells(void)                           /* SOFT BELLS: Gmaj7 Em7 Cmaj7 D, a tune on top */
{
    static const uint8_t C[4][3] = {{55, 59, 66}, {52, 55, 62}, {48, 55, 59}, {50, 54, 57}};
    static const uint8_t TUNE[][2] = {{74, 4}, {71, 4}, {74, 4}, {78, 4}, {79, 8}, {76, 8}, {76, 4}, {74, 4}, {71, 8},
                                      {69, 8}, {71, 4}, {74, 4}, {78, 4}, {79, 4}, {78, 8}, {74, 8}, {71, 8}, {69, 4}, {71, 4},
                                      {74, 8}, {66, 8}, {67, 8}};
    uint32_t b;
    prog("SOFT BELLS", 80);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 3, 16, 80);
    line(0, TUNE, NELEM(TUNE), 100);
}
static void p_acid(void)                            /* ACID LINE: a sixteenth line, the cutoff opening and closing */
{
    static const int8_t PAT[16] = {0, 0, 12, 0, 3, 0, 12, 10, 0, 0, 7, 0, 12, 3, 5, 7};
    static const uint8_t SLIDE[16] = {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1}, ROOT[2] = {45, 41};
    uint32_t b, i;
    prog("ACID LINE", 124);
    cut_from = 18; cut_to = 92;
    for (b = 0; b < 8u; b++)
        for (i = 0; i < 16u; i++) {
            uint32_t n = (uint32_t)(ROOT[(b / 2u) % 2u] + PAT[i]);
            if (SLIDE[i]) tie(b, i, n, 1, 100);
            else nt(b, i, n, 1, i % 4u ? 88 : 110);
        }
}

/* ---- 1.2: the programs made with SUB and DRIFT ---- */
static void p_subbass(void)                         /* SUB BASS: a line in E minor, an octave below it the sub */
{
    static const uint8_t L[][2] = {{40, 3}, {40, 1}, {0, 2}, {52, 2}, {40, 2}, {43, 3}, {45, 3}, {0, 2},
                                   {40, 3}, {40, 1}, {0, 2}, {50, 2}, {38, 2}, {47, 2}, {45, 2}, {43, 2}};
    uint32_t b;
    prog("SUB BASS", 98);
    for (b = 0; b < 8u; b += 2u)
        line(b, L, NELEM(L), 104);
}
static void p_reese(void)                           /* REESE: long gliding notes, two saws beating */
{
    static const uint8_t L[][2] = {{40, 16}, {43, 12}, {38, 4}, {36, 16}, {38, 8}, {35, 8}};
    uint32_t b;
    prog("REESE", 86);
    for (b = 0; b < 8u; b += 4u) {
        uint32_t i, six = 0;
        for (i = 0; i < NELEM(L); i++) {
            tie(b + six / 16u, six % 16u, L[i][0], L[i][1], 100);
            six += L[i][1];
        }
    }
}
static void p_polych(void)                          /* POLY CHORUS: Fmaj7 G6 Em7 Am9 in eighths, the push before the bar */
{
    static const uint8_t C[4][4] = {{53, 57, 60, 64}, {55, 59, 62, 64}, {52, 55, 59, 62}, {45, 55, 59, 60}};
    static const uint8_t HIT[6][2] = {{0, 3}, {3, 3}, {6, 2}, {8, 3}, {11, 3}, {14, 2}};
    uint32_t b, i;
    prog("POLY CHORUS", 104);
    for (b = 0; b < 8u; b++)
        for (i = 0; i < 6u; i++)
            chord(b, HIT[i][0], C[b % 4u], 4, HIT[i][1], i % 3u ? 88 : 104);
}
static void p_strings(void)                         /* OLD STRINGS: Cmaj7 Am7 Fmaj7 G6, slow */
{
    static const uint8_t C[4][4] = {{48, 55, 59, 64}, {45, 52, 55, 60}, {41, 52, 57, 60}, {43, 52, 55, 62}};
    uint32_t b;
    prog("OLD STRINGS", 68);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 4, 16, 92);
}
static void p_tape(void)                            /* WORN TAPE: Dm9 Bbmaj7 F/A Gm9, soft keys off a warped tape */
{
    static const uint8_t C[4][4] = {{50, 57, 60, 64}, {46, 53, 57, 62}, {45, 53, 57, 60}, {43, 50, 57, 58}};
    uint32_t b;
    prog("WORN TAPE", 76);
    for (b = 0; b < 8u; b++) {
        chord(b, 0, C[b % 4u], 4, 6, 88);
        chord(b, 6, C[b % 4u] + 1, 3, 4, 70);
        chord(b, 10, C[b % 4u], 4, 6, 80);
    }
}
static void p_driftlead(void)                       /* DRIFT LEAD: a tune in A minor, never quite in tune */
{
    static const uint8_t T[][2] = {{69, 6}, {72, 2}, {76, 8}, {74, 4}, {72, 4}, {71, 8}, {67, 6}, {71, 2}, {74, 8},
                                   {72, 4}, {71, 4}, {69, 16}, {0, 4}, {76, 4}, {79, 4}, {81, 4}, {79, 6}, {76, 2},
                                   {74, 8}, {72, 4}, {74, 4}, {76, 16}};
    uint32_t i, six = 0;
    prog("DRIFT LEAD", 96);
    for (i = 0; i < NELEM(T); i++) {
        if (T[i][0])
            tie(six / 16u, six % 16u, T[i][0], T[i][1], 100);
        six += T[i][1];
    }
}
static void p_subarp(void)                          /* SUB ARP: DOUBLE (each note twice, then its octave) on Am F C G */
{
    static const uint8_t C[4][3] = {{45, 48, 52}, {41, 45, 48}, {43, 48, 52}, {43, 47, 50}};
    uint32_t b;
    prog("SUB ARP", 112);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 3, 16, 100);
}
static void p_aged(void)                            /* AGED PAD: Ebmaj9 Cm9 Abmaj7 Bb6, DRIFT all the way */
{
    static const uint8_t C[4][4] = {{51, 58, 62, 65}, {48, 55, 58, 62}, {44, 51, 55, 60}, {46, 53, 55, 62}};
    uint32_t b;
    prog("AGED PAD", 64);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 4, 16, 90);
}
static void p_tape_dry(void)                        /* (WORN TAPE without DRIFT, to compare) */
{
    setp(GL_DRIFT, 0);
    p_tape();
}
static void p_bass_nosub(void)                      /* (SUB BASS without its SUB, to compare) */
{
    setp(GL_SUB, 0);
    p_subbass();
}

/* ---- 1.3 ---- */
static void p_supersaw(void)                        /* SUPERSAW: a trance lead, F#m D A E */
{
    static const uint8_t T[][2] = {{66, 3}, {69, 3}, {73, 2}, {74, 4}, {73, 2}, {71, 2}, {69, 3}, {71, 3}, {73, 2},
                                   {76, 8}, {74, 3}, {73, 3}, {71, 2}, {69, 4}, {68, 4}, {69, 16}, {0, 0}};
    uint32_t i, six = 0, r;
    prog("SUPERSAW", 136);
    for (r = 0; r < 2u; r++)
        for (i = 0; T[i][1]; i++) {
            if (T[i][0])
                tie(six / 16u, six % 16u, T[i][0], T[i][1], 104);
            six += T[i][1];
        }
}
static void p_morph(void)                           /* MORPH: BRIGHT ARP with OSC 1's MORPH, LFO 1 sweeping it */
{
    static const uint8_t C[4][3] = {{45, 48, 52}, {41, 45, 48}, {48, 52, 55}, {43, 47, 50}};
    uint32_t b;
    setp(GL_O1WAVE, 4);
    setp(GL_O1C1, 64);
    setp(GL_O1C2, 60);
    setp(GL_L1FREQ, 30);
    setp(GL_L1KEY, 0);
    prog("BRIGHT ARP", 116);
    for (b = 0; b < 8u; b++)
        chord(b, 0, C[b % 4u], 3, 16, 100);
}
static void p_wheel(void)                           /* the mod wheel: WARM LEAD, the wheel up in the middle */
{
    static const uint8_t T[][2] = {{64, 4}, {67, 4}, {69, 8}, {72, 4}, {71, 4}, {67, 8}, {69, 16}, {64, 4}, {67, 4},
                                   {69, 8}, {74, 4}, {72, 4}, {71, 8}, {69, 16}, {0, 0}};
    uint32_t i, six = 0;
    prog("WARM LEAD", 90);
    wheel_sweep = 1;
    for (i = 0; T[i][1]; i++) {
        tie(six / 16u, six % 16u, T[i][0], T[i][1], 100);
        six += T[i][1];
    }
}
static void p_chords(void)                          /* CHORD DIA7 in A minor: one note a time, sevenths of the scale */
{
    static const uint8_t R[8] = {57, 53, 60, 55, 57, 50, 52, 52};
    uint32_t b;
    setp(P_QUANT, 1);
    setp(P_ROOT, 9);
    setp(P_SCALE, 2);
    setp(P_CHRD, 2);
    setp(P_VOIC, 1);
    prog("POLY CHORUS", 96);
    for (b = 0; b < 8u; b++) {
        nt(b, 0, R[b], 6, 100);
        nt(b, 6, R[b], 4, 84);
        nt(b, 10, R[b], 6, 92);
    }
}

typedef struct { const char *file, *title, *text; void (*score)(void); int new; } piece_t;
static const piece_t PIECES[] = {                   /* (1.2: four of 1.1's and four new) */
    {"1_subbass", "Sub Bass", "A saw over the new SUB oscillator, a square an octave down: a line in E minor.", p_subbass, 1},
    {"2_worntape", "Worn Tape", "DRIFT high: soft keys off a warped tape, every note its own way out of tune. Dm9, Bbmaj7, F/A, Gm9.", p_tape, 1},
    {"3_sync", "Sync Pad", "Oscillator sync swept by an envelope, four voices wide: Dmaj7, Bm9, Gmaj7, Aadd9.", p_sync, 0},
    {"4_polychorus", "Poly Chorus", "A pulse with PWM on a sub square, a lush chorus, a little DRIFT: Fmaj7, G6, Em7, Am9.", p_polych, 1},
    {"5_arp", "Bright Arp", "The arpeggiator on held chords (Cm, Ab, Eb, Bb), in time with the tempo, the delay in time too.", p_arp, 0},
    {"6_agedpad", "Aged Pad", "DRIFT all the way, a breath of noise, a sine two octaves down: Ebmaj9, Cm9, Abmaj7, Bb6.", p_aged, 1},
    {"7_brass", "80s Brass", "Stabs of the brass patch on Eb, Bb, Cm, Ab: filter envelope on every hit.", p_brass, 0},
    {"8_acid", "Acid Sweep", "The squelchy line, with the cutoff opening and closing as the ALGORITHM knob does.", p_acid, 0},
};

static const piece_t PIECES12[] = {                 /* (render_sounds OUT 1.2: the 1.2 programs, two comparisons) */
    {"n1_subbass", "Sub Bass", "SUB BASS", p_subbass},
    {"n1b_subbass_nosub", "Sub Bass, no SUB", "SUB BASS with SUB LEVEL 0", p_bass_nosub},
    {"n2_reese", "Reese", "REESE", p_reese},
    {"n3_polychorus", "Poly Chorus", "POLY CHORUS", p_polych},
    {"n4_oldstrings", "Old Strings", "OLD STRINGS", p_strings},
    {"n5_worntape", "Worn Tape", "WORN TAPE", p_tape},
    {"n5b_worntape_nodrift", "Worn Tape, no DRIFT", "WORN TAPE with DRIFT 0", p_tape_dry},
    {"n6_driftlead", "Drift Lead", "DRIFT LEAD", p_driftlead},
    {"n7_subarp", "Sub Arp", "SUB ARP", p_subarp},
    {"n8_agedpad", "Aged Pad", "AGED PAD", p_aged},
};

static const piece_t PIECES13[] = {                 /* (render_sounds OUT 1.3) */
    {"m1_supersaw", "Supersaw", "SUPERSAW", p_supersaw},
    {"m2_morph", "Morph", "BRIGHT ARP with OSC 1 MORPH, LFO 1 on it", p_morph},
    {"m3_wheel", "Mod wheel", "WARM LEAD, the mod wheel up in the middle", p_wheel},
    {"m4_chords", "Chord + scale", "POLY CHORUS, CHORD DIA7 in A minor, single notes", p_chords},
};

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int v13 = argc > 2 && !strcmp(argv[2], "1.3");
    const piece_t *pieces = v13 ? PIECES13 : argc > 2 ? PIECES12 : PIECES;
    uint32_t npieces = v13 ? NELEM(PIECES13) : argc > 2 ? NELEM(PIECES12) : NELEM(PIECES);
    char p[512];
    uint32_t n, i, a, b;
    FILE *js;
    snprintf(p, sizeof p, "%s/sounds.json", dir);
    js = fopen(p, "w");
    if (!js)
        return 1;
    fprintf(js, "[");
    for (n = 0; n < npieces; n++) {
        const piece_t *pc = &pieces[n];
        nev = 0;
        cut_from = -1;
        nset = 0;
        wheel_sweep = 0;
        pc->score();
        for (a = 1; a < nev; a++)                   /* in time (stable: an off before an on at the same time) */
            for (b = a; b > 0 && ev[b - 1u].t > ev[b].t; b--) {
                ev_t x = ev[b]; ev[b] = ev[b - 1u]; ev[b - 1u] = x;
            }
        total = nev ? ev[nev - 1u].t / 2u : 1u;
        nsamp = 0;
        w = 0;
        {
            int16_t keep = cut_from;
            cut_from = -1;
            run_ms(80);
            cut_from = keep;
        }
        nsamp = 0;
        snprintf(p, sizeof p, "%s/%s.wav", dir, pc->file);
        w = fopen(p, "wb");
        frames = 0;
        wav_hdr(w, 0);
        for (i = 0; i < nev; i++) {
            uint32_t t = ev[i].t / 2u;
            if (t > cur_ms())
                run_ms(t - cur_ms());
            midi_note(ev[i].on, ev[i].note, ev[i].vel);
        }
        cut_from = -1;
        run_ms(4000);                                /* the tails */
        fseek(w, 0, SEEK_SET);
        wav_hdr(w, frames);
        fclose(w);
        w = 0;
        fprintf(js, "%s\n {\"file\": \"%s.mp3\", \"title\": \"%s\", \"text\": \"%s\", \"new\": %s, \"seconds\": %u, \"parts\": [\"GOLANA %s\"]}",
                n ? "," : "", pc->file, pc->title, pc->text, pc->new ? "true" : "false", frames / FS, prog_name);
    }
    fprintf(js, "\n]\n");
    fclose(js);
    return 0;
}
