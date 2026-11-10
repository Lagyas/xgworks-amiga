/* XGWorks Lite: the karaoke window - the chord now and the next ones, the
 * line being sung (the sung part highlighted) and the next line. A plain
 * Intuition window drawn by hand, so the MUI and the GadTools versions share
 * it. Keys: + / - text size, D letters (plain / ISO-8859-2 / Latin-1), Esc. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/text.h>
#include <graphics/rastport.h>
#include <libraries/diskfont.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <stdio.h>
#include <string.h>
#include "karawin.h"
#include "textmap.h"

struct Library *DiskfontBase;

static struct Window *kw;
static struct TextFont *font;
static struct DrawInfo *dri;
static long last_syl = -2, last_chord = -2;
static int last_w, last_h;
static char title[160];

int kw_is_open(void) { return kw != NULL; }
unsigned long kw_signal(void) { return kw ? 1UL << kw->UserPort->mp_SigBit : 0; }

static void set_font(App *a)
{
    struct TextAttr ta;
    if (font) { CloseFont(font); font = NULL; }
    ta.ta_Name = (STRPTR)"helvetica.font";
    ta.ta_YSize = (UWORD)a->kfont;
    ta.ta_Style = FS_NORMAL;
    ta.ta_Flags = 0;
    if (DiskfontBase) font = OpenDiskFont(&ta);
    if (font) SetFont(kw->RPort, font);
    else SetFont(kw->RPort, kw->WScreen->RastPort.Font);
}

static WORD fh(void) { return kw->RPort->TxHeight; }

static void fit_height(void)
{
    WORD want = kw->BorderTop + kw->BorderBottom + (WORD)(3 * (fh() + 8) + 8);
    if (want > kw->WScreen->Height - kw->TopEdge) want = kw->WScreen->Height - kw->TopEdge;
    if (want != kw->Height) SizeWindow(kw, 0, (WORD)(want - kw->Height));
}

int kw_open(App *a)
{
    struct Screen *scr;
    if (kw) { WindowToFront(kw); ActivateWindow(kw); return 1; }
    if (!DiskfontBase) DiskfontBase = OpenLibrary((CONST_STRPTR)"diskfont.library", 36);
    if (!(scr = LockPubScreen(NULL))) return 0;
    kw = OpenWindowTags(NULL,
        WA_Title, (ULONG)"Lyrics  (+ / - size, D letters)",
        WA_Left, 30, WA_Top, scr->Height > 300 ? 300 : 120,
        WA_Width, scr->Width > 680 ? 640 : scr->Width - 40, WA_Height, 120,
        WA_MinWidth, 200, WA_MinHeight, 60, WA_MaxWidth, ~0, WA_MaxHeight, ~0,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE, WA_SizeBBottom, TRUE,
        WA_SimpleRefresh, TRUE, WA_Activate, FALSE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY,
        TAG_DONE);
    if (kw) dri = GetScreenDrawInfo(scr);
    UnlockPubScreen(NULL, scr);
    if (!kw) return 0;
    set_font(a);
    fit_height();
    last_syl = last_chord = -2;
    kw_update(a, 0, 1);
    return 1;
}

void kw_close(void)
{
    if (!kw) return;
    if (dri) FreeScreenDrawInfo(kw->WScreen, dri);
    dri = NULL;
    CloseWindow(kw);
    kw = NULL;
    if (font) CloseFont(font);
    font = NULL;
}

/* One line centred at `y`: syllables first..first+n, those up to `sung` in the highlight pen. */
static void draw_line(const Lyrics *ly, const LyLine *ln, long sung, WORD y, WORD x0, WORD w)
{
    struct RastPort *rp = kw->RPort;
    UWORD hi = dri ? dri->dri_Pens[HIGHLIGHTTEXTPEN] : 3, tx = dri ? dri->dri_Pens[TEXTPEN] : 1;
    LONG width = 0;
    uint32_t j;
    WORD x;
    for (j = 0; j < ln->n; j++) {
        const char *t = ly->text + ly->syl[ln->first + j].off;
        width += TextLength(rp, (CONST_STRPTR)t, strlen(t));
    }
    x = (WORD)(x0 + (width < w ? (w - width) / 2 : 0));
    Move(rp, x, (WORD)(y + rp->TxBaseline));
    for (j = 0; j < ln->n; j++) {
        const char *t = ly->text + ly->syl[ln->first + j].off;
        SetAPen(rp, (long)(ln->first + j) <= sung ? hi : tx);
        Text(rp, (CONST_STRPTR)t, strlen(t));
    }
}

