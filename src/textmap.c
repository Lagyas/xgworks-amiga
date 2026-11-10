/* Song texts for the Amiga (see textmap.h). */
#include <stdio.h>
#include <stdlib.h>
#include "textmap.h"

/* Valid UTF-8 with at least one multi-byte letter? */
static int is_utf8(const uint8_t *s, size_t n)
{
    size_t i = 0;
    int multi = 0;
    while (i < n) {
        uint8_t c = s[i];
        int k, j;
        if (c < 0x80) { i++; continue; }
        k = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || (k == 1 && c < 0xC2) || i + (size_t)k >= n) return 0;
        for (j = 1; j <= k; j++)
            if ((s[i + j] & 0xC0) != 0x80) return 0;
        multi = 1;
        i += (size_t)k + 1;
    }
    return multi;
}

static uint32_t next_ucs(const uint8_t *s, size_t n, size_t *i, int utf8)
{
    uint8_t c = s[(*i)++];
    uint32_t u;
    int k;
    if (c < 0x80) return c;
    if (!utf8) return tm_cp1250[c - 0x80] ? tm_cp1250[c - 0x80] : '?';
    k = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : 3;
    u = c & (0x3F >> k);
    while (k-- && *i < n) u = (u << 6) | (s[(*i)++] & 0x3F);
    return u;
}

static const char *ascii_of(uint32_t u)
{
    unsigned lo = 0, hi = tm_ascii_n;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if (tm_ascii[mid].u == u) return tm_ascii[mid].a;
        if (tm_ascii[mid].u < u) lo = mid + 1; else hi = mid;
    }
    return "?";
}

size_t tm_convert(const uint8_t *in, size_t n, char *out, size_t max, int letters)
{
    size_t i = 0, o = 0;
    int utf8 = is_utf8(in, n);
    if (!max) return 0;
    while (i < n && o + 1 < max) {
        uint32_t u = next_ucs(in, n, &i, utf8);
        if (u < 0x80) { out[o++] = (char)u; continue; }
        if (letters == TM_LATIN1 && u >= 0xA0 && u <= 0xFF) { out[o++] = (char)u; continue; }
        if (letters == TM_LATIN2) {
            int k;
            for (k = 0; k < 96; k++)
                if (tm_latin2[k] == u) break;
            if (k < 96) { out[o++] = (char)(0xA0 + k); continue; }
        }
        {
            const char *a = ascii_of(u);
            while (*a && o + 1 < max) out[o++] = *a++;
        }
    }
    out[o] = 0;
    return o;
}

size_t tm_to_cp1250(const char *in, uint8_t *out, size_t max, int letters)
{
    size_t o = 0;
    for (; *in && o < max; in++) {
        uint8_t b = (uint8_t)*in;
        uint32_t u = b;
        int k;
        if (b < 0x80) { out[o++] = b; continue; }
        if (letters == TM_LATIN2 && b >= 0xA0) u = tm_latin2[b - 0xA0];
        for (k = 0; k < 128; k++)
            if (tm_cp1250[k] == u) break;
        out[o++] = k < 128 ? (uint8_t)(0x80 + k) : (uint8_t)'?';
    }
    return o;
}

size_t tm_from_cp1250_esc(const uint8_t *in, size_t n, char *out, size_t max, int letters)
{
    size_t i, o = 0;
    for (i = 0; i < n && o + 5 < max; i++) {
        uint8_t c = in[i];
        char one[8];
        uint8_t back;
        if (c == '\\') { out[o++] = '\\'; out[o++] = '\\'; continue; }
        if (c < 0x80) { out[o++] = (char)c; continue; }
        if (letters != TM_ASCII && tm_convert(&c, 1, one, sizeof one, letters) == 1 && (uint8_t)one[0] >= 0xA0 &&
            tm_to_cp1250(one, &back, 1, letters) == 1 && back == c) { out[o++] = one[0]; continue; }
        o += (size_t)sprintf(out + o, "\\x%02X", c);
    }
    out[o] = 0;
    return o;
}

size_t tm_to_cp1250_esc(const char *in, uint8_t *out, size_t max, int letters)
{
    size_t o = 0;
    while (*in && o < max) {
        if (in[0] == '\\' && in[1] == '\\') { out[o++] = '\\'; in += 2; continue; }
        if (in[0] == '\\' && in[1] == 'x' && in[2] && in[3]) {
            char hx[3];
            hx[0] = in[2]; hx[1] = in[3]; hx[2] = 0;
            out[o++] = (uint8_t)strtol(hx, NULL, 16);
            in += 4;
            continue;
        }
        {
            char one[2];
            one[0] = *in++; one[1] = 0;
            o += tm_to_cp1250(one, out + o, max - o, letters);
        }
    }
    return o;
}
