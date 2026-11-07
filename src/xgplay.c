/* XGPlay - XGWorks Lite stage 1: plays a Standard MIDI File on a MIDI
 * module from the AmigaOS Shell.
 *
 *   XGPlay FILE/A,RESET/K,DEVICE/K,UNIT/K/N,DUMP/S,QUIET/S
 *
 *   RESET=XG|GM|GS|OFF  a System On before the song (default OFF: the song's own)
 *   DEVICE / UNIT       the serial device (default serial.device unit 0)
 *   DUMP                print the messages with their times instead of playing
 *
 * Ctrl+C stops; every channel gets All Sound Off / Reset All Controllers.
 * Built on the PC as well (DUMP only), for the tests against the Rust core. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smf.h"
#include "midiout.h"

#ifdef __amigaos__
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
static const char version[] = "$VER: XGPlay 0.1 (07.11.2026) XGWorks Lite";
#endif

static const uint8_t XG_ON[] = {0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7};
static const uint8_t GM_ON[] = {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7};
static const uint8_t GS_ON[] = {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7};

typedef struct {
    const char *file, *reset, *device;
    long unit;
    int dump, quiet;
} Opts;

static int read_file(const char *name, uint8_t **out, size_t *size)
{
    FILE *f = fopen(name, "rb");
    long n;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0 || !(*out = (uint8_t *)malloc((size_t)n))) { fclose(f); return 0; }
    *size = fread(*out, 1, (size_t)n, f);
    fclose(f);
    return *size == (size_t)n;
}

/* The bytes of event `e` into `b` (SysEx: status + data); returns the count. */
static const uint8_t *ev_bytes(const XgSong *s, const XgEv *e, uint32_t *n)
{
    if (e->n) { *n = e->n; return e->msg; }
    *n = e->len;
    return s->buf + e->off;
}

static void dump(const XgSong *s, const uint8_t *reset, uint32_t rlen)
{
    uint32_t i, k, n;
    if (reset) {
        printf("0");
        for (k = 0; k < rlen; k++) printf(" %02X", reset[k]);
        printf("\n");
    }
    for (i = 0; i < s->nev; i++) {
        const XgEv *e = &s->ev[i];
        const uint8_t *b = ev_bytes(s, e, &n);
        printf("%lu", (unsigned long)xg_tick_us(s, e->tick));
        if (!e->n) printf(" %02X", e->msg[0]);
        for (k = 0; k < n; k++) printf(" %02X", b[k]);
        printf("\n");
    }
}

static void all_off(void)
{
    uint8_t m[3];
    int c;
    for (c = 0; c < 16; c++) {
        m[0] = (uint8_t)(0xB0 | c);
        m[1] = 0x78; m[2] = 0; mo_put(m, 3);   /* All Sound Off */
        m[1] = 0x79; mo_put(m, 3);             /* Reset All Controllers */
        m[1] = 0x7B; mo_put(m, 3);             /* All Notes Off */
    }
    mo_flush();
}

static int play(const XgSong *s, const Opts *o, const uint8_t *reset, uint32_t rlen)
{
    char msg[160];
    uint32_t i = 0, n, last_s = 0xFFFFFFFF, start_us = 0;
    int ok = 1;
    if (!mo_open(o->device, o->unit, msg, sizeof msg)) {
        printf("XGPlay: %s\n", msg);
        mo_close();
        return 20;
    }
    if (reset) {
        mo_put(reset, rlen);
        mo_flush();
        start_us = 200000;  /* the module needs a moment after a System On */
    }
    mo_clock_start();
    while (i < s->nev && ok) {
        uint32_t t = xg_tick_us(s, s->ev[i].tick);
        if (!(ok = mo_wait_until(start_us + t))) break;
        /* everything due now in one write */
        while (i < s->nev && xg_tick_us(s, s->ev[i].tick) <= t) {
            const XgEv *e = &s->ev[i++];
            const uint8_t *b = ev_bytes(s, e, &n);
            if (!e->n) mo_put(&e->msg[0], 1);
            mo_put(b, n);
        }
        mo_flush();
        if (!o->quiet && t / 1000000 != last_s) {
            last_s = t / 1000000;
            printf("\r%lu:%02lu ", (unsigned long)(last_s / 60), (unsigned long)(last_s % 60));
            fflush(stdout);
        }
    }
    all_off();
    mo_close();
    if (!o->quiet) printf(ok ? "\nEnd.\n" : "\n*** Break\n");
    return ok ? 0 : 5;
}

