# XGWorks Lite for AmigaOS 3.x (68020+).
# Amiga build: amiga-gcc (bebbo), e.g. the Docker image amigadev/crosstools:m68k-amigaos
#   make            -> XGPlay (Shell player) and XGWorksLite (the window), 68020, libnix
# PC build for the tests:
#   make host       -> build/xgplay-host
CC      ?= m68k-amigaos-gcc
CFLAGS  = -m68020 -O2 -noixemul -Wall -fomit-frame-pointer
SRC     = src/xgplay.c src/smf.c
HOSTCC  ?= cc

LITE    = src/main.c src/app.c src/player.c src/gui_mui.c src/gui_gadtools.c src/smf.c src/midiout_amiga.c

all: XGPlay XGWorksLite

XGPlay: $(SRC) src/midiout_amiga.c src/smf.h src/midiout.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/midiout_amiga.c

XGWorksLite: $(LITE) src/*.h
	$(CC) $(CFLAGS) -o $@ $(LITE) -lamiga

host: $(SRC) src/smf.h
	mkdir -p build
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/xgplay-host $(SRC)

clean:
	rm -rf XGPlay XGWorksLite build

.PHONY: all host clean
