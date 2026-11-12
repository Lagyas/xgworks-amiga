/* Voice list test (PC): reads the XG4 folder given (the user's XGworks
 * installation, not in the repository) and checks some names.
 *   test_voices <XG4 folder> */
#include <stdio.h>
#include <string.h>
#include "../src/voices.h"

int main(int argc, char **argv)
{
    int n, bad = 0, i, kits = 0, vars = 0;
    const char *s;
    if (argc < 2) return 10;
    n = vx_load(argv[1]);
    if (!n) { printf("no voice lists in %s (skipped)\n", argv[1]); return 0; }
    for (i = 0; i < n; i++) {
        const VxVoice *v = vx_get(i);
        kits += v->msb >= 126;
        vars += v->msb == 0 && v->lsb != 0;
    }
    printf("%d voices, %d variations, %d kits\n", n, vars, kits);
    s = vx_name(0, 0, 0); printf("0/0/0 = %s\n", s ? s : "-"); bad += !s;
    s = vx_name(0, 18, 0); printf("0/18/0 = %s\n", s ? s : "-");
    s = vx_name(127, 0, 0); printf("127/0/0 = %s\n", s ? s : "-"); bad += !s;
    s = vx_name(64, 0, 0); printf("64/0/0 = %s\n", s ? s : "-");
    s = vx_drum(127, 0, 38); printf("kit 0 key 38 = %s\n", s ? s : "-"); bad += !s;
    s = vx_drum(127, 25, 36); printf("kit 25 key 36 = %s\n", s ? s : "-");
    printf("%s\n", bad ? "FAIL" : "OK");
    s = vx_drum_of(9, 0, 0, 36); printf("ch10 GM bank key 36 = %s\n", s ? s : "-"); bad += !s;
    s = vx_drum_of(15, 127, 25, 36); printf("ch16 kit 25 key 36 = %s\n", s ? s : "-"); bad += !s;
    {
        uint8_t o[128];
        int k, seen = 0;
        vx_drum_order(o);
        for (k = 0; k < 128; k++) seen |= o[k] == 36;
        printf("rows: %d %d %d ... kick kind %d, hat kind %d\n", o[0], o[30], o[40], vx_drum_kind(36), vx_drum_kind(42));
        bad += !seen || vx_drum_kind(36) != 2 || vx_drum_kind(42) != 3;
    }
    printf("%s\n", bad ? "FAIL" : "OK (drums)");
    vx_free();
    return bad;
}
