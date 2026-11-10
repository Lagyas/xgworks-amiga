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
    uint32_t off;     /* in the file: SysEx data after the length; short message: its first data byte */
    uint32_t len;     /* SysEx: data length (without the status byte) */
    uint16_t trk;     /* the track (0 = first MTrk) */
    uint8_t  cls;     /* order within a tick: 0 reset, 1 other, 2 note-on */
    uint8_t  n;       /* bytes in msg (1..3); 0 = SysEx */
    uint8_t  msg[3];  /* short message; msg[0] = F0 / F7 for SysEx */
} XgEv;

typedef struct {
    uint32_t tick;
    uint32_t us_qn;   /* microseconds per quarter note */
    uint64_t at_num;  /* time of this tempo change in us * ppqn (exact, no rounding drift) */
} XgTempo;

/* A text event: lyric (FF 05), text (FF 01) or XF chord (FF 7F 43 7B 01 …). */
typedef struct {
    uint32_t tick;
    uint32_t off;     /* its data in the file */
    uint16_t len;
    uint16_t trk;
    uint8_t  type;    /* 0x05, 0x01, 0x7F */
} XgText;

typedef struct {
    const uint8_t *buf;
    size_t   size;
    uint16_t format, ntracks, ppqn;
    XgEv    *ev;
    uint32_t nev;
    XgTempo *tempo;
    uint32_t ntempo;
    uint32_t *trk_off;  /* each MTrk chunk: offset of its header */
    uint32_t ntrk;
    XgText  *txt;       /* by time */
    uint32_t ntxt;
} XgSong;

/* Errors of xg_smf_load. */
enum { XG_OK = 0, XG_ENOTSMF, XG_ESMPTE, XG_ENOMEM, XG_EBROKEN };

/* Read `buf` (kept by the caller while the song is used). */
int  xg_smf_load(XgSong *s, const uint8_t *buf, size_t size);
void xg_smf_free(XgSong *s);
const char *xg_error(int e);

/* Time of a tick in microseconds (tempo map), and back. */
uint32_t xg_tick_us(const XgSong *s, uint32_t tick);
uint32_t xg_us_tick(const XgSong *s, uint32_t us);

/* XG / GM / GS System On (or XG All Parameter Reset): sent first within its tick. */
int xg_is_reset(const uint8_t *sysex_after_f0, uint32_t len);

/* One message to put into a track at a tick (status + data, or F0 + length-less data). */
typedef struct {
    uint16_t trk;
    uint32_t tick;
    uint8_t  n;
    uint8_t  b[12];
} XgInsert;

/* A new file: `s`'s bytes with `patched` (same size as the file, or NULL =
 * the song's own) and the messages of `ins` put in; NULL = no memory.
 * Tracks with insertions are rewritten with explicit status bytes where a
 * running status would no longer be right. Only channel messages may be inserted. */
uint8_t *xg_smf_rebuild(const XgSong *s, const uint8_t *patched, const XgInsert *ins, uint32_t nins, size_t *out_size);

#endif