static int run(const Opts *o)
{
    uint8_t *data = NULL;
    size_t size = 0;
    XgSong s;
    const uint8_t *reset = NULL;
    uint32_t rlen = 0;
    int r;
    if (o->reset && !strcmp(o->reset, "XG")) { reset = XG_ON; rlen = sizeof XG_ON; }
    else if (o->reset && !strcmp(o->reset, "GM")) { reset = GM_ON; rlen = sizeof GM_ON; }
    else if (o->reset && !strcmp(o->reset, "GS")) { reset = GS_ON; rlen = sizeof GS_ON; }
    else if (o->reset && strcmp(o->reset, "OFF")) { printf("XGPlay: RESET=XG, GM, GS or OFF\n"); return 10; }
    if (!read_file(o->file, &data, &size)) { printf("XGPlay: %s cannot be read\n", o->file); return 20; }
    if ((r = xg_smf_load(&s, data, size)) != XG_OK) {
        printf("XGPlay: %s: %s\n", o->file, xg_error(r));
        free(data);
        return 20;
    }
    if (o->dump) {
        dump(&s, reset, rlen);
        r = 0;
    } else {
        if (!o->quiet) {
            uint32_t len = s.nev ? xg_tick_us(&s, s.ev[s.nev - 1].tick) / 1000000 : 0;
            printf("%s: format %u, %u tracks, %lu messages, %lu:%02lu\n", o->file, s.format, s.ntracks, (unsigned long)s.nev, (unsigned long)(len / 60), (unsigned long)(len % 60));
        }
        r = play(&s, o, reset, rlen);
    }
    xg_smf_free(&s);
    free(data);
    return r;
}

#ifdef __amigaos__
int main(void)
{
    LONG a[6] = {0};
    struct RDArgs *rd = ReadArgs((CONST_STRPTR)"FILE/A,RESET/K,DEVICE/K,UNIT/K/N,DUMP/S,QUIET/S", a, NULL);
    Opts o;
    int r;
    (void)version;
    if (!rd) { PrintFault(IoErr(), (CONST_STRPTR)"XGPlay"); return 10; }
    o.file = (const char *)a[0];
    o.reset = a[1] ? (const char *)a[1] : NULL;
    o.device = a[2] ? (const char *)a[2] : "serial.device";
    o.unit = a[3] ? *(LONG *)a[3] : 0;
    o.dump = a[4] != 0;
    o.quiet = a[5] != 0;
    {
        /* RESET is matched case-blind as in every Amiga command */
        static char rs[8];
        if (o.reset) {
            int k;
            for (k = 0; k < 7 && o.reset[k]; k++) rs[k] = (char)(o.reset[k] >= 'a' && o.reset[k] <= 'z' ? o.reset[k] - 32 : o.reset[k]);
            rs[k] = 0;
            o.reset = rs;
        }
    }
    r = run(&o);
    FreeArgs(rd);
    return r;
}
#else
/* PC build: XGPlay <file> [RESET=XG|GM|GS|OFF] DUMP */
int mo_open(const char *d, long u, char *m, int n) { (void)d; (void)u; snprintf(m, n, "the PC build has no MIDI output (use DUMP)"); return 0; }
void mo_close(void) {}
void mo_put(const uint8_t *b, uint32_t n) { (void)b; (void)n; }
void mo_flush(void) {}
void mo_clock_start(void) {}
int mo_wait_until(uint32_t us) { (void)us; return 1; }

int main(int argc, char **argv)
{
    Opts o;
    int i;
    memset(&o, 0, sizeof o);
    o.device = "serial.device";
    for (i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "RESET=", 6)) o.reset = argv[i] + 6;
        else if (!strcmp(argv[i], "DUMP")) o.dump = 1;
        else if (!strcmp(argv[i], "QUIET")) o.quiet = 1;
        else o.file = argv[i];
    }
    if (!o.file) { printf("XGPlay <file> [RESET=XG|GM|GS|OFF] DUMP\n"); return 10; }
    return run(&o);
}
#endif
