/* XGworks songs (.XWS): their blocks and track headers onto the edited song,
 * and writing an .XWS (as XGWorks NextGen's write_xws: one block chunk per
 * block, one track record per track, the pool of 101 records). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xws.h"
#include "edit.h"

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

typedef struct { uint8_t *b; size_t n, cap; } Buf;

static int put(Buf *o, const void *p, size_t n)
{
    if (o->n + n > o->cap) {
        size_t cap = o->cap ? o->cap : 16384;
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

static int put32(Buf *o, uint32_t v)
{
    uint8_t b[4];
    b[0] = (uint8_t)(v >> 24); b[1] = (uint8_t)(v >> 16); b[2] = (uint8_t)(v >> 8); b[3] = (uint8_t)v;
    return put(o, b, 4);
}

static void set32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
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

static int put_cstring(Buf *o, const uint8_t *s, size_t n)
{
    uint8_t h[3];
    if (n < 0xFF) { h[0] = (uint8_t)n; if (!put(o, h, 1)) return 0; }
    else { h[0] = 0xFF; h[1] = (uint8_t)n; h[2] = (uint8_t)(n >> 8); if (!put(o, h, 3)) return 0; }
    return put(o, s, n);
}

static int put_chunk(Buf *o, const char *tag, const uint8_t *b, size_t n)
{
    return put(o, tag, 4) && put32(o, (uint32_t)n) && (n == 0 || put(o, b, n));
}

int xws_attach_blocks(const uint8_t *d, size_t n, EdSong *s)
{
    size_t p, *blk_len = NULL;
    uint32_t nblk = 0, k = 0;
    for (p = 0; p + 8 <= n;) {
        uint32_t len = be32(d + p + 4);
        size_t e = p + 8 + len > n ? n : p + 8 + len;
        if (!memcmp(d + p, "S4WB", 4)) {
            size_t *b = (size_t *)realloc(blk_len, (nblk + 1) * sizeof(size_t));
            if (!b) { free(blk_len); return 0; }
            blk_len = b;
            blk_len[nblk++] = e - (p + 8);
        }
        p += 8 + (size_t)len;
    }
    for (p = 0; p + 8 <= n && k < s->ntrk;) {
        uint32_t len = be32(d + p + 4);
        size_t e = p + 8 + len > n ? n : p + 8 + len;
        if (!memcmp(d + p, "S4WT", 4) && e > p + 8 + 0x20 && d[p + 8] != 0) {
            const uint8_t *pay = d + p + 8;
            size_t pn = e - (p + 8), i = 0x20, ns, nl;
            int has_events = 0;
            EdTrack *t = &s->trk[k];
            static EdBlock tmp[256];
            uint32_t nb = 0;
            i = cstring(pay, pn, i, &ns, &nl);
            while (i + 32 <= pn && pay[i] == 1 && nb < 256) {
                EdBlock *b = &tmp[nb];
                uint32_t bi = be32(pay + i + 10);
                size_t bs, bl;
                memset(b, 0, sizeof *b);
                memcpy(b->raw, pay + i, 32);
                b->has_raw = 1;
                b->start = be32(pay + i + 2);
                b->end = be32(pay + i + 6);
                if (bi < nblk && blk_len[bi]) has_events = 1;
                i += 32;
                if (i < pn) {
                    i = cstring(pay, pn, i, &bs, &bl);
                    if (bl > 39) bl = 39;
                    memcpy(b->name, pay + bs, bl);
                }
                nb++;
            }
            /* the same tracks as xws_to_smf: used, with events */
            if (has_events) {
                memcpy(t->hdr, pay, 32);
                t->has_hdr = 1;
                if (nb && (t->blk = (EdBlock *)malloc(nb * sizeof(EdBlock)))) {
                    memcpy(t->blk, tmp, nb * sizeof(EdBlock));
                    t->nblk = nb;
                }
                k++;
            }
        }
        p += 8 + (size_t)len;
    }
    free(blk_len);
    return (int)k;
}

/* an event of a block: its time in the block, notes after the rest at a tick */
typedef struct { uint32_t t, seq; uint8_t note; const EdEv *e; } WItem;

static int witem_cmp(const void *a, const void *b)
{
    const WItem *x = (const WItem *)a, *y = (const WItem *)b;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    if (x->note != y->note) return x->note - y->note;
    return x->seq < y->seq ? -1 : (x->seq > y->seq);
}

static int track_channel(const EdTrack *t)
{
    uint32_t i;
    for (i = 0; i < t->n; i++)
        if ((t->ev[i].st & 0xF0) == 0x90 && t->ev[i].d2) return t->ev[i].st & 0x0F;
    return 0;
}

