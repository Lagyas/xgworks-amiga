/* XGWorks Lite: the MUI window (when muimaster.library is installed). */
#include <exec/types.h>
#include <libraries/mui.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "mixwin.h"
#include "karawin.h"

struct Library *MUIMasterBase;

#ifndef MAKE_ID
#define MAKE_ID(a, b, c, d) ((ULONG)(a) << 24 | (ULONG)(b) << 16 | (ULONG)(c) << 8 | (ULONG)(d))
#endif

enum { ID_PLAY = 1, ID_STOP, ID_PREV, ID_NEXT, ID_OPEN, ID_FOLDER, ID_LIST, ID_POS, ID_TEMPO, ID_TRANS, ID_RESET, ID_OUT, ID_MIX, ID_LYR };

static Object *app, *win, *list, *spos, *ttime, *stempo, *strans, *creset, *cout, *tinfo, *lv;
static Object *bprev, *bplay, *bstop, *bnext, *bopen, *bdir, *bmix, *blyr;

static ULONG getv(Object *o, ULONG attr)
{
    ULONG v = 0;
    GetAttr(attr, o, &v);
    return v;
}

static void fill_list(App *a)
{
    int i;
    DoMethod(list, MUIM_List_Clear);
    SetAttrs(list, MUIA_List_Quiet, TRUE, TAG_DONE);
    for (i = 0; i < a->count; i++) DoMethod(list, MUIM_List_InsertSingle, (ULONG)a->names[i], MUIV_List_Insert_Bottom);
    SetAttrs(list, MUIA_List_Quiet, FALSE, TAG_DONE);
}

static void show(App *a)
{
    static char t[40];
    char x[16], y[16];
    uint32_t pos = app_playing(a) ? a->pl.pos_us : getv(spos, MUIA_Numeric_Value) * 1000000UL;
    app_time(x, pos);
    app_time(y, a->len_us);
    sprintf(t, "%s / %s", x, y);
    SetAttrs(ttime, MUIA_Text_Contents, (ULONG)t, TAG_DONE);
    if (app_playing(a)) SetAttrs(spos, MUIA_NoNotify, TRUE, MUIA_Numeric_Value, a->pl.pos_us / 1000000, TAG_DONE);
    kw_update(a, pos, 0);
    SetAttrs(tinfo, MUIA_Text_Contents, (ULONG)a->info, TAG_DONE);
}

/* Load song k (when not loaded yet) and start it at `from`. */
static void start(App *a, int k, uint32_t from)
{
    if (k < 0) return;
    if (k != a->cur && !app_load(a, k)) { show(a); return; }
    mixmui_refresh(a);
    kw_update(a, from, 1);
    SetAttrs(spos, MUIA_NoNotify, TRUE, MUIA_Numeric_Max, a->len_us / 1000000 + 1, MUIA_Numeric_Value, from / 1000000, TAG_DONE);
    SetAttrs(list, MUIA_NoNotify, TRUE, MUIA_List_Active, k, TAG_DONE);
    app_play(a, from);
    show(a);
}

