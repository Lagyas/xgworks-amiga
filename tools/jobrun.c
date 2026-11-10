/* The same Jobs with the C code (for the comparison with the Rust core):
 *   jobrun <name> in.mid out.mid */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/edit.h"
#include "../src/jobs.h"

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    uint8_t *b, *o;
    size_t on;
    EdSong s;
    JbScope sc;
    JbMod m;
    const char *j;
    uint32_t bar3, bar5;
    if (argc != 4 || !(f = fopen(argv[2], "rb"))) return 10;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) return 10;
    fclose(f);
    if (!ed_load(&s, b, (size_t)n)) return 10;
    sc.trk = -1; sc.from = 0; sc.to = 0xFFFFFFFFUL; sc.lo = 0; sc.hi = 127;
    memset(&m, 0, sizeof m);
    bar3 = ed_tick_of(&s, 3, 1, 0);
    bar5 = ed_tick_of(&s, 5, 1, 0);
    j = argv[1];
    if (!strcmp(j, "transpose")) jb_transpose(&s, &sc, 5);
    else if (!strcmp(j, "velocity")) { m.rate = 80; m.offset = 10; jb_velocity(&s, &sc, m); }
    else if (!strcmp(j, "gate")) { m.absolute = 1; m.value = 100; jb_gate(&s, &sc, m); }
    else if (!strcmp(j, "shift")) jb_shift(&s, &sc, 37);
    else if (!strcmp(j, "sort")) jb_sort_chord(&s, &sc, 1);
    else if (!strcmp(j, "separate")) { jb_sort_chord(&s, &sc, 0); jb_separate_chord(&s, &sc, 10); }
    else if (!strcmp(j, "control")) { m.rate = 50; jb_modify_control(&s, &sc, JB_CC, 7, 11, m); }
    else if (!strcmp(j, "bend")) { m.rate = 50; m.offset = 100; jb_modify_control(&s, &sc, JB_BEND, 0, 0, m); }
    else if (!strcmp(j, "thin")) jb_thin_out(&s, &sc, JB_CC, 7);
    else if (!strcmp(j, "quantize")) { JbQntz q = {2, 80, 100, 60, 90, 110}; jb_quantize(&s, &sc, q); }
    else if (!strcmp(j, "stretch")) jb_stretch(&s, &sc, 150, 10);
    else if (!strcmp(j, "create")) jb_create_measure(&s, bar3, 3, 4, 2, 4, 4);
    else if (!strcmp(j, "delete")) jb_delete_measure(&s, bar3, bar5);
    else if (!strcmp(j, "chord")) jb_transpose_chord(&s, 0, 0xFFFFFFFFUL, 3);
    else return 10;
    o = ed_save(&s, &on);
    if (!(f = fopen(argv[3], "wb"))) return 10;
    fwrite(o, 1, on, f);
    fclose(f);
    return 0;
}
