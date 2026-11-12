/* XGWorks Lite: the Texts window - lyrics and chord tools (as XGWorks
 * NextGen's Lyrics / Chords windows): text onto notes (typed or from a text
 * file), chords onto bars, transposing chords, Midimusic export / import,
 * removing diacritics. The lyrics and chords themselves are edited in the
 * Event List (filter Lyrics / Chords); the karaoke window shows them. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lyrwin.h"
#include "lyrtools.h"
#include "rollops.h"
#include "edwin.h"
#include "textmap.h"
#include "screen.h"

extern struct Library *GadToolsBase, *AslBase;

enum { L_TEXT = 1, L_TBAR, L_TONOTES, L_CHORDS, L_CBAR, L_STEP, L_TOBARS, L_FILE, L_EXPORT, L_IMPORT, L_REPLACE,
       L_TDOWN, L_TUP, L_PLAIN, L_UNDO, L_INFO };

static struct Window *lw;
static struct Gadget *lgl, *lg_text, *lg_tbar, *lg_chords, *lg_cbar, *lg_info;
static void *lvi;
static int lstep, lreplace = 1;
static char linfo[120];

static const char *const STEPS[] = {"Bar", "Half bar", "Beat", NULL};

int lw_is_open(void) { return lw != NULL; }
unsigned long lw_signal(void) { return lw ? 1UL << lw->UserPort->mp_SigBit : 0; }

static void info(const char *t)
{
    snprintf(linfo, sizeof linfo, "%s", t);
    if (lw) GT_SetGadgetAttrs(lg_info, lw, NULL, GTTX_Text, (ULONG)linfo, TAG_DONE);
}

/* The notes the lyrics go on: the track selected in Tracks, else the lyrics track, else the first with notes. */
static int note_track(App *a)
{
    uint32_t t, i;
    int sel = edw_selected_track(), l = lt_lyrics_track(&a->ed);
    if (sel >= 0 && (uint32_t)sel < a->ed.ntrk) return sel;
    if (l >= 0) return l;
    for (t = 0; t < a->ed.ntrk; t++)
        for (i = 0; i < a->ed.trk[t].n; i++)
            if (ro_is_note(&a->ed.trk[t].ev[i], -1) && (a->ed.trk[t].ev[i].st & 0x0F) != 9) return (int)t;
    return 0;
}

static void title(App *a)
{
    static char t[100];
    snprintf(t, sizeof t, "Texts - lyrics on the notes of track %d", note_track(a) + 1);
    SetWindowTitles(lw, (CONST_STRPTR)t, (CONST_STRPTR)~0);
}

