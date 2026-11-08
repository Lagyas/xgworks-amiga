XGWorks Lite for AmigaOS 3.x (68020 or better)
==============================================

Plays Standard MIDI Files (.mid / .kar, format 0 / 1) on a MIDI module
(Yamaha XG, Roland GS, GM) through a serial MIDI interface or CAMD.

XGWorksLite - the window
------------------------
  XGWorksLite [FOLDER] [CLASSIC]

  The songs of a folder in a list (default: the last folder, else the
  Songs folder next to the program). Double click or Play plays, Stop stops,
  |< and >| go to the previous / next song; at the end of a song the next one
  starts. Open... picks a song, Folder... a folder.

  Position   drag to move in the song (the settings before that point are
             sent first, so the sounds are right)
  Tempo      25 - 400 %, also while playing
  Transpose  -12 .. +12 half-tones, every channel but the drums (10)
  Reset      Song's own / XG / GM / GS: a System On before the song
  Output     serial.device (31250 baud) or CAMD (camd.library, link out.0)

  With MUI installed the window is an MUI window; otherwise (or with CLASSIC)
  the classic GadTools window, which needs nothing extra. Keys in the classic
  window: P or Space = play, S = stop, Esc = quit.

  The settings (folder, reset, output, tempo) are kept in
  ENVARC:XGWorksLite.prefs. From Workbench the tool types FOLDER= and
  CLASSIC work the same.

XGPlay - the Shell command
--------------------------
  XGPlay FILE/A,RESET/K,DEVICE/K,UNIT/K/N,DUMP/S,QUIET/S

  RESET    XG, GM or GS: a System On before the song
  DEVICE   serial device (default serial.device), or CAMD
  UNIT     its unit (default 0; for CAMD the link out.<UNIT>)
  DUMP     print the MIDI messages with their times instead of playing
  QUIET    no time display
  Ctrl+C stops.

Notes
-----
The serial port is opened at 31250 baud (MIDI): close other MIDI programs
first. No ixemul.library is needed.
