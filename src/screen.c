/* XGWorks Lite: its own screen with more colours (see screen.h). */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/displayinfo.h>
#include <graphics/modeid.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include "screen.h"

#define PUBNAME "XGWorksLite"

const char *const scr_names[SCREENS + 1] = {"Workbench", "16 colours", "32 colours", "256 colours", NULL};
static const int depths[SCREENS] = {0, 4, 5, 8};
static struct Screen *own;

const char *scr_pubname(void) { return own ? PUBNAME : NULL; }

int scr_open(int setting)
{
    struct Screen *wb;
    struct DimensionInfo dims;
    struct TextAttr ta;
    char fname[64];
    ULONG mode;
    int depth, wbdepth, maxd = 8;
    static UWORD pens[] = {(UWORD)~0};
    if (own) return setting;
    if (setting <= 0 || setting >= SCREENS) return 0;
    if (!(wb = LockPubScreen((CONST_STRPTR)"Workbench"))) return 0;
    mode = GetVPModeID(&wb->ViewPort);
    wbdepth = wb->RastPort.BitMap->Depth;
    if (mode != INVALID_ID && GetDisplayInfoData(NULL, (UBYTE *)&dims, sizeof dims, DTAG_DIMS, mode))
        maxd = dims.MaxDepth;
    depth = depths[setting];
    if (depth > maxd) depth = maxd;
    /* the Workbench has these colours already (or more: an RTG screen) */
    if (depth <= wbdepth) { UnlockPubScreen(NULL, wb); return 0; }
    /* the Workbench's font */
    ta = *wb->Font;
    {
        int i = 0;
        const char *n = (const char *)wb->Font->ta_Name;
        while (n && n[i] && i < (int)sizeof fname - 1) { fname[i] = n[i]; i++; }
        fname[i] = 0;
        ta.ta_Name = (STRPTR)fname;
    }
    own = OpenScreenTags(NULL,
        SA_LikeWorkbench, TRUE,
        SA_DisplayID, mode,
        SA_Width, wb->Width, SA_Height, wb->Height,
        SA_Depth, depth,
        SA_Font, (ULONG)&ta,
        SA_Pens, (ULONG)pens,
        SA_SharePens, TRUE,
        SA_Interleaved, TRUE,
        SA_Title, (ULONG)"XGWorks Lite",
        SA_PubName, (ULONG)PUBNAME,
        TAG_DONE);
    UnlockPubScreen(NULL, wb);
    if (!own) return 0;
    PubScreenStatus(own, 0);    /* public: the windows of the program find it by its name */
    return setting;
}

void scr_close(void)
{
    int k;
    if (!own) return;
    /* private again (no new windows), then closed when its last window is gone */
    PubScreenStatus(own, PSNF_PRIVATE);
    for (k = 0; k < 100; k++) {
        if (CloseScreen(own)) { own = NULL; return; }
        Delay(10);
    }
    /* other programs' windows are still on it: it stays (public again) */
    PubScreenStatus(own, 0);
    own = NULL;
}
