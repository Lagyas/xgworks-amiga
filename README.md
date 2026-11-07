# XGWorks Lite for AmigaOS 3.x

A small player / XG controller for the Amiga (68020+, AmigaOS 3.0–3.2), the
little sibling of XGWorks NextGen. Plain C; built with amiga-gcc.

- `src/smf.c` – Standard MIDI File reading and the playback order (no AmigaOS
  calls; also built on the PC for the tests).
- `src/midiout_amiga.c` – MIDI out through `serial.device` at 31250 baud, timing
  from `timer.device`.
- `src/xgplay.c` – the `XGPlay` Shell command (see `docs/ReadMe.txt`).

## Tests

- `tools/refdump` (Rust, uses `../xgworks-core`) prints the playback plan of the
  XGWorks NextGen core; `tools/compare.py` compares it with `XGPlay … DUMP`
  (same messages, times within 2 µs).
- CI (`.github/workflows/amiga.yml`) builds `XGPlay` for the 68020, runs it in
  `vamos` (amitools) on a generated test song and compares with the PC build.

No songs or data of other makers are in this repository.
