/* MIDI output and timing. AmigaOS: serial.device at 31250 baud (a serial
 * MIDI interface) and timer.device; elsewhere: none (the PC build only
 * dumps the messages for the tests). */
#ifndef XG_MIDIOUT_H
#define XG_MIDIOUT_H

#include <stdint.h>

/* Open the output; 0 = failed (the message says why). */
int  mo_open(const char *device, long unit, char *msg, int msglen);
void mo_close(void);
/* Queue bytes; mo_flush sends what is queued. */
void mo_put(const uint8_t *b, uint32_t n);
void mo_flush(void);
/* Start the clock (time 0 = now). */
void mo_clock_start(void);
/* Wait until `us` after the clock start; 0 = Ctrl+C pressed. */
int  mo_wait_until(uint32_t us);

#endif
