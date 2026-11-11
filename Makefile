# XGWorks Lite for AmigaOS 3.x (68020+).
# Amiga build: amiga-gcc (bebbo), e.g. the Docker image amigadev/crosstools:m68k-amigaos
#   make            -> XGPlay (Shell player) and XGWorksLite (the window), 68020, libnix
# PC build for the tests:
#   make host       -> build/xgplay-host
CC      ?= m68k-amigaos-gcc
CFLAGS  = -m68020 -O2 -noixemul -Wall -Wno-pointer-sign -fomit-frame-pointer
SRC     = src/xgplay.c src/smf.c
HOSTCC  ?= cc

LITE    = src/main.c src/app.c src/player.c src/mixer.c src/fx.c src/xws.c src/gui_mui.c src/gui_gadtools.c src/mixwin_mui.c src/mixwin_gt.c src/karawin.c src/edwin.c src/pianoroll.c src/rollops.c src/drumwin.c src/drumops.c src/lyrwin.c src/lyrtools.c src/editors.c src/xgmwin.c src/voices.c src/lyredwin.c src/staffwin.c src/jobwin.c src/jobs.c src/trackview.c src/blocks.c src/xwswrite.c src/edit.c src/edtext.c src/lyrics.c src/textmap.c src/textmap_data.c src/smf.c src/midiout_amiga.c

all: XGPlay XGWorksLite

XGPlay: $(SRC) src/midiout_amiga.c src/smf.h src/midiout.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/midiout_amiga.c

XGWorksLite: $(LITE) src/*.h
	$(CC) $(CFLAGS) -o $@ $(LITE) -lamiga

host: $(SRC) src/smf.h src/mixer.c tools/test_mixer.c
	mkdir -p build
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/xgplay-host $(SRC)
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_mixer tools/test_mixer.c src/smf.c src/mixer.c src/fx.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/xws2mid tools/xws2mid.c src/xws.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_edit tools/test_edit.c src/edit.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_roll tools/test_roll.c src/edit.c src/rollops.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_voices tools/test_voices.c src/voices.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/jobrun tools/jobrun.c src/edit.c src/jobs.c src/drumops.c src/rollops.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_blocks tools/test_blocks.c src/edit.c src/blocks.c src/xws.c src/xwswrite.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_lyrtools tools/test_lyrtools.c src/edit.c src/drumops.c src/lyrtools.c src/lyrics.c src/rollops.c src/smf.c src/textmap.c src/textmap_data.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_edtext tools/test_edtext.c src/edit.c src/edtext.c src/lyrics.c src/smf.c src/textmap.c src/textmap_data.c
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/test_lyrics tools/test_lyrics.c src/smf.c src/lyrics.c src/textmap.c src/textmap_data.c

clean:
	rm -rf XGPlay XGWorksLite build

.PHONY: all host clean
