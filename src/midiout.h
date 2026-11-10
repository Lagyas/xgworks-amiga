/* MIDI output and timing. AmigaOS: serial.device at 31250 baud (a serial
 * MIDI interface) or camd.library, timing from timer.device; elsewhere:
 * none (the PC build only dumps the messages for the tests). */
#ifndef XG_MIDIOUT_H
#define XG_MIDIOUT_H

#include <stdint.h>

/* Open the output: `device` = "camd" (link out.<unit>) or a serial device
 * (unit = its unit); 0 = failed (the message says why). */
int  mo_open(const char *device, long unit, char *msg, int msglen);
void mo_close(void);
/* One message: status + data (SysEx: status F0 / F7 + the data after it). */
void mo_msg(uint8_t status, const uint8_t *data, uint32_t n);
/* Send what is queued. */
void mo_flush(void);
/* Start the clock (time 0 = now); the time since then. */
void mo_clock_start(void);
uint32_t mo_now_us(void);
/* Wait until `us` after the clock start; 0 = Ctrl+C (stop) arrived,
 * 2 = Ctrl+D (something to send now) arrived, 1 = the time came. */
int  mo_wait_until(uint32_t us);

#endif
