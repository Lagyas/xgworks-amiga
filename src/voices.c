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

/* a name without the model marks of the lists (" ***", " *4", " **") */
static size_t clean_len(const char *v, size_t vl)
{
    size_t l = vl;
    while (l && v[l - 1] == ' ') l--;
    if (l) {
        size_t k = l;
        while (k && v[k - 1] != ' ') k--;
        if (k && v[k] == '*') {
            l = k;
            while (l && v[l - 1] == ' ') l--;
        }
    }
    return l ? l : vl;
}

/* One list (XGworks' voice list format): the first section has prog=name,
 * a section named after a program has its variations lsb=name (a kit's:
 * key=drum name); a program without variations is its LSB 0 itself. */
static void load_list(const char *dir, const char *file, int msb, int kit)
{
    long n;
    char *b = read_file(dir, file, &n), *p, *sec = NULL;
    size_t seclen = 0;
    /* the programs of the top section, to know the sections of their variations */
    static struct { const char *name; size_t len; int prog; } progs[200];
    int np = 0, cur = -1, in_top = 0, nsec = 0, k, i, first = nv;
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
            in_top = nsec++ == 0;
            cur = -1;
            if (!in_top)
                for (k = 0; k < np; k++)
                    if (progs[k].len == seclen && !strncmp(progs[k].name, sec, seclen)) { cur = progs[k].prog; break; }
        } else if (len && *p != ';' && (eq = memchr(p, '=', len)) && eq > p && *p >= '0' && *p <= '9') {
            int key = atoi(p);
            const char *v = eq + 1;
            size_t vl = len - (size_t)(eq + 1 - p);
            if (in_top && key >= 0 && key < 128) {
                if (np < 200) { progs[np].name = v; progs[np].len = vl; progs[np].prog = key; np++; }
                /* a kit is a voice itself */
                if (kit) add_voice(msb, 0, key, v, clean_len(v, vl));
            } else if (cur >= 0 && key >= 0 && key < 128) {
                if (kit) add_drum(msb, cur, key, v, clean_len(v, vl));
                else add_voice(msb, key, cur, v, clean_len(v, vl));
            }
        }
        p = *e ? e + 1 : e;
    }
    /* programs without their LSB 0 among the variations: the program name */
    if (!kit)
        for (k = 0; k < np; k++) {
            /* (only this list's voices: fast on a 68000 too) */
            for (i = first; i < nv && !(vv[i].msb == msb && vv[i].prog == progs[k].prog && vv[i].lsb == 0); i++) {}
            if (i == nv) add_voice(msb, 0, progs[k].prog, progs[k].name, clean_len(progs[k].name, progs[k].len));
        }
    free(b);
}

/* ws.ini [voice list] msb=file and [kit list] msb=file: every list of the
   installation (Melody, MU Sampling, VL70-m, PLG boards, SFX, GM2, kits) */
static int load_ws(const char *dir)
{
    long n;
    char *b = read_file(dir, "ws.ini", &n), *p;
    int sect = 0, k, count = 0;
    static char files[128][64];
    static uint8_t has[128], kits[128];
    if (!b) return 0;
    memset(has, 0, sizeof has);
    memset(kits, 0, sizeof kits);
    for (p = b; *p;) {
        char *e = p, *eq;
        size_t len;
        while (*e && *e != '\n') e++;
        len = (size_t)(e - p);
        while (len && (p[len - 1] == '\r' || p[len - 1] == ' ')) len--;
        if (len && *p == '[') sect = len == 12 && !strncmp(p, "[voice list]", 12) ? 1 : len == 10 && !strncmp(p, "[kit list]", 10) ? 2 : 0;
        else if (sect && len && (eq = memchr(p, '=', len)) && *p >= '0' && *p <= '9') {
            int m = atoi(p);
            size_t fl = len - (size_t)(eq + 1 - p);
            if (m >= 0 && m < 128) {
                if (sect == 2) kits[m] = 1;
                else if (fl && fl < 58) { memcpy(files[m], eq + 1, fl); strcpy(files[m] + fl, ".ini"); has[m] = 1; }
            }
        }
        p = *e ? e + 1 : e;
    }
    free(b);
    for (k = 0; k < 128; k++)
        if (has[k]) { load_list(dir, files[k], k, kits[k]); count++; }
    return count;
}

