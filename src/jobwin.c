/* XGWorks Lite: the Job window - one of the jobs of XGworks' Job menu (and
 * Demix) on a track (or all) from bar to bar, with its own settings; the
 * gadgets are made again for each job. Undo-able. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jobwin.h"
#include "jobs.h"
#include "edwin.h"
#include "voices.h"

extern struct Library *GadToolsBase;

enum { J_TRANSPOSE, J_VELOCITY, J_GATE, J_SHIFT, J_SORT, J_SEPARATE, J_THIN, J_QUANTIZE, J_CREATE, J_DELETE, J_CHORD,
       J_CONTROL, J_STRETCH, J_DEMIXCH, J_DEMIXNOTE, J_N };
static const char *const JOBS[J_N + 1] = {"Transpose", "Velocity Modify", "Gate Time Modify", "Shift Clock", "Sort Chord",
    "Separate Chord", "Thin Out", "Quantize", "Create Measure", "Delete Measure", "Transpose Chord", "Modify Control Data",
    "Stretch Time", "Demix by Channel", "Demix by Note", NULL};
static const char *const MODES[] = {"Relative", "Absolute", NULL};
static const char *const KINDS[] = {"Poly Pressure", "Control Change", "Channel Pressure", "Pitch Bend", NULL};
static const char *const ORDERS[] = {"Low -> High", "High -> Low", NULL};

/* gadget ids; values kept between jobs */
enum { G_JOB = 1, G_TRK, G_FROM, G_THRU, G_LO, G_HI, G_DO, G_UNDO, G_INFO, G_P0 = 20 };
static struct Window *jw;
static struct Gadget *jgl;
static void *jvi;
static struct TextAttr *jfont;
static WORD jfh, jx0, jy0;
static int job, trk, from_bar = 1, thru_bar = 9999, lo, hi = 127;
static int semis, mode, absval = 100, rate = 100, offset, clocks, order, kind = 1, cc = 7, cc_to = 7, grid = 2, strength = 100,
           sens = 100, swing = 50, qgate = 100, qvel = 100, at_bar = 1, num = 4, den = 4, count = 1, stretch = 100;
static char chans[24] = "10", jinfo[120];
static struct Gadget *g_info, *g_chans;

int jw_is_open(void) { return jw != NULL; }
unsigned long jw_signal(void) { return jw ? 1UL << jw->UserPort->mp_SigBit : 0; }

/* the setting gadgets of a job: (id, label, kind, value / labels, min, max) */
typedef struct { int *v; const char *label; const char *const *cyc; int min, max; } Field;

static int fields(Field *f)
{
    int n = 0;
#define F(var, lab, cy, a, b) do { f[n].v = &(var); f[n].label = (lab); f[n].cyc = (cy); f[n].min = (a); f[n].max = (b); n++; } while (0)
    switch (job) {
    case J_TRANSPOSE: case J_CHORD: F(semis, "Semitones", NULL, -127, 127); break;
    case J_VELOCITY: case J_GATE:
        F(mode, "Mode", MODES, 0, 1);
        F(absval, "Value", NULL, job == J_GATE ? 1 : 1, job == J_GATE ? 99999 : 127);
        F(rate, "Rate %", NULL, 0, 1000);
        F(offset, "Offset", NULL, -99999, 99999);
        break;
    case J_SHIFT: case J_SEPARATE: F(clocks, "Clocks", NULL, -99999, 99999); break;
    case J_SORT: F(order, "Order", ORDERS, 0, 1); break;
    case J_THIN: F(kind, "Data", KINDS, 0, 3); F(cc, "Control", NULL, 0, 127); break;
    case J_QUANTIZE:
        F(grid, "Grid", jb_qntz_names, 0, 8);
        F(strength, "Strength %", NULL, 0, 100);
        F(sens, "Sensitivity %", NULL, -100, 100);
        F(swing, "Swing %", NULL, 50, 100);
        F(qgate, "Gate %", NULL, 0, 200);
        F(qvel, "Velocity %", NULL, 0, 200);
        break;
    case J_CREATE: F(at_bar, "Before bar", NULL, 1, 9999); F(num, "Beats", NULL, 1, 99); F(den, "Note value", NULL, 1, 64); F(count, "Bars", NULL, 1, 999); break;
    case J_CONTROL:
        F(kind, "Data", KINDS, 0, 3);
        F(cc, "Control", NULL, 0, 127);
        F(cc_to, "To control", NULL, 0, 127);
        F(mode, "Mode", MODES, 0, 1);
        F(absval, "Value", NULL, -8192, 8191);
        F(rate, "Rate %", NULL, 0, 1000);
        F(offset, "Offset", NULL, -8192, 8191);
        break;
    case J_STRETCH: F(stretch, "Stretch %", NULL, 1, 1000); F(offset, "Offset", NULL, -99999, 99999); break;
    default: break;
    }
#undef F
    return n;
}

