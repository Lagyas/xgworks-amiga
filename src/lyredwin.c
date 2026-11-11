/* XGWorks Lite: the Lyrics editor - the song's lyrics line by line (as the
 * karaoke window shows them). Select a line, change its text below and press
 * Return: with the same number of syllables they keep their times, else
 * they go onto the notes of the line. Typed: "-" between syllables of a
 * word; \- \/ \< are those characters, \xHH a byte the letters cannot show. */
#include <exec/types.h>
#include <exec/lists.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lyredwin.h"
#include "lyrtools.h"

extern struct Library *GadToolsBase;

enum { Y_LIST = 1, Y_EDIT, Y_APPLY, Y_UNDO, Y_INFO };

static struct Window *yw;
static struct Gadget *ygl, *g_list, *g_edit, *g_info;
static void *yvi;
static struct List ylist;
static struct Node *ynodes;
static char (*ytext)[96];
static uint32_t *yidx;
static LtLine *ylines;
static int ynl, ysel = -1, ytrk = -1;
static char yline[300], yinfo[120], yshown[300];

int yw_is_open(void) { return yw != NULL; }
unsigned long yw_signal(void) { return yw ? 1UL << yw->UserPort->mp_SigBit : 0; }

static void info(const char *t)
{
    snprintf(yinfo, sizeof yinfo, "%s", t);
    if (yw) GT_SetGadgetAttrs(g_info, yw, NULL, GTTX_Text, (ULONG)yinfo, TAG_DONE);
}

static void fill(App *a)
{
    int L;
    uint32_t total = 0;
    if (!yw) return;
    GT_SetGadgetAttrs(g_list, yw, NULL, GTLV_Labels, ~0UL, TAG_DONE);
    free(ynodes); free(ytext); free(yidx); free(ylines);
    ynodes = NULL; ytext = NULL; yidx = NULL; ylines = NULL; ynl = 0;
    NewList(&ylist);
    ytrk = lt_lyrics_track(&a->ed);
    if (ytrk >= 0) {
        total = a->ed.trk[ytrk].n + 1;
        yidx = (uint32_t *)malloc(total * sizeof(uint32_t));
        ylines = (LtLine *)malloc(total * sizeof(LtLine));
        if (yidx && ylines) ynl = lt_lines(&a->ed, (uint32_t)ytrk, yidx, total, ylines, (int)total);
        if (ynl && (ynodes = (struct Node *)calloc((size_t)ynl, sizeof(struct Node))) && (ytext = (char (*)[96])calloc((size_t)ynl, 96))) {
            for (L = 0; L < ynl; L++) {
                char t[300];
                uint32_t bar, beat, tk;
                ed_mbt(&a->ed, a->ed.trk[ytrk].ev[yidx[ylines[L].first]].tick, &bar, &beat, &tk);
                lt_line_text(&a->ed, (uint32_t)ytrk, yidx, &ylines[L], t, sizeof t, a->letters, 0);
                snprintf(ytext[L], 96, "%03lu:%lu  %s", (unsigned long)bar, (unsigned long)beat, t);
                ynodes[L].ln_Name = ytext[L];
                AddTail(&ylist, &ynodes[L]);
            }
        }
    }
    if (ysel >= ynl) ysel = ynl - 1;
    GT_SetGadgetAttrs(g_list, yw, NULL, GTLV_Labels, (ULONG)&ylist, GTLV_Selected, ysel < 0 ? ~0UL : (ULONG)ysel, TAG_DONE);
    if (ytrk < 0) info("The song has no lyrics: Texts... puts them on the notes.");
}