/* ---- GS (SC-8850) from the user's midi-file-editor.html ---- */
typedef struct { uint8_t msb, lsb, prog; uint32_t name; } GsVoice;
static GsVoice *gsv;
static int ngs, capgs, gs_on;
static uint32_t gskit[128];
static uint8_t gskit_has[128];

/* one JS value of a row: a number or a string ('...' / "..."), *p after it */
static int js_value(const char **pp, long *num, char *str, int max)
{
    const char *p = *pp;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        int o = 0;
        while (*p && *p != q) {
            char c = *p++;
            if (c == '\\' && *p) {
                c = *p++;
                if (c == 'u') { int h; for (h = 0; h < 4 && *p; h++) p++; c = '?'; }
                else if (c == 'n' || c == 't') c = ' ';
            }
            if (o < max - 1) str[o++] = c;
        }
        str[o] = 0;
        if (*p) p++;
        *pp = p;
        return 2;
    }
    if ((*p >= '0' && *p <= '9') || *p == '-') {
        *num = strtol(p, (char **)&p, 10);
        while ((*p >= '0' && *p <= '9') || *p == '.') p++;
        *pp = p;
        return 1;
    }
    return 0;
}

/* `const NAME = [[...], ...]`: calls row() for each row (up to 6 values) */
static void js_rows(const char *src, const char *name, void (*row)(int n, const long *num, char str[][64]))
{
    char head[64];
    const char *p;
    snprintf(head, sizeof head, "const %s =", name);
    if (!(p = strstr(src, head))) return;
    if (!(p = strchr(p, '['))) return;
    p++;
    for (;;) {
        long num[6];
        static char str[6][64];
        int n = 0;
        while (*p && *p != '[' && *p != ']') p++;
        if (*p != '[') return;
        p++;
        while (*p && n < 6) {
            int t;
            num[n] = -1; str[n][0] = 0;
            t = js_value(&p, &num[n], str[n], 64);
            if (!t) break;
            n++;
            while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
            if (*p == ',') p++;
            else break;
        }
        while (*p && *p != ']') p++;
        if (!*p) return;
        p++;
        row(n, num, str);
    }
}

static void gs_row(int n, const long *num, char str[][64])
{
    if (n < 4 || num[0] < 0 || num[0] > 127 || num[1] < 0 || num[1] > 127 || num[2] < 0 || num[2] > 127 || !str[3][0]) return;
    if (ngs == capgs) {
        GsVoice *x = (GsVoice *)realloc(gsv, (size_t)(capgs = capgs ? capgs * 2 : 512) * sizeof(GsVoice));
        if (!x) return;
        gsv = x;
    }
    gsv[ngs].msb = (uint8_t)num[0]; gsv[ngs].lsb = (uint8_t)num[1]; gsv[ngs].prog = (uint8_t)num[2];
    gsv[ngs].name = add_name(str[3], strlen(str[3]));
    ngs++;
}

static void gs_kit_row(int n, const long *num, char str[][64])
{
    if (n < 2 || !str[0][0] || num[1] < 0 || num[1] > 127) return;
    gskit[num[1]] = add_name(str[0], strlen(str[0]));
    gskit_has[num[1]] = 1;
}

static void load_gs(const char *dir)
{
    long n;
    char *b = read_file(dir, "midi-file-editor.html", &n);
    if (!b) return;
    js_rows(b, "SC8850_VOICES", gs_row);
    js_rows(b, "GS_DRUM_KITS", gs_kit_row);
    free(b);
}

void vx_set_gs(int on) { gs_on = on; }
int vx_gs(void) { return gs_on; }
int vx_gs_count(void) { return ngs; }

/* GS: the SC-8850 map (LSB 0 / 4 and the older maps by their numbers),
   the variation (MSB), else its capital tone (MSB 0) */
static const char *gs_name(int msb, int lsb, int prog)
{
    int i, pass;
    for (pass = 0; pass < 2; pass++)
        for (i = 0; i < ngs; i++)
            if (gsv[i].prog == prog && gsv[i].msb == (pass ? 0 : msb) && (gsv[i].lsb == lsb || lsb == 0 || lsb > 4 || gsv[i].lsb == 4))
                return names + gsv[i].name;
    return NULL;
}

