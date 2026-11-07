XGWorks Lite for AmigaOS 3.x (68020 or better)
==============================================

Stage 1: XGPlay, a Shell command that plays a Standard MIDI File (.mid,
format 0 / 1) on a MIDI module (Yamaha XG, Roland GS, GM) connected through a
serial MIDI interface.

  XGPlay FILE/A,RESET/K,DEVICE/K,UNIT/K/N,DUMP/S,QUIET/S

  FILE     the song
  RESET    XG, GM or GS: a System On before the song (default: OFF, the
           song's own reset is used)
  DEVICE   serial device (default serial.device), e.g. for a serial card
  UNIT     its unit (default 0)
  DUMP     print the MIDI messages with their times instead of playing
  QUIET    no time display

  Ctrl+C stops; every channel gets All Sound Off, Reset All Controllers and
  All Notes Off.

Examples:
  XGPlay Songs/song.mid
  XGPlay Songs/song.mid RESET=XG

The serial port is opened at 31250 baud (MIDI); close other MIDI programs
first. No extra libraries are needed (no ixemul.library).

Next stages: a window (MUI when muimaster.library is there, else the
classic GadTools look), the XG mixer, lyrics and chords, .XWS songs.
