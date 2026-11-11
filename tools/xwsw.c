/* .MID / .XWS -> .XWS with the C code (for the comparison with the Rust core):
 *   xwsw in out.xws */
#include <stdio.h>
#include <stdlib.h>
#include "../src/edit.h"
#include "../src/xws.h"

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    uint8_t *b, *mid, *o;
    size_t mn, on;
    EdSong s;
    if (argc != 3 || !(f = fopen(argv[1], "rb"))) return 10;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = (uint8_t *)malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) return 10;
    fclose(f);
    if (xws_is(b, (size_t)n)) {
        if (!(mid = xws_to_smf(b, (size_t)n, &mn)) || !ed_load(&s, mid, mn)) return 10;
        printf("%d tracks with blocks\n", xws_attach_blocks(b, (size_t)n, &s));
    } else if (!ed_load(&s, b, (size_t)n)) return 10;
    if (!(o = xws_write(&s, &on)) || !(f = fopen(argv[2], "wb"))) return 10;
    fwrite(o, 1, on, f);
    fclose(f);
    return 0;
}
