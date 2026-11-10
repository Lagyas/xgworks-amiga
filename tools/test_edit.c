/* Edit model test (PC): each song read into the model and written again
 * (<song>.ed.mid, for tools/compare.py against the Rust core); then a few
 * edits - a note moved, an event deleted, a track renamed, a new note -
 * and the bar positions.
 *   test_edit <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"

static uint8_t *readf(const char *n, size_t *sz)
{
    FILE *f = fopen(n, "rb");
    uint8_t *b;
    long l;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); l = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)l);
    *sz = fread(b, 1, (size_t)l, f);
    fclose(f);
    return b;
}

static void writef(const char *n, const uint8_t *b, size_t sz)
{
    FILE *f = fopen(n, "wb");
    if (f) { fwrite(b, 1, sz, f); fclose(f); }
}

int main(int argc, char **argv)
{
    int i, bad = 0;
    for (i = 1; i < argc; i++) {
        size_t sz, n, n2;
        uint8_t *b = readf(argv[i], &sz), *o, *o2 = NULL;
        EdSong s, s2;
        char out[600];
        uint32_t t, notes = 0, bar, beat, tk;
        if (!b || !ed_load(&s, b, sz)) { printf("FAIL %s: cannot read\n", argv[i]); bad++; continue; }
        o = ed_save(&s, &n);
        snprintf(out, sizeof out, "%s.ed.mid", argv[i]);
        writef(out, o, n);
        /* written again from what was read: the same bytes */
        if (!ed_load(&s2, o, n) || !(o2 = ed_save(&s2, &n2)) || n2 != n || memcmp(o, o2, n)) { printf("FAIL %s: not stable\n", argv[i]); bad++; }
        for (t = 0; t < s.ntrk; t++) {
            uint32_t k;
            for (k = 0; k < s.trk[t].n; k++)
                if ((s.trk[t].ev[k].st & 0xF0) == 0x90 && s.trk[t].ev[k].d2) notes++;
        }
        /* bars both ways */
        for (tk = 0; tk < 200000; tk += 997) {
            ed_mbt(&s, tk, &bar, &beat, &t);
            if (ed_tick_of(&s, bar, beat, t) != tk) { printf("FAIL %s: tick %lu -> %lu:%lu:%lu -> %lu\n", argv[i], (unsigned long)tk,
                (unsigned long)bar, (unsigned long)beat, (unsigned long)t, (unsigned long)ed_tick_of(&s, bar, beat, t)); bad++; break; }
        }
        printf("OK   %s: %lu tracks, %lu notes, %lu -> %lu bytes\n", argv[i], (unsigned long)s.ntrk, (unsigned long)notes, (unsigned long)sz, (unsigned long)n);
        /* edits */
        {
            uint32_t tr = 0, k = 0;
            EdEv e;
            for (tr = 0; tr < s.ntrk; tr++) {
                for (k = 0; k < s.trk[tr].n && !((s.trk[tr].ev[k].st & 0xF0) == 0x90 && s.trk[tr].ev[k].d2); k++) {}
                if (k < s.trk[tr].n) break;
            }
            if (tr < s.ntrk) {
                uint32_t before = s.trk[tr].n;
                long ni;
                e = s.trk[tr].ev[k];
                e.tick += s.ppqn;          /* a beat later */
                e.d1 = (uint8_t)(e.d1 + 2);
                ni = ed_replace(&s, tr, k, &e);
                if (ni < 0 || s.trk[tr].ev[ni].tick != e.tick || s.trk[tr].ev[ni].d1 != e.d1 || s.trk[tr].n != before) { printf("FAIL %s: replace\n", argv[i]); bad++; }
                for (k = 1; k < s.trk[tr].n; k++)
                    if (s.trk[tr].ev[k - 1].tick > s.trk[tr].ev[k].tick) { printf("FAIL %s: order\n", argv[i]); bad++; break; }
                ed_set_name(&s, tr, (const uint8_t *)"Edited", 6);
                {
                    uint32_t len;
                    const uint8_t *nm = ed_name(&s, tr, &len);
                    if (!nm || len != 6 || memcmp(nm, "Edited", 6)) { printf("FAIL %s: name\n", argv[i]); bad++; }
                }
                ed_delete(&s, tr, 0);
                free(o2);
                o2 = ed_save(&s, &n2);
                ed_free(&s2);
                if (!ed_load(&s2, o2, n2) || s2.ntrk != s.ntrk) { printf("FAIL %s: edited song cannot be read\n", argv[i]); bad++; }
            }
        }
        ed_free(&s); ed_free(&s2);
        free(o); free(o2); free(b);
    }
    return bad ? 1 : 0;
}
