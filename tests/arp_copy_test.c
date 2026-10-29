/* Every fixed riff with every progression in every mode, harmony mode, rate,
 * length, riff octave (and colour for TONIC) must not contain a 7-note figure of
 * the reference recordings (the same check the RANDOM riffs pass). */
#ifndef FM1_HOST_TEST
#define FM1_HOST_TEST
#endif
#include "../app/arp.c"
#include <stdio.h>
/* No Flash on the host: user presets are not part of this test. */
uint32_t fm1_flash_size(void) { return 0; }
int fm1_flash_read(uint32_t o, void *d, size_t n) { (void)o; (void)d; (void)n; return -1; }
int fm1_flash_write(uint32_t o, const void *d, size_t n) { (void)o; (void)d; (void)n; return -1; }
int fm1_flash_erase(uint32_t o) { (void)o; return -1; }
void arp_midi_note(unsigned c, unsigned n, unsigned v) { (void)c; (void)n; (void)v; }
static void set(unsigned p, unsigned v) { arp_edit(p, (int)v - (int)arp_get(p)); }
int main(void)
{
    arp_init();
    unsigned combos = 0, copies = 0, silent = 0;
    for (unsigned pat = 0; pat < FIXED_PATTERNS; ++pat)
        for (unsigned prog = 0; prog < FIXED_PROGS; ++prog)
            for (unsigned h = 0; h < 4; ++h)
                for (unsigned rate = 0; rate < 4; ++rate)
                    for (unsigned len = 0; len < 4; ++len)
                        for (unsigned oct = 0; oct < 5; ++oct)
                            for (unsigned mc = 0; mc < (progs[prog].count ? MODES : MODES * COLORS); ++mc) {
                                set(AP_PATTERN, pat); set(AP_PROG, prog); set(AP_HARMONY, h); set(AP_RATE, rate);
                                set(AP_CHORD_LEN, len); set(AP_ARP_OCT, oct);
                                /* every mode: the chords recoloured; TONIC also every colour */
                                if (progs[prog].count) set(AP_MODE, mc);
                                else { set(AP_MODE, mc / COLORS); set(AP_COLOR, mc % COLORS); }
                                ++combos;
                                {
                                    const struct arp_song *now = &arp_songs[arp_song_index];
                                    unsigned notes = 0;
                                    for (unsigned i = 0; i < now->pattern_len; ++i) notes += now->pattern[i] >= 0;
                                    silent += !notes;
                                }
                                if (song_copies(&arp_songs[arp_song_index])) {
                                    if (copies < 20)
                                        printf("COPY riff %s, chords %s, harmony %u, rate %u, len %u, oct %u, mode/color %u\n",
                                               patterns[pat].name, progs[prog].name, h, rate, len, oct, mc);
                                    ++copies;
                                }
                            }
    printf("%s %u of %u riff/chord combinations contain a reference figure\n", copies ? "FAIL" : "PASS", copies, combos);
    printf("%u publishes nudged a riff step, %u riffs had to fall silent\n", varied, silent);
    return copies != 0 || silent != 0;
}
