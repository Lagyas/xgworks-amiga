/* Prints the karaoke lines and chords of songs as the Amiga shows them.
 *   test_lyrics [-latin2] <song.mid>... */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/smf.h"
#include "../src/lyrics.h"
#include "../src/textmap.h"

int main(int argc, char **argv)
{
    int i, letters = TM_ASCII;
    for (i = 1; i < argc; i++) {
        FILE *f;
        uint8_t *b;
        long n;
        XgSong s;
        Lyrics ly;
        uint32_t k;
        if (!strcmp(argv[i], "-latin2")) { letters = TM_LATIN2; continue; }
        if (!(f = fopen(argv[i], "rb"))) continue;
        fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
        b = (uint8_t *)malloc((size_t)n);
        fread(b, 1, (size_t)n, f);
        fclose(f);
        if (xg_smf_load(&s, b, (size_t)n) != XG_OK || !ly_build(&ly, &s, letters)) { printf("%s: cannot read\n", argv[i]); continue; }
        printf("== %s: %lu syllables, %lu lines, %lu chords\n", argv[i], (unsigned long)ly.nsyl, (unsigned long)ly.nline, (unsigned long)ly.nchord);
        for (k = 0; k < ly.nline && k < 8; k++) {
            uint32_t j;
            printf("  |");
            for (j = 0; j < ly.line[k].n; j++) printf("%s", ly.text + ly.syl[ly.line[k].first + j].off);
            printf("|\n");
        }
        printf("  chords:");
        for (k = 0; k < ly.nchord && k < 12; k++) printf(" %s", ly.chord[k].name);
        printf("\n");
        ly_free(&ly);
        xg_smf_free(&s);
        free(b);
    }
    return 0;
}