static int uses_notes(void) { return job == J_TRANSPOSE || job == J_VELOCITY || job == J_GATE || job == J_QUANTIZE; }
static int uses_track(void) { return job != J_CREATE && job != J_DELETE && job != J_CHORD; }

static void info(const char *t)
{
    snprintf(jinfo, sizeof jinfo, "%s", t);
    if (jw && g_info) GT_SetGadgetAttrs(g_info, jw, NULL, GTTX_Text, (ULONG)jinfo, TAG_DONE);
}

/* the gadgets of the current job */
static struct Gadget *make(WORD *height)
{
    struct NewGadget ng;
    struct Gadget *g;
    Field f[8];
    int n = fields(f), k;
    WORD y = jy0, x0 = jx0, lw = 100;
    memset(&ng, 0, sizeof ng);
    ng.ng_TextAttr = jfont;
    ng.ng_VisualInfo = jvi;
    g = CreateContext(&jgl);
    ng.ng_Height = jfh + 4;
    ng.ng_Flags = PLACETEXT_LEFT;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_Width = 200; ng.ng_GadgetText = (UBYTE *)"Job"; ng.ng_GadgetID = G_JOB;
    g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)JOBS, GTCY_Active, job, TAG_DONE);
    y += jfh + 8;
    if (uses_track()) {
        ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_Width = 50; ng.ng_GadgetText = (UBYTE *)"Track (0 all)"; ng.ng_GadgetID = G_TRK;
        g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, trk, GTIN_MaxChars, 3, TAG_DONE);
        y += jfh + 6;
    }
    if (job != J_CREATE && job != J_DEMIXCH && job != J_DEMIXNOTE) {
        ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_Width = 60; ng.ng_GadgetText = (UBYTE *)"From bar"; ng.ng_GadgetID = G_FROM;
        g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, from_bar, GTIN_MaxChars, 5, TAG_DONE);
        ng.ng_LeftEdge = x0 + lw + 130; ng.ng_GadgetText = (UBYTE *)"Thru"; ng.ng_GadgetID = G_THRU;
        g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, thru_bar, GTIN_MaxChars, 5, TAG_DONE);
        y += jfh + 6;
    }
    if (uses_notes()) {
        ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_Width = 60; ng.ng_GadgetText = (UBYTE *)"Notes from"; ng.ng_GadgetID = G_LO;
        g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, lo, GTIN_MaxChars, 3, TAG_DONE);
        ng.ng_LeftEdge = x0 + lw + 130; ng.ng_GadgetText = (UBYTE *)"Thru"; ng.ng_GadgetID = G_HI;
        g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, hi, GTIN_MaxChars, 3, TAG_DONE);
        y += jfh + 6;
    }
    for (k = 0; k < n; k++) {
        ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_GadgetText = (UBYTE *)f[k].label; ng.ng_GadgetID = G_P0 + k;
        if (f[k].cyc) {
            ng.ng_Width = 170;
            g = CreateGadget(CYCLE_KIND, g, &ng, GTCY_Labels, (ULONG)f[k].cyc, GTCY_Active, *f[k].v, TAG_DONE);
        } else {
            ng.ng_Width = 70;
            g = CreateGadget(INTEGER_KIND, g, &ng, GTIN_Number, *f[k].v, GTIN_MaxChars, 6, TAG_DONE);
        }
        y += jfh + 6;
    }
    if (job == J_DEMIXNOTE) {
        ng.ng_TopEdge = y; ng.ng_LeftEdge = x0 + lw; ng.ng_Width = 120; ng.ng_GadgetText = (UBYTE *)"Channels"; ng.ng_GadgetID = G_P0 + 7;
        g = g_chans = CreateGadget(STRING_KIND, g, &ng, GTST_String, (ULONG)chans, GTST_MaxChars, 20, TAG_DONE);
        y += jfh + 6;
    } else g_chans = NULL;
    y += 4;
    ng.ng_Flags = PLACETEXT_IN; ng.ng_TopEdge = y; ng.ng_Height = jfh + 6;
    ng.ng_LeftEdge = x0; ng.ng_Width = 90; ng.ng_GadgetText = (UBYTE *)"Do it"; ng.ng_GadgetID = G_DO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    ng.ng_LeftEdge = x0 + 94; ng.ng_Width = 60; ng.ng_GadgetText = (UBYTE *)"Undo"; ng.ng_GadgetID = G_UNDO;
    g = CreateGadget(BUTTON_KIND, g, &ng, TAG_DONE);
    y += jfh + 10;
    ng.ng_TopEdge = y; ng.ng_LeftEdge = x0; ng.ng_Width = 330; ng.ng_Height = jfh + 4; ng.ng_GadgetText = NULL; ng.ng_GadgetID = G_INFO;
    g = g_info = CreateGadget(TEXT_KIND, g, &ng, GTTX_Text, (ULONG)jinfo, GTTX_Border, TRUE, TAG_DONE);
    y += jfh + 8;
    *height = y;
    return g;
}

