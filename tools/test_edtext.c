/* Event text test (PC): every event of each song as a line, read back from
 * that line; the event must be the same (meta / SysEx data compared). Also
 * a few typed lines. Prints the first lines of the first track.
 *   test_edtext <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"
#include "../src/edtext.h"
#include "../src/textmap.h"

static int same(const EdSong *s, const EdEv *a, const EdEv *b)
{
    if (a->tick != b->tick || a->st != b->st) return 0;
    if (a->st == 0xFF || a->st == 0xF0 || a->st == 0xF7)
        return (a->st != 0xFF || a->d1 == b->d1) && a->len == b->len && !memcmp(s->pool + a->data, s->pool + b->data, a->len);
    if ((a->st & 0xF0) == 0x90 && a->d2) return a->d1 == b->d1 && a->d2 == b->d2 && a->dur == b->dur;
    return a->d1 == b->d1 && ((a->st & 0xF0) == 0xC0 || (a->st & 0xF0) == 0xD0 || a->d2 == b->d2);
}

int main(int argc, char **argv)
{
    int i, bad = 0;
    for (i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        long n;
        uint8_t *b;
        EdSong s;
        uint32_t t, k, total = 0, fails = 0, shown = 0;
        char line[300], err[120];
        if (!f) continue;
        fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
        b = (uint8_t *)malloc((size_t)n);
        if (fread(b, 1, (size_t)n, f) != (size_t)n || !ed_load(&s, b, (size_t)n)) { fclose(f); continue; }
        fclose(f);
        for (t = 0; t < s.ntrk; t++)
            for (k = 0; k < s.trk[t].n; k++) {
                const EdEv *e = &s.trk[t].ev[k];
                EdEv back;
                /* texts with letters only a 1250 file has: the plain form loses them, so ISO-8859-2 */
                et_format(&s, e, line, sizeof line, TM_LATIN2);
                if (shown < 12 && t == (s.ntrk > 1 ? 1u : 0u)) { printf("  %s\n", line); shown++; }
                total++;
                if ((e->st & 0xF0) == 0x90 && !e->d2) continue;          /* shown as NoteOff, kept as it is */
                if (!et_parse(&s, line, &back, e->st < 0xF0 ? (e->st & 0x0F) : 0, TM_LATIN2, err, sizeof err) || !same(&s, e, &back)) {
                    if (fails < 5) printf("  FAIL %s\n    -> %s\n", line, err);
                    fails++;
                }
            }
        printf("%s %s: %lu events, %lu not read back\n", fails ? "FAIL" : "OK  ", argv[i], (unsigned long)total, (unsigned long)fails);
        bad += fails != 0;
        ed_free(&s);
        free(b);
    }
    {
        /* typed lines */
        EdSong s;
        EdEv e;
        char err[120];
        static const char *const ok[] = {"1:1:0 Note C3 100 480", "2:3:120 10 Note F#2", "3 Ctrl 7 90", "4:1:0 2 Prog 49", "5 Pitch -200",
                                         "6 SysEx F0 43 10 4C 00 00 7E 00", "7 Tempo 98.5", "8 TimeSig 3/4", "9 Lyric \"la \"", "10 Chord Am7/G", "11 Chord N.C."};
        static const char *const no[] = {"Note C3", "1:1:0 Nope 1", "1 Note H9", "1 Prog 200", "1 Chord X7"};
        unsigned j;
        memset(&s, 0, sizeof s);
        s.ppqn = 480;
        for (j = 0; j < sizeof ok / sizeof ok[0]; j++)
            if (!et_parse(&s, ok[j], &e, 0, TM_ASCII, err, sizeof err)) { printf("FAIL typed \"%s\": %s\n", ok[j], err); bad++; }
        for (j = 0; j < sizeof no / sizeof no[0]; j++)
            if (et_parse(&s, no[j], &e, 0, TM_ASCII, err, sizeof err)) { printf("FAIL typed \"%s\" was taken\n", no[j]); bad++; }
        printf("typed lines: done\n");
        free(s.pool);
    }
    return bad ? 1 : 0;
}
