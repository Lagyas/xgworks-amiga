/* Lyrics and chord tools (see lyrtools.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lyrtools.h"
#include "lyrics.h"
#include "rollops.h"
#include "textmap.h"

static int is_lyric(const EdEv *e) { return e->st == 0xFF && e->d1 == 0x05; }

static int is_chord(const EdSong *s, const EdEv *e)
{
    const uint8_t *d = s->pool + e->data;
    return e->st == 0xFF && e->d1 == 0x7F && e->len == 7 && d[0] == 0x43 && d[1] == 0x7B && d[2] == 0x01;
}

int lt_lyrics_track(const EdSong *s)
{
    uint32_t t, i, best = 0;
    int bt = -1;
    for (t = 0; t < s->ntrk; t++) {
        uint32_t n = 0;
        for (i = 0; i < s->trk[t].n; i++) n += is_lyric(&s->trk[t].ev[i]);
        if (n > best) { best = n; bt = (int)t; }
    }
    return bt;
}

static int add_meta(EdSong *s, uint32_t trk, uint32_t tick, uint8_t type, const uint8_t *d, uint32_t n)
{
    EdEv e;
    memset(&e, 0, sizeof e);
    e.tick = tick;
    e.st = 0xFF;
    e.d1 = type;
    e.len = n;
    if ((e.data = ed_pool_add(s, d, n)) == 0xFFFFFFFFUL) return 0;
    return ed_insert(s, trk, &e) > 0;
}

/* Remove the events of a kind in [t0, t1] (all tracks when trk < 0). */
static void remove_range(EdSong *s, int trk, int chords, uint32_t t0, uint32_t t1)
{
    uint32_t t, i, o;
    for (t = 0; t < s->ntrk; t++) {
        EdTrack *tr = &s->trk[t];
        if (trk >= 0 && (uint32_t)trk != t) continue;
        for (i = o = 0; i < tr->n; i++) {
            const EdEv *e = &tr->ev[i];
            if (e->tick >= t0 && e->tick <= t1 && (chords ? is_chord(s, e) : is_lyric(e))) continue;
            tr->ev[o++] = tr->ev[i];
        }
        tr->n = o;
    }
}

/* ---- text onto notes ---- */

/* Text into syllables with their marks ("/" new line, "<" new page at the start, " " at a word end). */
static int split(const char *text, char syl[][64], int max)
{
    int nsyl = 0, new_line = 0, new_page = 0;
    const char *p = text;
    while (*p && nsyl < max) {
        char w[64];
        int n = 0;
        /* marks and spaces before a syllable */
        while (*p == ' ' || *p == '\t' || *p == '/' || *p == '<' || *p == '\r' || *p == '\n') {
            if (*p == '/') new_line = 1;
            else if (*p == '<') new_page = 1;
            else if (*p == '\n') {
                /* a line end; an empty line: a new page */
                const char *q = p + 1;
                while (*q == ' ' || *q == '\t' || *q == '\r') q++;
                if (*q == '\n') new_page = 1;
                new_line = 1;
            }
            p++;
        }
        if (!*p) break;
        if (new_page) w[n++] = '<';
        else if (new_line && nsyl) w[n++] = '/';
        new_line = new_page = 0;
        while (*p && *p != ' ' && *p != '\t' && *p != '-' && *p != '/' && *p != '<' && *p != '\r' && *p != '\n' && n < 58) {
            /* \- \/ \< are the characters themselves; \\ and \xHH stay for the conversion */
            if (*p == '\\' && (p[1] == '-' || p[1] == '/' || p[1] == '<')) { w[n++] = p[1]; p += 2; continue; }
            if (*p == '\\' && p[1] == '\\') { w[n++] = '\\'; w[n++] = '\\'; p += 2; continue; }
            if (*p == '\\' && p[1] == 'x' && p[2] && p[3]) { memcpy(w + n, p, 4); n += 4; p += 4; continue; }
            w[n++] = *p++;
        }
        if (*p == '-') p++;                                /* a syllable of the word: no space */
        else if (!*p || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '/' || *p == '<') w[n++] = ' ';
        w[n] = 0;
        strcpy(syl[nsyl++], w);
    }
    return nsyl;
}

