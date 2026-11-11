/* XGworks songs (.XWS): read as a Standard MIDI File (as XGWorks NextGen
 * exports them), their blocks and track headers, and writing an .XWS. */
#ifndef XG_XWS_H
#define XG_XWS_H

#include <stddef.h>
#include <stdint.h>
#include "edit.h"

/* An .XWS file (starts with the S4WH chunk)? */
int xws_is(const uint8_t *d, size_t n);
/* The song as a format-1 MIDI file (malloc'ed; NULL = no memory). */
uint8_t *xws_to_smf(const uint8_t *d, size_t n, size_t *out_n);
/* The headers and blocks of the .XWS's tracks onto the song read from xws_to_smf (same order). */
int xws_attach_blocks(const uint8_t *d, size_t n, EdSong *s);
/* The song as an .XWS (malloc'ed; NULL = no memory). */
uint8_t *xws_write(const EdSong *s, size_t *out_n);

#endif
