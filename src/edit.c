/* The song as it is edited (see edit.h). */
#include <stdlib.h>
#include <string.h>
#include "edit.h"

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

static uint32_t varlen(const uint8_t **pp, const uint8_t *end)
{
    uint32_t v = 0;
    int i;
    for (i = 0; i < 4 && *pp < end; i++) {
        uint8_t c = *(*pp)++;
        v = (v << 7) | (c & 0x7F);
        if (!(c & 0x80)) break;
    }
    return v;
}

uint32_t ed_pool_add(EdSong *s, const uint8_t *d, uint32_t n)
{
    uint32_t at;
    if (s->npool + n > s->cappool) {
        uint32_t cap = s->cappool ? s->cappool : 4096;
        uint8_t *np;
        while (cap < s->npool + n) cap *= 2;
        if (!(np = (uint8_t *)realloc(s->pool, cap))) return 0xFFFFFFFFUL;
        s->pool = np; s->cappool = cap;
    }
    at = s->npool;
    if (n) memcpy(s->pool + at, d, n);
    s->npool += n;
    return at;
}

static int push(EdTrack *t, const EdEv *e)
{
    if (t->n == t->cap) {
        uint32_t cap = t->cap ? t->cap * 2 : 256;
        EdEv *nv = (EdEv *)realloc(t->ev, cap * sizeof(EdEv));
        if (!nv) return 0;
        t->ev = nv; t->cap = cap;
    }
    t->ev[t->n++] = *e;
    return 1;
}

/* One MTrk: events with notes paired (first note-on with the first note-off of the same key). */
static int load_track(EdSong *s, EdTrack *t, const uint8_t *p, const uint8_t *end)
{
    uint32_t tick = 0, open_n = 0, k;
    uint8_t running = 0;
    /* open notes: index into t->ev, per channel and key, in order */
    uint32_t *open = NULL;
    uint32_t cap_open = 0;
    while (p < end) {
        EdEv e;
        uint8_t st;
        tick += varlen(&p, end);
        if (p >= end) break;
        memset(&e, 0, sizeof e);
        e.tick = tick;
        st = *p;
        if (st & 0x80) p++;
        else if (running) st = running;
        else break;
        if (st == 0xFF) {
            uint8_t type;
            uint32_t len;
            if (p >= end) break;
            type = *p++;
            len = varlen(&p, end);
            if (len > (uint32_t)(end - p)) len = (uint32_t)(end - p);
            if (type == 0x2F) { t->end = tick; break; }
            e.st = 0xFF; e.d1 = type; e.len = len;
            if ((e.data = ed_pool_add(s, p, len)) == 0xFFFFFFFFUL) goto fail;
            p += len;
        } else if (st == 0xF0 || st == 0xF7) {
            uint32_t len = varlen(&p, end);
            if (len > (uint32_t)(end - p)) len = (uint32_t)(end - p);
            e.st = st; e.len = len;
            if ((e.data = ed_pool_add(s, p, len)) == 0xFFFFFFFFUL) goto fail;
            p += len;
            running = 0;
        } else if (st > 0xF0) {
            continue;
        } else {
            int nd = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
            if ((uint32_t)(end - p) < (uint32_t)nd) break;
            running = st;
            e.st = st; e.d1 = p[0] & 0x7F; e.d2 = nd == 2 ? (p[1] & 0x7F) : 0;
            p += nd;
            if ((st & 0xF0) == 0x80 || ((st & 0xF0) == 0x90 && e.d2 == 0)) {
                /* a note-off: the oldest open note of that key */
                int paired = 0;
                for (k = 0; k < open_n; k++) {
                    EdEv *on = &t->ev[open[k]];
                    if ((on->st & 0x0F) == (st & 0x0F) && on->d1 == e.d1) {
                        on->dur = tick - on->tick;
                        on->offv = (st & 0xF0) == 0x80 ? (uint8_t)(0x80 | e.d2) : 0;
                        memmove(open + k, open + k + 1, (open_n - k - 1) * sizeof(uint32_t));
                        open_n--;
                        paired = 1;
                        break;
                    }
                }
                /* a note-off without a note stays as it is */
                if (!paired && !push(t, &e)) goto fail;
                continue;
            }
            if ((st & 0xF0) == 0x90) {
                e.dur = 0xFFFFFFFFUL;   /* open until its note-off */
                if (!push(t, &e)) goto fail;
                if (open_n == cap_open) {
                    uint32_t *no = (uint32_t *)realloc(open, (cap_open = cap_open ? cap_open * 2 : 64) * sizeof(uint32_t));
                    if (!no) goto fail;
                    open = no;
                }
                open[open_n++] = t->n - 1;
                continue;
            }
        }
        if (!push(t, &e)) goto fail;
    }
    if (t->end < tick) t->end = tick;
    /* notes never released: up to the end of the track */
    for (k = 0; k < open_n; k++) {
        EdEv *on = &t->ev[open[k]];
        on->dur = t->end - on->tick;
        on->offv = 0;
    }
    free(open);
    return 1;
fail:
    free(open);
    return 0;
}

