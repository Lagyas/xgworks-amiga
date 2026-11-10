/* XGworks songs (.XWS) as a Standard MIDI File, the way XGWorks NextGen
 * exports them (export_named): one track per used track record, its blocks
 * at their places, notes with a gate as note-on + note-off, the track name
 * when the blocks have none. */
#include <stdlib.h>
#include <string.h>
#include "xws.h"

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static uint32_t varlen(const uint8_t *b, size_t n, size_t *i)
{
    uint32_t v = 0;
    int k;
    for (k = 0; k < 4 && *i < n; k++) {
        uint8_t c = b[(*i)++];
        v = (v << 7) | (c & 0x7F);
        if (!(c & 0x80)) break;
    }
    return v;
}

typedef struct { uint8_t *b; size_t n, cap; } Buf;

static int put(Buf *o, const void *p, size_t n)
{
    if (o->n + n > o->cap) {
        size_t cap = o->cap ? o->cap : 8192;
        uint8_t *nb;
        while (cap < o->n + n) cap *= 2;
        if (!(nb = (uint8_t *)realloc(o->b, cap))) return 0;
        o->b = nb; o->cap = cap;
    }
    memcpy(o->b + o->n, p, n);
    o->n += n;
    return 1;
}

static int putvar(Buf *o, uint32_t v)
{
    uint8_t t[5];
    int k = 4;
    t[k] = (uint8_t)(v & 0x7F);
    while ((v >>= 7) && k > 0) t[--k] = (uint8_t)(0x80 | (v & 0x7F));
    return put(o, t + k, (size_t)(5 - k));
}

int xws_is(const uint8_t *d, size_t n)
{
    return n >= 8 && !memcmp(d, "S4WH", 4);
}

/* One item of a track: time, order class (0 meta, 1 channel / SysEx, 2 note-on, 4 note-off), bytes. */
typedef struct {
    uint32_t t, seq;
    uint8_t cls;
    uint32_t off, len;   /* raw bytes in the source (meta / SysEx / channel) */
    uint8_t m[3], mn;    /* or a built message */
} Item;

typedef struct { Item *v; uint32_t n, cap; } Items;

static int add(Items *it, const Item *x)
{
    if (it->n == it->cap) {
        uint32_t cap = it->cap ? it->cap * 2 : 1024;
        Item *nv = (Item *)realloc(it->v, cap * sizeof(Item));
        if (!nv) return 0;
        it->v = nv; it->cap = cap;
    }
    it->v[it->n++] = *x;
    return 1;
}

static int item_cmp(const void *a, const void *b)
{
    const Item *x = (const Item *)a, *y = (const Item *)b;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    if (x->cls != y->cls) return x->cls < y->cls ? -1 : 1;
    return x->seq < y->seq ? -1 : (x->seq > y->seq);
}

/* The events of block `b` (n bytes, file offset base) shifted by `start`; has_name set on a track name. */
static int block(Items *it, const uint8_t *src, size_t base, size_t n, uint32_t start, uint32_t *seq, int *has_name)
{
    const uint8_t *b = src + base;
    size_t i = 0;
    uint32_t t = 0;
    uint8_t running = 0;
    while (i < n) {
        Item x;
        uint8_t st;
        t += varlen(b, n, &i);
        if (i >= n) break;
        memset(&x, 0, sizeof x);
        x.t = t + start;
        x.seq = (*seq)++;
        st = b[i];
        if (st == 0xFF) {
            size_t s0 = i;
            uint32_t len;
            uint8_t type;
            if (i + 1 >= n) break;
            type = b[i + 1];
            i += 2;
            len = varlen(b, n, &i);
            if (i + len > n) len = (uint32_t)(n - i);
            i += len;
            if (type == 0x03) *has_name = 1;
            /* the end of a block is not an event of the track */
            if (type == 0x2F) break;
            x.cls = 0; x.off = (uint32_t)(base + s0); x.len = (uint32_t)(i - s0);
            if (!add(it, &x)) return 0;
            continue;
        }
        if (st == 0xF0 || st == 0xF7) {
            size_t s0 = i;
            uint32_t len;
            i++;
            len = varlen(b, n, &i);
            if (i + len > n) len = (uint32_t)(n - i);
            i += len;
            x.cls = 1; x.off = (uint32_t)(base + s0); x.len = (uint32_t)(i - s0);
            if (!add(it, &x)) return 0;
            continue;
        }
        if (st & 0x80) { running = st; i++; }
        else if (!running) break;
        st = running;
        if ((st & 0xF0) == 0x90) {
            uint32_t gate;
            if (i + 6 > n) break;
            x.cls = 2; x.mn = 3; x.m[0] = st; x.m[1] = b[i]; x.m[2] = b[i + 1];
            gate = be32(b + i + 2);
            i += 6;
            if (!add(it, &x)) return 0;
            x.t = t + start + gate;
            x.seq = (*seq)++;
            x.cls = 4; x.m[2] = 0;
            if (!add(it, &x)) return 0;
        } else {
            int k = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
            if (i + (size_t)k > n) break;
            x.cls = 1; x.mn = (uint8_t)(1 + k); x.m[0] = st; x.m[1] = b[i]; x.m[2] = k == 2 ? b[i + 1] : 0;
            i += (size_t)k;
            if (!add(it, &x)) return 0;
        }
    }
    return 1;
}

