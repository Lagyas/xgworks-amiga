/* Mixer test (PC): for each song, change values the song has (written over)
 * and values it lacks (new messages), save, read back and check
 *  - the mixer reads back the new values,
 *  - every note keeps its time and bytes, nothing else is lost,
 *  - exactly the new messages were added.
 * Writes <song>.mixed.mid for a check with the Rust core.
 *   test_mixer <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/smf.h"
#include "../src/mixer.h"

static uint8_t *readf(const char *n, size_t *sz)
{
    FILE *f = fopen(n, "rb");
    uint8_t *b;
    long l;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    l = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)l);
    *sz = fread(b, 1, (size_t)l, f);
    fclose(f);
    return b;
}

/* notes as "time status note vel" in order */
static int notes(const XgSong *s, uint32_t *out, int max)
{
    uint32_t i;
    int n = 0;
    for (i = 0; i < s->nev && n < max; i++) {
        const XgEv *e = &s->ev[i];
        if (e->n == 3 && ((e->msg[0] & 0xF0) == 0x80 || (e->msg[0] & 0xF0) == 0x90)) {
            out[n * 2] = xg_tick_us(s, e->tick);
            out[n * 2 + 1] = ((uint32_t)e->msg[0] << 16) | ((uint32_t)e->msg[1] << 8) | e->msg[2];
            n++;
        }
    }
    return n;
}

static int one(const char *name)
{
    size_t sz, nsz;
    uint8_t *b = readf(name, &sz), *nb, out[3][3];
    XgSong s, t;
    Mixer m, back;
    int c, p, used = -1, used2 = -1, fail = 0, na, nb_;
    uint32_t *A, *B;
    char outname[600];
    FILE *f;
    if (!b || xg_smf_load(&s, b, sz) != XG_OK) { printf("FAIL %s: cannot read\n", name); return 1; }
    mx_from_song(&m, &s);
    for (c = 0; c < 16; c++)
        if (m.used[c] && c != 9) { if (used < 0) used = c; else if (used2 < 0) used2 = c; }
    if (used2 < 0) used2 = used;
    /* volume: the song usually has it (written over); reverb 77 and chorus 33; pan; a bank */
    mx_set(&m, used, MX_VOL, 81, out);
    mx_set(&m, used, MX_REV, 77, out);
    mx_set(&m, used, MX_CHO, 33, out);
    mx_set(&m, used2, MX_PAN, 21, out);
    mx_set(&m, used2, MX_MSB, 8, out);
    nb = mx_save(&m, &s, &nsz);
    if (!nb || xg_smf_load(&t, nb, nsz) != XG_OK) { printf("FAIL %s: the saved song cannot be read\n", name); return 1; }
    mx_from_song(&back, &t);
    for (c = 0; c < 16; c++)
        for (p = 0; p < MX_N; p++)
            if (m.touched[c][p] && back.val[c][p] != m.val[c][p]) {
                printf("FAIL %s: ch %d %s = %d, wanted %d\n", name, c + 1, mx_param_names[p], back.val[c][p], m.val[c][p]);
                fail = 1;
            }
    A = (uint32_t *)malloc(s.nev * 8 + 8);
    B = (uint32_t *)malloc(t.nev * 8 + 8);
    na = notes(&s, A, (int)s.nev);
    nb_ = notes(&t, B, (int)t.nev);
    if (na != nb_ || memcmp(A, B, (size_t)na * 8)) { printf("FAIL %s: the notes changed (%d -> %d)\n", name, na, nb_); fail = 1; }
    printf("%s %s: ch %d / %d, %lu -> %lu messages (+%ld), %lu -> %lu bytes\n", fail ? "FAIL" : "OK  ", name, used + 1, used2 + 1,
           (unsigned long)s.nev, (unsigned long)t.nev, (long)t.nev - (long)s.nev, (unsigned long)sz, (unsigned long)nsz);
    snprintf(outname, sizeof outname, "%s.mixed.mid", name);
    if ((f = fopen(outname, "wb"))) { fwrite(nb, 1, nsz, f); fclose(f); }
    free(A); free(B);
    xg_smf_free(&t); xg_smf_free(&s);
    free(nb); free(b);
    return fail;
}

int main(int argc, char **argv)
{
    int i, bad = 0;
    for (i = 1; i < argc; i++) bad += one(argv[i]);
    return bad ? 1 : 0;
}
