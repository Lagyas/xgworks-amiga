/* XGWorks Lite: the MIDI Setup window (see midisetup.h). A change is taken
 * at once (the next message opens the new output; a song playing keeps its
 * output until Stop) and saved with the settings. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include "midisetup.h"
#include "screen.h"

extern struct Library *GadToolsBase;

enum { G_OUT = 1, G_DEV, G_UNIT, G_LINK, G_TEST, G_WINUAE, G_DEFAULT, G_N };
static const char *const OUT_LABELS[] = {"serial.device (serial MIDI interface)", "CAMD (camd.library)", NULL};

static struct Window *mw;
static struct Gadget *mgl, *gad[G_N];
static void *mvi;

int ms_is_open(void) { return mw != NULL; }
unsigned long ms_signal(void) { return mw ? 1UL << mw->UserPort->mp_SigBit : 0; }

static void ghost(App *a)
{
    if (!mw) return;
    GT_SetGadgetAttrs(gad[G_DEV], mw, NULL, GA_Disabled, a->output == 1, TAG_DONE);
    GT_SetGadgetAttrs(gad[G_LINK], mw, NULL, GA_Disabled, a->output != 1, TAG_DONE);
}

void ms_refresh(App *a)
{
    if (!mw) return;
    GT_SetGadgetAttrs(gad[G_OUT], mw, NULL, GTCY_Active, a->output, TAG_DONE);
    GT_SetGadgetAttrs(gad[G_DEV], mw, NULL, GTST_String, (ULONG)a->serdev, TAG_DONE);
    GT_SetGadgetAttrs(gad[G_UNIT], mw, NULL, GTIN_Number, a->unit, TAG_DONE);
    GT_SetGadgetAttrs(gad[G_LINK], mw, NULL, GTST_String, (ULONG)a->camdlink, TAG_DONE);
    ghost(a);
}

int ms_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, ih, x0, y, w, lw;
    if (mw) { WindowToFront(mw); ActivateWindow(mw); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    mvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    ih = (WORD)(fh + 6);
    lw = (WORD)(14 * 8);           /* the labels on the left */
    x0 = (WORD)(scr->WBorLeft + 8 + lw);
    w = 300;
    y = (WORD)(scr->WBorTop + fh + 1 + 6);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = mvi;
    ng.ng_Height = ih;
    g = CreateContext(&mgl);

    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_GadgetText = (UBYTE *)"MIDI Out"; ng.ng_GadgetID = G_OUT;
    g = gad[G_OUT] = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)OUT_LABELS, GTCY_Active, a->output, TAG_DONE);
    y = (WORD)(y + ih + 4);
    ng.ng_TopEdge = y; ng.ng_Width = 200; ng.ng_GadgetText = (UBYTE *)"Serial device"; ng.ng_GadgetID = G_DEV;
    g = gad[G_DEV] = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)a->serdev, GTST_MaxChars, sizeof a->serdev - 1, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(x0 + 250); ng.ng_Width = 50; ng.ng_GadgetText = (UBYTE *)"Unit"; ng.ng_GadgetID = G_UNIT;
    g = gad[G_UNIT] = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, a->unit, GTIN_MaxChars, 3, TAG_DONE);
    y = (WORD)(y + ih + 4);
    ng.ng_LeftEdge = x0; ng.ng_TopEdge = y; ng.ng_Width = 200; ng.ng_GadgetText = (UBYTE *)"CAMD link"; ng.ng_GadgetID = G_LINK;
    g = gad[G_LINK] = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)a->camdlink, GTST_MaxChars, sizeof a->camdlink - 1, TAG_DONE);
    y = (WORD)(y + ih + 4);
    ng.ng_TopEdge = y; ng.ng_Width = w; ng.ng_GadgetText = (UBYTE *)"MIDI In"; ng.ng_GadgetID = 0;
    g = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"- (comes with recording)", GTTX_Border, TRUE, TAG_DONE);
    y = (WORD)(y + ih + 10);
    ng.ng_Flags = PLACETEXT_IN; ng.ng_TopEdge = y;
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 8); ng.ng_Width = 110; ng.ng_GadgetText = (UBYTE *)"Test note"; ng.ng_GadgetID = G_TEST;
    g = gad[G_TEST] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 8 + 118); ng.ng_Width = 150; ng.ng_GadgetText = (UBYTE *)"WinUAE / 88emu"; ng.ng_GadgetID = G_WINUAE;
    g = gad[G_WINUAE] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 8 + 276); ng.ng_Width = (WORD)(x0 + w - (scr->WBorLeft + 8 + 276)); ng.ng_GadgetText = (UBYTE *)"Default"; ng.ng_GadgetID = G_DEFAULT;
    g = gad[G_DEFAULT] = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    if (!g) {
        FreeGadgets(mgl); mgl = NULL; FreeVisualInfo(mvi); mvi = NULL; UnlockPubScreen(NULL, scr);
        return 0;
    }
    mw = OpenWindowTags(NULL, WA_Title, (ULONG)"MIDI Setup", WA_Left, 40, WA_Top, 40,
        WA_InnerWidth, x0 + w + 8 - scr->WBorLeft, WA_InnerHeight, y + ih + 8 - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)mgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_SimpleRefresh, TRUE, WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY | CYCLEIDCMP | STRINGIDCMP | INTEGERIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!mw) {
        FreeGadgets(mgl); mgl = NULL; FreeVisualInfo(mvi); mvi = NULL;
        return 0;
    }
    GT_RefreshWindow(mw, NULL);
    ghost(a);
    return 1;
}

