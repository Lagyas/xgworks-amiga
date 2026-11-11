/* Blocks of the Track View (see blocks.h). */
#include <stdlib.h>
#include <string.h>
#include "blocks.h"

static int blk_cmp(const void *a, const void *b)
{
    const EdBlock *x = (const EdBlock *)a, *y = (const EdBlock *)b;
    return x->start < y->start ? -1 : (x->start > y->start);
}

static uint32_t track_end(const EdTrack *t)
{
    uint32_t i, e = 0;
    for (i = 0; i < t->n; i++) {
        uint32_t x = t->ev[i].tick + ((t->ev[i].st & 0xF0) == 0x90 && t->ev[i].d2 ? t->ev[i].dur : 0);
        if (x > e) e = x;
    }
    return e;
}

static void name_of(const EdSong *s, uint32_t trk, char *out)
{
    uint32_t len;
    const uint8_t *n = ed_name(s, trk, &len);
    if (len > 39) len = 39;
    if (n) memcpy(out, n, len);
    out[n ? len : 0] = 0;
}

int bk_view(const EdSong *s, uint32_t trk, uint32_t bar, EdBlock *out, int max)
{
    const EdTrack *t;
    uint32_t i;
    if (trk >= s->ntrk || max < 1) return 0;
    t = &s->trk[trk];
    if (t->nblk) {
        for (i = 0; i < t->nblk && (int)i < max; i++) out[i] = t->blk[i];
        return (int)i;
    }
    if (!t->n) return 0;
    memset(out, 0, sizeof *out);
    out->end = (track_end(t) + bar - 1) / bar * bar;
    name_of(s, trk, out->name);
    return 1;
}

void bk_materialize(EdSong *s, uint32_t trk, uint32_t bar)
{
    EdBlock b;
    if (trk >= s->ntrk || s->trk[trk].nblk) return;
    if (bk_view(s, trk, bar, &b, 1) == 1 && (s->trk[trk].blk = (EdBlock *)malloc(sizeof(EdBlock)))) {
        s->trk[trk].blk[0] = b;
        s->trk[trk].nblk = 1;
    }
}

uint32_t bk_owner(const EdTrack *t, uint32_t tick)
{
    uint32_t j, o = 0;
    for (j = 0; j < t->nblk; j++)
        if (t->blk[j].start <= tick) o = j;
    return o;
}

static int add_block(EdTrack *t, const EdBlock *b)
{
    EdBlock *nb = (EdBlock *)realloc(t->blk, (t->nblk + 1) * sizeof(EdBlock));
    if (!nb) return 0;
    t->blk = nb;
    t->blk[t->nblk++] = *b;
    qsort(t->blk, t->nblk, sizeof(EdBlock), blk_cmp);
    return 1;
}

int bk_create(EdSong *s, uint32_t trk, uint32_t start, uint32_t end, const char *name, uint32_t bar)
{
    EdBlock b;
    if (trk >= s->ntrk || end <= start) return 0;
    bk_materialize(s, trk, bar);
    memset(&b, 0, sizeof b);
    b.start = start;
    b.end = end;
    if (name) { strncpy(b.name, name, 39); b.name[39] = 0; }
    return add_block(&s->trk[trk], &b);
}

void bk_remove(EdSong *s, uint32_t trk, uint32_t bi)
{
    EdTrack *t;
    uint32_t i, o;
    if (trk >= s->ntrk || bi >= s->trk[trk].nblk) return;
    t = &s->trk[trk];
    for (i = o = 0; i < t->n; i++) {
        if (bk_owner(t, t->ev[i].tick) == bi) continue;
        t->ev[o++] = t->ev[i];
    }
    t->n = o;
    memmove(t->blk + bi, t->blk + bi + 1, (t->nblk - bi - 1) * sizeof(EdBlock));
    t->nblk--;
}

void bk_clip_free(BkClip *c)
{
    free(c->ev);
    free(c->data);
    memset(c, 0, sizeof *c);
}