void kw_update(App *a, uint32_t pos_us, int force)
{
    struct RastPort *rp;
    const Lyrics *ly = &a->ly;
    uint32_t tick;
    long syl, ch;
    WORD x0, y0, w, h, row;
    UWORD hi, tx, bg;
    if (!kw) return;
    tick = a->cur >= 0 ? xg_us_tick(&a->song, pos_us) : 0;
    syl = ly_syl_at(ly, tick);
    ch = ly_chord_at(ly, tick);
    w = (WORD)(kw->Width - kw->BorderLeft - kw->BorderRight);
    h = (WORD)(kw->Height - kw->BorderTop - kw->BorderBottom);
    if (!force && syl == last_syl && ch == last_chord && w == last_w && h == last_h) return;
    last_syl = syl; last_chord = ch; last_w = w; last_h = h;
    rp = kw->RPort;
    hi = dri ? dri->dri_Pens[HIGHLIGHTTEXTPEN] : 3;
    tx = dri ? dri->dri_Pens[TEXTPEN] : 1;
    bg = dri ? dri->dri_Pens[BACKGROUNDPEN] : 0;
    x0 = kw->BorderLeft + 4;
    y0 = kw->BorderTop + 4;
    w -= 8;
    row = (WORD)(fh() + 8);
    SetAPen(rp, bg);
    RectFill(rp, kw->BorderLeft, kw->BorderTop, kw->Width - kw->BorderRight - 1, kw->Height - kw->BorderBottom - 1);
    SetDrMd(rp, JAM1);
    /* chords: the one now (highlighted) and the next three */
    if (ly->nchord) {
        char next[80];
        long k;
        const char *now = ch >= 0 ? ly->chord[ch].name : "";
        next[0] = 0;
        for (k = ch + 1; k < (long)ly->nchord && k <= ch + 3; k++) {
            strcat(next, "   ");
            strcat(next, ly->chord[k].name);
        }
        Move(rp, x0, (WORD)(y0 + rp->TxBaseline));
        SetAPen(rp, hi);
        Text(rp, (CONST_STRPTR)now, strlen(now));
        SetAPen(rp, tx);
        Text(rp, (CONST_STRPTR)next, strlen(next));
    }
    /* the line being sung and the next one */
    if (ly->nline) {
        uint32_t L = syl >= 0 ? ly->syl[syl].line : 0;
        draw_line(ly, &ly->line[L], syl, (WORD)(y0 + row), x0, w);
        if (L + 1 < ly->nline) draw_line(ly, &ly->line[L + 1], -1, (WORD)(y0 + 2 * row), x0, w);
    } else {
        static const char none[] = "(no lyrics in this song)";
        SetAPen(rp, tx);
        Move(rp, x0, (WORD)(y0 + row + rp->TxBaseline));
        Text(rp, (CONST_STRPTR)none, sizeof none - 1);
    }
    snprintf(title, sizeof title, "Lyrics - %s  (+/- size, D letters: %s)", a->cur >= 0 ? a->names[a->cur] : "",
             a->letters == TM_LATIN2 ? "ISO-8859-2" : a->letters == TM_LATIN1 ? "Latin-1" : "plain");
    SetWindowTitles(kw, (CONST_STRPTR)title, (CONST_STRPTR)~0);
}

int kw_handle(App *a, uint32_t pos_us)
{
    struct IntuiMessage *m;
    int open = 1;
    while (kw && (m = (struct IntuiMessage *)GetMsg(kw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        ReplyMsg((struct Message *)m);
        switch (cls) {
        case IDCMP_CLOSEWINDOW:
            open = 0;
            break;
        case IDCMP_REFRESHWINDOW:
            BeginRefresh(kw);
            EndRefresh(kw, TRUE);
            kw_update(a, pos_us, 1);
            break;
        case IDCMP_NEWSIZE:
            kw_update(a, pos_us, 1);
            break;
        case IDCMP_VANILLAKEY:
            if (code == 27) open = 0;
            else if (code == '+' || code == '-') {
                a->kfont += code == '+' ? 2 : -2;
                if (a->kfont < 8) a->kfont = 8;
                if (a->kfont > 64) a->kfont = 64;
                set_font(a);
                fit_height();
                kw_update(a, pos_us, 1);
            } else if (code == 'd' || code == 'D') {
                app_set_letters(a, (a->letters + 1) % 3);
                kw_update(a, pos_us, 1);
            }
            break;
        }
    }
    if (!open) kw_close();
    return open;
}