int lt_text_to_notes(EdSong *s, uint32_t lyr_trk, uint32_t note_trk, int ch, uint32_t from, const char *text, int letters, int *left)
{
    static char syl[300][64];
    int nsyl, i, placed = 0;
    uint32_t *starts, nstarts = 0, k, last = 0;
    *left = 0;
    if (lyr_trk >= s->ntrk || note_trk >= s->ntrk) return 0;
    if (!(nsyl = split(text, syl, 300))) return 0;
    /* the note starts from `from` (a chord counts once) */
    if (!(starts = (uint32_t *)malloc((s->trk[note_trk].n + 1) * sizeof(uint32_t)))) return 0;
    for (k = 0; k < s->trk[note_trk].n; k++) {
        const EdEv *e = &s->trk[note_trk].ev[k];
        if (e->tick < from || !ro_is_note(e, ch)) continue;
        if (nstarts && starts[nstarts - 1] == e->tick) continue;
        starts[nstarts++] = e->tick;
    }
    for (i = 0; i < nsyl && (uint32_t)i < nstarts; i++) last = starts[i];
    /* the old lyrics there go */
    if (nstarts) remove_range(s, (int)lyr_trk, 0, from, last);
    for (i = 0; i < nsyl && (uint32_t)i < nstarts; i++) {
        uint8_t enc[80];
        size_t n = tm_to_cp1250_esc(syl[i], enc, sizeof enc, letters);
        if (!add_meta(s, lyr_trk, starts[i], 0x05, enc, (uint32_t)n)) break;
        placed++;
    }
    *left = nsyl - placed;
    free(starts);
    return placed;
}

/* ---- chords onto bars ---- */

int lt_chords_to_bars(EdSong *s, uint32_t trk, uint32_t bar, int step, const char *seq, int *bad)
{
    uint32_t t = ed_tick_of(s, bar ? bar : 1, 1, 0), t0 = t, tk[64], num[64], bl[64];
    int nsig, placed = 0;
    const char *p = seq;
    uint8_t d[7];
    struct { uint32_t t; uint8_t d[7]; } list[256];
    int nl = 0, i;
    *bad = 0;
    if (trk >= s->ntrk) return 0;
    nsig = ed_sigs(s, tk, num, bl, 64);
    while (*p && nl < 256) {
        char w[24];
        int n = 0, k;
        uint32_t len;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '|') p++;
        if (!*p) break;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && *p != '|' && n < 22) w[n++] = *p++;
        w[n] = 0;
        /* the step length where this chord goes */
        for (k = nsig - 1; k > 0 && tk[k] > t; k--) {}
        len = step == 0 ? num[k] * bl[k] : step == 1 ? num[k] * bl[k] / 2 : bl[k];
        if (strcmp(w, "-") && strcmp(w, ".")) {
            if (ly_chord_data(w, d)) { list[nl].t = t; memcpy(list[nl].d, d, 7); nl++; }
            else (*bad)++;
        }
        t += len ? len : 1;
    }
    if (!nl) return 0;
    remove_range(s, -1, 1, t0, t - 1);
    for (i = 0; i < nl; i++)
        if (add_meta(s, trk, list[i].t, 0x7F, list[i].d, 7)) placed++;
    return placed;
}

/* ---- transposing ---- */

static int pc_of(uint8_t b)
{
    static const int base[7] = {0, 2, 4, 5, 7, 9, 11};
    int n = b & 0x0F, f = (b >> 4) & 7;
    if (n < 1 || n > 7 || f > 6) return -1;
    return ((base[n - 1] + f - 3) % 12 + 12) % 12;
}

static uint8_t byte_of(int pc)
{
    /* spelled C C# D Eb E F F# G Ab A Bb B */
    static const uint8_t B[12] = {0x31, 0x41, 0x32, 0x23, 0x33, 0x34, 0x44, 0x35, 0x26, 0x36, 0x27, 0x37};
    return B[(pc % 12 + 12) % 12];
}

int lt_transpose_chords(EdSong *s, int semis)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            uint8_t d[7];
            int pc;
            if (!is_chord(s, e)) continue;
            memcpy(d, s->pool + e->data, 7);
            if (d[3] == 0x7F || (pc = pc_of(d[3])) < 0) continue;
            d[3] = byte_of(pc + semis);
            if (d[5] != 0x7F && (pc = pc_of(d[5])) >= 0) d[5] = byte_of(pc + semis);
            /* new data (the old may be shared by an undo copy) */
            {
                uint32_t at = ed_pool_add(s, d, 7);
                if (at == 0xFFFFFFFFUL) return n;
                s->trk[t].ev[i].data = at;
            }
            n++;
        }
    return n;
}