int bk_take(const EdSong *s, uint32_t trk, uint32_t bi, uint32_t t0, BkClip *c)
{
    const EdTrack *t;
    const EdBlock *b;
    uint32_t i, nd = 0;
    memset(c, 0, sizeof *c);
    if (trk >= s->ntrk || bi >= s->trk[trk].nblk) return 0;
    t = &s->trk[trk];
    b = &t->blk[bi];
    c->trk = (int)trk;
    memcpy(c->name, b->name, sizeof c->name);
    c->start = b->start - t0;
    c->end = (b->end > b->start ? b->end : b->start) - t0;
    for (i = 0; i < t->n; i++)
        if (bk_owner(t, t->ev[i].tick) == bi && t->ev[i].st >= 0xF0) nd += t->ev[i].len;
    if (!(c->ev = (EdEv *)malloc((t->n + 1) * sizeof(EdEv))) || !(c->data = (uint8_t *)malloc(nd + 1))) { bk_clip_free(c); return 0; }
    for (i = 0; i < t->n; i++) {
        EdEv e = t->ev[i];
        if (bk_owner(t, e.tick) != bi) continue;
        e.tick = e.tick > t0 ? e.tick - t0 : 0;
        e.sel = 0;
        if (e.st >= 0xF0) {
            memcpy(c->data + c->ndata, s->pool + e.data, e.len);
            e.data = c->ndata;
            c->ndata += e.len;
        }
        c->ev[c->n++] = e;
    }
    return 1;
}

long bk_paste(EdSong *s, uint32_t trk, const BkClip *c, uint32_t at, uint32_t bar)
{
    EdTrack *t;
    EdBlock b;
    uint32_t i;
    if (trk >= s->ntrk) return -1;
    bk_materialize(s, trk, bar);
    for (i = 0; i < c->n; i++) {
        EdEv e = c->ev[i];
        e.tick += at;
        if (e.st >= 0xF0 && (e.data = ed_pool_add(s, c->data + c->ev[i].data, e.len)) == 0xFFFFFFFFUL) return -1;
        if (!ed_insert(s, trk, &e)) return -1;
    }
    memset(&b, 0, sizeof b);
    memcpy(b.name, c->name, sizeof b.name);
    b.start = c->start + at;
    b.end = c->end + at;
    t = &s->trk[trk];
    if (!add_block(t, &b)) return -1;
    for (i = 0; i < t->nblk; i++)
        if (t->blk[i].start == b.start && !strcmp(t->blk[i].name, b.name)) return (long)i;
    return 0;
}

void bk_rename(EdSong *s, uint32_t trk, uint32_t bi, const char *name)
{
    if (trk >= s->ntrk || bi >= s->trk[trk].nblk) return;
    strncpy(s->trk[trk].blk[bi].name, name, 39);
    s->trk[trk].blk[bi].name[39] = 0;
}

int bk_repeat(EdSong *s, uint32_t from, uint32_t to, uint32_t at, uint32_t count, uint32_t bar)
{
    uint32_t t, bi, k;
    int n = 0;
    if (to <= from) return 0;
    for (t = 0; t < s->ntrk; t++) {
        BkClip clips[64];
        int nc = 0, c;
        bk_materialize(s, t, bar);
        for (bi = 0; bi < s->trk[t].nblk && nc < 64; bi++) {
            const EdBlock *b = &s->trk[t].blk[bi];
            uint32_t be = b->end > b->start ? b->end : b->start;
            if (b->start >= from && be <= to && bk_take(s, t, bi, from, &clips[nc])) nc++;
        }
        for (k = 0; k < count; k++)
            for (c = 0; c < nc; c++)
                if (bk_paste(s, t, &clips[c], at + k * (to - from), bar) >= 0) n++;
        for (c = 0; c < nc; c++) bk_clip_free(&clips[c]);
    }
    return n;
}

long bk_move(EdSong *s, uint32_t trk, uint32_t bi, long delta, uint32_t bar)
{
    BkClip c;
    long to;
    uint32_t st;
    if (trk >= s->ntrk || bi >= s->trk[trk].nblk || !delta) return (long)bi;
    st = s->trk[trk].blk[bi].start;
    if (!bk_take(s, trk, bi, st, &c)) return -1;
    bk_remove(s, trk, bi);
    to = (long)st + delta;
    if (to < 0) to = 0;
    to = bk_paste(s, trk, &c, (uint32_t)to, bar);
    bk_clip_free(&c);
    return to;
}