/* the job changed: its gadgets instead of the old ones */
static void rebuild(void)
{
    WORD h;
    RemoveGList(jw, jgl, -1);
    FreeGadgets(jgl);
    jgl = NULL;
    SetAPen(jw->RPort, 0);
    RectFill(jw->RPort, jw->BorderLeft, jw->BorderTop, jw->Width - jw->BorderRight - 1, jw->Height - jw->BorderBottom - 1);
    if (!make(&h)) return;
    if (h + jw->BorderBottom > jw->Height) ChangeWindowBox(jw, jw->LeftEdge, jw->TopEdge, jw->Width, (WORD)(h + jw->BorderBottom + 2));
    AddGList(jw, jgl, (UWORD)~0, -1, NULL);
    RefreshGList(jgl, jw, NULL, -1);
    GT_RefreshWindow(jw, NULL);
}

int jw_open_job(App *a, int j)
{
    job = j;
    return jw_open(a);
}

int jw_open(App *a)
{
    struct Screen *scr;
    WORD h;
    if (a->cur < 0) { snprintf(a->info, sizeof a->info, "Load a song first."); return 0; }
    trk = edw_selected_track() >= 0 ? edw_selected_track() + 1 : 0;
    if (jw) { WindowToFront(jw); ActivateWindow(jw); rebuild(); return 1; }
    if (!(scr = LockPubScreen(NULL))) return 0;
    jvi = GetVisualInfo(scr, TAG_DONE);
    jfont = scr->Font;
    jfh = scr->Font->ta_YSize;
    jx0 = (WORD)(scr->WBorLeft + 6);
    jy0 = (WORD)(scr->WBorTop + jfh + 1 + 4);
    if (!make(&h)) { FreeGadgets(jgl); jgl = NULL; FreeVisualInfo(jvi); UnlockPubScreen(NULL, scr); return 0; }
    jw = OpenWindowTags(NULL, WA_Title, (ULONG)"Job", WA_Left, 80, WA_Top, 20,
        WA_InnerWidth, 344, WA_InnerHeight, (WORD)(12 * (jfh + 6) + 40),
        WA_Gadgets, (ULONG)jgl, WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_PubScreen, (ULONG)scr,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | CYCLEIDCMP | INTEGERIDCMP | STRINGIDCMP | BUTTONIDCMP, TAG_DONE);
    UnlockPubScreen(NULL, scr);
    if (!jw) { FreeGadgets(jgl); jgl = NULL; FreeVisualInfo(jvi); return 0; }
    GT_RefreshWindow(jw, NULL);
    return 1;
}