int gui_mui(App *a)
{
    ULONG sigs = 0, id;
    int running = 1;
    if (!(MUIMasterBase = OpenLibrary((CONST_STRPTR)MUIMASTER_NAME, 19))) return -1;
    app = ApplicationObject,
        MUIA_Application_Title, (ULONG)"XGWorks Lite",
        MUIA_Application_Version, (ULONG)"$VER: XGWorks Lite 0.2 (07.11.2026)",
        MUIA_Application_Copyright, (ULONG)"Lagyas",
        MUIA_Application_Description, (ULONG)"MIDI / XG song player",
        MUIA_Application_Base, (ULONG)"XGWORKSLITE",
        MUIA_Application_HelpFile, (ULONG)"PROGDIR:XGWorksLite.guide",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, (ULONG)"XGWorks Lite",
            MUIA_Window_ID, MAKE_ID('X', 'G', 'W', 'L'),
            MUIA_HelpNode, (ULONG)"Main_Window",
            WindowContents, VGroup,
                Child, lv = ListviewObject,
                    MUIA_Listview_List, list = ListObject,
                        InputListFrame,
                        MUIA_List_ConstructHook, MUIV_List_ConstructHook_String,
                        MUIA_List_DestructHook, MUIV_List_DestructHook_String,
                    End,
                End,
                Child, HGroup,
                    Child, bprev = SimpleButton("|<"),
                    Child, bplay = SimpleButton("_Play"),
                    Child, bstop = SimpleButton("_Stop"),
                    Child, bnext = SimpleButton(">|"),
                    Child, HSpace(0),
                    Child, bmix = SimpleButton("_Mixer..."),
                    Child, blyr = SimpleButton("_Lyrics..."),
                    Child, bopen = SimpleButton("_Open..."),
                    Child, bdir = SimpleButton("_Folder..."),
                End,
                Child, ColGroup(2),
                    Child, Label("Position"),
                    Child, HGroup,
                        Child, spos = SliderObject, MUIA_Numeric_Min, 0, MUIA_Numeric_Max, 1, MUIA_Numeric_Value, 0, MUIA_Numeric_Format, (ULONG)"%ld s", End,
                        Child, ttime = TextObject, MUIA_Text_Contents, (ULONG)"0:00 / 0:00", MUIA_Text_SetMin, TRUE, End,
                    End,
                    Child, Label("Tempo"),
                    Child, stempo = SliderObject, MUIA_Numeric_Min, 25, MUIA_Numeric_Max, 400, MUIA_Numeric_Value, a->pl.tempo, MUIA_Numeric_Format, (ULONG)"%ld %%", End,
                    Child, Label("Transpose"),
                    Child, strans = SliderObject, MUIA_Numeric_Min, -12, MUIA_Numeric_Max, 12, MUIA_Numeric_Value, 0, End,
                    Child, Label("Reset"),
                    Child, HGroup,
                        Child, creset = CycleObject, MUIA_Cycle_Entries, (ULONG)app_reset_names, MUIA_Cycle_Active, a->reset, End,
                        Child, Label("Output"),
                        Child, cout = CycleObject, MUIA_Cycle_Entries, (ULONG)app_output_names, MUIA_Cycle_Active, a->output, End,
                    End,
                End,
                Child, tinfo = TextObject, TextFrame, MUIA_Background, MUII_TextBack, MUIA_Text_Contents, (ULONG)"", End,
            End,
        End,
    End;
    if (!app) {
        CloseLibrary(MUIMasterBase);
        MUIMasterBase = NULL;
        return -1;
    }
    {
        Object *const btn[8] = {bprev, bplay, bstop, bnext, bopen, bdir, bmix, blyr};
        static const ULONG ids[8] = {ID_PREV, ID_PLAY, ID_STOP, ID_NEXT, ID_OPEN, ID_FOLDER, ID_MIX, ID_LYR};
        int k;
        mixmui_create(a, app);
        for (k = 0; k < 8; k++) DoMethod(btn[k], MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, ids[k]);
    }
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(lv, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, (ULONG)app, 2, MUIM_Application_ReturnID, ID_LIST);
    DoMethod(spos, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, ID_POS);
    DoMethod(stempo, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, ID_TEMPO);
    DoMethod(strans, MUIM_Notify, MUIA_Numeric_Value, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, ID_TRANS);
    DoMethod(creset, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, ID_RESET);
    DoMethod(cout, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, ID_OUT);
    fill_list(a);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    show(a);
    while (running) {
        id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);
        switch (id) {
        case MUIV_Application_ReturnID_Quit:
            running = 0;
            break;
        case ID_LIST:
            start(a, (int)(LONG)getv(list, MUIA_List_Active), 0);
            break;
        case ID_PLAY: {
            int k = (int)(LONG)getv(list, MUIA_List_Active);
            if (k < 0) k = a->cur >= 0 ? a->cur : 0;
            start(a, k, k == a->cur ? getv(spos, MUIA_Numeric_Value) * 1000000UL : 0);
            break;
        }
        case ID_STOP:
            app_stop(a);
            show(a);
            break;
        case ID_PREV:
        case ID_NEXT: {
            int k = a->cur + (id == ID_NEXT ? 1 : -1);
            if (k >= 0 && k < a->count) start(a, k, 0);
            break;
        }
        case ID_OPEN:
        case ID_FOLDER: {
            struct Window *w = NULL;
            int r;
            GetAttr(MUIA_Window_Window, win, (ULONG *)&w);
            SetAttrs(app, MUIA_Application_Sleep, TRUE, TAG_DONE);
            r = id == ID_OPEN ? app_ask_file(a, w) : app_ask_folder(a, w);
            SetAttrs(app, MUIA_Application_Sleep, FALSE, TAG_DONE);
            if (r) fill_list(a);
            if (r >= 2) start(a, r - 2, 0);
            show(a);
            break;
        }
        case ID_POS:
            if (app_playing(a)) app_play(a, getv(spos, MUIA_Numeric_Value) * 1000000UL);
            show(a);
            break;
        case ID_TEMPO:
            app_set_tempo(a, (int)(LONG)getv(stempo, MUIA_Numeric_Value));
            break;
        case ID_TRANS:
            app_set_transpose(a, (int)(LONG)getv(strans, MUIA_Numeric_Value));
            break;
        case ID_RESET:
            a->reset = (int)getv(creset, MUIA_Cycle_Active);
            break;
        case ID_OUT:
            a->output = (int)getv(cout, MUIA_Cycle_Active);
            break;
        case ID_MIX:
            mixmui_refresh(a);
            mixmui_open(1);
            break;
        case ID_LYR:
            kw_open(a);
            break;
        default:
            if (mixmui_handle(a, id)) {
                if (a->cur >= 0) SetAttrs(spos, MUIA_NoNotify, TRUE, MUIA_Numeric_Max, a->len_us / 1000000 + 1, TAG_DONE);
                show(a);
            }
            break;
        }
        if (running && sigs) {
            sigs = Wait(sigs | pl_signal() | kw_signal() | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) running = 0;
            if (sigs & kw_signal()) kw_handle(a, app_playing(a) ? a->pl.pos_us : getv(spos, MUIA_Numeric_Value) * 1000000UL);
            if (sigs & pl_signal()) {
                if (app_poll(a) && a->autonext && a->cur + 1 < a->count) start(a, a->cur + 1, 0);
                else show(a);
            }
            sigs &= ~(pl_signal() | SIGBREAKF_CTRL_C);
            if (kw_is_open() || (sigs & kw_signal())) sigs &= ~kw_signal();
        }
    }
    app_stop(a);
    kw_close();
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    CloseLibrary(MUIMasterBase);
    MUIMasterBase = NULL;
    return 0;
}
