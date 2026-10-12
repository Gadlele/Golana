/* SPDX-License-Identifier: GPL-3.0-only
 * Felucca on the M-Vave SMK-37 Pro (BOARD_SMK37) */
/* SMK-37 Pro store owner tag (storage.c st_owner_check, build with -DBOARD_SMK37=1): a simulated flash with
 * another firmware's store, an untagged one and Felucca's own; only FL_DATA objects, the voice bank and the tag
 * sector are ever erased, and ours is kept. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif
#include "fm1_flash_map.h"

static uint8_t nor[0x100000];
static int bad_write;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || !(FL_IN(off, 0x1000u, 0x9D000u, 0xAF000u) || off == 0xAF000u || off == 0xB0000u))
        bad_write++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off != 0xAF000u)
        bad_write++;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"

static int bad;
static void check(const char *what, int ok) { printf("smk owner: %-74s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

static void fill(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(nor + 0x9D000, 0x11, 0x12000);          /* objects */
    memset(nor + 0xB0000, 0x22, 0x1000);           /* voice bank */
    memset(nor + 0xC1000, 0x33, 0x1000);           /* USRTRIM: never touched */
    memset(nor + 0xC2000, 0x44, 0x1000);           /* a user sample */
}
static int store_erased(void)
{
    uint32_t i;
    for (i = 0x9D000; i < 0xAF000; i++) if (nor[i] != 0xFF) return 0;
    for (i = 0xB0000; i < 0xB1000; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}
static int others_kept(void) { return nor[0xC1000] == 0x33 && nor[0xC1FFF] == 0x33 && nor[0xC2000] == 0x44; }

int main(void)
{
    fill();
    memcpy(nor + 0xAF000, "SMK37DATASLOOP\0", 16);
    st_owner_check();
    check("another family's store (SLOOP): cleared, tagged FELUCCA", store_erased() && st_owner_cleared &&
          !memcmp(nor + 0xAF000, "SMK37DATAFELUCCA", 16));
    check("... calibration (USRTRIM) and samples untouched", others_kept());
    fill();
    memcpy(nor + 0xAF000, "SMK37DATAFELUCCA", 16);
    st_owner_cleared = 0;
    st_owner_check();
    check("our own store: kept as it is", nor[0x9D000] == 0x11 && nor[0xB0000] == 0x22 && !st_owner_cleared);
    fill();
    st_owner_check();
    check("an untagged store (FELUCCA_DATA_UNTAGGED_OK 0): cleared, tagged", store_erased() &&
          !memcmp(nor + 0xAF000, "SMK37DATAFELUCCA", 16) && others_kept());
    check("nothing written outside FL_DATA's objects, the voice bank and the tag", bad_write == 0);
    printf("%s\n", bad ? "SMK OWNER TEST FAILED" : "smk owner test passed");
    return bad != 0;
}
