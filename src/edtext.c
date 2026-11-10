/* Events as text lines and back (see edtext.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edtext.h"
#include "lyrics.h"
#include "textmap.h"

static const char *const KEYS[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

void et_key(int k, char *out)
{
    sprintf(out, "%s%d", KEYS[k % 12], k / 12 - 2);
}

int et_key_of(const char *s)
{
    int n, acc = 0, oct, neg = 0;
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    char c = *s;
    if (c >= 'a' && c <= 'g') c = (char)(c - 32);
    if (c < 'A' || c > 'G') {
        /* a plain number */
        char *e;
        long v = strtol(s, &e, 10);
        return (*e == 0 && e != s && v >= 0 && v < 128) ? (int)v : -1;
    }
    n = base[c - 'A'];
    s++;
    if (*s == '#') { acc = 1; s++; }
    else if (*s == 'b') { acc = -1; s++; }
    if (*s == '-') { neg = 1; s++; }
    if (*s < '0' || *s > '9') return -1;
    oct = atoi(s);
    if (neg) oct = -oct;
    n = (oct + 2) * 12 + n + acc;
    return n >= 0 && n < 128 ? n : -1;
}

static const char *ctrl_name(int c)
{
    switch (c) {
    case 0: return "Bank MSB"; case 1: return "Modulation"; case 5: return "Porta Time"; case 6: return "Data MSB";
    case 7: return "Volume"; case 10: return "Pan"; case 11: return "Expression"; case 32: return "Bank LSB";
    case 38: return "Data LSB"; case 64: return "Sustain"; case 65: return "Portamento"; case 66: return "Sostenuto";
    case 67: return "Soft Pedal"; case 71: return "Resonance"; case 72: return "Release"; case 73: return "Attack";
    case 74: return "Cutoff"; case 84: return "Porta Ctrl"; case 91: return "Reverb"; case 93: return "Chorus";
    case 94: return "Variation"; case 98: return "NRPN LSB"; case 99: return "NRPN MSB"; case 100: return "RPN LSB";
    case 101: return "RPN MSB"; case 120: return "All Sound Off"; case 121: return "Reset Ctrls"; case 123: return "All Notes Off";
    default: return "";
    }
}

static const char *meta_name(int t)
{
    switch (t) {
    case 0x01: return "Text"; case 0x02: return "Copyright"; case 0x03: return "Name"; case 0x04: return "Instrument";
    case 0x05: return "Lyric"; case 0x06: return "Marker"; case 0x07: return "Cue";
    default: return NULL;
    }
}