/* ---- Midimusic text ---- */

typedef struct { uint32_t t; int chord; uint32_t trk, i; } Row;

static int row_cmp(const void *a, const void *b)
{
    const Row *x = (const Row *)a, *y = (const Row *)b;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    if (x->chord != y->chord) return x->chord - y->chord;   /* lyrics first */
    return x->i < y->i ? -1 : (x->i > y->i);
}

char *lt_export(const EdSong *s, int lyr_trk, const char *file, int mode, size_t *outn)
{
    Row *rows;
    uint32_t t, i, nr = 0, total = 0;
    char *o;
    size_t n = 0, cap;
    for (t = 0; t < s->ntrk; t++) total += s->trk[t].n;
    if (!(rows = (Row *)malloc((total + 1) * sizeof(Row)))) return NULL;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            if (mode != 2 && (int)t == lyr_trk && is_lyric(e)) { rows[nr].t = e->tick; rows[nr].chord = 0; rows[nr].trk = t; rows[nr].i = i; nr++; }
            else if (mode != 1 && is_chord(s, e)) { rows[nr].t = e->tick; rows[nr].chord = 1; rows[nr].trk = t; rows[nr].i = i; nr++; }
        }
    qsort(rows, nr, sizeof(Row), row_cmp);
    cap = 256 + (size_t)nr * 80 + strlen(file);
    if (!(o = (char *)malloc(cap))) { free(rows); return NULL; }
    n += (size_t)sprintf(o + n, "Midimusic Chord and Lyrics Protocol\r\nFile:  %s\r\n\r\n[start chords+lyrics]\r\n", file);
    for (i = 0; i < nr; i++) {
        const EdEv *e = &s->trk[rows[i].trk].ev[rows[i].i];
        uint32_t bar, beat, tt, j;
        ed_mbt(s, e->tick, &bar, &beat, &tt);
        if (rows[i].chord) {
            char name[16];
            ly_chord_name(s->pool + e->data, name);
            n += (size_t)sprintf(o + n, "Chd\t%03lu:%02lu:%03lu\t%s\r\n", (unsigned long)bar, (unsigned long)beat, (unsigned long)tt, name);
        } else {
            n += (size_t)sprintf(o + n, "Lyr\t%03lu:%02lu:%03lu\t\"", (unsigned long)bar, (unsigned long)beat, (unsigned long)tt);
            for (j = 0; j < e->len && j < 60; j++) {
                uint8_t c = s->pool[e->data + j];
                o[n++] = (char)(c == '\r' || c == '\n' ? ' ' : c);
            }
            n += (size_t)sprintf(o + n, "\"\r\n");
        }
    }
    n += (size_t)sprintf(o + n, "[end list]\r\n");
    free(rows);
    *outn = n;
    return o;
}

int lt_import(EdSong *s, uint32_t lyr_trk, uint32_t chord_trk, const char *text, size_t n, int replace, int *bad)
{
    const char *p = text, *end = text + n;
    int lines = 0, cleared_l = 0, cleared_c = 0;
    *bad = 0;
    if (lyr_trk >= s->ntrk || chord_trk >= s->ntrk) return 0;
    while (p < end) {
        const char *e = p, *t1, *t2;
        char kind[4], body[200];
        unsigned long bar, beat, tick;
        size_t bl;
        while (e < end && *e != '\n') e++;
        t1 = memchr(p, '\t', (size_t)(e - p));
        t2 = t1 ? memchr(t1 + 1, '\t', (size_t)(e - t1 - 1)) : NULL;
        if (t1 && t2 && t1 - p == 3) {
            memcpy(kind, p, 3);
            kind[3] = 0;
            if ((!strcmp(kind, "Lyr") || !strcmp(kind, "Chd"))) {
                if (sscanf(t1 + 1, "%lu:%lu:%lu", &bar, &beat, &tick) != 3) { (*bad)++; p = e + 1; continue; }
                bl = (size_t)(e - t2 - 1);
                if (bl >= sizeof body) bl = sizeof body - 1;
                memcpy(body, t2 + 1, bl);
                body[bl] = 0;
                while (bl && (body[bl - 1] == '\r' || body[bl - 1] == ' ')) body[--bl] = 0;
                if (!strcmp(kind, "Lyr")) {
                    char *b = body;
                    if (replace && !cleared_l) { remove_range(s, (int)lyr_trk, 0, 0, 0xFFFFFFFFUL); cleared_l = 1; }
                    if (*b == '"' && bl >= 2 && body[bl - 1] == '"') { body[bl - 1] = 0; b++; }
                    add_meta(s, lyr_trk, ed_tick_of(s, (uint32_t)bar, (uint32_t)beat, (uint32_t)tick), 0x05, (const uint8_t *)b, (uint32_t)strlen(b));
                    lines++;
                } else {
                    uint8_t d[7];
                    if (!ly_chord_data(body, d)) { (*bad)++; p = e + 1; continue; }
                    if (replace && !cleared_c) { remove_range(s, -1, 1, 0, 0xFFFFFFFFUL); cleared_c = 1; }
                    add_meta(s, chord_trk, ed_tick_of(s, (uint32_t)bar, (uint32_t)beat, (uint32_t)tick), 0x7F, d, 7);
                    lines++;
                }
            }
        }
        p = e + 1;
    }
    return lines;
}

