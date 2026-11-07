# XGWorks Lite for AmigaOS 3.x (68020+).
# Amiga build: amiga-gcc (bebbo), e.g. the Docker image amigadev/crosstools:m68k-amigaos
#   make            -> XGPlay (68020, libnix: no ixemul.library needed)
# PC build for the tests:
#   make host       -> build/xgplay-host
CC      ?= m68k-amigaos-gcc
CFLAGS  = -m68020 -O2 -noixemul -Wall -fomit-frame-pointer
SRC     = src/xgplay.c src/smf.c
HOSTCC  ?= cc

XGPlay: $(SRC) src/midiout_amiga.c src/smf.h src/midiout.h
	$(CC) $(CFLAGS) -o $@ $(SRC) src/midiout_amiga.c

host: $(SRC) src/smf.h
	mkdir -p build
	$(HOSTCC) -O2 -Wall -Wextra -std=c99 -o build/xgplay-host $(SRC)

clean:
	rm -rf XGPlay build

.PHONY: host clean
