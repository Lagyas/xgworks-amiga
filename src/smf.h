/* XGWorks Lite for AmigaOS 3.x - Standard MIDI File reading and the
 * playback order. Plain C99, no AmigaOS calls: the same code is built for
 * the Amiga and for the PC tests (compared with the Rust core of XGWorks
 * NextGen). */
#ifndef XG_SMF_H
#define XG_SMF_H

#include <stddef.h>
#include <stdint.h>

/* One message to send. Short messages are kept inline; SysEx (status F0 or
 * F7) points into the file buffer. */
typedef struct {
    uint32_t tick;
    uint32_t seq;     /* file order: keeps the sort stable */
    uint32_t off;     /* SysEx: the data after the length, in the file buffer */
    uint32_t len;     /* SysEx: data length (without the status byte) */
    uint8_t  cls;     /* order within a tick: 0 reset, 1 other, 2 note-on */
    uint8_t  n;       /* bytes in msg (1..3); 0 = SysEx */
    uint8_t  msg[3];  /* short message; msg[0] = F0 / F7 for SysEx */
} XgEv;

typedef struct {
    uint32_t tick;
    uint32_t us_qn;   /* microseconds per quarter note */
    uint64_t at_num;  /* time of this tempo change in us * ppqn (exact, no rounding drift) */
} XgTempo;

typedef struct {
    const uint8_t *buf;
    size_t   size;
    uint16_t format, ntracks, ppqn;
    XgEv    *ev;
    uint32_t nev;
    XgTempo *tempo;
    uint32_t ntempo;
} XgSong;

/* Errors of xg_smf_load. */
enum { XG_OK = 0, XG_ENOTSMF, XG_ESMPTE, XG_ENOMEM, XG_EBROKEN };

/* Read `buf` (kept by the caller while the song is used). */
int  xg_smf_load(XgSong *s, const uint8_t *buf, size_t size);
void xg_smf_free(XgSong *s);
const char *xg_error(int e);

/* Time of a tick in microseconds (tempo map). */
uint32_t xg_tick_us(const XgSong *s, uint32_t tick);

/* XG / GM / GS System On (or XG All Parameter Reset): sent first within its tick. */
int xg_is_reset(const uint8_t *sysex_after_f0, uint32_t len);

#endif
