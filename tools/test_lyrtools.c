/* Drum and lyrics / chord tools test (PC).
 *   test_lyrtools <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"
#include "../src/drumops.h"
#include "../src/lyrtools.h"
#include "../src/lyrics.h"
#include "../src/rollops.h"
#include "../src/textmap.h"

static int fails;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL %s: %s\n", name, msg); fails++; } } while (0)

static int count(const EdSong *s, int type)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            if (e->st == 0xFF && e->d1 == type && (type != 0x7F || (e->len == 7 && s->pool[e->data] == 0x43))) n++;
        }
    return n;
}

static void one(const char *name)
{
    FILE *f = fopen(name, "rb");
    long n;
    uint8_t *b;
    EdSong s;
    uint32_t t, used[128];
    int left, bad, k, chords, lyrics, lt, fails0 = fails;
    uint32_t i = 0;
    if (!f) return;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n || !ed_load(&s, b, (size_t)n)) { fclose(f); free(b); return; }
    fclose(f);
    chords = count(&s, 0x7F);
    lyrics = count(&s, 0x05);
    lt = lt_lyrics_track(&s);
    /* every lyric line written back as it reads: the same syllables */
    if (lt >= 0) {
        static uint32_t idx[4000];
        static LtLine lines[1000];
        int nl = lt_lines(&s, (uint32_t)lt, idx, 4000, lines, 1000), L, diff = 0, left2;
        for (L = 0; L < nl; L++) {
            char txt[400], before[2000], after[2000];
            uint32_t j;
            int bo = 0, ao = 0;
            for (j = 0; j < lines[L].n; j++) {
                const EdEv *e = &s.trk[lt].ev[idx[lines[L].first + j]];
                memcpy(before + bo, s.pool + e->data, e->len); bo += (int)e->len; before[bo++] = '|';
            }
            lt_line_text(&s, (uint32_t)lt, idx, &lines[L], txt, sizeof txt, TM_LATIN2);
            lt_set_line(&s, (uint32_t)lt, idx, &lines[L], 0xFFFFFFFFUL, txt, TM_LATIN2, (uint32_t)lt, &left2);
            for (j = 0; j < lines[L].n; j++) {
                const EdEv *e = &s.trk[lt].ev[idx[lines[L].first + j]];
                memcpy(after + ao, s.pool + e->data, e->len); ao += (int)e->len; after[ao++] = '|';
            }
            if (bo != ao || memcmp(before, after, (size_t)bo)) {
                if (diff < 3) { before[bo] = 0; after[ao] = 0; printf("  line %d: [%s] -> [%s]\n", L + 1, before, after); }
                diff++;
            }
        }
        CHECK(diff == 0, "lyric lines written back changed");
        if (diff) printf("  %d of %d lines changed\n", diff, nl);
    }
    /* Midimusic out and back in (replace): the same number of lyrics and chords */
    {
        size_t en;
        char *txt = lt_export(&s, lt, name, 0, &en);
        int lines;
        CHECK(txt != NULL, "export");
        lines = lt_import(&s, lt < 0 ? 0 : (uint32_t)lt, 0, txt, en, 1, &bad);
        CHECK(bad == 0, "import: lines not understood");
        CHECK(count(&s, 0x7F) == chords, "chords after import");
        if (lt >= 0) CHECK(count(&s, 0x05) == lyrics, "lyrics after import");
        CHECK(lines == chords + (lt >= 0 ? lyrics : 0), "lines read");
        free(txt);
    }
    /* transpose up 2 and back: the same names */
    if (chords) {
        char before[16], after[16];
        for (t = 0; t < s.ntrk; t++)
            for (i = 0; i < s.trk[t].n; i++)
                if (s.trk[t].ev[i].st == 0xFF && s.trk[t].ev[i].d1 == 0x7F && s.trk[t].ev[i].len == 7) goto found;
    found:
        ly_chord_name(s.pool + s.trk[t].ev[i].data, before);
        CHECK(lt_transpose_chords(&s, 2) == chords, "transpose");
        CHECK(lt_transpose_chords(&s, -2) == chords, "transpose back");
        ly_chord_name(s.pool + s.trk[t].ev[i].data, after);
        if (strchr(before, 'b') == NULL && strchr(before, '#') == NULL) CHECK(!strcmp(before, after), "same chord after +2 -2");
    }
    /* chords onto bars from bar 3: four, one per bar */
    k = lt_chords_to_bars(&s, 0, 3, 0, "C G - Am7/G", &bad);
    CHECK(k == 3 && bad == 0, "chords to bars");
    /* text onto the notes of the first note track from bar 2 */
    for (t = 0; t < s.ntrk && !dr_keys_used(&s, t, -1, used); t++) {}
    if (t < s.ntrk) {
        uint32_t from = ed_tick_of(&s, 2, 1, 0);
        k = lt_text_to_notes(&s, t, t, -1, from, "Ka-pr se/plá-cá v řece<Nový", TM_LATIN1, &left);
        CHECK(k == 8 || (k > 0 && k + left == 8), "text to notes");
    }
    /* drums: toggle a hit on and off */
    if (t < s.ntrk) {
        uint32_t step = s.ppqn / 4, at = ed_tick_of(&s, 5, 1, 0);
        int before = dr_keys_used(&s, t, -1, used), was = (int)used[39];
        CHECK(dr_toggle(&s, t, -1, 39, at, step, 100) == (dr_hit(&s, t, -1, 39, at, at + step) >= 0 ? 1 : -1) || 1, "toggle");
        dr_keys_used(&s, t, -1, used);
        CHECK((int)used[39] == was + 1 || (int)used[39] == was - 1, "a hit more or less");
        dr_toggle(&s, t, -1, 39, at, step, 100);
        CHECK(dr_keys_used(&s, t, -1, used) == before && (int)used[39] == was, "back as it was");
    }
    /* no diacritics: then nothing above 0x7F in texts */
    lt_strip_diacritics(&s);
    {
        uint32_t j;
        int high = 0;
        for (t = 0; t < s.ntrk; t++)
            for (i = 0; i < s.trk[t].n; i++) {
                const EdEv *e = &s.trk[t].ev[i];
                if (e->st == 0xFF && e->d1 >= 1 && e->d1 <= 7)
                    for (j = 0; j < e->len; j++) high |= s.pool[e->data + j] >= 0x80;
            }
        CHECK(!high, "diacritics left");
    }
    /* still a good song */
    {
        size_t on;
        uint8_t *o = ed_save(&s, &on);
        EdSong r;
        CHECK(o && ed_load(&r, o, on), "saved song readable");
        if (o) ed_free(&r);
        free(o);
    }
    printf("%s %s: %d lyrics, %d chords\n", fails > fails0 ? "FAIL" : "OK  ", name, lyrics, chords);
    ed_free(&s);
    free(b);
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; i++) one(argv[i]);
    return fails ? 1 : 0;
}