int ed_load(EdSong *s, const uint8_t *buf, size_t n)
{
    const uint8_t *p, *end = buf + n;
    uint32_t hlen;
    memset(s, 0, sizeof *s);
    if (n < 14 || memcmp(buf, "MThd", 4)) return 0;
    hlen = be32(buf + 4);
    s->ppqn = (uint16_t)((buf[12] << 8) | buf[13]);
    if (!s->ppqn || (s->ppqn & 0x8000)) s->ppqn = 480;
    ed_pool_add(s, (const uint8_t *)"", 0);
    for (p = buf + 8 + hlen; p + 8 <= end;) {
        uint32_t len = be32(p + 4);
        const uint8_t *d = p + 8, *e = len > (uint32_t)(end - d) ? end : d + len;
        if (!memcmp(p, "MTrk", 4)) {
            EdTrack *nt = (EdTrack *)realloc(s->trk, (s->ntrk + 1) * sizeof(EdTrack));
            if (!nt) { ed_free(s); return 0; }
            s->trk = nt;
            memset(&s->trk[s->ntrk], 0, sizeof(EdTrack));
            if (!load_track(s, &s->trk[s->ntrk], d, e)) { s->ntrk++; ed_free(s); return 0; }
            s->ntrk++;
        }
        p = e;
    }
    return 1;
}

void ed_free(EdSong *s)
{
    uint32_t i;
    for (i = 0; i < s->ntrk; i++) { free(s->trk[i].ev); free(s->trk[i].blk); }
    free(s->trk);
    free(s->pool);
    memset(s, 0, sizeof *s);
}

int ed_copy(EdSong *d, const EdSong *s)
{
    uint32_t i;
    memset(d, 0, sizeof *d);
    d->ppqn = s->ppqn;
    if (s->npool && !(d->pool = (uint8_t *)malloc(s->npool))) return 0;
    if (s->npool) memcpy(d->pool, s->pool, s->npool);
    d->npool = d->cappool = s->npool;
    if (s->ntrk && !(d->trk = (EdTrack *)calloc(s->ntrk, sizeof(EdTrack)))) { ed_free(d); return 0; }
    d->ntrk = s->ntrk;
    for (i = 0; i < s->ntrk; i++) {
        d->trk[i] = s->trk[i];
        d->trk[i].ev = NULL;
        d->trk[i].blk = NULL;
        d->trk[i].cap = s->trk[i].n;
        if (s->trk[i].nblk) {
            if (!(d->trk[i].blk = (EdBlock *)malloc(s->trk[i].nblk * sizeof(EdBlock)))) { ed_free(d); return 0; }
            memcpy(d->trk[i].blk, s->trk[i].blk, s->trk[i].nblk * sizeof(EdBlock));
        }
        if (s->trk[i].n) {
            if (!(d->trk[i].ev = (EdEv *)malloc(s->trk[i].n * sizeof(EdEv)))) { ed_free(d); return 0; }
            memcpy(d->trk[i].ev, s->trk[i].ev, s->trk[i].n * sizeof(EdEv));
        }
    }
    return 1;
}

