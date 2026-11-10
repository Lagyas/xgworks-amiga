XGWorks Lite for AmigaOS 3.x (68020 or better)
==============================================

Plays Standard MIDI Files (.mid / .kar, format 0 / 1) and XGworks songs
(.XWS) on a MIDI module (Yamaha XG, Roland GS, GM) through a serial MIDI
interface or CAMD. The full help: XGWorksLite.guide (the Help key in the
window, or MultiView).

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

  Mixer...   the 16 parts of the song: sound (program, bank MSB / LSB),
             volume, pan, reverb, chorus, mute (M), solo (S). The values are
             read from the song's setup; a change is heard at once, also while
             playing, and wins over the song's own messages.
             Save into the song writes the changes into the file - over the
             song's own messages, else as new ones right after its reset (the
             first save keeps the old file as .bak). Undo goes back to the
             song's values.
             Under the parts: the song's reverb and chorus type and return
             (XG System Effect, or GS macros for a GS song), saved the same way.
             An .XWS song is saved as <name>.mid next to it.

  Lyrics...  the karaoke window: the chord now (highlighted) and the next
             ones, the line being sung with the sung part highlighted, and
             the next line. Lyrics of XF songs (lyric events, "/" new line,
             "<" new page) and of .kar files (text events) are read.
             Keys: + / - text size (helvetica.font from FONTS:, else the
             screen font), D the letters: plain (no diacritics, any font),
             ISO-8859-2 (Czech / Polish ... with a Central European font)
             or Latin-1. Song texts in Windows-1250 or UTF-8 are converted.

  Tracks...  the tracks: name, channel, mute / solo, new / delete, Undo,
             Save, Save as; Events... opens the Event List of a track: every
             event as a line (0012:1:000  1  Note  C3  100  480) - change the
             line and press Return, Shift+Return inserts, Del deletes.

  Roll...    the Piano Roll of the track selected in Tracks: Select / Draw /
             Erase, drag to move, drag the right end for the length, a
             rectangle selects; grid, channel, velocity, zoom; Del, cursor
             keys, A (all), C (copy a bar later). Velocities under the notes.

  With MUI installed the window is an MUI window; otherwise (or with CLASSIC)
  the classic GadTools window, which needs nothing extra. Keys in the classic
  window: P or Space = play, S = stop, Esc = quit.

  The settings (folder, reset, output, tempo, letters, text size) are kept in
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
