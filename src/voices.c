/* Sound names from the XGworks voice lists (see voices.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "voices.h"
#include "drumops.h"

static VxVoice *vv;
static int nv, capv;
static char *names;
static uint32_t nnames, capnames;
/* drum names: (msb, prog, key) -> name */
typedef struct { uint8_t msb, prog, key; uint32_t name; } VxDrum;
static VxDrum *dr;
static int nd, capd;
/* inst.ini: the Drum window's row order and the drum kinds */
static uint8_t dorder[128], dkind[128];
static int ndorder;

static uint32_t add_name(const char *s, size_t n)
{
    uint32_t at;
    if (nnames + n + 1 > capnames) {
        uint32_t cap = capnames ? capnames * 2 : 16384;
        char *nb;
        while (cap < nnames + n + 1) cap *= 2;
        if (!(nb = (char *)realloc(names, cap))) return 0;
        names = nb;
        capnames = cap;
    }
    at = nnames;
    memcpy(names + at, s, n);
    names[at + n] = 0;
    nnames += (uint32_t)n + 1;
    return at;
}

static void add_voice(int msb, int lsb, int prog, const char *s, size_t n)
{
    if (nv == capv) {
        VxVoice *x = (VxVoice *)realloc(vv, (size_t)(capv = capv ? capv * 2 : 512) * sizeof(VxVoice));
        if (!x) return;
        vv = x;
    }
    vv[nv].msb = (uint8_t)msb; vv[nv].lsb = (uint8_t)lsb; vv[nv].prog = (uint8_t)prog;
    vv[nv].name = add_name(s, n);
    nv++;
}

static void add_drum(int msb, int prog, int key, const char *s, size_t n)
{
    if (nd == capd) {
        VxDrum *x = (VxDrum *)realloc(dr, (size_t)(capd = capd ? capd * 2 : 1024) * sizeof(VxDrum));
        if (!x) return;
        dr = x;
    }
    dr[nd].msb = (uint8_t)msb; dr[nd].prog = (uint8_t)prog; dr[nd].key = (uint8_t)key;
    dr[nd].name = add_name(s, n);
    nd++;
}

static char *read_file(const char *dir, const char *file, long *n)
{
    char path[300];
    FILE *f;
    char *b;
    size_t l = strlen(dir);
    snprintf(path, sizeof path, "%s%s%s", dir, l && dir[l - 1] != ':' && dir[l - 1] != '/' ? "/" : "", file);
    if (!(f = fopen(path, "rb"))) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (*n <= 0 || !(b = (char *)malloc((size_t)*n + 1))) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    b[*n] = 0;
    fclose(f);
    return b;
}

/* One list: [top] prog=name; [name] lsb=variation (kits: key=drum name). */
static void load_list(const char *dir, const char *file, const char *top, int msb, int kit)
{
    long n;
    char *b = read_file(dir, file, &n), *p, *sec = NULL;
    size_t seclen = 0;
    /* the programs of the top section, to know the sections of their variations */
    struct { const char *name; size_t len; int prog; } progs[200];
    int np = 0, cur = -1, in_top = 0;
    if (!b) return;
    for (p = b; *p;) {
        char *e = p, *eq;
        size_t len;
        while (*e && *e != '\n') e++;
        len = (size_t)(e - p);
        while (len && (p[len - 1] == '\r' || p[len - 1] == ' ')) len--;
        if (len && *p == '[') {
            sec = p + 1;
            seclen = len >= 2 ? len - 2 : 0;
            in_top = seclen == strlen(top) && !strncmp(sec, top, seclen);
            cur = -1;
            if (!in_top) {
                int k;
                for (k = 0; k < np; k++)
                    if (progs[k].len == seclen && !strncmp(progs[k].name, sec, seclen)) { cur = progs[k].prog; break; }
            }
        } else if (len && *p != ';' && (eq = memchr(p, '=', len)) && eq > p) {
            int key = atoi(p);
            const char *v = eq + 1;
            size_t vl = len - (size_t)(eq + 1 - p);
            if (in_top && key >= 0 && key < 128) {
                if (np < 200) { progs[np].name = v; progs[np].len = vl; progs[np].prog = key; np++; }
                /* a kit / an SFX program is a voice itself; Melody gets its variations (LSB 0 among them) */
                if (kit || msb == 64) add_voice(msb, 0, key, v, vl);
            } else if (cur >= 0 && key >= 0 && key < 128) {
                if (kit) add_drum(msb, cur, key, v, vl);
                else if (msb != 64 || key != 0) add_voice(msb, key, cur, v, vl);
            }
        }
        p = *e ? e + 1 : e;
    }
    /* Melody programs without a section of variations: the program name as LSB 0 */
    if (!kit && msb == 0) {
        int k, i;
        for (k = 0; k < np; k++) {
            for (i = 0; i < nv && !(vv[i].msb == 0 && vv[i].prog == progs[k].prog && vv[i].lsb == 0); i++) {}
            if (i == nv) add_voice(0, 0, progs[k].prog, progs[k].name, progs[k].len);
        }
    }
    free(b);
}

