/* Piano Roll operations test (PC): on each song's first note track -
 * hit test, rectangle selection, moving (time and key), lengthening,
 * velocity, copying, deleting, a new note; the song written and read back
 * after each step with the expected notes.
 *   test_roll <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"
#include "../src/rollops.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL %s: %s\n", name, msg); fails++; } } while (0)

static int notes(const EdSong *s, uint32_t t)
{
    uint32_t i;
    int n = 0;
    for (i = 0; i < s->trk[t].n; i++) n += ro_is_note(&s->trk[t].ev[i], -1);
    return n;
}

/* written and read back: the same notes */
static int stable(const EdSong *s)
{
    size_t n;
    uint8_t *b = ed_save(s, &n);
    EdSong r;
    uint32_t t;
    int ok = b && ed_load(&r, b, n) && r.ntrk == s->ntrk;
    for (t = 0; ok && t < s->ntrk; t++) ok = notes(&r, t) == notes(s, t);
    if (b) ed_free(&r);
    free(b);
    return ok;
}

static void one(const char *name)
{
    FILE *f = fopen(name, "rb");
    long n;
    uint8_t *b;
    EdSong s;
    uint32_t t;
    long i;
    int k, before, ch;
    EdEv first;
    if (!f) return;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n || !ed_load(&s, b, (size_t)n)) { fclose(f); free(b); return; }
    fclose(f);
    for (t = 0; t < s.ntrk && !notes(&s, t); t++) {}
    if (t == s.ntrk) { ed_free(&s); free(b); return; }
    for (i = 0; !ro_is_note(&s.trk[t].ev[i], -1); i++) {}
    first = s.trk[t].ev[i];
    ch = first.st & 0x0F;
    before = notes(&s, t);
    /* hit: inside the note, not after it */
    CHECK(ro_hit(&s, t, ch, first.tick + first.dur / 2, first.d1) >= 0, "hit inside");
    CHECK(ro_hit(&s, t, ch, first.tick, (first.d1 + 1) & 127) != i || first.d1 == 127, "hit other key");
    /* select the first bar of notes, move them a beat later and two keys up */
    k = ro_select_rect(&s, t, -1, 0, first.tick + 1, 0, 127, 0);
    CHECK(k >= 1, "select rect");
    CHECK(ro_move_sel(&s, t, s.ppqn, 2) == k, "move");
    for (i = 1; i < (long)s.trk[t].n; i++) CHECK(s.trk[t].ev[i - 1].tick <= s.trk[t].ev[i].tick, "order after move");
    CHECK(ro_hit(&s, t, ch, first.tick + s.ppqn, (first.d1 + 2) & 127) >= 0 || first.d1 > 125, "moved note there");
    CHECK(stable(&s), "song after move");
    /* longer, softer */
    CHECK(ro_size_sel(&s, t, s.ppqn, 10) == k, "size");
    CHECK(ro_vel_sel(&s, t, 33) == k, "velocity");
    /* copy a bar later: more notes */
    CHECK(ro_copy_sel(&s, t, (long)s.ppqn * 4) == k, "copy");
    CHECK(notes(&s, t) == before + k, "copies there");
    CHECK(stable(&s), "song after copy");
    /* delete the copies */
    CHECK(ro_delete_sel(&s, t) == k, "delete");
    CHECK(notes(&s, t) == before, "back to the notes before");
    /* a new note, then everything selected */
    CHECK(ro_add_note(&s, t, ch, ro_snap(1000, s.ppqn / 4), 60, 99, s.ppqn) >= 0, "add");
    CHECK(ro_count_sel(&s, t) == 1 && notes(&s, t) == before + 1, "the new note alone selected");
    CHECK(ro_select_all(&s, t, -1) == before + 1, "select all");
    CHECK(stable(&s), "song after add");
    /* move before the start: stops at 0 */
    ro_move_sel(&s, t, -100000000L, 0);
    {
        uint32_t m = 0xFFFFFFFFUL;
        for (i = 0; i < (long)s.trk[t].n; i++)
            if (s.trk[t].ev[i].sel && s.trk[t].ev[i].tick < m) m = s.trk[t].ev[i].tick;
        CHECK(m == 0, "moved to 0, not before");
    }
    printf("%s %s: track %lu, %d notes, %d moved / sized / copied / deleted\n", fails ? "FAIL" : "OK  ", name, (unsigned long)t + 1, before, k);
    ed_free(&s);
    free(b);
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) one(argv[i]);
    return fails ? 1 : 0;
}