/* MFC CString: a length byte (0xFF: a 16-bit little-endian length follows), the bytes. */
static size_t cstring(const uint8_t *p, size_t n, size_t i, size_t *start, size_t *len)
{
    size_t l;
    if (i >= n) { *start = n; *len = 0; return n; }
    if (p[i] == 0xFF && i + 2 < n) { l = (size_t)(p[i + 1] | (p[i + 2] << 8)); i += 3; }
    else { l = p[i]; i++; }
    if (i + l > n) l = n - i;
    *start = i; *len = l;
    return i + l;
}

uint8_t *xws_to_smf(const uint8_t *d, size_t n, size_t *out_n)
{
    size_t p = 0, *blk_off = NULL, *blk_len = NULL;
    uint32_t nblk = 0, ntrk = 0, ppqn = 480;
    Buf o = {NULL, 0, 0};
    Items it = {NULL, 0, 0};
    uint8_t hdr[14] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 0, 0, 0};
    /* the blocks */
    for (p = 0; p + 8 <= n;) {
        uint32_t len = be32(d + p + 4);
        size_t e = p + 8 + len > n ? n : p + 8 + len;
        if (!memcmp(d + p, "S4WH", 4) && e - p >= 18) ppqn = be16(d + p + 16) ? be16(d + p + 16) : 1;
        if (!memcmp(d + p, "S4WB", 4)) {
            size_t *a = (size_t *)realloc(blk_off, (nblk + 1) * sizeof(size_t)), *b;
            if (!a) goto fail;
            blk_off = a;
            if (!(b = (size_t *)realloc(blk_len, (nblk + 1) * sizeof(size_t)))) goto fail;
            blk_len = b;
            blk_off[nblk] = p + 8;
            blk_len[nblk] = e - (p + 8);
            nblk++;
        }
        p += 8 + (size_t)len;
    }
    if (!put(&o, hdr, 14)) goto fail;
    /* the tracks */
    for (p = 0; p + 8 <= n;) {
        uint32_t len = be32(d + p + 4);
        size_t e = p + 8 + len > n ? n : p + 8 + len;
        if (!memcmp(d + p, "S4WT", 4) && e > p + 8 && d[p + 8] != 0) {
            const uint8_t *pay = d + p + 8;
            size_t pn = e - (p + 8), i = 0x20, ns, nl, j, ps, pl;
            uint32_t seq = 0, k;
            int has_name = 0;
            it.n = 0;
            i = cstring(pay, pn, i, &ns, &nl);
            /* the track's own placement, then each named part's (as parse_s4wt) */
            if (i + 14 <= pn) {
                uint32_t bi = be32(pay + i + 10);
                if (bi < nblk && !block(&it, d, blk_off[bi], blk_len[bi], be32(pay + i + 2), &seq, &has_name)) goto fail;
            }
            i += 32;
            while (i + 14 <= pn) {
                uint32_t bi;
                j = cstring(pay, pn, i, &ps, &pl);
                if (!pl || j + 14 > pn) break;
                bi = be32(pay + j + 10);
                if (bi < nblk && !block(&it, d, blk_off[bi], blk_len[bi], be32(pay + j + 2), &seq, &has_name)) goto fail;
                i = j + 32;
            }
            if (it.n) {
                size_t h = o.n;
                uint32_t last = 0, tl;
                static const uint8_t mtrk[8] = {'M', 'T', 'r', 'k', 0, 0, 0, 0};
                qsort(it.v, it.n, sizeof(Item), item_cmp);
                if (!put(&o, mtrk, 8)) goto fail;
                if (nl && !has_name) {
                    uint8_t m[2] = {0xFF, 0x03};
                    if (!putvar(&o, 0) || !put(&o, m, 2) || !putvar(&o, (uint32_t)nl) || !put(&o, pay + ns, nl)) goto fail;
                }
                for (k = 0; k < it.n; k++) {
                    const Item *x = &it.v[k];
                    if (!putvar(&o, x->t - last)) goto fail;
                    last = x->t;
                    if (x->mn ? !put(&o, x->m, x->mn) : !put(&o, d + x->off, x->len)) goto fail;
                }
                {
                    static const uint8_t eot[4] = {0x00, 0xFF, 0x2F, 0x00};
                    if (!put(&o, eot, 4)) goto fail;
                }
                tl = (uint32_t)(o.n - h - 8);
                o.b[h + 4] = (uint8_t)(tl >> 24); o.b[h + 5] = (uint8_t)(tl >> 16); o.b[h + 6] = (uint8_t)(tl >> 8); o.b[h + 7] = (uint8_t)tl;
                ntrk++;
            }
        }
        p += 8 + (size_t)len;
    }
    o.b[10] = (uint8_t)(ntrk >> 8); o.b[11] = (uint8_t)ntrk;
    o.b[12] = (uint8_t)(ppqn >> 8); o.b[13] = (uint8_t)ppqn;
    free(blk_off); free(blk_len); free(it.v);
    *out_n = o.n;
    return o.b;
fail:
    free(blk_off); free(blk_len); free(it.v); free(o.b);
    return NULL;
}