/* ---- writing ---- */

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

/* One output item: time, order (0 note-off, 1 the event in its place), event index. */
typedef struct { uint32_t t, i; uint8_t off; } Out;

static int out_cmp(const void *a, const void *b)
{
    const Out *x = (const Out *)a, *y = (const Out *)b;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    if (x->off != y->off) return x->off > y->off ? -1 : 1;   /* note-offs first */
    return x->i < y->i ? -1 : (x->i > y->i);
}

static int write_track(Buf *o, const EdSong *s, const EdTrack *t)
{
    Out *it;
    uint32_t i, n = 0, last = 0, endt = t->end;
    uint8_t running = 0;
    size_t h = o->n;
    static const uint8_t mtrk[8] = {'M', 'T', 'r', 'k', 0, 0, 0, 0};
    if (!put(o, mtrk, 8)) return 0;
    if (!(it = (Out *)malloc((t->n * 2 + 1) * sizeof(Out)))) return 0;
    for (i = 0; i < t->n; i++) {
        const EdEv *e = &t->ev[i];
        it[n].t = e->tick; it[n].i = i; it[n].off = 0; n++;
        if ((e->st & 0xF0) == 0x90 && e->d2 && e->dur != 0xFFFFFFFFUL) {
            it[n].t = e->tick + e->dur; it[n].i = i; it[n].off = 1; n++;
            if (e->tick + e->dur > endt) endt = e->tick + e->dur;
        }
        if (e->tick > endt) endt = e->tick;
    }
    qsort(it, n, sizeof(Out), out_cmp);
    for (i = 0; i < n; i++) {
        const EdEv *e = &t->ev[it[i].i];
        uint8_t m[3];
        int k;
        if (!putvar(o, it[i].t - last)) goto fail;
        last = it[i].t;
        if (it[i].off) {
            if (e->offv & 0x80) { m[0] = (uint8_t)(0x80 | (e->st & 0x0F)); m[2] = e->offv & 0x7F; }
            else { m[0] = e->st; m[2] = 0; }
            m[1] = e->d1;
            k = 3;
        } else if (e->st == 0xFF) {
            uint8_t h2[2];
            h2[0] = 0xFF; h2[1] = e->d1;
            if (!put(o, h2, 2) || !putvar(o, e->len) || !put(o, s->pool + e->data, e->len)) goto fail;
            continue;
        } else if (e->st == 0xF0 || e->st == 0xF7) {
            if (!put(o, &e->st, 1) || !putvar(o, e->len) || !put(o, s->pool + e->data, e->len)) goto fail;
            running = 0;
            continue;
        } else {
            m[0] = e->st; m[1] = e->d1; m[2] = e->d2;
            k = ((e->st & 0xF0) == 0xC0 || (e->st & 0xF0) == 0xD0) ? 2 : 3;
        }
        /* running status */
        if (m[0] == running) { if (!put(o, m + 1, (size_t)(k - 1))) goto fail; }
        else { if (!put(o, m, (size_t)k)) goto fail; running = m[0]; }
    }
    {
        static const uint8_t eot[3] = {0xFF, 0x2F, 0x00};
        uint32_t tl;
        if (!putvar(o, endt > last ? endt - last : 0) || !put(o, eot, 3)) goto fail;
        tl = (uint32_t)(o->n - h - 8);
        o->b[h + 4] = (uint8_t)(tl >> 24); o->b[h + 5] = (uint8_t)(tl >> 16); o->b[h + 6] = (uint8_t)(tl >> 8); o->b[h + 7] = (uint8_t)tl;
    }
    free(it);
    return 1;
fail:
    free(it);
    return 0;
}