void jw_close(void)
{
    if (!jw) return;
    CloseWindow(jw);
    jw = NULL;
    FreeGadgets(jgl); jgl = NULL;
    FreeVisualInfo(jvi); jvi = NULL;
}

/* "10", "1-16", "1,2,10" -> channel bits */
static uint16_t channel_bits(const char *s)
{
    uint16_t m = 0;
    while (*s) {
        int a = atoi(s), b;
        while (*s >= '0' && *s <= '9') s++;
        b = a;
        if (*s == '-') { s++; b = atoi(s); while (*s >= '0' && *s <= '9') s++; }
        for (; a <= b && a <= 16; a++) if (a >= 1) m |= (uint16_t)(1u << (a - 1));
        while (*s && (*s < '0' || *s > '9')) s++;
    }
    return m;
}

static const char *drum_label(int ch, int key)
{
    (void)ch;
    return vx_drum(127, 0, key);
}

/* the numbers as they are typed (also without Return) */
static void sync_values(void)
{
    struct Gadget *g;
    Field f[8];
    int n = fields(f);
    for (g = jgl; g; g = g->NextGadget) {
        long v;
        if ((g->GadgetType & GTYP_GTYPEMASK) != GTYP_STRGADGET || !g->SpecialInfo) continue;
        v = ((struct StringInfo *)g->SpecialInfo)->LongInt;
        switch (g->GadgetID) {
        case G_TRK: trk = (int)v; break;
        case G_FROM: from_bar = (int)v; break;
        case G_THRU: thru_bar = (int)v; break;
        case G_LO: lo = (int)v; break;
        case G_HI: hi = (int)v; break;
        default:
            if (g->GadgetID >= G_P0 && g->GadgetID < G_P0 + n && !f[g->GadgetID - G_P0].cyc) {
                int k = g->GadgetID - G_P0;
                if (v < f[k].min) v = f[k].min;
                if (v > f[k].max) v = f[k].max;
                *f[k].v = (int)v;
            }
        }
    }
}

