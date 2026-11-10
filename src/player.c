/* The player process (see player.h). */
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "player.h"
#include "midiout.h"

static Player *cur;             /* the one the process plays */
static struct Task *owner;      /* the window task */
static BYTE sigbit = -1;
static struct Process *proc;

int pl_init(Player *p)
{
    memset(p, 0, sizeof *p);
    p->tempo = 100;
    strcpy(p->device, "serial.device");
    owner = FindTask(NULL);
    sigbit = AllocSignal(-1);
    return sigbit >= 0;
}

unsigned long pl_signal(void)
{
    return sigbit >= 0 ? 1UL << sigbit : 0;
}

void pl_free(Player *p)
{
    pl_stop(p);
    if (sigbit >= 0) FreeSignal(sigbit);
    sigbit = -1;
}

static void tell(void)
{
    Signal(owner, 1UL << sigbit);
}

static int is_note(uint8_t st)
{
    uint8_t k = st & 0xF0;
    return k == 0x80 || k == 0x90 || k == 0xA0;
}

static void send(const Player *p, const XgSong *s, const XgEv *e, int transpose)
{
    uint8_t m[3], out[3];
    if (!e->n) {
        if (p->mixer && e->msg[0] == 0xF0 && !mx_filter_sysex(p->mixer, s->buf + e->off, e->len)) return;
        mo_msg(e->msg[0], s->buf + e->off, e->len);
        return;
    }
    memcpy(m, e->msg, 3);
    if (transpose && is_note(m[0]) && (m[0] & 0x0F) != 9) {
        int k = m[1] + transpose;
        if (k < 0 || k > 127) return;
        m[1] = (uint8_t)k;
    }
    if (e->trk < 64 && p->tmute[e->trk] && (m[0] & 0xF0) == 0x90 && m[2]) return;
    if (p->mixer && !mx_filter(p->mixer, m, e->n, out)) return;
    mo_msg(p->mixer ? out[0] : m[0], p->mixer ? out + 1 : m + 1, e->n - 1u);
}

/* what the window queued */
static void drain(Player *p)
{
    while (p->qr != p->qw) {
        const uint8_t *m = p->q[p->qr % 64];
        mo_msg(m[0], m + 1, p->qn[p->qr % 64] - 1u);
        p->qr++;
    }
    mo_flush();
}

int pl_queue(Player *p, const uint8_t *msg, int n)
{
    if (p->qw - p->qr >= 64 || n < 1 || n > 16) return 0;
    memcpy(p->q[p->qw % 64], msg, (size_t)n);
    p->qn[p->qw % 64] = (uint8_t)n;
    p->qw++;
    Forbid();
    if (p->running && proc) Signal((struct Task *)proc, SIGBREAKF_CTRL_D);
    Permit();
    return 1;
}

static void notes_off(void)
{
    static const uint8_t ano[2] = {0x7B, 0}, aso[2] = {0x78, 0};
    int c;
    for (c = 0; c < 16; c++) {
        mo_msg((uint8_t)(0xB0 | c), aso, 2);
        mo_msg((uint8_t)(0xB0 | c), ano, 2);
    }
    mo_flush();
}

/* song time -> real time and back, around an anchor (tempo changes re-anchor) */
typedef struct { uint32_t real, song; int pct; } Anchor;
static uint32_t to_real(const Anchor *a, uint32_t song)
{
    if (song <= a->song) return a->real;
    return a->real + (uint32_t)(((uint64_t)(song - a->song) * 100) / (uint32_t)a->pct);
}
static uint32_t to_song(const Anchor *a, uint32_t real)
{
    if (real <= a->real) return a->song;
    return a->song + (uint32_t)(((uint64_t)(real - a->real) * (uint32_t)a->pct) / 100);
}

