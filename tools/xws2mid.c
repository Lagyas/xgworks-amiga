/* .XWS -> MIDI with the C code of XGWorks Lite (for the comparison with the Rust core).
 *   xws2mid in.xws out.mid */
#include <stdio.h>
#include <stdlib.h>
#include "../src/xws.h"

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    uint8_t *b, *m;
    size_t mn;
    if (argc != 3 || !(f = fopen(argv[1], "rb"))) return 10;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) return 10;
    fclose(f);
    if (!xws_is(b, (size_t)n) || !(m = xws_to_smf(b, (size_t)n, &mn))) { printf("not an XWS\n"); return 10; }
    if (!(f = fopen(argv[2], "wb"))) return 10;
    fwrite(m, 1, mn, f);
    fclose(f);
    return 0;
}