uint8_t *xws_write(const EdSong *s, size_t *out_n)
{
    Buf blocks = {NULL, 0, 0}, o = {NULL, 0, 0};
    Buf *recs = NULL;
    uint32_t nblocks = 0, k;
    WItem *it = NULL;
    static const uint8_t zeros[138] = {0};
    if (!(recs = (Buf *)calloc(s->ntrk + 1, sizeof(Buf)))) return NULL;
    for (k = 0; k < s->ntrk; k++) {
        const EdTrack *t = &s->trk[k];
        Buf *rc = &recs[k];
        EdBlock one;
        const EdBlock *sp;
        uint32_t nsp, b, i, end = 0, len;
        uint8_t rec[32], name[80];
        size_t nn;
        const uint8_t *nm = ed_name(s, k, &len);
        int conductor = k == 0 && !(nm && len);
        if (nm && len) { nn = len > 79 ? 79 : len; memcpy(name, nm, nn); }
        else if (conductor) nn = 0;
        else nn = (size_t)sprintf((char *)name, "Track%lu", (unsigned long)k);
        for (i = 0; i < t->n; i++) {
            uint32_t te = t->ev[i].tick + ((t->ev[i].st & 0xF0) == 0x90 && t->ev[i].d2 ? t->ev[i].dur : 0);
            if (te > end) end = te;
        }
        if (t->nblk) { sp = t->blk; nsp = t->nblk; }
        else {
            memset(&one, 0, sizeof one);
            one.end = end;
            memcpy(one.name, name, nn > 39 ? 39 : nn);
            sp = &one;
            nsp = 1;
        }
        /* the record: the header, the track name, then each block's placement and name */
        for (i = 0; i < 32; i++)
            rec[i] = t->has_hdr ? t->hdr[i] : (uint8_t)(i == 0 ? 1 : i == 6 ? (track_channel(t) != 9) : i == 12 ? 100 : 0);
        if (!rec[0]) rec[0] = 1;
        rec[2] = (uint8_t)(track_channel(t) & 0x0F);
        if (!put(rc, rec, 32) || !put_cstring(rc, name, nn)) goto fail;
        free(it);
        if (!(it = (WItem *)malloc((t->n + 1) * sizeof(WItem)))) goto fail;
        for (b = 0; b < nsp; b++) {
            uint32_t ni = 0, last = 0, bend, last_out = 0, bl;
            size_t h = blocks.n;
            uint8_t pl[32];
            for (i = 0; i < t->n; i++) {
                const EdEv *e = &t->ev[i];
                uint32_t owner = 0, j, rel;
                /* a lone note-off has no place in an .XWS */
                if ((e->st & 0xF0) == 0x80 || ((e->st & 0xF0) == 0x90 && !e->d2)) continue;
                for (j = 0; j < nsp; j++)
                    if (sp[j].start <= e->tick) owner = j;
                if (owner != b) continue;
                rel = e->tick - (sp[b].start < e->tick ? sp[b].start : e->tick);
                it[ni].t = rel;
                it[ni].seq = i;
                it[ni].e = e;
                it[ni].note = (uint8_t)((e->st & 0xF0) == 0x90);
                if (rel + (it[ni].note ? e->dur : 0) > last) last = rel + (it[ni].note ? e->dur : 0);
                ni++;
            }
            qsort(it, ni, sizeof(WItem), witem_cmp);
            if (!put(&blocks, "S4WB", 4) || !put32(&blocks, 0)) goto fail;
            for (i = 0; i < ni; i++) {
                const EdEv *e = it[i].e;
                uint8_t m[3];
                if (!putvar(&blocks, it[i].t - last_out)) goto fail;
                last_out = it[i].t;
                m[0] = e->st; m[1] = e->d1; m[2] = e->d2;
                if (it[i].note) {
                    if (!put(&blocks, m, 3) || !put32(&blocks, e->dur)) goto fail;
                } else if (e->st == 0xFF) {
                    if (!put(&blocks, m, 2) || !putvar(&blocks, e->len) || !put(&blocks, s->pool + e->data, e->len)) goto fail;
                } else if (e->st == 0xF0 || e->st == 0xF7) {
                    if (!put(&blocks, m, 1) || !putvar(&blocks, e->len) || !put(&blocks, s->pool + e->data, e->len)) goto fail;
                } else {
                    if (!put(&blocks, m, ((e->st & 0xF0) == 0xC0 || (e->st & 0xF0) == 0xD0) ? 2 : 3)) goto fail;
                }
            }
            bl = (uint32_t)(blocks.n - h - 8);
            set32(blocks.b + h + 4, bl);
            bend = sp[b].end > sp[b].start + last ? sp[b].end : sp[b].start + last;
            if (sp[b].has_raw) memcpy(pl, sp[b].raw, 32); else memset(pl, 0, 32);
            pl[0] = 1; pl[1] = 0;
            set32(pl + 2, sp[b].start);
            set32(pl + 6, bend);
            set32(pl + 10, nblocks);
            nblocks++;
            if (!put(rc, pl, 32) || !put_cstring(rc, (const uint8_t *)sp[b].name, strlen(sp[b].name))) goto fail;
        }
    }
    {
        uint8_t h[64];
        memset(h, 0, sizeof h);
        h[0] = 4;
        h[3] = 101;
        set32(h + 4, nblocks);
        h[8] = (uint8_t)(s->ppqn >> 8);
        h[9] = (uint8_t)s->ppqn;
        if (!put_chunk(&o, "S4WH", h, 64) || !put(&o, blocks.b, blocks.n)) goto fail;
    }
    for (k = 0; k < s->ntrk; k++)
        if (!put_chunk(&o, "S4WT", recs[k].b, recs[k].n)) goto fail;
    for (; k < 101; k++)
        if (!put_chunk(&o, "S4WT", zeros, 33)) goto fail;
    if (!put_chunk(&o, "S4ST", zeros, 138)) goto fail;
    for (k = 0; k < s->ntrk; k++) free(recs[k].b);
    free(recs);
    free(it);
    free(blocks.b);
    *out_n = o.n;
    return o.b;
fail:
    for (k = 0; k < s->ntrk; k++) free(recs[k].b);
    free(recs);
    free(it);
    free(blocks.b);
    free(o.b);
    return NULL;
}