uint8_t *ed_save(const EdSong *s, size_t *n)
{
    Buf o = {NULL, 0, 0};
    uint8_t h[14] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 0, 0, 0};
    uint32_t i;
    h[10] = (uint8_t)(s->ntrk >> 8); h[11] = (uint8_t)s->ntrk;
    h[12] = (uint8_t)(s->ppqn >> 8); h[13] = (uint8_t)s->ppqn;
    if (!put(&o, h, 14)) return NULL;
    for (i = 0; i < s->ntrk; i++)
        if (!write_track(&o, s, &s->trk[i])) { free(o.b); return NULL; }
    *n = o.n;
    return o.b;
}

/* ---- editing ---- */

int ed_insert(EdSong *s, uint32_t trk, const EdEv *e)
{
    EdTrack *t;
    uint32_t i;
    if (trk >= s->ntrk) return 0;
    t = &s->trk[trk];
    if (!push(t, e)) return 0;
    /* after the last event at or before its tick */
    for (i = t->n - 1; i > 0 && t->ev[i - 1].tick > e->tick; i--) t->ev[i] = t->ev[i - 1];
    t->ev[i] = *e;
    return (int)i + 1;
}

void ed_delete(EdSong *s, uint32_t trk, uint32_t i)
{
    EdTrack *t;
    if (trk >= s->ntrk || i >= s->trk[trk].n) return;
    t = &s->trk[trk];
    memmove(t->ev + i, t->ev + i + 1, (t->n - i - 1) * sizeof(EdEv));
    t->n--;
}

long ed_replace(EdSong *s, uint32_t trk, uint32_t i, const EdEv *e)
{
    if (trk >= s->ntrk || i >= s->trk[trk].n) return -1;
    ed_delete(s, trk, i);
    return ed_insert(s, trk, e) - 1L;
}

int ed_add_track(EdSong *s, uint32_t at)
{
    EdTrack *nt = (EdTrack *)realloc(s->trk, (s->ntrk + 1) * sizeof(EdTrack));
    if (!nt) return 0;
    s->trk = nt;
    if (at > s->ntrk) at = s->ntrk;
    memmove(s->trk + at + 1, s->trk + at, (s->ntrk - at) * sizeof(EdTrack));
    memset(&s->trk[at], 0, sizeof(EdTrack));
    s->ntrk++;
    return 1;
}

void ed_del_track(EdSong *s, uint32_t t)
{
    if (t >= s->ntrk) return;
    free(s->trk[t].ev);
    free(s->trk[t].blk);
    memmove(s->trk + t, s->trk + t + 1, (s->ntrk - t - 1) * sizeof(EdTrack));
    s->ntrk--;
}

const uint8_t *ed_name(const EdSong *s, uint32_t trk, uint32_t *len)
{
    uint32_t i;
    *len = 0;
    if (trk >= s->ntrk) return NULL;
    for (i = 0; i < s->trk[trk].n; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        if (e->st == 0xFF && e->d1 == 0x03) { *len = e->len; return s->pool + e->data; }
    }
    return NULL;
}

int ed_channel(const EdSong *s, uint32_t trk)
{
    uint32_t i;
    if (trk >= s->ntrk) return -1;
    for (i = 0; i < s->trk[trk].n; i++)
        if (s->trk[trk].ev[i].st < 0xF0) return s->trk[trk].ev[i].st & 0x0F;
    return -1;
}

int ed_set_name(EdSong *s, uint32_t trk, const uint8_t *name, uint32_t len)
{
    uint32_t i, at;
    EdEv e;
    if (trk >= s->ntrk || (at = ed_pool_add(s, name, len)) == 0xFFFFFFFFUL) return 0;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *x = &s->trk[trk].ev[i];
        if (x->st == 0xFF && x->d1 == 0x03) { x->data = at; x->len = len; return 1; }
    }
    memset(&e, 0, sizeof e);
    e.st = 0xFF; e.d1 = 0x03; e.data = at; e.len = len;
    return ed_insert(s, trk, &e) > 0;
}