const char *vx_sound(int ch, int msb, int lsb, int prog)
{
    const char *n;
    extern const char *const mx_gm_names[128];
    prog &= 127;
    if (gs_on) {
        /* GS: channel 10 plays the kit of the program */
        if ((ch & 15) == 9) return gskit_has[prog] ? names + gskit[prog] : gskit_has[prog & 0x78] ? names + gskit[prog & 0x78] : "Drums";
        n = gs_name(msb, lsb, prog);
        return n ? n : mx_gm_names[prog];
    }
    /* XG / GM: channel 10 without a drum bank is the drum bank */
    if ((ch & 15) == 9 && msb < 126 && msb != 120) msb = 127;
    n = vx_name(msb, lsb, prog);
    if (!n && msb >= 126) n = vx_name(msb, 0, prog);
    if (!n && msb == 127) n = vx_name(127, 0, 0);
    if (!n) n = msb >= 126 || msb == 120 ? "Drums" : mx_gm_names[prog];
    return n;
}

static int drum_cmp(const void *a, const void *b)
{
    const VxDrum *x = (const VxDrum *)a, *y = (const VxDrum *)b;
    if (x->msb != y->msb) return x->msb - y->msb;
    if (x->prog != y->prog) return x->prog - y->prog;
    return x->key - y->key;
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
    if (gs_on) return dr_gm_names[key & 127];
    /* GM2 kits (MSB 120) have the names of the XG kits of the same number */
    if (msb == 120) msb = 127;
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
    /* every list the installation's ws.ini names, else the four main ones */
    if (!load_ws(dir)) {
        load_list(dir, "Melody.ini", 0, 0);
        load_list(dir, "SFX.ini", 64, 0);
        load_list(dir, "SFX Kit.ini", 126, 1);
        load_list(dir, "Drum Kit.ini", 127, 1);
    }
    /* the SC-8850 (GS) names, when the user's midi-file-editor.html is there */
    load_gs(dir);
    if (nv) qsort(vv, (size_t)nv, sizeof(VxVoice), voice_cmp);
    if (nd) qsort(dr, (size_t)nd, sizeof(VxDrum), drum_cmp);
    return nv;
}

void vx_free(void)
{
    free(vv); free(names); free(dr); free(gsv);
    vv = NULL; names = NULL; dr = NULL; gsv = NULL;
    nv = capv = nd = capd = ngs = capgs = 0;
    memset(gskit_has, 0, sizeof gskit_has);
    nnames = capnames = 0;
}

int vx_count(void) { return nv; }
const VxVoice *vx_get(int i) { return i >= 0 && i < nv ? &vv[i] : NULL; }
const char *vx_text(uint32_t off) { return names ? names + off : ""; }

const char *vx_name(int msb, int lsb, int prog)
{
    int i, base = -1, vars = 0, lo = 0, hi = nv, kit = msb >= 126;
    /* the voices are sorted (kits last, then program, bank MSB, LSB): the first of msb / prog */
    while (lo < hi) {
        int mid = (lo + hi) / 2, mk = vv[mid].msb >= 126;
        if (mk < kit || (mk == kit && (vv[mid].prog < prog || (vv[mid].prog == prog && vv[mid].msb < msb)))) lo = mid + 1;
        else hi = mid;
    }
    for (i = lo; i < nv && vv[i].msb == msb && vv[i].prog == prog; i++)
        {
            if (msb >= 126 || vv[i].lsb == lsb) return names + vv[i].name;
            if (vv[i].lsb == 0) base = i; else vars++;
        }
    /* a program without variations (a PLG / SFX list): its name for every LSB */
    return base >= 0 && !vars ? names + vv[base].name : NULL;
}

const char *vx_drum(int msb, int prog, int key)
{
    VxDrum k;
    const VxDrum *f;
    k.msb = (uint8_t)msb; k.prog = (uint8_t)prog; k.key = (uint8_t)key;
    if (nd && (f = (const VxDrum *)bsearch(&k, dr, (size_t)nd, sizeof(VxDrum), drum_cmp))) return names + f->name;
    /* a kit without its own name for the key: the Standard Kit's */
    if (msb == 127 && prog != 0) return vx_drum(127, 0, key);
    return NULL;
}
