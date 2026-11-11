/* Block test (PC): blocks made on each song's tracks, moved, repeated,
 * removed; events follow their blocks; the song written as .XWS keeps the
 * blocks.
 *   test_blocks <song>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"
#include "../src/blocks.h"
#include "../src/xws.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL %s: %s\n", name, msg); fails++; } } while (0)

static uint32_t count_in(const EdTrack *t, uint32_t a, uint32_t b)
{
    uint32_t i, n = 0;
    for (i = 0; i < t->n; i++) n += t->ev[i].tick >= a && t->ev[i].tick < b;
    return n;
}

static void one(const char *name)
{
    FILE *f = fopen(name, "rb");
    long n;
    uint8_t *b, *mid = NULL, *x;
    size_t mn, xn;
    EdSong s, r;
    uint32_t t, bar, before, total0, total1, k;
    int f0 = fails;
    if (!f) return;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); return; }
    fclose(f);
    if (xws_is(b, (size_t)n)) {
        mid = xws_to_smf(b, (size_t)n, &mn);
        ed_load(&s, mid, mn);
        CHECK(xws_attach_blocks(b, (size_t)n, &s) == (int)s.ntrk, "blocks of every track");
    } else ed_load(&s, b, (size_t)n);
    bar = s.ppqn * 4;
    /* a track with events */
    for (t = 0; t < s.ntrk && s.trk[t].n < 20; t++) {}
    if (t == s.ntrk) { ed_free(&s); free(b); return; }
    for (total0 = k = 0; k < s.ntrk; k++) total0 += s.trk[k].n;
    bk_materialize(&s, t, bar);
    CHECK(s.trk[t].nblk >= 1, "a block at least");
    /* a block from bar 3 to 5: it owns the events from bar 3 on */
    before = count_in(&s.trk[t], 2 * bar, 0xFFFFFFFFUL);
    CHECK(bk_create(&s, t, 2 * bar, 4 * bar, "Verse", bar), "create");
    {
        uint32_t bi;
        for (bi = 0; bi < s.trk[t].nblk && strcmp(s.trk[t].blk[bi].name, "Verse"); bi++) {}
        CHECK(bi < s.trk[t].nblk, "the new block is there");
        /* move it 2 bars later: its events move with it */
        if (bi < s.trk[t].nblk) {
            long nb = bk_move(&s, t, bi, 2 * (long)bar, bar);
            CHECK(nb >= 0 && s.trk[t].blk[nb].start == 4 * bar, "moved to bar 5");
            CHECK(count_in(&s.trk[t], 4 * bar, 0xFFFFFFFFUL) == before, "its events moved too");
            /* repeat everything of bars 5..6 three times after it */
            k = (uint32_t)bk_repeat(&s, 4 * bar, s.trk[t].blk[nb].end, s.trk[t].blk[nb].end, 3, bar);
            CHECK(k >= 3, "repeated");
            for (total1 = k = 0; k < s.ntrk; k++) total1 += s.trk[k].n;
            CHECK(total1 > total0, "more events after the repeat");
            bk_rename(&s, t, (uint32_t)nb, "Chorus");
            CHECK(!strcmp(s.trk[t].blk[nb].name, "Chorus"), "renamed");
        }
    }
    /* as .XWS and back: the same blocks (start, name) on that track */
    x = xws_write(&s, &xn);
    CHECK(x != NULL, "xws written");
    if (x) {
        size_t rn;
        uint8_t *rm = xws_to_smf(x, xn, &rn);
        uint32_t i;
        int same;
        CHECK(rm && ed_load(&r, rm, rn), "xws read back");
        xws_attach_blocks(x, xn, &r);
        same = r.ntrk == s.ntrk && r.trk[t].nblk == s.trk[t].nblk;
        for (i = 0; same && i < s.trk[t].nblk; i++)
            same = r.trk[t].blk[i].start == s.trk[t].blk[i].start && !strcmp(r.trk[t].blk[i].name, s.trk[t].blk[i].name);
        CHECK(same, "the blocks back from the .XWS");
        for (total1 = k = 0; k < r.ntrk; k++) total1 += r.trk[k].n;
        ed_free(&r);
        free(rm);
        free(x);
    }
    /* remove the first block: its events go */
    {
        uint32_t own = 0, i;
        for (i = 0; i < s.trk[t].n; i++) own += bk_owner(&s.trk[t], s.trk[t].ev[i].tick) == 0;
        before = s.trk[t].n;
        bk_remove(&s, t, 0);
        CHECK(s.trk[t].n == before - own, "removed with its events");
    }
    printf("%s %s: track %lu, %lu blocks\n", fails > f0 ? "FAIL" : "OK  ", name, (unsigned long)t + 1, (unsigned long)s.trk[t].nblk);
    ed_free(&s);
    free(mid);
    free(b);
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) one(argv[i]);
    return fails ? 1 : 0;
}