static void play(Player *p)
{
    const XgSong *s = p->song;
    uint32_t i = 0, last_tell = 0;
    int tr, ok = 1;
    Anchor a;
    if (!mo_open(p->device, p->unit, p->error, sizeof p->error)) {
        mo_close();
        return;
    }
    a.real = 0;
    if (p->reset) {
        mo_msg(p->reset[0], p->reset + 1, p->rlen - 1);
        mo_flush();
        a.real = 200000;  /* the module needs a moment after a System On */
    }
    /* from the middle: the settings before that point (no notes) */
    while (i < s->nev && xg_tick_us(s, s->ev[i].tick) < p->from_us) {
        if (s->ev[i].n == 0 || !is_note(s->ev[i].msg[0])) send(p, s, &s->ev[i], 0);
        i++;
    }
    /* the mixer's values (they win over the song's) */
    if (p->mixer) {
        uint8_t m[16 * 8][3];
        int k, n = mx_setup(p->mixer, m, 16 * 8);
        uint8_t sx[FX_N][16];
        int len[FX_N];
        for (k = 0; k < n; k++) mo_msg(m[k][0], m[k] + 1, (m[k][0] & 0xF0) == 0xC0 ? 1 : 2);
        n = mx_setup_sysex(p->mixer, sx, len, FX_N);
        for (k = 0; k < n; k++) mo_msg(0xF0, sx[k] + 1, (uint32_t)len[k] - 1);
    }
    p->qr = p->qw;
    mo_flush();
    mo_clock_start();
    a.song = p->from_us;
    a.pct = p->tempo;
    tr = p->transpose;
    p->pos_us = p->from_us;
    while (i < s->nev && ok) {
        uint32_t t = xg_tick_us(s, s->ev[i].tick), due, now;
        if (p->qr != p->qw) drain(p);
        if (p->tempo != a.pct) {
            now = mo_now_us();
            a.song = to_song(&a, now);
            a.real = now;
            a.pct = p->tempo;
        }
        if (p->transpose != tr) {
            notes_off();
            tr = p->transpose;
        }
        due = to_real(&a, t);
        now = mo_now_us();
        if (due > now + 100000) {
            /* a long rest: wake up every 0.1 s for the position and changes */
            ok = mo_wait_until(now + 100000);
            p->pos_us = to_song(&a, mo_now_us());
            tell();
            continue;
        }
        if (!(ok = mo_wait_until(due))) break;
        if (ok == 2) continue;  /* the window has something to send: then on */
        while (i < s->nev && xg_tick_us(s, s->ev[i].tick) <= t) send(p, s, &s->ev[i++], tr);
        mo_flush();
        p->pos_us = t;
        if (t - last_tell >= 100000 || t < last_tell) {
            last_tell = t;
            tell();
        }
    }
    notes_off();
    p->ended = ok && i >= s->nev;
    mo_close();
}

static void entry(void)
{
    Player *p = cur;
    play(p);
    /* the window task may unload the program as soon as it sees this:
       nothing of ours may run after it, so stay in Forbid until we end */
    Forbid();
    p->running = 0;
    tell();
}

int pl_start(Player *p)
{
    pl_stop(p);
    p->error[0] = 0;
    p->ended = 0;
    p->running = 1;
    cur = p;
    SetSignal(0, pl_signal());
    proc = CreateNewProcTags(NP_Entry, (ULONG)entry, NP_Name, (ULONG)"XGWorks Lite player", NP_Priority, 20, NP_StackSize, 8192, TAG_DONE);
    if (!proc) {
        p->running = 0;
        strcpy(p->error, "the player process cannot start");
        return 0;
    }
    return 1;
}

void pl_stop(Player *p)
{
    if (!p->running) return;
    Forbid();
    if (p->running && proc) Signal((struct Task *)proc, SIGBREAKF_CTRL_C);
    Permit();
    while (p->running) Wait(pl_signal());
    /* the end signal may have been eaten above: give it back for the window */
    SetSignal(pl_signal(), pl_signal());
    proc = NULL;
}