void ed_set_channel(EdSong *s, uint32_t trk, int ch)
{
    uint32_t i;
    if (trk >= s->ntrk || ch < 0 || ch > 15) return;
    for (i = 0; i < s->trk[trk].n; i++) {
        EdEv *e = &s->trk[trk].ev[i];
        if (e->st < 0xF0) e->st = (uint8_t)((e->st & 0xF0) | ch);
    }
}

/* ---- bars ---- */

/* The time signatures: (tick, beats per bar, ticks per beat), sorted. */
int ed_sigs(const EdSong *s, uint32_t *tk, uint32_t *num, uint32_t *bl, int max)
{
    int n = 0;
    uint32_t t, i;
    tk[0] = 0; num[0] = 4; bl[0] = s->ppqn; n = 1;
    for (t = 0; t < s->ntrk; t++)
        for (i = 0; i < s->trk[t].n; i++) {
            const EdEv *e = &s->trk[t].ev[i];
            if (e->st == 0xFF && e->d1 == 0x58 && e->len >= 2) {
                const uint8_t *d = s->pool + e->data;
                uint32_t den = 1u << (d[1] > 6 ? 6 : d[1]);
                int k = n, j;
                if (e->tick == 0) k = 0;
                else if (n >= max) continue;
                else {
                    for (k = 0; k < n && tk[k] < e->tick; k++) {}
                    if (k < n && tk[k] == e->tick) {} else {
                        for (j = n; j > k; j--) { tk[j] = tk[j - 1]; num[j] = num[j - 1]; bl[j] = bl[j - 1]; }
                        n++;
                    }
                }
                tk[k] = e->tick;
                num[k] = d[0] ? d[0] : 4;
                bl[k] = (uint32_t)s->ppqn * 4 / den;
                if (!bl[k]) bl[k] = 1;
            }
        }
    return n;
}

void ed_mbt(const EdSong *s, uint32_t tick, uint32_t *bar, uint32_t *beat, uint32_t *t)
{
    uint32_t tk[64], num[64], bl[64], b = 1, rem;
    int n = ed_sigs(s, tk, num, bl, 64), k;
    for (k = 0; k < n; k++) {
        uint32_t barlen = num[k] * bl[k], until = k + 1 < n ? tk[k + 1] : 0xFFFFFFFFUL;
        if (tick < until || k + 1 == n) {
            rem = tick - tk[k];
            b += rem / barlen;
            rem %= barlen;
            *bar = b; *beat = rem / bl[k] + 1; *t = rem % bl[k];
            return;
        }
        /* whole bars of this signature (a change inside a bar starts a new bar) */
        b += (until - tk[k] + barlen - 1) / barlen;
    }
    *bar = 1; *beat = 1; *t = tick;
}

uint32_t ed_tick_of(const EdSong *s, uint32_t bar, uint32_t beat, uint32_t t)
{
    uint32_t tk[64], num[64], bl[64], b = 1;
    int n = ed_sigs(s, tk, num, bl, 64), k;
    if (bar < 1) bar = 1;
    if (beat < 1) beat = 1;
    for (k = 0; k < n; k++) {
        uint32_t barlen = num[k] * bl[k], until = k + 1 < n ? tk[k + 1] : 0xFFFFFFFFUL;
        uint32_t bars = k + 1 < n ? (until - tk[k] + barlen - 1) / barlen : 0xFFFFFFFFUL;
        if (bar < b + bars || k + 1 == n) return tk[k] + (bar - b) * barlen + (beat - 1) * bl[k] + t;
        b += bars;
    }
    return t;
}

void ed_sort(EdSong *s, uint32_t trk)
{
    EdTrack *t;
    uint32_t i, j;
    if (trk >= s->ntrk) return;
    t = &s->trk[trk];
    for (i = 1; i < t->n; i++) {
        EdEv x = t->ev[i];
        for (j = i; j > 0 && t->ev[j - 1].tick > x.tick; j--) t->ev[j] = t->ev[j - 1];
        t->ev[j] = x;
    }
}