char *et_format(const EdSong *s, const EdEv *e, char *out, int max, int letters)
{
    char pos[48], ch[4], body[200];
    uint32_t bar, beat, t;
    int k = e->st & 0xF0;
    ed_mbt(s, e->tick, &bar, &beat, &t);
    snprintf(pos, sizeof pos, "%04lu:%lu:%03lu", (unsigned long)bar, (unsigned long)beat, (unsigned long)t);
    if (e->st < 0xF0) sprintf(ch, "%2d", (e->st & 0x0F) + 1); else strcpy(ch, " -");
    body[0] = 0;
    if (k == 0x90 && e->d2) {
        char key[8];
        et_key(e->d1, key);
        snprintf(body, sizeof body, "Note      %-4s %3d %6lu", key, e->d2, (unsigned long)e->dur);
    } else if (k == 0x80 || k == 0x90) {
        char key[8];
        et_key(e->d1, key);
        snprintf(body, sizeof body, "NoteOff   %-4s %3d", key, k == 0x80 ? e->d2 : 0);
    } else if (k == 0xA0) {
        char key[8];
        et_key(e->d1, key);
        snprintf(body, sizeof body, "PolyPress %-4s %3d", key, e->d2);
    } else if (k == 0xB0) {
        snprintf(body, sizeof body, "Ctrl      %3d %3d  %s", e->d1, e->d2, ctrl_name(e->d1));
    } else if (k == 0xC0) {
        snprintf(body, sizeof body, "Prog      %3d", e->d1 + 1);
    } else if (k == 0xD0) {
        snprintf(body, sizeof body, "ChPress   %3d", e->d1);
    } else if (k == 0xE0) {
        snprintf(body, sizeof body, "Pitch     %+5d", (int)((e->d2 << 7) | e->d1) - 8192);
    } else if (e->st == 0xF0 || e->st == 0xF7) {
        uint32_t i;
        int n = snprintf(body, sizeof body, "SysEx     %02X", e->st);
        for (i = 0; i < e->len && n < (int)sizeof body - 4; i++) n += sprintf(body + n, " %02X", s->pool[e->data + i]);
    } else if (e->st == 0xFF) {
        const uint8_t *d = s->pool + e->data;
        const char *mn = meta_name(e->d1);
        if (e->d1 == 0x51 && e->len == 3) {
            uint32_t us = ((uint32_t)d[0] << 16) | ((uint32_t)d[1] << 8) | d[2];
            uint32_t bpm100 = us ? (uint32_t)((6000000000ULL + us / 2) / us) : 0;
            snprintf(body, sizeof body, "Tempo     %lu.%02lu  (%lu us)", (unsigned long)(bpm100 / 100), (unsigned long)(bpm100 % 100), (unsigned long)us);
        } else if (e->d1 == 0x58 && e->len >= 2) {
            snprintf(body, sizeof body, "TimeSig   %d/%d", d[0], 1 << (d[1] > 6 ? 6 : d[1]));
        } else if (e->d1 == 0x7F && e->len == 7 && d[0] == 0x43 && d[1] == 0x7B && d[2] == 0x01) {
            char cn[16];
            ly_chord_name(d, cn);
            /* an unusual bass chord type byte is shown, to be written back */
            if (d[3] != 0x7F && d[5] != 0x7F && d[6] != 0) snprintf(body, sizeof body, "Chord     %s (%02X)", cn, d[6]);
            else snprintf(body, sizeof body, "Chord     %s", cn);
        } else if (mn) {
            char esc[200];
            int o = 0;
            uint32_t j;
            /* line / page ends and backslashes written out (\r \n \\); a letter
               the chosen set lacks as \xHH, the song's own byte */
            for (j = 0; j < e->len && o < (int)sizeof esc - 5; j++) {
                uint8_t c = d[j];
                char one[8];
                if (c == '\r') { esc[o++] = '\\'; esc[o++] = 'r'; continue; }
                if (c == '\n') { esc[o++] = '\\'; esc[o++] = 'n'; continue; }
                if (c == '\\') { esc[o++] = '\\'; esc[o++] = '\\'; continue; }
                if (c < 0x80) { esc[o++] = (char)c; continue; }
                if (letters != TM_ASCII && tm_convert(&c, 1, one, sizeof one, letters) == 1 && (uint8_t)one[0] >= 0xA0) {
                    uint8_t back;
                    if (tm_to_cp1250(one, &back, 1, letters) == 1 && back == c) { esc[o++] = one[0]; continue; }
                }
                o += sprintf(esc + o, "\\x%02X", c);
            }
            esc[o] = 0;
            snprintf(body, sizeof body, "%-9s \"%.180s\"", mn, esc);
        } else {
            uint32_t i;
            int n = snprintf(body, sizeof body, "Meta      %02X", e->d1);
            for (i = 0; i < e->len && n < (int)sizeof body - 4; i++) n += sprintf(body + n, " %02X", d[i]);
        }
    }
    snprintf(out, (size_t)max, "%.20s %s  %.170s", pos, ch, body);
    return out;
}

/* ---- parsing ---- */

static const char *skip(const char *p) { while (*p == ' ' || *p == '\t') p++; return p; }

static int word(const char **p, char *w, int max)
{
    int n = 0;
    *p = skip(*p);
    while (**p && **p != ' ' && **p != '\t' && n < max - 1) w[n++] = *(*p)++;
    w[n] = 0;
    return n;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a >= 'a' && *a <= 'z' ? (char)(*a - 32) : *a, y = *b >= 'a' && *b <= 'z' ? (char)(*b - 32) : *b;
        if (x != y) return 0;
        a++; b++;
    }
    return *a == *b;
}

static int num(const char *w, long lo, long hi, long *v)
{
    char *e;
    *v = strtol(w, &e, 10);
    return *w && *e == 0 && *v >= lo && *v <= hi;
}

#define FAIL(msg) do { snprintf(err, (size_t)errmax, "%s", msg); return 0; } while (0)

