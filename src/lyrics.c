/* Lyrics and chords for the karaoke window (see lyrics.h). */
#include <stdlib.h>
#include <string.h>
#include "lyrics.h"
#include "textmap.h"

static const char *const XF_TYPES[34] = {
    "", "6", "maj7", "maj7(#11)", "add9", "maj7(9)", "6(9)", "aug", "m", "m6", "m7", "m7(b5)", "m(9)", "m7(9)", "m7(11)", "mMaj7",
    "mMaj7(9)", "dim", "dim7", "7", "7sus4", "7(b5)", "7(9)", "7(#11)", "7(13)", "7(b9)", "7(b13)", "7(#9)", "maj7aug", "7aug", "1+8",
    "1+5", "sus4", "1+2+5",
};

static int root(uint8_t b, char *out)
{
    static const char L[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    static const char *const ACC[7] = {"bbb", "bb", "b", "", "#", "##", "###"};
    int n = b & 0x0F, f = (b >> 4) & 7;
    if (n < 1 || n > 7 || f > 6) return 0;
    out[0] = L[n - 1];
    strcpy(out + 1, ACC[f]);
    return 1;
}

void ly_chord_name(const uint8_t *d, char *out)
{
    char r[8], b[8];
    out[0] = 0;
    if (d[3] == 0x7F) { strcpy(out, "N.C."); return; }
    if (!root(d[3], r)) return;
    strcpy(out, r);
    if (d[4] < 34) strcat(out, XF_TYPES[d[4]]);
    if (d[5] != 0x7F && root(d[5], b) && strlen(out) + strlen(b) < 15) {
        strcat(out, "/");
        strcat(out, b);
    }
}

typedef struct { char *b; uint32_t n, cap; } Str;

static int sput(Str *s, const char *t, uint32_t n)
{
    if (s->n + n + 1 > s->cap) {
        uint32_t cap = s->cap ? s->cap * 2 : 4096;
        char *nb;
        while (cap < s->n + n + 1) cap *= 2;
        if (!(nb = (char *)realloc(s->b, cap))) return 0;
        s->b = nb; s->cap = cap;
    }
    memcpy(s->b + s->n, t, n);
    s->n += n;
    s->b[s->n] = 0;
    return 1;
}

int ly_build(Lyrics *ly, const XgSong *s, int letters)
{
    uint32_t i, best = 0, *cnt;
    int type = 0x05, trk = -1;
    Str txt = {NULL, 0, 0};
    int brk = 1;            /* the next syllable starts a line */
    int brk_page = 0;       /* ... a page */
    uint32_t page = 0;
    memset(ly, 0, sizeof *ly);
    if (!(cnt = (uint32_t *)calloc(s->ntrk + 1, sizeof(uint32_t)))) return 0;
    /* the track with the most lyric events; else the most .kar texts */
    for (i = 0; i < s->ntxt; i++)
        if (s->txt[i].type == 0x05) cnt[s->txt[i].trk]++;
    for (i = 0; i < s->ntrk; i++)
        if (cnt[i] > best) { best = cnt[i]; trk = (int)i; }
    if (trk < 0) {
        type = 0x01;
        memset(cnt, 0, (s->ntrk + 1) * sizeof(uint32_t));
        for (i = 0; i < s->ntxt; i++)
            if (s->txt[i].type == 0x01 && s->txt[i].len && s->buf[s->txt[i].off] != '@') cnt[s->txt[i].trk]++;
        for (i = 0; i < s->ntrk; i++)
            if (cnt[i] > best) { best = cnt[i]; trk = (int)i; }
        /* a few texts are a title, not lyrics */
        if (best < 8) trk = -1;
    }
    free(cnt);
    ly->syl = (LySyl *)malloc((s->ntxt + 1) * sizeof(LySyl));
    ly->line = (LyLine *)malloc((s->ntxt + 1) * sizeof(LyLine));
    ly->chord = (LyChord *)malloc((s->ntxt + 1) * sizeof(LyChord));
    if (!ly->syl || !ly->line || !ly->chord) { ly_free(ly); return 0; }
    if (!sput(&txt, "", 0)) { ly_free(ly); return 0; }
    for (i = 0; i < s->ntxt; i++) {
        const XgText *t = &s->txt[i];
        const uint8_t *d = s->buf + t->off;
        char conv[512];
        uint32_t len = t->len, a = 0;
        int new_line, end_line, new_page, end_page;
        if (t->type == 0x7F) {
            LyChord *c = &ly->chord[ly->nchord];
            c->tick = t->tick;
            ly_chord_name(d, c->name);
            if (c->name[0] && (!ly->nchord || strcmp(ly->chord[ly->nchord - 1].name, c->name))) ly->nchord++;
            continue;
        }
        if (t->type != type || (int)t->trk != trk || !len) continue;
        if (type == 0x01 && d[0] == '@') continue;
        /* the marks (as XGWorks NextGen) */
        new_page = d[0] == '<' || d[0] == '\\';
        new_line = d[0] == '/' || new_page;
        end_page = (len >= 4 && !memcmp(d + len - 4, "\r\n\r\n", 4)) || (len >= 2 && d[len - 1] == '\n' && d[len - 2] == '\n');
        while (a < len && (d[a] == '/' || d[a] == '<' || d[a] == '\\')) a++;
        end_line = len > a && (d[len - 1] == '\r' || d[len - 1] == '\n' || d[len - 1] == '/');
        while (len > a && (d[len - 1] == '\r' || d[len - 1] == '\n' || d[len - 1] == '/')) len--;
        tm_convert(d + a, len - a, conv, sizeof conv, letters);
        {
            char *p;
            for (p = conv; *p; p++)
                if (*p == '\r' || *p == '\n') *p = ' ';
        }
        if ((new_page || brk_page) && ly->nline && ly->line[ly->nline - 1].n) page++;
        if ((new_line || brk || new_page || brk_page) && (ly->nline == 0 || ly->line[ly->nline - 1].n)) {
            ly->line[ly->nline].first = ly->nsyl;
            ly->line[ly->nline].n = 0;
            ly->line[ly->nline].page = page;
            ly->nline++;
        }
        brk = end_line;
        brk_page = end_page;
        ly->syl[ly->nsyl].tick = t->tick;
        ly->syl[ly->nsyl].off = txt.n;
        ly->syl[ly->nsyl].line = (uint16_t)(ly->nline - 1);
        if (!sput(&txt, conv, (uint32_t)strlen(conv) + 1)) { ly_free(ly); return 0; }
        txt.n += 0;
        ly->line[ly->nline - 1].n++;
        ly->nsyl++;
    }
    ly->text = txt.b;
    return 1;
}

void ly_free(Lyrics *ly)
{
    free(ly->syl);
    free(ly->line);
    free(ly->chord);
    free(ly->text);
    memset(ly, 0, sizeof *ly);
}

long ly_syl_at(const Lyrics *ly, uint32_t tick)
{
    long lo = 0, hi = (long)ly->nsyl;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (ly->syl[mid].tick <= tick) lo = mid + 1; else hi = mid;
    }
    return lo - 1;
}

