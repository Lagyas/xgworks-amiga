/* The Drum editor's operations (see drumops.h). */
#include <string.h>
#include "drumops.h"
#include "rollops.h"

/* General MIDI percussion (MMA, public): GM Level 1 keys 35-81, GM Level 2
 * adds 27-34 and 82-87. */
const char *const dr_gm_names[128] = {
    [27] = "High Q", [28] = "Slap", [29] = "Scratch Push", [30] = "Scratch Pull", [31] = "Sticks",
    [32] = "Square Click", [33] = "Metronome Click", [34] = "Metronome Bell",
    [82] = "Shaker", [83] = "Jingle Bell", [84] = "Belltree", [85] = "Castanets", [86] = "Mute Surdo", [87] = "Open Surdo",
    [35] = "Acoustic Bass Drum", [36] = "Bass Drum 1", [37] = "Side Stick", [38] = "Acoustic Snare", [39] = "Hand Clap",
    [40] = "Electric Snare", [41] = "Low Floor Tom", [42] = "Closed Hi-Hat", [43] = "High Floor Tom", [44] = "Pedal Hi-Hat",
    [45] = "Low Tom", [46] = "Open Hi-Hat", [47] = "Low-Mid Tom", [48] = "Hi-Mid Tom", [49] = "Crash Cymbal 1",
    [50] = "High Tom", [51] = "Ride Cymbal 1", [52] = "Chinese Cymbal", [53] = "Ride Bell", [54] = "Tambourine",
    [55] = "Splash Cymbal", [56] = "Cowbell", [57] = "Crash Cymbal 2", [58] = "Vibraslap", [59] = "Ride Cymbal 2",
    [60] = "Hi Bongo", [61] = "Low Bongo", [62] = "Mute Hi Conga", [63] = "Open Hi Conga", [64] = "Low Conga",
    [65] = "High Timbale", [66] = "Low Timbale", [67] = "High Agogo", [68] = "Low Agogo", [69] = "Cabasa",
    [70] = "Maracas", [71] = "Short Whistle", [72] = "Long Whistle", [73] = "Short Guiro", [74] = "Long Guiro",
    [75] = "Claves", [76] = "Hi Wood Block", [77] = "Low Wood Block", [78] = "Mute Cuica", [79] = "Open Cuica",
    [80] = "Mute Triangle", [81] = "Open Triangle",
};

int dr_keys_used(const EdSong *s, uint32_t trk, int ch, uint32_t used[128])
{
    uint32_t i;
    int n = 0, k;
    memset(used, 0, 128 * sizeof(uint32_t));
    if (trk >= s->ntrk) return 0;
    for (i = 0; i < s->trk[trk].n; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        if (ro_is_note(e, ch)) used[e->d1]++;
    }
    for (k = 0; k < 128; k++) n += used[k] != 0;
    return n;
}

long dr_hit(const EdSong *s, uint32_t trk, int ch, int key, uint32_t t0, uint32_t t1)
{
    long i;
    if (trk >= s->ntrk) return -1;
    for (i = 0; i < (long)s->trk[trk].n; i++) {
        const EdEv *e = &s->trk[trk].ev[i];
        if (e->tick >= t1) break;
        if (e->tick >= t0 && ro_is_note(e, ch) && e->d1 == key) return i;
    }
    return -1;
}

int dr_toggle(EdSong *s, uint32_t trk, int ch, int key, uint32_t t, uint32_t step, int vel)
{
    long i = dr_hit(s, trk, ch, key, t, t + (step ? step : 1));
    EdEv e;
    if (i >= 0) {
        ed_delete(s, trk, (uint32_t)i);
        return -1;
    }
    memset(&e, 0, sizeof e);
    e.tick = t;
    e.st = (uint8_t)(0x90 | ((ch < 0 ? 9 : ch) & 15));
    e.d1 = (uint8_t)key;
    e.d2 = (uint8_t)(vel < 1 ? 1 : vel > 127 ? 127 : vel);
    e.dur = step / 2 ? step / 2 : 1;
    return ed_insert(s, trk, &e) > 0 ? 1 : 0;
}