int lt_lines(const EdSong *s, uint32_t trk, uint32_t *idx, uint32_t maxidx, LtLine *lines, int maxlines)
{
    uint32_t i, n = 0;
    int nl = 0, brk = 1;
    if (trk >= s->ntrk) return 0;
    for (i = 0; i < s->trk[trk].n && n < maxidx; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        const uint8_t *d = s->pool + e->data;
        int new_line, end_line;
        if (!is_lyric(e) || !e->len) continue;
        new_line = d[0] == '/' || d[0] == '<' || d[0] == '\\';
        end_line = d[e->len - 1] == '\r' || d[e->len - 1] == '\n' || (e->len > 1 && d[e->len - 1] == '/');
        if ((new_line || brk) && (nl == 0 || lines[nl - 1].n)) {
            if (nl == maxlines) break;
            lines[nl].first = n;
            lines[nl].n = 0;
            nl++;
        }
        brk = end_line;
        idx[n++] = i;
        lines[nl - 1].n++;
    }
    return nl;
}

/* The text of a lyric event without its marks (line / page marks, line ends); its length. */
static uint32_t core(const EdSong *s, const EdEv *e, uint32_t *start)
{
    const uint8_t *d = s->pool + e->data;
    uint32_t a = 0, l = e->len;
    while (a < l && (d[a] == '/' || d[a] == '<' || d[a] == '\\')) a++;
    while (l > a && (d[l - 1] == '\r' || d[l - 1] == '\n' || d[l - 1] == '/')) l--;
    *start = a;
    return l - a;
}

/* The syllables of a line with text (the separators "\r" alone are not syllables). */
static int real_syllables(const EdSong *s, uint32_t trk, const uint32_t *idx, const LtLine *ln, uint32_t *out)
{
    uint32_t j, st;
    int n = 0;
    for (j = 0; j < ln->n; j++) {
        const EdEv *e = &s->trk[trk].ev[idx[ln->first + j]];
        uint32_t l = core(s, e, &st), k;
        int text = 0;
        for (k = 0; k < l; k++) text |= s->pool[e->data + st + k] != ' ';
        if (text && n < 400) out[n++] = idx[ln->first + j];   /* the callers have 400 places */
    }
    return n;
}

void lt_line_text(const EdSong *s, uint32_t trk, const uint32_t *idx, const LtLine *ln, char *out, int max, int letters)
{
    static uint32_t real[400];
    int n = real_syllables(s, trk, idx, ln, real), j, o = 0;
    out[0] = 0;
    for (j = 0; j < n && j < 400; j++) {
        const EdEv *e = &s->trk[trk].ev[real[j]];
        uint32_t st, l = core(s, e, &st);
        char conv[200];
        size_t cl = tm_from_cp1250_esc(s->pool + e->data + st, l, conv, sizeof conv, letters), q;
        if (o + (int)cl * 2 + 2 >= max) break;
        /* - / < in the text are written \- \/ \< (they are marks when typed) */
        for (q = 0; q < cl; q++) {
            if (conv[q] == '-' || conv[q] == '/' || conv[q] == '<') out[o++] = '\\';
            out[o++] = conv[q];
        }
        /* a syllable of a word goes on with "-" */
        if (j + 1 < n && cl && conv[cl - 1] != ' ') out[o++] = '-';
    }
    while (o && out[o - 1] == ' ') o--;
    out[o] = 0;
}