long ly_chord_at(const Lyrics *ly, uint32_t tick)
{
    long lo = 0, hi = (long)ly->nchord;
    while (lo < hi) {
        long mid = (lo + hi) / 2;
        if (ly->chord[mid].tick <= tick) lo = mid + 1; else hi = mid;
    }
    return lo - 1;
}

static int root_byte(const char *s, int *used)
{
    static const char L[] = "CDEFGAB";
    const char *x;
    int f = 3, n;
    char c = *s >= 'a' && *s <= 'g' ? (char)(*s - 32) : *s;
    if (!c || !(x = strchr(L, c))) return -1;
    n = (int)(x - L) + 1;
    *used = 1;
    if (s[1] == '#') { f = 4; (*used)++; if (s[2] == '#') { f = 5; (*used)++; } }
    else if (s[1] == 'b') { f = 2; (*used)++; if (s[2] == 'b') { f = 1; (*used)++; } }
    return (f << 4) | n;
}

int ly_chord_data(const char *name, uint8_t *d)
{
    char main_[24], *slash;
    int r, used, t;
    d[0] = 0x43; d[1] = 0x7B; d[2] = 0x01;
    if (!strcmp(name, "N.C.") || !strcmp(name, "NC")) { d[3] = 0x7F; d[4] = 0; d[5] = 0x7F; d[6] = 0x7F; return 1; }
    strncpy(main_, name, sizeof main_ - 1);
    main_[sizeof main_ - 1] = 0;
    d[5] = 0x7F; d[6] = 0x7F;
    if ((slash = strchr(main_, '/'))) {
        int b, u2;
        *slash = 0;
        if ((b = root_byte(slash + 1, &u2)) < 0 || slash[1 + u2]) return 0;
        d[5] = (uint8_t)b; d[6] = 0;
    }
    if ((r = root_byte(main_, &used)) < 0) return 0;
    for (t = 0; t < 34; t++)
        if (!strcmp(main_ + used, XF_TYPES[t])) break;
    if (t == 34) return 0;
    d[3] = (uint8_t)r; d[4] = (uint8_t)t;
    return 1;
}

void ly_page_at(const Lyrics *ly, uint32_t tick, uint32_t *first, uint32_t *n)
{
    long s = ly_syl_at(ly, tick);
    uint32_t L = s >= 0 ? ly->syl[s].line : 0, a, b;
    *first = 0;
    *n = 0;
    if (!ly->nline) return;
    for (a = L; a > 0 && ly->line[a - 1].page == ly->line[L].page; a--) {}
    for (b = L; b + 1 < ly->nline && ly->line[b + 1].page == ly->line[L].page; b++) {}
    *first = a;
    *n = b - a + 1;
}