void ms_close(void)
{
    if (!mw) return;
    CloseWindow(mw);
    mw = NULL;
    FreeGadgets(mgl); mgl = NULL;
    FreeVisualInfo(mvi); mvi = NULL;
}

static void take_strings(App *a)
{
    const char *d = (const char *)((struct StringInfo *)gad[G_DEV]->SpecialInfo)->Buffer;
    const char *l = (const char *)((struct StringInfo *)gad[G_LINK]->SpecialInfo)->Buffer;
    strncpy(a->serdev, *d ? d : "serial.device", sizeof a->serdev - 1);
    a->serdev[sizeof a->serdev - 1] = 0;
    strncpy(a->camdlink, *l ? l : "out.0", sizeof a->camdlink - 1);
    a->camdlink[sizeof a->camdlink - 1] = 0;
    a->unit = ((struct StringInfo *)gad[G_UNIT]->SpecialInfo)->LongInt;
    if (a->unit < 0) a->unit = 0;
}

int ms_handle(App *a)
{
    struct IntuiMessage *m;
    int close = 0;
    while (mw && (m = GT_GetIMsg(mw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW: close = 1; break;
        case IDCMP_REFRESHWINDOW: GT_BeginRefresh(mw); GT_EndRefresh(mw, TRUE); break;
        case IDCMP_VANILLAKEY: if (code == 27) close = 1; break;
        case IDCMP_GADGETUP:
            switch (src->GadgetID) {
            case G_OUT: a->output = code; take_strings(a); app_output_changed(a); ghost(a); break;
            case G_DEV: case G_UNIT: case G_LINK: take_strings(a); app_output_changed(a); break;
            case G_TEST:
                take_strings(a);
                a->info[0] = 0;
                app_note(a, 0, 60, 100);
                Delay(20);
                app_note(a, 0, 60, 0);
                if (!a->info[0]) snprintf(a->info, sizeof a->info, "Test note sent (C4, channel 1).");
                break;
            case G_WINUAE:
            case G_DEFAULT:
                a->output = 0;
                strcpy(a->serdev, "serial.device");
                a->unit = 0;
                strcpy(a->camdlink, "out.0");
                ms_refresh(a);
                app_output_changed(a);
                if (src->GadgetID == G_WINUAE)
                    snprintf(a->info, sizeof a->info, "WinUAE: IO ports - Serial port on, MIDI Out = the loopMIDI port of 88emuPlayer.");
                break;
            }
            break;
        }
    }
    if (close) ms_close();
    return 0;
}