int lw_open(App *a)
{
    struct Screen *scr;
    struct NewGadget ng;
    struct Gadget *g;
    WORD fh, x0, y, w = 560, bh;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    if (lw) { WindowToFront(lw); ActivateWindow(lw); title(a); return 1; }
    if (!(scr = LockPubScreen((CONST_STRPTR)scr_pubname()))) return 0;
    lvi = GetVisualInfo(scr, TAG_DONE);
    fh = scr->Font->ta_YSize;
    bh = fh + 6;
    x0 = (WORD)(scr->WBorLeft + 60);
    y = (WORD)(scr->WBorTop + fh + 1 + 4);
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = scr->Font;
    ng.ng_VisualInfo = lvi;
    g = CreateContext(&lgl);
    ng.ng_Height = bh; ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = 300; ng.ng_GadgetText = (UBYTE *)"Text"; ng.ng_GadgetID = L_TEXT;
    g = lg_text = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 250, TAG_DONE);
    ng.ng_LeftEdge = x0 + 336; ng.ng_Width = 46; ng.ng_GadgetText = (UBYTE *)"Bar"; ng.ng_GadgetID = L_TBAR;
    g = lg_tbar = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 4, TAG_DONE);
    ng.ng_LeftEdge = x0 + 388; ng.ng_Width = 110; ng.ng_GadgetText = (UBYTE *)"Onto notes"; ng.ng_Flags = PLACETEXT_IN; ng.ng_GadgetID = L_TONOTES;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += bh + 4;
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = 220; ng.ng_GadgetText = (UBYTE *)"Chords"; ng.ng_GadgetID = L_CHORDS;
    g = lg_chords = CreateGadget(STRING_KIND, g, &ng, GTST_MaxChars, 250, TAG_DONE);
    ng.ng_LeftEdge = x0 + 254; ng.ng_Width = 46; ng.ng_GadgetText = (UBYTE *)"Bar"; ng.ng_GadgetID = L_CBAR;
    g = lg_cbar = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, 1, GTIN_MaxChars, 4, TAG_DONE);
    ng.ng_LeftEdge = x0 + 304; ng.ng_Width = 80; ng.ng_GadgetText = NULL; ng.ng_GadgetID = L_STEP;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)STEPS, GTCY_Active, lstep, TAG_DONE);
    ng.ng_LeftEdge = x0 + 388; ng.ng_Width = 110; ng.ng_GadgetText = (UBYTE *)"Onto bars"; ng.ng_Flags = PLACETEXT_IN; ng.ng_GadgetID = L_TOBARS;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += bh + 6;
    {
        static const char *const txt[4] = {"Text file...", "Export...", "Import...", NULL};
        static const int ids[3] = {L_FILE, L_EXPORT, L_IMPORT};
        WORD x = (WORD)(scr->WBorLeft + 6);
        int k;
        for (k = 0; k < 3; k++) {
            ng.ng_TopEdge = y; ng.ng_LeftEdge = x; ng.ng_Width = 100; ng.ng_GadgetText = (UBYTE *)txt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += 104;
        }
        ng.ng_LeftEdge = x + 4; ng.ng_Width = 26; ng.ng_GadgetText = (UBYTE *)"Replace"; ng.ng_Flags = PLACETEXT_RIGHT; ng.ng_GadgetID = L_REPLACE;
        g = CreateGadget(CHECKBOX_KIND, g, &ng, GTCB_Checked, lreplace, GTCB_Scaled, TRUE, TAG_DONE);
    }
    y += bh + 4;
    {
        static const char *const txt[4] = {"Chords -1", "Chords +1", "No diacritics", "Undo"};
        static const int ids[4] = {L_TDOWN, L_TUP, L_PLAIN, L_UNDO};
        static const WORD bw[4] = {90, 90, 120, 60};
        WORD x = (WORD)(scr->WBorLeft + 6);
        int k;
        ng.ng_Flags = PLACETEXT_IN;
        for (k = 0; k < 4; k++) {
            ng.ng_TopEdge = y; ng.ng_LeftEdge = x; ng.ng_Width = bw[k]; ng.ng_GadgetText = (UBYTE *)txt[k]; ng.ng_GadgetID = ids[k];
            g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
            x += bw[k] + 4;
        }
    }
    y += bh + 4;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = (WORD)(scr->WBorLeft + 6); ng.ng_Width = w; ng.ng_Height = fh + 4; ng.ng_GadgetText = NULL; ng.ng_GadgetID = L_INFO;
    g = lg_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)"Text: Ka-pr se / pla-ca v re-ce  (- syllables, / new line, < new page)", GTTX_Border, TRUE, TAG_DONE);
    y += fh + 8;
    if (!g) { FreeGadgets(lgl); lgl = NULL; FreeVisualInfo(lvi); UnlockPubScreen(NULL, scr); return 0; }
    lw = OpenWindowTags(NULL, WA_Title, (ULONG)"Texts", WA_Left, 40, WA_Top, 40,
        WA_InnerWidth, w + 12, WA_InnerHeight, y - scr->WBorTop - fh - 1,
        WA_Gadgets, (ULONG)lgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | STRINGIDCMP | INTEGERIDCMP | BUTTONIDCMP | CYCLEIDCMP | CHECKBOXIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!lw) { FreeGadgets(lgl); lgl = NULL; FreeVisualInfo(lvi); return 0; }
    GT_RefreshWindow(lw, NULL);
    title(a);
    return 1;
}

void lw_close(void)
{
    if (!lw) return;
    CloseWindow(lw);
    lw = NULL;
    FreeGadgets(lgl); lgl = NULL;
    FreeVisualInfo(lvi); lvi = NULL;
}

void lw_refresh(App *a)
{
    if (!lw) return;
    if (a->cur < 0) { lw_close(); return; }
    title(a);
}

/* A file through the ASL requester; 0 = cancelled. */
static int ask(App *a, int save, const char *title, const char *file, char *path, int max)
{
    struct FileRequester *fr;
    int ok = 0;
    if (!AslBase) return 0;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, ASLFR_TitleText, (ULONG)title, ASLFR_DoSaveMode, save,
        ASLFR_InitialDrawer, (ULONG)a->folder, ASLFR_InitialFile, (ULONG)file, ASLFR_DoPatterns, TRUE,
        ASLFR_InitialPattern, (ULONG)"#?.(txt|lyrics)", ASLFR_Window, (ULONG)lw, TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL) && fr->fr_File && fr->fr_File[0]) {
        strncpy(path, (const char *)fr->fr_Drawer, (size_t)max - 1);
        path[max - 1] = 0;
        AddPart((STRPTR)path, (CONST_STRPTR)fr->fr_File, (ULONG)max);
        ok = 1;
    }
    FreeAslRequest(fr);
    return ok;
}

static char *read_all(const char *path, LONG *n)
{
    BPTR fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
    char *b = NULL;
    if (!fh) return NULL;
    Seek(fh, 0, OFFSET_END);
    *n = Seek(fh, 0, OFFSET_BEGINNING);
    if (*n >= 0 && (b = (char *)malloc((size_t)*n + 1))) {
        if (Read(fh, b, *n) != *n) { free(b); b = NULL; }
        else b[*n] = 0;
    }
    Close(fh);
    return b;
}

static long num(struct Gadget *g) { return ((struct StringInfo *)g->SpecialInfo)->LongInt; }
static const char *str(struct Gadget *g) { return (const char *)((struct StringInfo *)g->SpecialInfo)->Buffer; }