int lt_set_line(EdSong *s, uint32_t trk, const uint32_t *idx, const LtLine *ln, uint32_t next, const char *text, int letters, uint32_t note_trk, int *left)
{
    static char syl[200][64];
    static uint32_t real[400];
    int nsyl, nreal, i, placed = 0;
    char lead = 0;
    int tail = 0;
    uint32_t from, k, *starts = NULL, nstarts = 0;
    *left = 0;
    if (trk >= s->ntrk || !ln->n) return 0;
    nreal = real_syllables(s, trk, idx, ln, real);
    if (!(nsyl = split(text, syl, 200))) return 0;
    if (nreal) {
        /* the marks of the first and the last syllable stay with them */
        const EdEv *f = &s->trk[trk].ev[real[0]], *l = &s->trk[trk].ev[real[nreal - 1]];
        const uint8_t *fd = s->pool + f->data, *ld = s->pool + l->data;
        if (f->len && (fd[0] == '/' || fd[0] == '<' || fd[0] == '\\')) lead = (char)fd[0];
        if (l->len && (ld[l->len - 1] == '\r' || ld[l->len - 1] == '\n' || (l->len > 1 && ld[l->len - 1] == '/'))) tail = ld[l->len - 1];
        from = f->tick;
    } else {
        from = s->trk[trk].ev[idx[ln->first]].tick;
    }
    if (lead && syl[0][0] != '/' && syl[0][0] != '<') {
        memmove(syl[0] + 1, syl[0], 62);
        syl[0][0] = lead;
        syl[0][63] = 0;
    }
    {
        size_t L = strlen(syl[nsyl - 1]);
        /* the line's last syllable: its own ending (a space only when it had one) */
        if (nreal) {
            const EdEv *l = &s->trk[trk].ev[real[nreal - 1]];
            uint32_t st, cl = core(s, l, &st);
            int had_space = cl && s->pool[l->data + st + cl - 1] == ' ';
            if (!had_space && L && syl[nsyl - 1][L - 1] == ' ') syl[nsyl - 1][--L] = 0;
        }
        if (tail && L < 62) { syl[nsyl - 1][L] = (char)tail; syl[nsyl - 1][L + 1] = 0; }
    }
    if (nsyl == nreal) {
        /* the same syllables: only the texts change */
        for (i = 0; i < nsyl; i++) {
            uint8_t enc[80];
            size_t n = tm_to_cp1250_esc(syl[i], enc, sizeof enc, letters);
            uint32_t at = ed_pool_add(s, enc, (uint32_t)n);
            EdEv *e = &s->trk[trk].ev[real[i]];
            if (at == 0xFFFFFFFFUL) break;
            e->data = at;
            e->len = (uint32_t)n;
            placed++;
        }
        return placed;
    }
    /* other syllables: onto the notes of the line */
    if (note_trk >= s->ntrk || !(starts = (uint32_t *)malloc((s->trk[note_trk].n + 1) * sizeof(uint32_t)))) return 0;
    for (k = 0; k < s->trk[note_trk].n; k++) {
        const EdEv *e = &s->trk[note_trk].ev[k];
        if (e->tick < from || e->tick >= next || !ro_is_note(e, -1)) continue;
        if (nstarts && starts[nstarts - 1] == e->tick) continue;
        starts[nstarts++] = e->tick;
    }
    /* the old syllables go (from the last: the indices stay right); the separators stay */
    for (i = nreal; i > 0; i--) ed_delete(s, trk, real[i - 1]);
    for (i = 0; i < nsyl && (uint32_t)i < nstarts; i++) {
        uint8_t enc[80];
        size_t n = tm_to_cp1250_esc(syl[i], enc, sizeof enc, letters);
        if (!add_meta(s, trk, starts[i], 0x05, enc, (uint32_t)n)) break;
        placed++;
    }
    *left = nsyl - placed;
    free(starts);
    return placed;
}

int lt_strip_diacritics(EdSong *s)
{
    uint32_t t, i;
    int n = 0;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            EdEv *e = &s->trk[t].ev[i];
            char plain[300];
            size_t m;
            uint32_t j, at;
            int high = 0;
            if (e->st != 0xFF || e->d1 < 1 || e->d1 > 7) continue;
            for (j = 0; j < e->len; j++) high |= s->pool[e->data + j] >= 0x80;
            if (!high) continue;
            m = tm_convert(s->pool + e->data, e->len, plain, sizeof plain, TM_ASCII);
            if ((at = ed_pool_add(s, (const uint8_t *)plain, (uint32_t)m)) == 0xFFFFFFFFUL) return n;
            e->data = at;
            e->len = (uint32_t)m;
            n++;
        }
    return n;
}

