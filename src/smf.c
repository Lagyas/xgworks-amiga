/* Standard MIDI File reading and the playback order (see smf.h). */
#include <stdlib.h>
#include <string.h>
#include "smf.h"

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

/* Variable-length number; 0 bytes left = stops at `end`. */
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

const char *xg_error(int e)
{
    switch (e) {
    case XG_OK: return "OK";
    case XG_ENOTSMF: return "not a Standard MIDI File";
    case XG_ESMPTE: return "SMPTE time division is not supported";
    case XG_ENOMEM: return "not enough memory";
    default: return "the file is damaged";
    }
}

/* GM System On (7E dd 09 01), XG System On (43 1n 4C 00 00 7E 00) or
 * XG All Parameter Reset (… 00 00 7F 00), as in the Rust core. */
int xg_is_reset(const uint8_t *d, uint32_t len)
{
    if (len >= 4 && d[0] == 0x7E && d[2] == 0x09 && d[3] == 0x01) return 1;
    if (len >= 7 && d[0] == 0x43 && (d[1] & 0xF0) == 0x10 && d[2] == 0x4C && d[3] == 0x00 && d[4] == 0x00 && (d[5] == 0x7E || d[5] == 0x7F)) return 1;
    return 0;
}

typedef struct {
    XgEv *ev; uint32_t n, cap;
    XgTempo *tp; uint32_t ntp, cptp;
} Lists;

static int push_ev(Lists *l, const XgEv *e)
{
    if (l->n == l->cap) {
        uint32_t cap = l->cap ? l->cap * 2 : 1024;
        XgEv *nv = (XgEv *)realloc(l->ev, cap * sizeof(XgEv));
        if (!nv) return 0;
        l->ev = nv; l->cap = cap;
    }
    l->ev[l->n++] = *e;
    return 1;
}

static int push_tempo(Lists *l, uint32_t tick, uint32_t us)
{
    if (l->ntp == l->cptp) {
        uint32_t cap = l->cptp ? l->cptp * 2 : 16;
        XgTempo *nv = (XgTempo *)realloc(l->tp, cap * sizeof(XgTempo));
        if (!nv) return 0;
        l->tp = nv; l->cptp = cap;
    }
    l->tp[l->ntp].tick = tick; l->tp[l->ntp].us_qn = us; l->tp[l->ntp].at_num = 0;
    l->ntp++;
    return 1;
}

static int track(Lists *l, const uint8_t *base, const uint8_t *p, const uint8_t *end, uint32_t *seq, uint16_t trk)
{
    uint32_t tick = 0;
    uint8_t running = 0;
    while (p < end) {
        XgEv e;
        uint8_t st;
        tick += varlen(&p, end);
        if (p >= end) break;
        st = *p;
        if (st & 0x80) p++;
        else if (running) st = running;
        else return XG_EBROKEN;
        memset(&e, 0, sizeof e);
        e.tick = tick;
        e.trk = trk;
        if (st == 0xFF) {
            uint8_t type;
            uint32_t len;
            if (p >= end) break;
            type = *p++;
            len = varlen(&p, end);
            if (len > (uint32_t)(end - p)) return XG_EBROKEN;
            if (type == 0x51 && len == 3 && !push_tempo(l, tick, ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2])) return XG_ENOMEM;
            p += len;
            if (type == 0x2F) break;
            continue;
        }
        if (st == 0xF0 || st == 0xF7) {
            uint32_t len = varlen(&p, end);
            if (len > (uint32_t)(end - p)) return XG_EBROKEN;
            e.msg[0] = st; e.n = 0;
            e.off = (uint32_t)(p - base); e.len = len;
            e.cls = (st == 0xF0 && xg_is_reset(p, len)) ? 0 : 1;
            p += len;
            running = 0;
        } else if (st >= 0xF0) {
            /* other system messages do not belong in a file: skip the byte */
            continue;
        } else {
            int nd = ((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0) ? 1 : 2;
            if ((uint32_t)(end - p) < (uint32_t)nd) return XG_EBROKEN;
            running = st;
            e.off = (uint32_t)(p - base);
            e.msg[0] = st; e.msg[1] = p[0] & 0x7F; e.msg[2] = nd == 2 ? (p[1] & 0x7F) : 0;
            e.n = (uint8_t)(1 + nd);
            e.cls = ((st & 0xF0) == 0x90 && e.msg[2] > 0) ? 2 : 1;
            p += nd;
        }
        e.seq = (*seq)++;
        if (!push_ev(l, &e)) return XG_ENOMEM;
    }
    return XG_OK;
}

static int ev_cmp(const void *a, const void *b)
{
    const XgEv *x = (const XgEv *)a, *y = (const XgEv *)b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    if (x->cls != y->cls) return x->cls < y->cls ? -1 : 1;
    return x->seq < y->seq ? -1 : (x->seq > y->seq);
}

