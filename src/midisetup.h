/* The MIDI Setup window of XGWorks Lite (as the PC / web version's MIDI
 * Devices): the output - serial.device or CAMD -, the serial device and its
 * unit, the CAMD link, a Test note and the WinUAE / 88emu preset. */
#ifndef XG_MIDISETUP_H
#define XG_MIDISETUP_H

#include "app.h"

int  ms_open(App *a);
void ms_close(void);
int  ms_is_open(void);
unsigned long ms_signal(void);
int  ms_handle(App *a);
/* Show the settings again (changed elsewhere, e.g. the Songs window). */
void ms_refresh(App *a);

#endif