static int voice_cmp(const void *a, const void *b)
{
    const VxVoice *x = (const VxVoice *)a, *y = (const VxVoice *)b;
    /* tones by program, then by bank; kits after */
    int kx = x->msb >= 126, ky = y->msb >= 126;
    if (kx != ky) return kx - ky;
    if (x->prog != y->prog) return x->prog - y->prog;
    if (x->msb != y->msb) return x->msb - y->msb;
    return x->lsb - y->lsb;
}

/* inst.ini [User]: "row=NNname" (NN the key) and "InstColor<row>=kind" */
static void load_inst(const char *dir)
{
    long n;
    char *b = read_file(dir, "inst.ini", &n), *p;
    int in_user = 0, k;
    uint8_t seen[128];
    ndorder = 0;
    memset(seen, 0, sizeof seen);
    for (k = 0; k < 128; k++) {
        /* without inst.ini: the kinds of the GM keys (as the PC version) */
        dkind[k] = (uint8_t)(k == 35 || k == 36 ? 2 : k == 42 || k == 44 || k == 46 ? 3
            : k == 49 || k == 51 || k == 52 || k == 53 || k == 55 || k == 57 || k == 59 ? 4
            : k == 41 || k == 43 || k == 45 || k == 47 || k == 48 || k == 50 ? 5
            : k == 37 || k == 38 || k == 40 ? 0 : 1);
    }
    if (b) {
        uint8_t rowkey[128];
        int nrow = 0;
        for (p = b; *p;) {
            char *e = p;
            size_t len;
            while (*e && *e != '\n') e++;
            len = (size_t)(e - p);
            while (len && (p[len - 1] == '\r' || p[len - 1] == ' ')) len--;
            if (len && *p == '[') in_user = len == 6 && !strncmp(p, "[User]", 6);
            else if (in_user && len) {
                char *eq = memchr(p, '=', len);
                if (eq && !strncmp(p, "InstColor", 9)) {
                    int row = atoi(p + 9);
                    if (row >= 0 && row < nrow) dkind[rowkey[row]] = (uint8_t)atoi(eq + 1);
                } else if (eq && *p >= '0' && *p <= '9' && eq[1] >= '0' && eq[1] <= '9') {
                    int key = atoi(eq + 1);
                    if (key >= 0 && key < 128 && !seen[key] && nrow < 128) {
                        seen[key] = 1;
                        rowkey[nrow++] = (uint8_t)key;
                        dorder[ndorder++] = (uint8_t)key;
                    }
                }
            }
            p = *e ? e + 1 : e;
        }
        free(b);
    }
    if (!ndorder)
        for (k = 127; k >= 0; k--) { dorder[ndorder++] = (uint8_t)k; seen[k] = 1; }
    for (k = 0; k < 128; k++)
        if (!seen[k]) dorder[ndorder++] = (uint8_t)k;
}

void vx_drum_order(uint8_t order[128])
{
    if (!ndorder) load_inst("");
    memcpy(order, dorder, 128);
}

int vx_drum_kind(int key)
{
    if (!ndorder) load_inst("");
    return dkind[key & 127];
}

const char *vx_drum_of(int ch, int msb, int prog, int key)
{
    const char *n;
    /* channel 10 without a drum bank: the drum bank (XG) */
    if (msb < 126) { msb = 127; if ((ch & 15) != 9) prog = 0; }
    n = vx_drum(msb, prog, key);
    if (!n && msb == 126) n = vx_drum(127, 0, key);
    if (n) {
        const char *q = n;
        while (*q == ' ') q++;
        if (!*q || *q == '*') n = NULL;
    }
    if (!n) n = dr_gm_names[key & 127];
    return n;
}

int vx_load(const char *dir)
{
    vx_free();
    load_inst(dir);
    load_list(dir, "Melody.ini", "Melody", 0, 0);
    load_list(dir, "SFX.ini", "SFX", 64, 0);
    load_list(dir, "SFX Kit.ini", "SFX Kit", 126, 1);
    load_list(dir, "Drum Kit.ini", "Drum Kit", 127, 1);
    if (nv) qsort(vv, (size_t)nv, sizeof(VxVoice), voice_cmp);
    return nv;
}

void vx_free(void)
{
    free(vv); free(names); free(dr);
    vv = NULL; names = NULL; dr = NULL;
    nv = capv = nd = capd = 0;
    nnames = capnames = 0;
}

int vx_count(void) { return nv; }
const VxVoice *vx_get(int i) { return i >= 0 && i < nv ? &vv[i] : NULL; }
const char *vx_text(uint32_t off) { return names ? names + off : ""; }

const char *vx_name(int msb, int lsb, int prog)
{
    int i;
    for (i = 0; i < nv; i++)
        if (vv[i].msb == msb && vv[i].prog == prog && (msb >= 126 || vv[i].lsb == lsb)) return names + vv[i].name;
    return NULL;
}

const char *vx_drum(int msb, int prog, int key)
{
    int i;
    for (i = 0; i < nd; i++)
        if (dr[i].msb == msb && dr[i].prog == prog && dr[i].key == key) return names + dr[i].name;
    /* a kit without its own name for the key: the Standard Kit's */
    if (msb == 127 && prog != 0) return vx_drum(127, 0, key);
    return NULL;
}