const char *const lt_fill_methods[3] = {"Not at lyrics", "Not in beats with lyrics", "Not in measures with lyrics"};

static void sig_at(const EdSong *s, uint32_t tick, uint32_t *bar, uint32_t *beat)
{
    uint32_t tk[64], num[64], bl[64];
    int n = ed_sigs(s, tk, num, bl, 64), k = n - 1;
    while (k > 0 && tk[k] > tick) k--;
    *beat = bl[k] ? bl[k] : 1;
    *bar = num[k] * *beat;
    if (!*bar) *bar = 1;
}

int lt_fill_insert(EdSong *s, uint32_t trk, const char *measure, const char *beat, const char *end, uint32_t linefeed, int method, int letters)
{
    uint32_t *had, nh = 0, i, t, song_end = 0, m;
    int n = 0, only_one = !beat[0] && !end[0];
    uint8_t enc[3][80];
    size_t el[3];
    if (trk >= s->ntrk) return 0;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            uint32_t x = e->tick + ((e->st & 0xF0) == 0x90 && e->d2 ? e->dur : 0);
            if (x > song_end) song_end = x;
        }
    if (!(had = (uint32_t *)malloc((s->trk[trk].n + 1) * sizeof(uint32_t)))) return 0;
    for (i = 0; i < s->trk[trk].n; i++)
        if (is_lyric(&s->trk[trk].ev[i])) had[nh++] = s->trk[trk].ev[i].tick;
    el[0] = tm_to_cp1250_esc(measure, enc[0], 79, letters);
    el[1] = tm_to_cp1250_esc(beat, enc[1], 79, letters);
    el[2] = tm_to_cp1250_esc(end, enc[2], 79, letters);
    for (m = nh ? 1 : 2;; m++) {
        uint32_t ms = ed_tick_of(s, m, 1, 0), bar, bt, beats, me, k, last = 0xFFFFFFFFUL;
        int any = 0;
        if (ms >= song_end) break;
        sig_at(s, ms, &bar, &bt);
        beats = bar / bt ? bar / bt : 1;
        me = ms + bar;
        for (i = 0; i < nh && !any; i++) any = had[i] >= ms && had[i] < me;
        if (!(method == 2 && any)) {
            for (k = 0; k < beats; k++) {
                uint32_t tt = ms + k * bt, j;
                int skip = 0, w;
                for (j = 0; j < nh && !skip; j++) skip = method == 0 ? had[j] == tt : (had[j] >= tt && had[j] < tt + bt);
                if (skip || (only_one && k > 0)) continue;
                w = k == 0 ? 0 : k + 1 == beats ? 2 : 1;
                if (!el[w]) continue;
                add_meta(s, trk, tt, 0x05, enc[w], (uint32_t)el[w]);
                last = ms + (beats - 1) * bt;
                n++;
            }
        }
        if (last != 0xFFFFFFFFUL && linefeed && last + linefeed < me) add_meta(s, trk, last + linefeed, 0x05, (const uint8_t *)"\r", 1);
    }
    free(had);
    return n;
}

int lt_fill_delete(EdSong *s, uint32_t trk, const char *measure, const char *beat, const char *end, int letters)
{
    uint8_t enc[3][80];
    size_t el[3];
    uint32_t i, o;
    int gone = 0, prev_fill = 0, w;
    EdTrack *t;
    if (trk >= s->ntrk) return 0;
    el[0] = tm_to_cp1250_esc(measure, enc[0], 79, letters);
    el[1] = tm_to_cp1250_esc(beat, enc[1], 79, letters);
    el[2] = tm_to_cp1250_esc(end, enc[2], 79, letters);
    t = &s->trk[trk];
    for (i = o = 0; i < t->n; i++) {
        const EdEv *e = &t->ev[i];
        int fill = 0, lf;
        if (!is_lyric(e)) { t->ev[o++] = t->ev[i]; continue; }
        for (w = 0; w < 3; w++) fill |= el[w] && e->len == el[w] && !memcmp(s->pool + e->data, enc[w], el[w]);
        lf = e->len == 1 && s->pool[e->data] == '\r';
        if (fill || (lf && prev_fill)) { gone++; prev_fill = fill; continue; }
        prev_fill = fill;
        t->ev[o++] = t->ev[i];
    }
    t->n = o;
    return gone;
}
