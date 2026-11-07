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

static int track(Lists *l, const uint8_t *base, const uint8_t *p, const uint8_t *end, uint32_t *seq)
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
        if (memcmp(p, "MTrk", 4) == 0) r = track(&l, buf, d, e, &seq);
        p = e;
    }
    if (r != XG_OK) { free(l.ev); free(l.tp); return r; }
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