static int to_notes(App *a, const char *text, int letters)
{
    int nt = note_track(a), lt = lt_lyrics_track(&a->ed), left, n;
    long bar = num(lg_tbar);
    char m[120];
    if (lt < 0) lt = nt;
    app_ed_begin(a);
    n = lt_text_to_notes(&a->ed, (uint32_t)lt, (uint32_t)nt, -1, ed_tick_of(&a->ed, (uint32_t)(bar < 1 ? 1 : bar), 1, 0), text, letters, &left);
    app_ed_commit(a);
    snprintf(m, sizeof m, "%d syllables on the notes of track %d%s", n, nt + 1, left ? " - more syllables than notes" : ".");
    if (left) snprintf(m + strlen(m), sizeof m - strlen(m), " (%d left)", left);
    info(m);
    return 1;
}

int lw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (lw && (m = GT_GetIMsg(lw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        char msg[120], path[256];
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { close = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(lw); GT_EndRefresh(lw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP || a->cur < 0) continue;
        switch (src->GadgetID) {
        case L_STEP: lstep = code; break;
        case L_REPLACE: lreplace = (src->Flags & GFLG_SELECTED) != 0; break;
        case L_TONOTES:
            if (str(lg_text)[0]) changed |= to_notes(a, str(lg_text), a->letters);
            break;
        case L_TOBARS: {
            int bad, n;
            long bar = num(lg_cbar);
            app_ed_begin(a);
            n = lt_chords_to_bars(&a->ed, 0, (uint32_t)(bar < 1 ? 1 : bar), lstep, str(lg_chords), &bad);
            app_ed_commit(a);
            snprintf(msg, sizeof msg, "%d chords placed%s", n, bad ? " - some names not understood (Am7, C/E, N.C.)" : ".");
            info(msg);
            changed = 1;
            break;
        }
        case L_FILE:
            if (ask(a, 0, "Lyrics text file onto the notes", "", path, sizeof path)) {
                LONG n;
                char *t = read_all(path, &n), *conv;
                if (!t) { info("The file cannot be read."); break; }
                /* PC files: Windows-1250 or UTF-8 -> ISO-8859-2 */
                if ((conv = (char *)malloc((size_t)n * 3 + 1))) {
                    tm_convert((const uint8_t *)t, (size_t)n, conv, (size_t)n * 3 + 1, TM_LATIN2);
                    changed |= to_notes(a, conv, TM_LATIN2);
                    free(conv);
                }
                free(t);
            }
            break;
        case L_EXPORT: {
            char name[140], *dot;
            strncpy(name, a->names[a->cur], sizeof name - 12);
            name[sizeof name - 12] = 0;
            if ((dot = strrchr(name, '.'))) *dot = 0;
            strcat(name, "_lyrics.txt");
            if (ask(a, 1, "Export lyrics and chords (Midimusic)", name, path, sizeof path)) {
                size_t n;
                char *t = lt_export(&a->ed, lt_lyrics_track(&a->ed), a->names[a->cur], 0, &n);
                BPTR fh;
                if (t && (fh = Open((CONST_STRPTR)path, MODE_NEWFILE))) {
                    Write(fh, t, (LONG)n);
                    Close(fh);
                    info("Exported.");
                } else info("Cannot write the file.");
                free(t);
            }
            break;
        }
        case L_IMPORT:
            if (ask(a, 0, "Import lyrics and chords (Midimusic)", "", path, sizeof path)) {
                LONG n;
                char *t = read_all(path, &n);
                int bad, lines, lt = lt_lyrics_track(&a->ed);
                if (!t) { info("The file cannot be read."); break; }
                if (lt < 0) lt = note_track(a);
                app_ed_begin(a);
                lines = lt_import(&a->ed, (uint32_t)lt, 0, t, (size_t)n, lreplace, &bad);
                app_ed_commit(a);
                snprintf(msg, sizeof msg, "%d lines read%s%s", lines, lreplace ? ", the old ones replaced" : ", added", bad ? " - some not understood" : ".");
                info(msg);
                free(t);
                changed = 1;
            }
            break;
        case L_TDOWN:
        case L_TUP: {
            int n;
            app_ed_begin(a);
            n = lt_transpose_chords(&a->ed, src->GadgetID == L_TUP ? 1 : -1);
            app_ed_commit(a);
            snprintf(msg, sizeof msg, "%d chords transposed %s1.", n, src->GadgetID == L_TUP ? "+" : "-");
            info(msg);
            changed = 1;
            break;
        }
        case L_PLAIN: {
            int n;
            app_ed_begin(a);
            n = lt_strip_diacritics(&a->ed);
            app_ed_commit(a);
            snprintf(msg, sizeof msg, "%d texts without diacritics now.", n);
            info(msg);
            changed = 1;
            break;
        }
        case L_UNDO:
            app_undo(a);
            info(a->info);
            changed = 1;
            break;
        }
    }
    if (close) lw_close();
    return changed;
}