static int run(App *a)
{
    JbScope sc;
    JbMod m;
    int n = 0;
    char msg[120];
    uint32_t t0 = ed_tick_of(&a->ed, (uint32_t)(from_bar < 1 ? 1 : from_bar), 1, 0);
    uint32_t t1 = thru_bar >= 9999 ? 0xFFFFFFFFUL : ed_tick_of(&a->ed, (uint32_t)thru_bar + 1, 1, 0);
    sync_values();
    t0 = ed_tick_of(&a->ed, (uint32_t)(from_bar < 1 ? 1 : from_bar), 1, 0);
    t1 = thru_bar >= 9999 ? 0xFFFFFFFFUL : ed_tick_of(&a->ed, (uint32_t)thru_bar + 1, 1, 0);
    sc.trk = trk <= 0 ? -1 : trk - 1;
    sc.from = t0; sc.to = t1;
    sc.lo = (uint8_t)(lo < 0 ? 0 : lo > 127 ? 127 : lo);
    sc.hi = (uint8_t)(hi < 0 ? 0 : hi > 127 ? 127 : hi);
    m.absolute = mode == 1; m.value = absval; m.rate = rate; m.offset = offset;
    if (sc.trk >= (int)a->ed.ntrk) { info("No such track."); return 0; }
    app_ed_begin(a);
    switch (job) {
    case J_TRANSPOSE: n = jb_transpose(&a->ed, &sc, semis); break;
    case J_VELOCITY: n = jb_velocity(&a->ed, &sc, m); break;
    case J_GATE: n = jb_gate(&a->ed, &sc, m); break;
    case J_SHIFT: n = jb_shift(&a->ed, &sc, clocks); break;
    case J_SORT: jb_sort_chord(&a->ed, &sc, order); n = -1; break;
    case J_SEPARATE: jb_separate_chord(&a->ed, &sc, clocks); n = -1; break;
    case J_THIN: n = jb_thin_out(&a->ed, &sc, kind, cc); break;
    case J_QUANTIZE: {
        JbQntz q;
        q.grid = grid; q.strength = strength; q.sens = sens; q.swing = swing; q.gate = qgate; q.vel = qvel;
        n = jb_quantize(&a->ed, &sc, q);
        break;
    }
    case J_CREATE: {
        uint32_t at = ed_tick_of(&a->ed, (uint32_t)at_bar, 1, 0), tk[64], nm[64], bl[64];
        int ns = ed_sigs(&a->ed, tk, nm, bl, 64), k = ns - 1, old_den;
        while (k > 0 && tk[k] > at) k--;
        old_den = (int)(a->ed.ppqn * 4 / bl[k]);
        jb_create_measure(&a->ed, at, num, den, (uint32_t)count, (int)nm[k], old_den);
        n = -1;
        break;
    }
    case J_DELETE: jb_delete_measure(&a->ed, t0, t1 == 0xFFFFFFFFUL ? ed_tick_of(&a->ed, (uint32_t)from_bar + 1, 1, 0) : t1); n = -1; break;
    case J_CHORD: n = jb_transpose_chord(&a->ed, t0, t1, semis); break;
    case J_CONTROL: n = jb_modify_control(&a->ed, &sc, kind, cc, kind == JB_CC ? cc_to : cc, m); break;
    case J_STRETCH: n = jb_stretch(&a->ed, &sc, (uint32_t)stretch, offset); break;
    case J_DEMIXCH:
        if (sc.trk < 0) { info("Demix: choose a track."); return 0; }
        n = jb_demix_channels(&a->ed, (uint32_t)sc.trk);
        break;
    case J_DEMIXNOTE:
        if (sc.trk < 0) { info("Demix: choose a track."); return 0; }
        if (g_chans) snprintf(chans, sizeof chans, "%s", (const char *)((struct StringInfo *)g_chans->SpecialInfo)->Buffer);
        n = jb_demix_notes(&a->ed, (uint32_t)sc.trk, channel_bits(chans), drum_label);
        break;
    }
    app_ed_commit(a);
    if (n < 0) snprintf(msg, sizeof msg, "%s: done.", JOBS[job]);
    else snprintf(msg, sizeof msg, "%s: %d %s.", JOBS[job], n, job >= J_DEMIXCH ? "new tracks" : "events");
    info(msg);
    return 1;
}

int jw_handle(App *a)
{
    struct IntuiMessage *m;
    int changed = 0, close = 0;
    while (jw && (m = GT_GetIMsg(jw->UserPort))) {
        ULONG cls = m->Class;
        UWORD code = m->Code;
        struct Gadget *src = (struct Gadget *)m->IAddress;
        long v = 0;
        GT_ReplyIMsg(m);
        if (cls == IDCMP_CLOSEWINDOW) { close = 1; break; }
        if (cls == IDCMP_REFRESHWINDOW) { GT_BeginRefresh(jw); GT_EndRefresh(jw, TRUE); continue; }
        if (cls != IDCMP_GADGETUP) continue;
        if (src->SpecialInfo && (src->GadgetType & GTYP_GTYPEMASK) == GTYP_STRGADGET) v = ((struct StringInfo *)src->SpecialInfo)->LongInt;
        switch (src->GadgetID) {
        case G_JOB: job = code; rebuild(); break;
        case G_TRK: trk = (int)v; break;
        case G_FROM: from_bar = (int)v; break;
        case G_THRU: thru_bar = (int)v; break;
        case G_LO: lo = (int)v; break;
        case G_HI: hi = (int)v; break;
        case G_DO: changed |= run(a); break;
        case G_UNDO: app_undo(a); info(a->info); changed = 1; break;
        default:
            if (src->GadgetID >= G_P0 && src->GadgetID < G_P0 + 7) {
                Field f[8];
                int n = fields(f), k = src->GadgetID - G_P0;
                if (k < n) {
                    int val = f[k].cyc ? (int)code : (int)v;
                    if (val < f[k].min) val = f[k].min;
                    if (val > f[k].max) val = f[k].max;
                    *f[k].v = val;
                    if (f[k].v == &kind || f[k].v == &mode) rebuild();
                }
            }
            break;
        }
    }
    if (close) jw_close();
    return changed;
}
