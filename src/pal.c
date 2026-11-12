/* XGWorks Lite: the colours of the PC / web version (see pal.h). */
#include <exec/types.h>
#include <graphics/view.h>
#include <intuition/screens.h>
#include <proto/graphics.h>
#include "pal.h"

static const ULONG rgb[PAL_N] = {
    0xFFFFFF, 0x000000, 0x004080, 0x660066, 0x828282, 0x6666FF, 0xDC0000, 0x0000DC,
    0x2828FF, 0x2060A0, 0xFFCC66, 0xC8C8C8, 0xECECEC, 0x787878, 0xE1E1E1, 0xCDCDCD,
    0xD6A028, 0xC84038, 0x00DCDC,
    0x1428E6, 0x28AA3C, 0x464646, 0xFF6E14, 0xDC28DC, 0x46A0FF,
};
static LONG pen[PAL_N];
static struct ColorMap *cm;
static int users;

void pal_open(struct Screen *scr)
{
    int c;
    if (users++) return;
    cm = scr->ViewPort.ColorMap;
    for (c = 0; c < PAL_N; c++) {
        ULONG r = (rgb[c] >> 16) & 255, g = (rgb[c] >> 8) & 255, b = rgb[c] & 255;
        pen[c] = (cm && scr->RastPort.BitMap->Depth >= 4)
            ? ObtainBestPen(cm, r * 0x01010101UL, g * 0x01010101UL, b * 0x01010101UL, OBP_Precision, PRECISION_GUI, TAG_DONE) : -1;
    }
}

void pal_close(void)
{
    int c;
    if (!users || --users) return;
    for (c = 0; c < PAL_N; c++) {
        if (pen[c] >= 0 && cm) ReleasePen(cm, (ULONG)pen[c]);
        pen[c] = -1;
    }
    cm = NULL;
}

UWORD pal(int c, UWORD def) { return users && pen[c] >= 0 ? (UWORD)pen[c] : def; }
int pal_ok(void) { return users && pen[PAL_WHITE] >= 0; }
