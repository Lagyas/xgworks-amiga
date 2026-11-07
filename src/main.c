/* XGWorks Lite for AmigaOS 3.x: the window player.
 *
 *   XGWorksLite [FOLDER] [CLASSIC]
 *
 * FOLDER   the folder of the songs (default: the last one, else PROGDIR:Songs)
 * CLASSIC  the GadTools window even when MUI is installed
 * From Workbench: the same with the tool types FOLDER= and CLASSIC. */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *IconBase;

static const char version[] = "$VER: XGWorks Lite 0.2 (07.11.2026)";
static App app;

/* FOLDER= and CLASSIC from the program's icon. */
static void tooltypes(struct WBStartup *wb, char *folder, int len, int *classic)
{
    struct DiskObject *dob;
    BPTR old;
    if (!wb || wb->sm_NumArgs < 1 || !(IconBase = OpenLibrary((CONST_STRPTR)"icon.library", 37))) return;
    old = CurrentDir(wb->sm_ArgList[0].wa_Lock);
    if ((dob = GetDiskObject(wb->sm_ArgList[0].wa_Name))) {
        STRPTR v;
        if ((v = FindToolType(dob->do_ToolTypes, (CONST_STRPTR)"FOLDER"))) { strncpy(folder, (const char *)v, len - 1); folder[len - 1] = 0; }
        if (FindToolType(dob->do_ToolTypes, (CONST_STRPTR)"CLASSIC")) *classic = 1;
        FreeDiskObject(dob);
    }
    CurrentDir(old);
    CloseLibrary(IconBase);
    IconBase = NULL;
}

int main(int argc, char **argv)
{
    char folder[256] = "";
    int classic = 0, r;
    (void)version;
    if (argc > 0) {
        LONG a[2] = {0, 0};
        struct RDArgs *rd = ReadArgs((CONST_STRPTR)"FOLDER,CLASSIC/S", a, NULL);
        if (!rd) { PrintFault(IoErr(), (CONST_STRPTR)"XGWorksLite"); return 10; }
        if (a[0]) { strncpy(folder, (const char *)a[0], sizeof folder - 1); }
        classic = a[1] != 0;
        FreeArgs(rd);
    } else {
        tooltypes((struct WBStartup *)argv, folder, sizeof folder, &classic);
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
    if (!IntuitionBase || !GfxBase) {
        printf("XGWorks Lite needs AmigaOS 3.0 or newer.\n");
        r = 20;
    } else if (!app_init(&app, folder[0] ? folder : NULL)) {
        printf("XGWorks Lite: no free signal.\n");
        r = 20;
    } else {
        r = classic ? -1 : gui_mui(&app);
        if (r < 0) r = gui_gadtools(&app);
        if (r < 0) printf("XGWorks Lite: the window cannot open.\n");
        app_save_prefs(&app);
        app_free(&app);
        r = r < 0 ? 20 : 0;
    }
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return r;
}