int et_parse(EdSong *s, const char *line, EdEv *e, int ch, int letters, char *err, int errmax)
{
    const char *p = line;
    char w[64], kind[16];
    unsigned long bar = 1, beat = 1, t = 0;
    long v1, v2, v3;
    memset(e, 0, sizeof *e);
    if (ch < 0) ch = 0;
    /* the position */
    word(&p, w, sizeof w);
    if (sscanf(w, "%lu:%lu:%lu", &bar, &beat, &t) < 1) FAIL("Start with the position, e.g. 12:1:000");
    e->tick = ed_tick_of(s, (uint32_t)bar, (uint32_t)beat, (uint32_t)t);
    /* the channel (optional) */
    {
        const char *q = p;
        word(&q, w, sizeof w);
        if (num(w, 1, 16, &v1)) { ch = (int)v1 - 1; p = q; }
        else if (!strcmp(w, "-")) p = q;
    }
    word(&p, kind, sizeof kind);
    if (ieq(kind, "Note") || ieq(kind, "NoteOff") || ieq(kind, "PolyPress")) {
        int k;
        word(&p, w, sizeof w);
        if ((k = et_key_of(w)) < 0) FAIL("A key: C3, F#4 or 60");
        e->d1 = (uint8_t)k;
        word(&p, w, sizeof w);
        if (!num(w, 0, 127, &v2)) v2 = ieq(kind, "Note") ? 100 : 64;
        if (ieq(kind, "Note")) {
            word(&p, w, sizeof w);
            if (!num(w, 1, 0x7FFFFFF, &v3)) v3 = s->ppqn;
            e->st = (uint8_t)(0x90 | ch); e->d2 = (uint8_t)(v2 ? v2 : 1); e->dur = (uint32_t)v3; e->offv = 0;
        } else if (ieq(kind, "NoteOff")) {
            e->st = (uint8_t)(0x80 | ch); e->d2 = (uint8_t)v2;
        } else {
            e->st = (uint8_t)(0xA0 | ch); e->d2 = (uint8_t)v2;
        }
        return 1;
    }
    if (ieq(kind, "Ctrl")) {
        word(&p, w, sizeof w);
        if (!num(w, 0, 127, &v1)) FAIL("Ctrl number value, e.g. Ctrl 7 100");
        word(&p, w, sizeof w);
        if (!num(w, 0, 127, &v2)) FAIL("Ctrl number value, e.g. Ctrl 7 100");
        e->st = (uint8_t)(0xB0 | ch); e->d1 = (uint8_t)v1; e->d2 = (uint8_t)v2;
        return 1;
    }
    if (ieq(kind, "Prog")) {
        word(&p, w, sizeof w);
        if (!num(w, 1, 128, &v1)) FAIL("Prog 1-128");
        e->st = (uint8_t)(0xC0 | ch); e->d1 = (uint8_t)(v1 - 1);
        return 1;
    }
    if (ieq(kind, "ChPress")) {
        word(&p, w, sizeof w);
        if (!num(w, 0, 127, &v1)) FAIL("ChPress 0-127");
        e->st = (uint8_t)(0xD0 | ch); e->d1 = (uint8_t)v1;
        return 1;
    }
    if (ieq(kind, "Pitch")) {
        word(&p, w, sizeof w);
        if (!num(w[0] == '+' ? w + 1 : w, -8192, 8191, &v1)) FAIL("Pitch -8192..8191");
        v1 += 8192;
        e->st = (uint8_t)(0xE0 | ch); e->d1 = (uint8_t)(v1 & 0x7F); e->d2 = (uint8_t)(v1 >> 7);
        return 1;
    }
    if (ieq(kind, "SysEx") || ieq(kind, "Meta")) {
        uint8_t b[256];
        int n = 0, meta = ieq(kind, "Meta");
        while (word(&p, w, sizeof w) && n < (int)sizeof b) {
            char *x;
            long v = strtol(w, &x, 16);
            if (*x || v < 0 || v > 255) FAIL("Hex bytes, e.g. F0 43 10 4C 00 00 7E 00 F7");
            b[n++] = (uint8_t)v;
        }
        if (meta) {
            if (n < 1) FAIL("Meta type then hex data");
            e->st = 0xFF; e->d1 = b[0];
            if ((e->data = ed_pool_add(s, b + 1, (uint32_t)(n - 1))) == 0xFFFFFFFFUL) FAIL("No memory");
            e->len = (uint32_t)(n - 1);
        } else {
            if (n < 2 || (b[0] != 0xF0 && b[0] != 0xF7)) FAIL("SysEx starts with F0 (or F7)");
            if (b[0] == 0xF0 && b[n - 1] != 0xF7) b[n++] = 0xF7;
            e->st = b[0];
            if ((e->data = ed_pool_add(s, b + 1, (uint32_t)(n - 1))) == 0xFFFFFFFFUL) FAIL("No memory");
            e->len = (uint32_t)(n - 1);
        }
        return 1;
    }
    if (ieq(kind, "Tempo")) {
        /* BPM with up to two decimals, in hundredths (no floating point: the Amiga has no FPU) */
        uint32_t bpm100 = 0, us;
        uint8_t d[3];
        const char *q;
        int dec = -1;
        word(&p, w, sizeof w);
        for (q = w; *q; q++) {
            if (*q == '.' || *q == ',') { if (dec >= 0) break; dec = 0; continue; }
            if (*q < '0' || *q > '9') break;
            if (dec < 0) bpm100 = bpm100 * 10 + (uint32_t)(*q - '0');
            else if (dec < 2) { bpm100 = bpm100 * 10 + (uint32_t)(*q - '0'); dec++; }
        }
        if (dec < 0) dec = 0;
        while (dec++ < 2) bpm100 *= 10;
        if (*q || bpm100 < 400 || bpm100 > 99900) FAIL("Tempo in BPM, e.g. Tempo 120");
        us = (uint32_t)((6000000000ULL + bpm100 / 2) / bpm100);
        /* the exact value, when it is there and fits the BPM: "(496689 us)" */
        {
            const char *r = strchr(p, '(');
            unsigned long x;
            if (r && sscanf(r + 1, "%lu", &x) == 1 && x > 0 && x < 0x1000000UL &&
                (uint32_t)((6000000000ULL + x / 2) / x) == bpm100) us = (uint32_t)x;
        }
        d[0] = (uint8_t)(us >> 16); d[1] = (uint8_t)(us >> 8); d[2] = (uint8_t)us;
        e->st = 0xFF; e->d1 = 0x51; e->len = 3;
        if ((e->data = ed_pool_add(s, d, 3)) == 0xFFFFFFFFUL) FAIL("No memory");
        return 1;
    }
    if (ieq(kind, "TimeSig")) {
        unsigned nn = 4, dd = 4;
        uint8_t d[4];
        int l = 0;
        word(&p, w, sizeof w);
        if (sscanf(w, "%u/%u", &nn, &dd) != 2 || !nn || nn > 99) FAIL("TimeSig e.g. 3/4");
        while ((1u << l) < dd && l < 6) l++;
        d[0] = (uint8_t)nn; d[1] = (uint8_t)l; d[2] = 24; d[3] = 8;
        e->st = 0xFF; e->d1 = 0x58; e->len = 4;
        if ((e->data = ed_pool_add(s, d, 4)) == 0xFFFFFFFFUL) FAIL("No memory");
        return 1;
    }
    if (ieq(kind, "Chord")) {
        uint8_t d[7];
        word(&p, w, sizeof w);
        if (!ly_chord_data(w, d)) FAIL("A chord, e.g. Chord Am7, C/E, N.C.");
        {
            const char *q = strchr(p, '(');
            unsigned x;
            if (q && d[5] != 0x7F && sscanf(q + 1, "%x", &x) == 1 && x < 256) d[6] = (uint8_t)x;
        }
        e->st = 0xFF; e->d1 = 0x7F; e->len = 7;
        if ((e->data = ed_pool_add(s, d, 7)) == 0xFFFFFFFFUL) FAIL("No memory");
        return 1;
    }
    {
        static const struct { const char *n; uint8_t t; } T[] = {{"Text", 1}, {"Copyright", 2}, {"Name", 3}, {"Instrument", 4}, {"Lyric", 5}, {"Marker", 6}, {"Cue", 7}};
        int i;
        for (i = 0; i < 7; i++)
            if (ieq(kind, T[i].n)) {
                char txt[200], *q, *r;
                uint8_t enc[200];
                size_t n;
                p = skip(p);
                strncpy(txt, *p == '"' ? p + 1 : p, sizeof txt - 1);
                txt[sizeof txt - 1] = 0;
                if ((q = strrchr(txt, '"'))) *q = 0;
                /* \r \n \\ \xHH back to the bytes; the letters into Windows-1250 */
                n = 0;
                for (q = txt; *q && n < sizeof enc; q++) {
                    if (*q == '\\' && (q[1] == 'r' || q[1] == 'n' || q[1] == '\\')) {
                        q++;
                        enc[n++] = (uint8_t)(*q == 'r' ? '\r' : *q == 'n' ? '\n' : '\\');
                    } else if (*q == '\\' && q[1] == 'x' && q[2] && q[3]) {
                        char hx[3];
                        hx[0] = q[2]; hx[1] = q[3]; hx[2] = 0;
                        enc[n++] = (uint8_t)strtol(hx, NULL, 16);
                        q += 3;
                    } else {
                        char one[2];
                        one[0] = *q; one[1] = 0;
                        n += tm_to_cp1250(one, enc + n, sizeof enc - n, letters);
                    }
                }
                (void)r;
                e->st = 0xFF; e->d1 = T[i].t; e->len = (uint32_t)n;
                if ((e->data = ed_pool_add(s, enc, (uint32_t)n)) == 0xFFFFFFFFUL) FAIL("No memory");
                return 1;
            }
    }
    FAIL("Kinds: Note NoteOff PolyPress Ctrl Prog ChPress Pitch SysEx Tempo TimeSig Text Lyric Marker Name Cue Chord Meta");
}