static int tp_cmp(const void *a, const void *b)
{
    const XgTempo *x = (const XgTempo *)a, *y = (const XgTempo *)b;
    return x->tick < y->tick ? -1 : (x->tick > y->tick);
}

int xg_smf_load(XgSong *s, const uint8_t *buf, size_t size)
{
    Lists l;
    const uint8_t *p, *end = buf + size;
    uint32_t hlen, seq = 0, i, k;
    int r = XG_OK;
    memset(s, 0, sizeof *s);
    memset(&l, 0, sizeof l);
    if (size < 14 || memcmp(buf, "MThd", 4) != 0) return XG_ENOTSMF;
    hlen = be32(buf + 4);
    if (hlen < 6 || hlen > size - 8) return XG_EBROKEN;
    s->buf = buf; s->size = size;
    s->format = be16(buf + 8);
    s->ntracks = be16(buf + 10);
    s->ppqn = be16(buf + 12);
    if (s->ppqn & 0x8000) return XG_ESMPTE;
    if (s->ppqn == 0) s->ppqn = 1;
    p = buf + 8 + hlen;
    while (p + 8 <= end && r == XG_OK) {
        uint32_t len = be32(p + 4);
        const uint8_t *d = p + 8, *e = (len > (uint32_t)(end - d)) ? end : d + len;
        if (memcmp(p, "MTrk", 4) == 0) {
            uint32_t *nt = (uint32_t *)realloc(s->trk_off, (s->ntrk + 1) * sizeof(uint32_t));
            if (!nt) { r = XG_ENOMEM; break; }
            s->trk_off = nt;
            s->trk_off[s->ntrk] = (uint32_t)(p - buf);
            r = track(&l, buf, d, e, &seq, (uint16_t)s->ntrk);
            s->ntrk++;
        }
        p = e;
    }
    if (r != XG_OK) { free(l.ev); free(l.tp); free(s->trk_off); s->trk_off = NULL; return r; }
    /* the tempo map: 120 BPM at the start when the song sets none there */
    for (i = 0; i < l.ntp && l.tp[i].tick != 0; i++) {}
    if (i == l.ntp && !push_tempo(&l, 0, 500000)) { free(l.ev); free(l.tp); return XG_ENOMEM; }
    qsort(l.tp, l.ntp, sizeof(XgTempo), tp_cmp);
    /* one tempo per tick (the last one written wins, as a player hears it) */
    for (i = 0, k = 0; i < l.ntp; i++) {
        if (k > 0 && l.tp[k - 1].tick == l.tp[i].tick) l.tp[k - 1] = l.tp[i];
        else l.tp[k++] = l.tp[i];
    }
    l.ntp = k;
    for (i = 1; i < l.ntp; i++)
        l.tp[i].at_num = l.tp[i - 1].at_num + (uint64_t)(l.tp[i].tick - l.tp[i - 1].tick) * l.tp[i - 1].us_qn;
    qsort(l.ev, l.n, sizeof(XgEv), ev_cmp);
    s->ev = l.ev; s->nev = l.n;
    s->tempo = l.tp; s->ntempo = l.ntp;
    return XG_OK;
}

void xg_smf_free(XgSong *s)
{
    free(s->ev);
    free(s->tempo);
    free(s->trk_off);
    memset(s, 0, sizeof *s);
}

uint32_t xg_tick_us(const XgSong *s, uint32_t tick)
{
    uint32_t lo = 0, hi = s->ntempo;
    /* the last tempo at or before `tick` */
    while (hi - lo > 1) {
        uint32_t mid = (lo + hi) / 2;
        if (s->tempo[mid].tick <= tick) lo = mid; else hi = mid;
    }
    return (uint32_t)((s->tempo[lo].at_num + (uint64_t)(tick - s->tempo[lo].tick) * s->tempo[lo].us_qn) / s->ppqn);
}

/* ---- writing ---------------------------------------------------------- */

typedef struct { uint8_t *b; size_t n, cap; } Buf;

static int bput(Buf *o, const uint8_t *b, size_t n)
{
    if (o->n + n > o->cap) {
        size_t cap = o->cap ? o->cap : 4096;
        uint8_t *nb;
        while (cap < o->n + n) cap *= 2;
        if (!(nb = (uint8_t *)realloc(o->b, cap))) return 0;
        o->b = nb; o->cap = cap;
    }
    memcpy(o->b + o->n, b, n);
    o->n += n;
    return 1;
}

static int bvar(Buf *o, uint32_t v)
{
    uint8_t t[5];
    int k = 4;
    t[k] = (uint8_t)(v & 0x7F);
    while ((v >>= 7) && k > 0) t[--k] = (uint8_t)(0x80 | (v & 0x7F));
    return bput(o, t + k, (size_t)(5 - k));
}