int yw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, x0, y, w = 580, lh;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    if (yw) { WindowToFront(yw); ActivateWindow(yw); fill(a); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    yvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    x0 = (WORD)(scr->WBorLeft + 6);
    y = (WORD)(scr->WBorTop + fh + 1 + 4);
    lh = (WORD)(fh * (scr->Height > 300 ? 20 : 12) + 4);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = yvi;
    g = CreateContext(&ygl);
    NewList(&ylist);
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = w; ng.ng_Height = lh; ng.ng_GadgetID = Y_LIST;
    g = g_list = CreateGadget(LISTVIEW_KIND, g, &ng, GTLV_Labels, (ULONG)&ylist, GTLV_ShowSelected, 0, TAG_DONE);
    y += lh + 4;
    ng.ng_TopEdge = y; ng.ng_Height = fh + 6; ng.ng_GadgetID = Y_EDIT;
    g = g_edit = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 280, TAG_DONE);
    y += fh + 8;
    ng.ng_TopEdge = y; ng.ng_Width = 70; ng.ng_Flags = PLACETEXT_IN; ng.ng_GadgetText = (UBYTE *)"Apply"; ng.ng_GadgetID = Y_APPLY;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 74; ng.ng_GadgetText = (UBYTE *)"Undo"; ng.ng_GadgetID = Y_UNDO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 150; ng.ng_Width = w - 150; ng.ng_GadgetText = NULL; ng.ng_GadgetID = Y_INFO;
    g = g_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Click a line, change it, Return. \"-\" between syllables.", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 10;
    if (!g) { FreeGadgets(ygl); ygl = NULL; FreeVisualInfo(yvi); UnlockPubScreen(NULL, scr); return 0; }
    yw = OpenWindowTags(NULL, WA_Title, (ULONG)"Lyrics editor", WA_Left, 24, WA_Top, 20,
        WA_InnerWidth, w + 12, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)ygl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | LISTVIEWIDCMP | STRINGIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!yw) { FreeGadgets(ygl); ygl = NULL; FreeVisualInfo(yvi); return 0; }
    GT_RefreshWindow(yw, NULL);
    ysel = -1;
    fill(a);
    return 1;
}

void yw_close(void)
{
    if (!yw) return;
    CloseWindow(yw);
    yw = NULL;
    FreeGadgets(ygl); ygl = NULL;
    FreeVisualInfo(yvi); yvi = NULL;
    free(ynodes); free(ytext); free(yidx); free(ylines);
    ynodes = NULL; ytext = NULL; yidx = NULL; ylines = NULL;
}

void yw_refresh(App *a)
{
    if (!yw) return;
    if (a->cur < 0) { yw_close(); return; }
    fill(a);
}

static int apply(App *a)
{
    const char *t = (const char *)((struct StringInfo *)g_edit->SpecialInfo)->Buffer;
    uint32_t next;
    int left, n;
    char m[120];
    if (ysel < 0 || ysel >= ynl || ytrk < 0) { info("Click a line first."); return 0; }
    /* an unchanged line is not written (letters the window cannot show stay) */
    if (!strcmp(t, yshown)) { info("Not changed."); return 0; }
    next = ysel + 1 < ynl ? a->ed.trk[ytrk].ev[yidx[ylines[ysel + 1].first]].tick : 0xFFFFFFFFUL;
    app_ed_begin(a);
    n = lt_set_line(&a->ed, (uint32_t)ytrk, yidx, &ylines[ysel], next, t, a->letters, (uint32_t)ytrk, &left);
    app_ed_commit(a);
    snprintf(m, sizeof m, "Line %d: %d syllables%s", ysel + 1, n, left ? " - more syllables than notes in the line" : ".");
    info(m);
    return 1;
}

int yw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (yw && (m = GT_GetIMsg(yw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { close = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(yw); GT_EndRefresh(yw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP) continue;
        switch (src->GadgetID) {
        case Y_LIST:
            ysel = code;
            if (ysel < ynl) {
                lt_line_text(&a->ed, (uint32_t)ytrk, yidx, &ylines[ysel], yline, sizeof yline, a->letters, 0);
                strcpy(yshown, yline);
                GT_SetGadgetAttrs(g_edit, yw, NULL, GTST_String, (ULONG)yline, TAG_DONE);
                ActivateGadget(g_edit, yw, NULL);
            }
            break;
        case Y_EDIT:
            if (code == 0 || code == 13) changed |= apply(a);
            break;
        case Y_APPLY:
            changed |= apply(a);
            break;
        case Y_UNDO:
            app_undo(a);
            info(a->info);
            changed = 1;
            break;
        }
    }
    if (close) yw_close();
    return changed;
}
