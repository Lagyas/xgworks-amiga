/* The sound names read from an XG4 folder, dumped (PC and 68k / vamos give
 * the same text: the voice list reading is checked on the Amiga too).
 *   vxdump <XG4 folder> */
#include <stdio.h>
#include "../src/voices.h"

int main(int argc, char **argv)
{
    static const int msbs[] = {0, 16, 64, 120, 121, 126, 127};
    uint8_t order[128];
    int n, m, p, l, k;
    const char *s;
    if (argc < 2) return 10;
    n = vx_load(argv[1]);
    printf("voices %d gs %d\n", n, vx_gs_count());
    for (m = 0; m < (int)(sizeof msbs / sizeof msbs[0]); m++)
        for (p = 0; p < 128; p++)
            for (l = 0; l < 4; l++)
                if ((s = vx_name(msbs[m], l, p))) printf("v %d/%d/%d %s\n", msbs[m], l, p, s);
    for (p = 0; p < 2; p++)
        for (k = 0; k < 128; k++)
            printf("d %d %d %s\n", p, k, (s = vx_drum_of(9, p ? 127 : 0, p * 25, k)) ? s : "-");
    vx_drum_order(order);
    for (k = 0; k < 128; k++) printf("o %d %d %d\n", k, order[k], vx_drum_kind(order[k]));
    for (p = 0; p < 128; p++) printf("s %d %s / %s\n", p, vx_sound(0, 0, 0, p), vx_sound(9, 0, 0, p));
    vx_set_gs(1);
    for (p = 0; p < 128; p++) printf("g %d %s / %s / %s\n", p, vx_sound(0, 0, 0, p), vx_sound(0, 8, 0, p), vx_sound(9, 0, 0, p));
    vx_free();
    return 0;
}
