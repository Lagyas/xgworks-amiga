/* XGWorks Lite: its own screen with more colours (the Screen setting).
 * Workbench keeps its colours; the program opens a public screen like the
 * Workbench (the same mode and size) with 16, 32 or 256 colours when the
 * display can show them, and all its windows open there. */
#ifndef XG_SCREEN_H
#define XG_SCREEN_H

#define SCREENS 4
/* "Workbench", "16 colours", "32 colours", "256 colours" */
extern const char *const scr_names[SCREENS + 1];

/* Opens the screen of the setting (0 = Workbench: nothing to open). The
 * result is the setting used (0 when the screen could not open or the
 * Workbench has the colours already). */
int scr_open(int setting);
/* Closes it (after all the windows of the program are closed). */
void scr_close(void);
/* The public screen name for LockPubScreen (NULL = the default one). */
const char *scr_pubname(void);

#endif