static int ins_cmp(const void *a, const void *b)
{
    const XgInsert *x = (const XgInsert *)a, *y = (const XgInsert *)b;
    if (x->trk != y->trk) return x->trk < y->trk ? -1 : 1;
    return x->tick < y->tick ? -1 : (x->tick > y->tick);
}

/* One inserted message (its delta from `*out_tick`). */
static int put_ins(Buf *o, const XgInsert *in, uint32_t *out_tick, uint8_t *emitted)
{
    uint32_t d = in->tick > *out_tick ? in->tick - *out_tick : 0;
    if (!bvar(o, d) || !bput(o, in->b, in->n)) return 0;
    if (in->tick > *out_tick) *out_tick = in->tick;
    *emitted = in->b[0];
    return 1;
}

/* Track data `d`..`end` with the insertions `in` (sorted by tick) into `o`. */
static int rewrite_track(Buf *o, const uint8_t *d, const uint8_t *end, const XgInsert *in, uint32_t nin)
{
    uint32_t tick = 0, out_tick = 0, k = 0;
    uint8_t running = 0, emitted = 0;
    const uint8_t *p = d;
    while (p < end) {
        const uint8_t *ev;
        uint32_t t = tick + varlen(&p, end);
        uint8_t st;
        size_t n;
        if (p >= end) break;
        tick = t;
        ev = p;
        st = *p;
        /* the insertions before this event (all the rest before the end of the track) */
        while (k < nin && (in[k].tick < t || (st == 0xFF && p + 1 < end && p[1] == 0x2F)))
            if (!put_ins(o, &in[k++], &out_tick, &emitted)) return 0;
        if (!bvar(o, tick > out_tick ? tick - out_tick : 0)) return 0;
        if (tick > out_tick) out_tick = tick;
        if (st == 0xFF || st == 0xF0 || st == 0xF7) {
            uint32_t len;
            p += st == 0xFF ? 2 : 1;
            len = varlen(&p, end);
            p += len;
            if (p > end) p = end;
            if (!bput(o, ev, (size_t)(p - ev))) return 0;
            if (st != 0xFF) running = 0;
            emitted = 0;
            continue;
        }
        if (st & 0x80) { running = st; p++; }
        n = ((running & 0xF0) == 0xC0 || (running & 0xF0) == 0xD0) ? 1 : 2;
        if ((size_t)(end - p) < n) n = (size_t)(end - p);
        /* a running status after an insertion (or a meta) is written out */
        if (running != emitted && !bput(o, &running, 1)) return 0;
        if (!bput(o, p, n)) return 0;
        emitted = running;
        p += n;
    }
    /* a track without an end: the rest at its end */
    while (k < nin)
        if (!put_ins(o, &in[k++], &out_tick, &emitted)) return 0;
    return 1;
}

uint8_t *xg_smf_rebuild(const XgSong *s, const uint8_t *patched, const XgInsert *ins, uint32_t nins, size_t *out_size)
{
    const uint8_t *src = patched ? patched : s->buf;
    XgInsert *sorted = NULL;
    Buf o = {NULL, 0, 0};
    uint32_t t, k = 0, first = s->ntrk ? s->trk_off[0] : (uint32_t)s->size;
    if (nins) {
        if (!(sorted = (XgInsert *)malloc(nins * sizeof(XgInsert)))) return NULL;
        memcpy(sorted, ins, nins * sizeof(XgInsert));
        qsort(sorted, nins, sizeof(XgInsert), ins_cmp);
    }
    if (!bput(&o, src, first)) goto fail;
    for (t = 0; t < s->ntrk; t++) {
        uint32_t off = s->trk_off[t], len = be32(src + off + 4), k0 = k;
        const uint8_t *d = src + off + 8, *e = d + len;
        if (e > src + s->size) e = src + s->size;
        while (k < nins && sorted[k].trk == t) k++;
        if (k == k0) {
            if (!bput(&o, src + off, (size_t)(e - (src + off)))) goto fail;
        } else {
            size_t hdr = o.n;
            uint32_t n;
            static const uint8_t h[8] = {'M', 'T', 'r', 'k', 0, 0, 0, 0};
            if (!bput(&o, h, 8) || !rewrite_track(&o, d, e, sorted + k0, k - k0)) goto fail;
            n = (uint32_t)(o.n - hdr - 8);
            o.b[hdr + 4] = (uint8_t)(n >> 24); o.b[hdr + 5] = (uint8_t)(n >> 16);
            o.b[hdr + 6] = (uint8_t)(n >> 8); o.b[hdr + 7] = (uint8_t)n;
        }
    }
    free(sorted);
    *out_size = o.n;
    return o.b;
fail:
    free(sorted);
    free(o.b);
    return NULL;
}
