/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* ARPHONY control side: modes, chord colours, voicings, riffs,
 * progressions, presets, tempo and MIDI.
 *
 * The harmonic language is modal pop harmony as heard in the reference
 * recordings:
 *  - modal rather than functional: Dorian vamps with a major IV (i9 - IV),
 *    Aeolian bVI - bVII that never resolve, Lydian/Mixolydian IV - V,
 *    major/minor mixture over one bass note (I - IV/I - i - bVII/I);
 *  - coloured chords: add9, maj7/maj9, m7/m9, 6 and 6/9 instead of plain
 *    triads; open voicings (bass, fifth, ninth or tenth);
 *  - pedal points: a bass note that stays while the chords change above it
 *    (Imaj9 - IV/I), and inner pedals in the riff;
 *  - one repeating riff whose notes are shared by the chords, so the same
 *    figure changes its meaning when the bass moves (LEAD mode). */
#include "arp_internal.h"
#include "instrument.h"
#include <string.h>

static const char note_names[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* ------------------------------------------------------------ modes */
static const struct { char name[9]; uint8_t step[7]; } modes[] = {
    {"MAJOR", {0, 2, 4, 5, 7, 9, 11}},  {"DORIAN", {0, 2, 3, 5, 7, 9, 10}}, {"PHRYGIAN", {0, 1, 3, 5, 7, 8, 10}},
    {"LYDIAN", {0, 2, 4, 6, 7, 9, 11}}, {"MIXOLYD", {0, 2, 4, 5, 7, 9, 10}}, {"MINOR", {0, 2, 3, 5, 7, 8, 10}},
};
enum { MODES = sizeof modes / sizeof modes[0], M_MAJOR = 0, M_DORIAN, M_PHRYGIAN, M_LYDIAN, M_MIXOLYD, M_MINOR };

/* ------------------------------------------------------------ chords */
/* Chord qualities as pitch-class masks relative to the root (bit 0 = root). */
enum quality { Q_MAJ, Q_MIN, Q_SUS2, Q_SUS4, Q_ADD9, Q_MADD9, Q_MAJ7, Q_MIN7, Q_DOM7, Q_MAJ9, Q_MIN9,
               Q_SIX, Q_SIX9, Q_MIN6, Q_MIN11, Q_DIM, Q_HALFDIM, Q_POWER, Q_MAJ7S5,
               Q_DOM9, Q_MIN69, Q_SUS4_7, Q_MAJ7S11, Q_MIN7B9, Q_MAJ6S11, QUALITIES };
static const struct { char suffix[8]; uint16_t mask; } qualities[QUALITIES] = {
    [Q_MAJ] = {"", 0x091},     /* 0 4 7 */
    [Q_MIN] = {"-", 0x089},    /* 0 3 7 (minor is written with a dash: the LCD has no lower case) */
    [Q_SUS2] = {"SUS2", 0x085}, [Q_SUS4] = {"SUS4", 0x0a1},
    [Q_ADD9] = {"ADD9", 0x095}, [Q_MADD9] = {"-ADD9", 0x08d},
    [Q_MAJ7] = {"MAJ7", 0x891}, [Q_MIN7] = {"-7", 0x489}, [Q_DOM7] = {"7", 0x491},
    [Q_MAJ9] = {"MAJ9", 0x895}, [Q_MIN9] = {"-9", 0x48d},
    [Q_SIX] = {"6", 0x291}, [Q_SIX9] = {"6/9", 0x295}, [Q_MIN6] = {"-6", 0x289},
    [Q_MIN11] = {"-11", 0x4a9}, [Q_DIM] = {"DIM", 0x049}, [Q_HALFDIM] = {"-7B5", 0x449}, [Q_POWER] = {"5", 0x081},
    [Q_MAJ7S5] = {"MAJ7#5", 0x911}, /* 0 4 8 11: the held third and seventh over a falling bass */
    [Q_DOM9] = {"9", 0x495}, [Q_MIN69] = {"-6/9", 0x28d}, [Q_SUS4_7] = {"7SUS4", 0x4a1},
    [Q_MAJ7S11] = {"MAJ7#11", 0x8d1}, [Q_MIN7B9] = {"-7B9", 0x48b}, [Q_MAJ6S11] = {"6#11", 0x2d1},
};
struct chord { uint8_t root, bass; uint16_t mask; }; /* absolute pitch classes; mask relative to the root */

/* Name: root, quality, "/bass" for slash chords. Unknown sets show the root. */
static void chord_text(const struct chord *c, char out[16])
{
    strcpy(out, note_names[c->root % 12U]);
    for (unsigned q = 0; q < QUALITIES; ++q)
        if (qualities[q].mask == c->mask) { strcat(out, qualities[q].suffix); break; }
    if (c->bass != c->root && strlen(out) + strlen(note_names[c->bass % 12U]) < 15) {
        strcat(out, "/");
        strcat(out, note_names[c->bass % 12U]);
    }
}

/* ------------------------------------------------------------ progressions */
/* Roots and basses in semitones above the key's tonic; bass -1 = the root.
 * Written for the reference sounds; none of them copies a song's chords in
 * order and rhythm, they are the modal moves those songs are built from. */
struct prog_chord { uint8_t root, quality; int8_t bass; };
static const struct prog {
    char name[13];
    uint8_t count, mode;   /* mode: the one the chords are written in (MODE jumps there) */
    struct prog_chord chord[ARP_MAX_CHORDS];
} progs[] = {
    {"I-9 IV", 2, M_DORIAN, {{0, Q_MIN9, -1}, {5, Q_ADD9, -1}}},                                    /* Dorian vamp, major IV */
    {"I III VII IV", 4, M_DORIAN, {{0, Q_MIN7, -1}, {3, Q_MAJ7, -1}, {10, Q_ADD9, -1}, {5, Q_SIX9, -1}}}, /* Dorian four */
    {"BVI BVII", 2, M_MINOR, {{8, Q_MAJ7, -1}, {10, Q_ADD9, -1}}},                                 /* Aeolian, never resolves */
    {"VI VII I", 4, M_MINOR, {{8, Q_MAJ7, -1}, {10, Q_SIX9, -1}, {0, Q_MADD9, -1}, {0, Q_MIN7, -1}}},
    {"MAJ9 PEDAL", 2, M_MAJOR, {{0, Q_MAJ9, -1}, {5, Q_ADD9, 0}}},                                 /* Imaj9 - IV/I over the tonic */
    {"II III IV", 4, M_MAJOR, {{2, Q_MIN7, -1}, {4, Q_MIN7, -1}, {5, Q_MAJ7, -1}, {4, Q_MIN7, -1}}},  /* rising steps */
    {"IV V LYDIAN", 4, M_MAJOR, {{5, Q_ADD9, -1}, {7, Q_SUS2, -1}, {5, Q_MAJ7, -1}, {2, Q_MIN9, -1}}}, /* IV - V that floats */
    {"I BIII", 2, M_MAJOR, {{0, Q_ADD9, -1}, {3, Q_MAJ7, -1}}},                                    /* chromatic mediant */
    {"MIX PEDAL", 4, M_MIXOLYD, {{0, Q_ADD9, -1}, {5, Q_ADD9, 0}, {0, Q_MADD9, -1}, {10, Q_ADD9, 0}}},  /* I IV/I i bVII/I */
    {"I IV MINOR", 4, M_MINOR, {{0, Q_MADD9, -1}, {5, Q_MIN7, -1}, {0, Q_MADD9, -1}, {10, Q_SUS2, -1}}},
    {"III IV", 4, M_DORIAN, {{3, Q_ADD9, -1}, {5, Q_ADD9, -1}, {3, Q_MAJ7, -1}, {5, Q_SIX, -1}}},     /* Dorian, bass up a tone */
    {"I VII IV III", 4, M_DORIAN, {{0, Q_MIN9, -1}, {10, Q_ADD9, -1}, {5, Q_SIX, -1}, {3, Q_MAJ7, -1}}}, /* Dorian wheel */
    {"LINE DOWN", 4, M_MAJOR, {{0, Q_ADD9, -1}, {0, Q_MAJ7, 11}, {0, Q_DOM7, 10}, {5, Q_SIX9, 9}}}, /* I - I/7 - I/b7 - IV/6 */
    {"IV PENTA", 4, M_MAJOR, {{5, Q_MAJ9, -1}, {5, Q_MAJ9, -1}, {7, Q_SIX9, -1}, {9, Q_MIN7, -1}}},  /* use HARMONY PENTA */
    {"TONIC", 0, 255, {{0, 0, -1}}}, /* one chord: the key's own, from MODE and COLOR */
};
enum { FIXED_PROGS = sizeof progs / sizeof progs[0], PROGS = FIXED_PROGS + 1, PROG_RANDOM = FIXED_PROGS };
/* RANDOM chords (the last progression), from arp_random_chords: roots in
 * semitones above the key, a colour each (built in MODE, so turning MODE
 * recolours them too), a bass (-1 = the root, 0 = over the tonic). */
static struct { uint8_t count, root[ARP_MAX_CHORDS], color[ARP_MAX_CHORDS]; int8_t bass[ARP_MAX_CHORDS]; } random_prog = {
    2, {0, 5}, {1, 1}, {-1, -1}};
static const char *prog_name(unsigned i) { return i < FIXED_PROGS ? progs[i].name : "RANDOM"; }

/* ------------------------------------------------------------ riffs */
/* Ladder indices (0 = the chord's lowest riff tone; with a four-tone chord
 * 4 is the octave). R and T both leave a step out: the note before holds
 * through it (no silent gaps). */
#define R ARP_REST
#define T ARP_TIE
static const struct { char name[11]; uint8_t len; int8_t step[ARP_PATTERN_MAX]; } patterns[] = {
    {"RISE", 16, {0, 3, 5, 4, 6, 5, 8, 7, 5, 6, 4, 5, 3, 4, 2, 1}},  /* climbs in zig-zags, sinks back */
    {"PEDAL", 8, {1, 2, 1, 3, 1, 2, 1, 4}},                          /* an inner pedal tone, neighbours above */
    {"CASCADE", 16, {8, 7, 6, 4, 7, 6, 4, 3, 6, 4, 3, 2, 4, 3, 2, 1}},
    {"BROKEN", 8, {0, 2, 1, 3, 2, 4, 3, 5}},                         /* broken thirds, climbing */
    {"OCTAVES", 8, {0, 4, 0, 4, 2, 6, 2, 5}},
    {"TRIPLET", 12, {0, 2, 4, 5, 4, 2, 1, 3, 5, 6, 5, 3}},           /* for 1/8T: a 12/8 shuffle */
    {"PING", 16, {4, R, R, 6, R, R, 4, R, 7, R, R, 6, R, R, 3, R}},  /* few, long notes for the delay */
    {"WHEEL", 6, {0, 2, 4, 3, 5, 4}},                                /* six against the bar */
    {"UP", 8, {0, 1, 2, 3, 4, 5, 6, 7}},
    {"DOWN", 8, {8, 7, 6, 5, 4, 3, 2, 1}},
    {"UP DOWN", 14, {0, 1, 2, 3, 4, 5, 6, 7, 6, 5, 4, 3, 2, 1}},
    {"PULSE", 8, {4, 4, 5, 4, 4, 6, 4, 5}},                          /* repeated-note drive */
    {"SWAY", 8, {3, 4, 3, 2, 3, 4, 5, 4}},
    {"LEAPS", 8, {0, 5, 1, 6, 2, 7, 3, 8}},
    {"SPARKLE", 16, {8, 9, 10, 9, 8, 7, 8, 6, 8, 9, 10, 11, 10, 9, 8, 7}},
    {"SIGH", 16, {6, T, 5, 4, 5, T, 4, 3, 4, T, 3, 2, 3, T, 2, R}},  /* a falling line with held notes */
    {"INTERLOCK", 16, {2, 5, 3, 6, 1, 5, 3, 7, 2, 5, 3, 6, 4, 5, 3, 8}}, /* a busy ostinato around one held tone */
};
#undef R
#undef T
enum { FIXED_PATTERNS = sizeof patterns / sizeof patterns[0], PATTERNS = FIXED_PATTERNS + 1 };
/* RANDOM (the last riff): written by arp_random_riff. */
static struct { char name[11]; uint8_t len; int8_t step[ARP_PATTERN_MAX]; } random_riff = {
    "RANDOM", 8, {0, 2, 1, 3, 2, 4, 3, 1}};
static unsigned random_count;
static const char *pattern_name(unsigned i) { return i < FIXED_PATTERNS ? patterns[i].name : random_riff.name; }
static unsigned pattern_len(unsigned i) { return i < FIXED_PATTERNS ? patterns[i].len : random_riff.len; }
static const int8_t *pattern_steps(unsigned i) { return i < FIXED_PATTERNS ? patterns[i].step : random_riff.step; }

/* Bass rhythms per 16th of a bar: 1 root, 2 octave up, 3 fifth. */
static const struct { char name[9]; uint8_t hit[16]; } rhythms[] = {
    {"HOLD", {0}},
    {"8THS", {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
    {"16THS", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {"SYNC", {1, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0}},
    {"OCTAVES", {1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0}},
    {"ROOT 5", {1, 0, 0, 0, 3, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0}},
    {"QUARTERS", {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"DOTTED", {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
};
enum { RHYTHMS = sizeof rhythms / sizeof rhythms[0] };
static const struct { char name[6]; uint8_t ticks, swing; } rates[] = {
    {"1/8", 48, 1}, {"1/8T", 32, 0}, {"1/16", 24, 1}, {"1/16T", 16, 0}};
static const char colors[][6] = {"TRIAD", "ADD9", "7TH", "9TH", "SUS2", "6TH"};
enum { COLORS = sizeof colors / sizeof colors[0], COLOR_TRIAD = 0, COLOR_ADD9, COLOR_7TH, COLOR_9TH, COLOR_SUS2, COLOR_6TH };
static const char harmony_modes[4][7] = {"FOLLOW", "PEDAL", "LEAD", "PENTA"};
enum { H_FOLLOW, H_PEDAL, H_LEAD, H_PENTA };
static const char twin_modes[3][7] = {"OFF", "OCTAVE", "PINGS"};
/* TWIN PINGS, syncopated, two octaves up the riff ladder (0 = its root). */
static const int8_t pings[16] = {-1, -1, -1, 2, -1, -1, -1, -1, -1, -1, 4, -1, -1, 1, -1, -1};
static const char chord_lengths[4][6] = {"1/2", "1 BAR", "2 BAR", "4 BAR"};
static const uint16_t chord_ticks[4] = {ARP_BAR / 2, ARP_BAR, 2 * ARP_BAR, 4 * ARP_BAR};
static const float fm_ratios[8] = {0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f, 3.5f};
static const char fm_ratio_names[8][4] = {"0.5", "1", "2", "3", "4", "5", "7", "3.5"};

/* ------------------------------------------------------------ parameters */
static const struct arp_param_info info[AP_COUNT] = {
    [AP_KEY] = {"KEY", 11}, [AP_MODE] = {"MODE", MODES - 1}, [AP_PROG] = {"CHORDS", PROGS - 1},
    [AP_CHORD_LEN] = {"LENGTH", 3}, [AP_COLOR] = {"COLOR", COLORS - 1},
    [AP_PATTERN] = {"RIFF", PATTERNS - 1}, [AP_RATE] = {"RATE", 3}, [AP_HARMONY] = {"HARMONY", 3},
    [AP_ARP_OCT] = {"ARP OCT", 4}, [AP_GATE] = {"GATE", 99}, [AP_SWING] = {"SWING", 99},
    [AP_ACCENT] = {"ACCENT", 99}, [AP_MUTATE] = {"MUTATE", 99},
    [AP_BASS_RHY] = {"BASS RHY", RHYTHMS - 1}, [AP_NOTES] = {"NOTES", 2}, [AP_BASS_OCT] = {"BASS OCT", 2},
    [AP_BASS_LVL] = {"BASS LVL", 99}, [AP_CHORD_LVL] = {"CHORD LVL", 99}, [AP_HARM_FX] = {"HARM FX", 99},
    [AP_A_SHAPE] = {"SHAPE", 99}, [AP_A_DETUNE] = {"DETUNE", 99}, [AP_A_OSC2] = {"OSC 2", 99},
    [AP_A_FM] = {"FM BELL", 99}, [AP_A_RATIO] = {"FM RATIO", 7}, [AP_A_TONE] = {"TONE", 99},
    [AP_A_TONE_ENV] = {"TONE ENV", 99}, [AP_A_ATTACK] = {"ATTACK", 99}, [AP_A_DECAY] = {"DECAY", 99},
    [AP_A_SUSTAIN] = {"SUSTAIN", 99}, [AP_A_RELEASE] = {"RELEASE", 99}, [AP_A_LEVEL] = {"ARP LVL", 99},
    [AP_H_SHAPE] = {"SHAPE", 99}, [AP_H_DETUNE] = {"DETUNE", 99}, [AP_H_OSC2] = {"OSC 2", 99},
    [AP_H_SUB] = {"SUB", 99}, [AP_H_TONE] = {"TONE", 99}, [AP_H_TONE_ENV] = {"TONE ENV", 99},
    [AP_H_ATTACK] = {"ATTACK", 99}, [AP_H_DECAY] = {"DECAY", 99}, [AP_H_SUSTAIN] = {"SUSTAIN", 99},
    [AP_H_RELEASE] = {"RELEASE", 99},
    [AP_DRIFT] = {"DRIFT", 99}, [AP_SPREAD] = {"WIDTH", 99}, [AP_VIB_RATE] = {"VIB RATE", 99},
    [AP_VIB_DEPTH] = {"VIBRATO", 99},
    [AP_FILTER] = {"CUTOFF", 99}, [AP_RESO] = {"RESO", 99}, [AP_DRIVE] = {"DRIVE", 99},
    [AP_LFO_RATE] = {"LFO RATE", 99}, [AP_LFO_DEPTH] = {"LFO AMT", 99},
    [AP_DELAY_TIME] = {"DLY TIME", 99}, [AP_DELAY_FEEDBACK] = {"DLY FB", 99}, [AP_DELAY_MIX] = {"DLY MIX", 99},
    [AP_TAPE_TIME] = {"TAPE TM", 99}, [AP_TAPE_FEEDBACK] = {"TAPE FB", 120}, [AP_TAPE_WOW] = {"WOW", 99},
    [AP_TAPE_TONE] = {"TAPE TONE", 99}, [AP_TAPE_MIX] = {"TAPE MIX", 99},
    [AP_TAPE_SYNC] = {"TAPE SYNC", 1}, [AP_DELAY_SYNC] = {"DLY SYNC", 1}, [AP_LFO_SYNC] = {"LFO SYNC", 1},
    [AP_REV_SIZE] = {"SIZE", 99}, [AP_REV_DAMP] = {"DAMP", 99}, [AP_SHIMMER] = {"SHIMMER", 99},
    [AP_REV_MIX] = {"REV MIX", 99}, [AP_BPM] = {"BPM", 200}, [AP_LEVEL] = {"LEVEL", 99},
    [AP_TWIN] = {"TWIN", 2}, [AP_TWIN_LVL] = {"TWIN LVL", 99}, [AP_LEAD] = {"LEAD", 5}, [AP_MIDI_OUT] = {"MIDI OUT", 1},
};
const struct arp_param_info *arp_param_info(unsigned p) { return p < AP_COUNT ? info + p : 0; }

static const struct { char name[6]; float beats; } echo_notes[] = {
    {"1/16", 0.25f}, {"1/8T", 1.0f / 3.0f}, {"1/16D", 0.375f}, {"1/8", 0.5f}, {"1/4T", 2.0f / 3.0f},
    {"1/8D", 0.75f}, {"1/4", 1.0f}, {"1/2T", 4.0f / 3.0f}, {"1/4D", 1.5f}, {"1/2", 2.0f}, {"1/1", 4.0f}};
static const struct { char name[7]; float beats; } lfo_notes[] = {
    {"8 BARS", 32.0f}, {"4 BARS", 16.0f}, {"2 BARS", 8.0f}, {"1 BAR", 4.0f},
    {"1/2", 2.0f}, {"1/4", 1.0f}, {"1/8", 0.5f}, {"1/16", 0.25f}};
enum { ECHO_NOTES = sizeof echo_notes / sizeof echo_notes[0], LFO_NOTES = sizeof lfo_notes / sizeof lfo_notes[0] };
static unsigned note_index(unsigned v, unsigned count) { unsigned i = v * count / 100U; return i < count ? i : count - 1U; }

static float unit(unsigned v) { return (float)v / 99.0f; }

/* LEAD: ready sounds for the ARP layer (PLAY 3/3 K4), chosen by ear.
 * SINE LONG is the default for every preset; CLASSIC is the preset's own
 * ARP sound of the first versions.
 * Columns: SHAPE DETUNE OSC2 FM RATIO TONE TONE-ENV ATTACK DECAY SUSTAIN RELEASE */
static const struct { char name[10]; uint8_t v[11]; } leads[] = {
    {"SINE LONG", {0, 0, 0, 0, 2, 99, 0, 0, 80, 20, 70}},
    {"SINE", {0, 0, 0, 0, 2, 99, 0, 0, 40, 40, 35}},
    {"SINE SOFT", {0, 0, 0, 0, 2, 99, 0, 20, 60, 60, 50}},
    {"HARP", {20, 0, 0, 15, 1, 45, 40, 0, 70, 0, 65}},
    {"AIRY", {25, 40, 60, 10, 1, 50, 20, 25, 70, 60, 70}},
    {"CLASSIC", {0}},
};
enum { LEADS = sizeof leads / sizeof leads[0], LEAD_CLASSIC = LEADS - 1 };
_Static_assert(LEADS == 6, "AP_LEAD max");
static const uint8_t lead_params[11] = {AP_A_SHAPE, AP_A_DETUNE, AP_A_OSC2, AP_A_FM, AP_A_RATIO, AP_A_TONE,
                                        AP_A_TONE_ENV, AP_A_ATTACK, AP_A_DECAY, AP_A_SUSTAIN, AP_A_RELEASE};
static float attack_s(unsigned v) { float u = unit(v); return 0.001f + u * u * u * 4.0f; }
static float decay_s(unsigned v) { float u = unit(v); return 0.02f + u * u * 4.0f; }
static float release_s(unsigned v) { float u = unit(v); return 0.01f + u * u * 5.0f; }
static float vib_hz(unsigned v) { return 0.5f + unit(v) * 8.5f; }
static float lfo_hz(unsigned v) { return 0.03f * steel_exp2(unit(v) * 7.6f); }
static float tape_s(unsigned v) { float u = unit(v); return 0.04f + u * u * (STEEL_TAPE_MAX_S - 0.04f); }
static float delay_s(unsigned v) { float u = unit(v); return 0.02f + u * u * (STEEL_DELAY_MAX_S - 0.02f); }
static float one_pole(float hz) { return 1.0f - steel_exp2(-6.2831853f * 1.4426950f / STEEL_RATE * hz); }

static struct arp_preset current;
static float beat_s(void) { return 60.0f / (float)(40U + current.value[AP_BPM]); }
static float tape_time_s(void)
{
    return current.value[AP_TAPE_SYNC] ? echo_notes[note_index(current.value[AP_TAPE_TIME], ECHO_NOTES)].beats * beat_s()
                                       : tape_s(current.value[AP_TAPE_TIME]);
}
static float delay_time_s(void)
{
    return current.value[AP_DELAY_SYNC] ? echo_notes[note_index(current.value[AP_DELAY_TIME], ECHO_NOTES)].beats * beat_s()
                                        : delay_s(current.value[AP_DELAY_TIME]);
}
static float lfo_rate_hz(void)
{
    return current.value[AP_LFO_SYNC] ? 1.0f / (lfo_notes[note_index(current.value[AP_LFO_RATE], LFO_NOTES)].beats * beat_s())
                                      : lfo_hz(current.value[AP_LFO_RATE]);
}
static void number(char *out, unsigned v)
{
    char tmp[8]; unsigned n = 0;
    do { tmp[n++] = (char)('0' + v % 10U); v /= 10U; } while (v && n < 7);
    while (n) *out++ = tmp[--n];
    *out = 0;
}
static void seconds(char *out, float s)
{
    if (s < 1.0f) { number(out, (unsigned)(s * 1000.0f + 0.5f)); strcat(out, "MS"); return; }
    unsigned tenths = (unsigned)(s * 10.0f + 0.5f);
    number(out, tenths / 10U); strcat(out, "."); number(out + strlen(out), tenths % 10U); strcat(out, "S");
}
static void hertz(char *out, float hz)
{
    unsigned t = (unsigned)(hz * 100.0f + 0.5f);
    number(out, t / 100U); strcat(out, ".");
    if (t % 100U < 10U) strcat(out, "0");
    number(out + strlen(out), t % 100U); strcat(out, "HZ");
}
void arp_param_text(unsigned p, unsigned v, char out[16])
{
    switch (p) {
    case AP_KEY: strcpy(out, note_names[v % 12U]); return;
    case AP_MODE: strcpy(out, modes[v < MODES ? v : 0].name); return;
    case AP_PROG: strcpy(out, prog_name(v < PROGS ? v : 0)); return;
    case AP_CHORD_LEN: strcpy(out, chord_lengths[v & 3U]); return;
    case AP_COLOR: strcpy(out, colors[v < COLORS ? v : 0]); return;
    case AP_PATTERN: strcpy(out, pattern_name(v < PATTERNS ? v : 0)); return;
    case AP_RATE: strcpy(out, rates[v & 3U].name); return;
    case AP_HARMONY: strcpy(out, harmony_modes[v < 4 ? v : 0]); return;
    case AP_TWIN: strcpy(out, twin_modes[v < 3 ? v : 0]); return;
    case AP_MIDI_OUT: strcpy(out, v ? "CH 1 2 3" : "OFF"); return;
    case AP_LEAD: strcpy(out, leads[v < LEADS ? v : 0].name); return;
    case AP_ARP_OCT: strcpy(out, "C"); number(out + 1, 3U + v); return;
    case AP_BASS_OCT: strcpy(out, "G"); number(out + 1, 1U + v); return;
    case AP_BASS_RHY: strcpy(out, rhythms[v < RHYTHMS ? v : 0].name); return;
    case AP_NOTES: number(out, v + 1U); return;
    case AP_A_RATIO: strcpy(out, fm_ratio_names[v & 7U]); return;
    case AP_SWING: number(out, 50U + v * 25U / 99U); strcat(out, "%"); return;
    case AP_GATE: number(out, 5U + v * 115U / 99U); strcat(out, "%"); return;
    case AP_A_ATTACK: case AP_H_ATTACK: seconds(out, attack_s(v)); return;
    case AP_A_DECAY: case AP_H_DECAY: seconds(out, decay_s(v)); return;
    case AP_A_RELEASE: case AP_H_RELEASE: seconds(out, release_s(v)); return;
    case AP_VIB_RATE: hertz(out, vib_hz(v)); return;
    case AP_TAPE_FEEDBACK: number(out, v); strcat(out, "%"); return;
    case AP_TAPE_TIME:
        if (current.value[AP_TAPE_SYNC]) { strcpy(out, echo_notes[note_index(v, ECHO_NOTES)].name); return; }
        seconds(out, tape_s(v)); return;
    case AP_DELAY_TIME:
        if (current.value[AP_DELAY_SYNC]) { strcpy(out, echo_notes[note_index(v, ECHO_NOTES)].name); return; }
        seconds(out, delay_s(v)); return;
    case AP_TAPE_SYNC: case AP_DELAY_SYNC: case AP_LFO_SYNC: strcpy(out, v ? "SYNC" : "FREE"); return;
    case AP_LFO_RATE:
        if (current.value[AP_LFO_SYNC]) { strcpy(out, lfo_notes[note_index(v, LFO_NOTES)].name); return; }
        hertz(out, lfo_hz(v)); return;
    case AP_BPM: number(out, 40U + v); return;
    case AP_A_DETUNE: case AP_H_DETUNE: number(out, v * 30U / 99U); strcat(out, "CT"); return;
    default: number(out, v); return;
    }
}

/* ------------------------------------------------------------ presets */
/* Every preset is the default sound with a few settings changed. */
static const uint8_t defaults[AP_COUNT] = {
    [AP_KEY] = 2, [AP_MODE] = M_DORIAN, [AP_PROG] = 0, [AP_CHORD_LEN] = 1, [AP_COLOR] = COLOR_ADD9,
    [AP_PATTERN] = 0, [AP_RATE] = 2, [AP_HARMONY] = H_LEAD, [AP_ARP_OCT] = 1, [AP_GATE] = 45, [AP_SWING] = 0,
    [AP_ACCENT] = 40, [AP_MUTATE] = 0,
    [AP_BASS_RHY] = 1, [AP_NOTES] = 2, [AP_BASS_OCT] = 0, [AP_BASS_LVL] = 60, [AP_CHORD_LVL] = 40, [AP_HARM_FX] = 25,
    [AP_A_SHAPE] = 55, [AP_A_DETUNE] = 25, [AP_A_OSC2] = 45, [AP_A_FM] = 25, [AP_A_RATIO] = 2, [AP_A_TONE] = 55,
    [AP_A_TONE_ENV] = 45, [AP_A_ATTACK] = 2, [AP_A_DECAY] = 30, [AP_A_SUSTAIN] = 35, [AP_A_RELEASE] = 30,
    [AP_A_LEVEL] = 80,
    [AP_H_SHAPE] = 66, [AP_H_DETUNE] = 30, [AP_H_OSC2] = 55, [AP_H_SUB] = 20, [AP_H_TONE] = 38, [AP_H_TONE_ENV] = 20,
    [AP_H_ATTACK] = 8, [AP_H_DECAY] = 55, [AP_H_SUSTAIN] = 75, [AP_H_RELEASE] = 30,
    [AP_DRIFT] = 20, [AP_SPREAD] = 60, [AP_VIB_RATE] = 40, [AP_VIB_DEPTH] = 0,
    [AP_FILTER] = 99, [AP_RESO] = 0, [AP_DRIVE] = 0, [AP_LFO_RATE] = 30, [AP_LFO_DEPTH] = 0,
    [AP_DELAY_TIME] = 50, [AP_DELAY_FEEDBACK] = 52, [AP_DELAY_MIX] = 55,
    [AP_TAPE_TIME] = 60, [AP_TAPE_FEEDBACK] = 30, [AP_TAPE_WOW] = 25, [AP_TAPE_TONE] = 50, [AP_TAPE_MIX] = 0,
    [AP_TAPE_SYNC] = 1, [AP_DELAY_SYNC] = 1, [AP_LFO_SYNC] = 1,
    [AP_REV_SIZE] = 70, [AP_REV_DAMP] = 50, [AP_SHIMMER] = 10, [AP_REV_MIX] = 30,
    [AP_BPM] = 56, [AP_LEVEL] = 72, [AP_TWIN] = 0, [AP_TWIN_LVL] = 55,
};
#define END 255
static const struct { char name[13]; uint8_t set[48]; } presets[] = {
    /* E Dorian, i9 - IV: a pulsing octave riff, 8th bass. */
    {"DORIAN SWAY", {AP_KEY, 4, AP_MODE, M_DORIAN, AP_PROG, 0, AP_CHORD_LEN, 2, AP_PATTERN, 4, AP_RATE, 0,
                     AP_BPM, 64, AP_BASS_RHY, 1, AP_A_FM, 15, AP_A_SHAPE, 70, AP_DELAY_TIME, 50, AP_DELAY_MIX, 50, END}},
    /* i - III - VII - IV with broken thirds. */
    {"MODAL FOUR", {AP_KEY, 11, AP_MODE, M_DORIAN, AP_PROG, 1, AP_PATTERN, 3, AP_RATE, 2, AP_BPM, 76,
                    AP_BASS_RHY, 6, AP_A_FM, 35, AP_A_RATIO, 3, AP_REV_MIX, 35, END}},
    /* D minor, bVI - bVII: sparse pings in a long delay, syncopated bass. */
    {"SIX TO SEVEN", {AP_KEY, 2, AP_MODE, M_MINOR, AP_PROG, 2, AP_PATTERN, 6, AP_RATE, 2, AP_BPM, 68,
                      AP_BASS_RHY, 3, AP_DELAY_TIME, 50, AP_DELAY_FEEDBACK, 60, AP_DELAY_MIX, 75, AP_A_FM, 55,
                      AP_A_RATIO, 4, AP_A_DECAY, 45, AP_GATE, 30, AP_REV_MIX, 40, AP_SHIMMER, 25, END}},
    /* F major, Imaj9 - IV/F in a 12/8 shuffle, the bass on the pedal. */
    {"MAJ9 SHUFFLE", {AP_KEY, 5, AP_MODE, M_MAJOR, AP_PROG, 4, AP_PATTERN, 5, AP_RATE, 1, AP_BPM, 62,
                      AP_HARMONY, H_FOLLOW, AP_BASS_RHY, 7, AP_GATE, 60, AP_A_SHAPE, 80, AP_A_FM, 10,
                      AP_DELAY_TIME, 40, AP_DELAY_MIX, 45, AP_SWING, 0, END}},
    /* ii - iii - IV - iii, up and down in 16ths. */
    {"RISING STEPS", {AP_KEY, 9, AP_MODE, M_MAJOR, AP_PROG, 5, AP_PATTERN, 10, AP_RATE, 2, AP_BPM, 72,
                      AP_BASS_RHY, 1, AP_A_FM, 30, AP_REV_MIX, 30, END}},
    /* A: IV - V that never lands, a falling line, held harmony. */
    {"LYDIAN FLOAT", {AP_KEY, 9, AP_MODE, M_MAJOR, AP_PROG, 6, AP_PATTERN, 15, AP_RATE, 0, AP_BPM, 50,
                      AP_HARMONY, H_FOLLOW, AP_BASS_RHY, 0, AP_CHORD_LVL, 55, AP_GATE, 90, AP_A_ATTACK, 10,
                      AP_A_SUSTAIN, 60, AP_A_RELEASE, 45, AP_A_FM, 20, AP_REV_MIX, 45, AP_SHIMMER, 30,
                      AP_H_ATTACK, 35, AP_H_RELEASE, 55, END}},
    /* E add9 - G maj7: the inner pedal riff over a chromatic mediant. */
    {"MEDIANT", {AP_KEY, 4, AP_MODE, M_MAJOR, AP_PROG, 7, AP_CHORD_LEN, 2, AP_PATTERN, 1, AP_RATE, 2, AP_BPM, 56,
                 AP_BASS_RHY, 6, AP_A_FM, 40, AP_A_RATIO, 2, AP_DELAY_TIME, 50, AP_DELAY_MIX, 60, END}},
    /* F#: I - IV/F# - i - bVII/F# over one bass note. */
    {"PEDAL MIX", {AP_KEY, 6, AP_MODE, M_MIXOLYD, AP_PROG, 8, AP_PATTERN, 0, AP_RATE, 2, AP_BPM, 56,
                   AP_BASS_RHY, 1, AP_A_FM, 30, AP_REV_MIX, 35, END}},
    /* E minor: i - iv - i - bVII, driving 16ths, darker and filtered. */
    {"MINOR DRIVE", {AP_KEY, 4, AP_MODE, M_MINOR, AP_PROG, 9, AP_PATTERN, 11, AP_RATE, 2, AP_BPM, 60,
                     AP_BASS_RHY, 2, AP_BASS_LVL, 80, AP_FILTER, 75, AP_RESO, 30, AP_DRIVE, 25, AP_LFO_RATE, 40,
                     AP_LFO_DEPTH, 25, AP_A_FM, 0, AP_A_SHAPE, 99, AP_DELAY_MIX, 45, END}},
    /* C Dorian: III - IV, the bass a tone apart, a falling cascade. */
    {"THIRD FOURTH", {AP_KEY, 0, AP_MODE, M_DORIAN, AP_PROG, 10, AP_CHORD_LEN, 1, AP_PATTERN, 2, AP_RATE, 2,
                      AP_BPM, 44, AP_BASS_RHY, 4, AP_A_FM, 45, AP_A_RATIO, 3, AP_DELAY_MIX, 50, END}},
    /* C Dorian: i - VII - IV - III, six notes against the bar. */
    {"DORIAN WHEEL", {AP_KEY, 0, AP_MODE, M_DORIAN, AP_PROG, 11, AP_PATTERN, 7, AP_RATE, 2, AP_BPM, 70,
                      AP_BASS_RHY, 5, AP_A_FM, 25, AP_REV_MIX, 30, END}},
    /* D: the chord stays, the bass falls by semitones D - C# - C - B underneath. */
    {"LINE DOWN", {AP_KEY, 2, AP_MODE, M_MAJOR, AP_PROG, 12, AP_CHORD_LEN, 1, AP_PATTERN, 12, AP_RATE, 0,
                   AP_HARMONY, H_PEDAL, AP_BPM, 58, AP_BASS_RHY, 0, AP_NOTES, 2, AP_CHORD_LVL, 50,
                   AP_A_FM, 20, AP_A_SHAPE, 75, AP_H_ATTACK, 20, AP_H_RELEASE, 45, AP_REV_MIX, 40, END}},
    /* Eb pentatonic over IV (Ab): a busy ostinato, syncopated pings above, held bass. */
    {"PENTA IV", {AP_KEY, 3, AP_MODE, M_MAJOR, AP_PROG, 13, AP_CHORD_LEN, 2, AP_PATTERN, 16, AP_RATE, 2,
                  AP_HARMONY, H_PENTA, AP_ARP_OCT, 0, AP_BPM, 54, AP_TWIN, 2, AP_TWIN_LVL, 65,
                  AP_BASS_RHY, 0, AP_NOTES, 1, AP_GATE, 40, AP_A_FM, 45, AP_A_RATIO, 3, AP_A_DECAY, 22,
                  AP_A_SUSTAIN, 15, AP_A_SHAPE, 40, AP_DELAY_MIX, 45, AP_REV_MIX, 35, END}},
    /* C Lydian, one chord: play the chords on the keys. */
    {"GLASS KEYS", {AP_KEY, 0, AP_MODE, M_LYDIAN, AP_PROG, 14, AP_COLOR, COLOR_9TH, AP_PATTERN, 13, AP_RATE, 2,
                    AP_BPM, 50, AP_ARP_OCT, 2, AP_A_FM, 70, AP_A_RATIO, 6, AP_A_SHAPE, 0, AP_A_OSC2, 20,
                    AP_A_DECAY, 40, AP_A_SUSTAIN, 10, AP_A_RELEASE, 55, AP_BASS_RHY, 0, AP_REV_MIX, 55,
                    AP_SHIMMER, 45, AP_DELAY_MIX, 60, END}},
    /* D: the inner pedal over the tonic, played by hand. */
    {"HAND DORIAN", {AP_KEY, 2, AP_MODE, M_DORIAN, AP_PROG, 14, AP_COLOR, COLOR_ADD9, AP_PATTERN, 1, AP_RATE, 2,
                     AP_BPM, 60, AP_BASS_RHY, 1, AP_A_FM, 30, END}},
};
#undef END
enum { PRESETS = sizeof presets / sizeof presets[0] };
unsigned arp_preset_count(void) { return PRESETS; }
const char *arp_preset_name(unsigned i) { return i < PRESETS ? presets[i].name : ""; }

/* ------------------------------------------------------------ harmony */
static unsigned preset_index;
static uint8_t hand, hand_pc, hand_pedal; /* HAND: the chord on the keys */
static struct chord prog_chord[ARP_MAX_CHORDS];
static unsigned prog_count;

/* The diatonic chord on pitch class pc (relative to the key) in the current
 * mode, in COLOR. Roots outside the mode are borrowed as major chords
 * (bIII, bVI, bVII ...), the way modal mixture works. */
/* Internal colours used when a progression is recoloured (not on the COLOR knob). */
enum { COLOR_SUS4 = COLORS, COLOR_POWER };
/* The chord on `rel` semitones above the key in `mode`, in colour `color`.
 * Roots outside the mode are major (borrowed). */
static uint16_t diatonic_in(unsigned mode, unsigned rel, unsigned color)
{
    const uint8_t *s = modes[mode < MODES ? mode : 0].step;
    int d = -1;
    for (int i = 0; i < 7; ++i) if (s[i] == rel % 12U) d = i;
    unsigned third = 4, fifth = 7, seventh = 10, ninth = 2, sixth = 9;
    if (d >= 0) {
#define ABOVE(k) ((unsigned)(s[(d + (k)) % 7] + 12 - s[d]) % 12U)
        third = ABOVE(2); fifth = ABOVE(4); seventh = ABOVE(6); ninth = ABOVE(1); sixth = ABOVE(5);
#undef ABOVE
    }
    uint16_t m = (uint16_t)(1U | 1U << third | 1U << fifth);
    switch (color) {
    case COLOR_ADD9: if (ninth == 2) m |= 1U << 2; else m |= (uint16_t)(1U << seventh); break;
    case COLOR_7TH: m |= (uint16_t)(1U << seventh); break;
    case COLOR_9TH: m |= (uint16_t)(1U << seventh); if (ninth == 2) m |= 1U << 2; break;
    case COLOR_SUS2: m = (uint16_t)(1U | (ninth == 2 ? 1U << 2 : 1U << 5) | 1U << fifth); break;
    case COLOR_6TH: /* only a major sixth; a b6 (Phrygian, Aeolian v) would sour the chord */
        if (sixth == 9) m |= (uint16_t)(1U << sixth);
        if (ninth == 2 && fifth == 7) m |= 1U << 2;
        break;
    case COLOR_SUS4: m = (uint16_t)(1U | 1U << 5 | 1U << fifth); break;
    case COLOR_POWER: m = (uint16_t)(1U | 1U << fifth); break;
    default: break;
    }
    return m;
}
static uint16_t diatonic(unsigned rel) { return diatonic_in(current.value[AP_MODE], rel, current.value[AP_COLOR]); }
static unsigned in_mode(unsigned mode, unsigned rel)
{
    for (unsigned i = 0; i < 7; ++i) if (modes[mode < MODES ? mode : 0].step[i] == rel % 12U) return 1;
    return 0;
}
/* The colour a written chord has (triad, add9, 7th ...), to rebuild it in another mode. */
static unsigned color_of(unsigned quality)
{
    switch (quality) {
    case Q_ADD9: case Q_MADD9: return COLOR_ADD9;
    case Q_MAJ7: case Q_MIN7: case Q_DOM7: case Q_HALFDIM: case Q_MAJ7S5: return COLOR_7TH;
    case Q_MAJ9: case Q_MIN9: case Q_MIN11: return COLOR_9TH;
    case Q_SUS2: return COLOR_SUS2;
    case Q_SUS4: return COLOR_SUS4;
    case Q_SIX: case Q_SIX9: case Q_MIN6: return COLOR_6TH;
    case Q_POWER: return COLOR_POWER;
    default: return COLOR_TRIAD;
    }
}
/* A written chord, recoloured from its progression's mode into MODE: the
 * root stays, the chord takes MODE's notes in the same colour. Chords that
 * are borrowed already in the progression's own mode (a root outside it, or
 * the other third, like i in a Mixolydian I - i) stay as written. */
static uint16_t recolour(unsigned native, unsigned rel, unsigned quality)
{
    unsigned mode = current.value[AP_MODE];
    uint16_t written = qualities[quality].mask;
    if (native >= MODES || mode == native || !in_mode(native, rel) || !in_mode(mode, rel)) return written;
    uint16_t own = diatonic_in(native, rel, COLOR_TRIAD);
    unsigned third = written & (1U << 3) ? 3U : written & (1U << 4) ? 4U : 0U;
    if (third && !(own & (1U << third))) return written; /* borrowed in its own mode */
    return diatonic_in(mode, rel, color_of(quality));
}
static void build_chords(void)
{
    unsigned key = current.value[AP_KEY] % 12U;
    if (current.value[AP_PROG] == PROG_RANDOM) {
        prog_count = random_prog.count;
        for (unsigned i = 0; i < prog_count; ++i) {
            prog_chord[i].root = (uint8_t)((key + random_prog.root[i]) % 12U);
            prog_chord[i].bass = (uint8_t)(random_prog.bass[i] < 0 ? prog_chord[i].root : (key + (unsigned)random_prog.bass[i]) % 12U);
            prog_chord[i].mask = diatonic_in(current.value[AP_MODE], random_prog.root[i], random_prog.color[i]);
        }
        return;
    }
    const struct prog *pr = &progs[current.value[AP_PROG] < FIXED_PROGS ? current.value[AP_PROG] : 0];
    if (!pr->count) {
        prog_count = 1;
        prog_chord[0].root = prog_chord[0].bass = (uint8_t)key;
        prog_chord[0].mask = diatonic(0);
        return;
    }
    prog_count = pr->count;
    for (unsigned i = 0; i < pr->count; ++i) {
        const struct prog_chord *c = &pr->chord[i];
        prog_chord[i].root = (uint8_t)((key + c->root) % 12U);
        prog_chord[i].bass = (uint8_t)(c->bass < 0 ? prog_chord[i].root : (key + (unsigned)c->bass) % 12U);
        prog_chord[i].mask = recolour(pr->mode, c->root, c->quality);
    }
}
static struct chord hand_chord(unsigned pc, unsigned pedal)
{
    unsigned key = current.value[AP_KEY] % 12U;
    struct chord c;
    c.root = (uint8_t)(pc % 12U);
    c.bass = (uint8_t)(pedal ? key : c.root);
    c.mask = diatonic((pc + 12U - key) % 12U);
    return c;
}
/* The tonic chord, the riff's home in PEDAL and LEAD. */
static struct chord tonic_chord(void)
{
    if (!hand) return prog_chord[0];
    unsigned key = current.value[AP_KEY] % 12U;
    struct chord c = {(uint8_t)key, (uint8_t)key, diatonic(0)};
    return c;
}
/* Riff pitches of chord c, root nearest to `base`, ascending. */
static void ladder_of(const struct chord *c, float base, float out[ARP_LADDER])
{
    uint8_t tone[12]; unsigned n = 0;
    for (unsigned k = 0; k < 12; ++k) if (c->mask & (1U << k)) tone[n++] = (uint8_t)k;
    if (!n) { tone[0] = 0; n = 1; }
    int root = (int)base + (int)((c->root + 12U - (unsigned)base % 12U) % 12U);
    if (root - base > 6) root -= 12;
    for (unsigned i = 0, o = 0, k = 0; i < ARP_LADDER; ++i) {
        float p = (float)(root + 12 * (int)o + tone[k]);
        while (p > 105.0f) p -= 12.0f; /* keep the top in range */
        out[i] = p;
        if (++k == n) { k = 0; ++o; }
    }
}
/* LEAD: move each riff note to the nearest tone of chord c (ties go down),
 * so tones the chords share stay where they are. */
static float nearest_tone(const struct chord *c, float pitch)
{
    for (int d = 0; d <= 6; ++d) {
        int down = (int)pitch - d, up = (int)pitch + d;
        unsigned rd = (unsigned)(down - (int)c->root + 120) % 12U, ru = (unsigned)(up - (int)c->root + 120) % 12U;
        if (c->mask & (1U << rd)) return (float)down;
        if (c->mask & (1U << ru)) return (float)up;
    }
    return pitch;
}
/* Harmony: the bass in the bass octave (G1..F#2 at BASS OCT 0, where the
 * reference basses sit); the upper tones an octave higher, spread open
 * (fifth + octave, then the colour tone), like held pad notes over a synth bass. */
static void voicing(const struct chord *c, float out[3], float *fifth, int *previous)
{
    int low = 31 + 12 * (int)current.value[AP_BASS_OCT]; /* G1, G2, G3 */
    int bass = low + (int)((c->bass + 12U - (unsigned)low % 12U) % 12U);
    /* Stepwise bass: the octave nearest the previous bass, a few semitones
     * past the window at most (F#2 -> G2, not down to G1). */
    if (*previous >= 0) {
        int other = bass > *previous ? bass - 12 : bass + 12;
        int d = bass - *previous, e = other - *previous;
        if ((e < 0 ? -e : e) < (d < 0 ? -d : d) && other >= low - 3 && other <= low + 15) bass = other;
    }
    *previous = bass;
    *fifth = (float)(bass + ((c->mask & (1U << 6)) && !(c->mask & (1U << 7)) ? 6 : 7));
    out[0] = (float)bass;
    /* The colour tone: ninth, seventh, sixth, else the third as a tenth. */
    unsigned m = c->mask, colour;
    if (m & (1U << 2)) colour = 14;
    else if (m & (1U << 11)) colour = 11;
    else if (m & (1U << 10)) colour = 10;
    else if (m & (1U << 9)) colour = 9;
    else if (m & (1U << 4)) colour = 16;
    else if (m & (1U << 3)) colour = 15;
    else if (m & (1U << 5)) colour = 17;
    else colour = 12;
    if (c->bass == c->root) {
        unsigned fifth_iv = (m & (1U << 7)) ? 7 : (m & (1U << 6)) ? 6 : (m & (1U << 8)) ? 8 : 12;
        out[1] = (float)(bass + 12 + (int)fifth_iv);
        out[2] = (float)(bass + 12 + (int)colour);
    } else {
        /* Slash chord over a pedal: the chord's root and colour above the bass. */
        int root = bass + 19 + (int)((c->root + 12U - (unsigned)(bass + 19) % 12U) % 12U);
        out[1] = (float)root;
        int up = root + (int)(colour % 12U);
        if (up <= root) up += 12;
        out[2] = (float)up;
    }
}

/* ------------------------------------------------------------ publish */
static unsigned song_copies(const struct arp_song *s);
static unsigned varied;                        /* riffs nudged away from a reference figure (tests) */
static void derive(void)
{
    struct arp_patch *ap = &arp_patches[arp_patch_index ^ 1U];
    struct steel_patch *p = &ap->fx;
    const uint8_t *v = current.value;
    for (unsigned l = 0; l < 2; ++l) {
        struct arp_layer *L = &ap->layer[l];
        unsigned b = l ? AP_H_SHAPE : AP_A_SHAPE;
        L->shape = unit(v[b]) * 3.0f;
        L->detune = steel_exp2(unit(v[b + 1]) * 30.0f / 1200.0f);
        L->osc2 = unit(v[b + 2]);
        if (l == 0) {
            L->sub = 0.0f;
            L->fm = unit(v[AP_A_FM]) * 1.4f;
            L->fm_index = 0.25f + unit(v[AP_A_FM]) * 2.2f;
            L->fm_ratio = fm_ratios[v[AP_A_RATIO] & 7U];
        } else {
            L->sub = unit(v[AP_H_SUB]);
            L->fm = L->fm_index = 0.0f; L->fm_ratio = 1.0f;
        }
        unsigned t = l ? AP_H_TONE : AP_A_TONE, a = l ? AP_H_ATTACK : AP_A_ATTACK;
        L->tone = 30.0f + (float)v[t];
        L->tone_env = unit(v[t + 1]) * 48.0f;
        L->attack = 1.0f / (attack_s(v[a]) * STEEL_RATE);
        L->decay = 1.0f - steel_exp2(-9.965784f / (decay_s(v[a + 1]) * STEEL_RATE));
        L->sustain = unit(v[a + 2]);
        L->release = 1.0f - steel_exp2(-9.965784f / (release_s(v[a + 3]) * STEEL_RATE));
    }
    ap->arp_level = unit(v[AP_A_LEVEL]) * 1.6f; /* the riff leads the mix */
    ap->bass_level = unit(v[AP_BASS_LVL]) * 1.1f;
    ap->chord_level = unit(v[AP_CHORD_LVL]) * 0.7f;
    ap->harm_fx = unit(v[AP_HARM_FX]);
    ap->drift = unit(v[AP_DRIFT]) * unit(v[AP_DRIFT]) * 0.25f;
    ap->spread = unit(v[AP_SPREAD]);
    ap->vib_rate = vib_hz(v[AP_VIB_RATE]);
    ap->vib_depth = unit(v[AP_VIB_DEPTH]) * unit(v[AP_VIB_DEPTH]) * 0.5f;
    ap->level = unit(v[AP_LEVEL]) * unit(v[AP_LEVEL]) * 0.7f;
    p->filter_on = v[AP_FILTER] < 99 || v[AP_RESO] || v[AP_DRIVE];
    p->cutoff = 20.0f + (float)v[AP_FILTER] * 1.17f;
    p->resonance = unit(v[AP_RESO]) * 3.9f;
    p->drive = 1.0f + unit(v[AP_DRIVE]) * 4.0f;
    p->lfo_step = lfo_rate_hz() * FM1_BLOCK_FRAMES / STEEL_RATE;
    p->lfo_depth = unit(v[AP_LFO_DEPTH]) * 36.0f;
    p->tape_time = steel_clamp(tape_time_s(), 0.01f, STEEL_TAPE_MAX_S) * STEEL_RATE;
    p->tape_feedback = (float)v[AP_TAPE_FEEDBACK] * 0.01f;
    p->tape_wow = unit(v[AP_TAPE_WOW]);
    p->tape_tone = one_pole(600.0f * steel_exp2(unit(v[AP_TAPE_TONE]) * 3.5f));
    p->tape_mix = unit(v[AP_TAPE_MIX]) * 0.9f;
    p->delay_time = steel_clamp(delay_time_s(), 0.01f, STEEL_DELAY_MAX_S) * STEEL_RATE;
    p->delay_feedback = unit(v[AP_DELAY_FEEDBACK]) * 0.92f;
    p->delay_mix = unit(v[AP_DELAY_MIX]) * 1.1f; /* the echoes up front, as in the references */
    p->rev_decay = 0.25f + unit(v[AP_REV_SIZE]) * 0.70f;
    p->rev_damp = one_pole(1200.0f * steel_exp2((1.0f - unit(v[AP_REV_DAMP])) * 3.5f) * 0.5f);
    p->rev_shimmer = unit(v[AP_SHIMMER]) * 0.95f;
    p->rev_mix = unit(v[AP_REV_MIX]) * 1.6f;
    p->rev_dry = 1.0f - 0.9f * steel_clamp(unit(v[AP_REV_MIX]) - 0.5f, 0.0f, 0.5f);
    p->bpm = (float)(40U + v[AP_BPM]);
    STEEL_BARRIER();
    arp_patch_index ^= 1U;
}
static unsigned voice_cap = ARP_VOICES;
static struct chord shown[ARP_MAX_CHORDS];  /* what the song holds, for the display */
static unsigned shown_count;
static uint8_t hand_slot;                   /* HAND: the song slot the next chord goes to */
/* Compute and publish the song. `jump`: switch to `jump_chord` on the next beat. */
static void publish(unsigned jump)
{
    struct arp_song *s = &arp_songs[arp_song_index ^ 1U];
    const uint8_t *v = current.value;
    build_chords();
    struct chord list[ARP_MAX_CHORDS];
    unsigned count;
    if (hand) {
        /* Two slots: the playing chord and the next one. */
        list[0] = shown[0]; list[1] = shown[1];
        count = 2;
        s->chord_ticks = ARP_HOLD;
        s->jump_chord = hand_slot;
    } else {
        for (unsigned i = 0; i < prog_count; ++i) list[i] = prog_chord[i];
        count = prog_count;
        s->chord_ticks = chord_ticks[v[AP_CHORD_LEN] & 3U];
        s->jump_chord = 0;
        for (unsigned i = 0; i < count; ++i) shown[i] = list[i];
    }
    shown_count = count;
    float base = (float)(48 + 12 * (int)v[AP_ARP_OCT]); /* C3 .. C7 */
    struct chord home = tonic_chord();
    if (v[AP_HARMONY] == H_PENTA) {
        /* The key's pentatonic: major for MAJOR/LYDIAN/MIXOLYD, minor otherwise.
         * Over IV or V it leaves out the clashing tones and adds 6, 9, 13. */
        unsigned m = v[AP_MODE];
        home.root = home.bass = (uint8_t)(v[AP_KEY] % 12U);
        home.mask = m == M_MAJOR || m == M_LYDIAN || m == M_MIXOLYD ? 0x295 : 0x4a9;
    }
    float home_ladder[ARP_LADDER];
    ladder_of(&home, base, home_ladder);
    /* AUTO: the bass leads from chord to chord. HAND: the playing slot keeps
     * its bass, the new chord leads on from it. */
    static int slot_bass[2] = {-1, -1};
    int previous = -1;
    for (unsigned k = 0; k < count; ++k) {
        unsigned c = hand ? (k == 0 ? hand_slot ^ 1U : hand_slot) : k;
        if (hand && k == 0) previous = slot_bass[c];
        switch (v[AP_HARMONY]) {
        case H_FOLLOW: ladder_of(&list[c], base, s->ladder[c]); break;
        case H_PEDAL: case H_PENTA: for (unsigned i = 0; i < ARP_LADDER; ++i) s->ladder[c][i] = home_ladder[i]; break;
        default: for (unsigned i = 0; i < ARP_LADDER; ++i) s->ladder[c][i] = nearest_tone(&list[c], home_ladder[i]); break;
        }
        voicing(&list[c], s->harm[c], &s->fifth[c], &previous);
        if (hand) slot_bass[c] = previous;
    }
    s->chords = (uint8_t)count;
    const unsigned pat = v[AP_PATTERN] < PATTERNS ? v[AP_PATTERN] : 0;
    s->pattern_len = (uint8_t)pattern_len(pat);
    for (unsigned i = 0; i < ARP_PATTERN_MAX; ++i) s->pattern[i] = pattern_steps(pat)[i];
    const unsigned rhy = v[AP_BASS_RHY] < RHYTHMS ? v[AP_BASS_RHY] : 0;
    for (unsigned i = 0; i < 16; ++i) s->bass_rhythm[i] = rhythms[rhy].hit[i];
    const unsigned rate = v[AP_RATE] & 3U;
    s->step_ticks = rates[rate].ticks;
    s->swing_ticks = rates[rate].swing ? (uint8_t)(unit(v[AP_SWING]) * (float)rates[rate].ticks * 0.5f + 0.5f) : 0;
    s->gate = 0.05f + unit(v[AP_GATE]) * 1.15f;
    s->accent = unit(v[AP_ACCENT]);
    s->mutate = unit(v[AP_MUTATE]) * 0.5f;
    s->notes = (uint8_t)(v[AP_NOTES] + 1U);
    s->voices = (uint8_t)voice_cap;
    s->midi_out = v[AP_MIDI_OUT] != 0;
    /* Never a copy of a reference figure: if the riff over these chords holds
     * one, nudge one step a ladder tone up or down until none is left. */
    if (song_copies(s)) {
        int8_t base_steps[ARP_PATTERN_MAX];
        for (unsigned i = 0; i < ARP_PATTERN_MAX; ++i) base_steps[i] = s->pattern[i];
        unsigned done = 0;
        for (unsigned j = 0; j < s->pattern_len && !done; ++j)
            for (int d = 1; d >= -1 && !done; d -= 2) {
                for (unsigned i = 0; i < ARP_PATTERN_MAX; ++i) s->pattern[i] = base_steps[i];
                if (s->pattern[j] < 0) break;
                int nv = s->pattern[j] + d;
                if (nv < 0 || nv >= ARP_LADDER) continue;
                s->pattern[j] = (int8_t)nv;
                done = !song_copies(s);
            }
        for (unsigned j = 0; j + 1 < s->pattern_len && !done; ++j) { /* two steps */
            for (unsigned i = 0; i < ARP_PATTERN_MAX; ++i) s->pattern[i] = base_steps[i];
            if (s->pattern[j] < 0 || s->pattern[j + 1] < 0) continue;
            s->pattern[j] = (int8_t)(s->pattern[j] + 1);
            s->pattern[j + 1] = (int8_t)(s->pattern[j + 1] > 0 ? s->pattern[j + 1] - 1 : 1);
            done = !song_copies(s);
        }
        if (!done) for (unsigned i = 0; i < ARP_PATTERN_MAX; ++i) s->pattern[i] = ARP_REST; /* silence, never a copy */
        ++varied;
    }
    /* TWIN: pings sit two octaves up the tonic's ladder, whatever its size. */
    unsigned tones = 0;
    for (unsigned k = 0; k < 12; ++k) tones += (home.mask >> k) & 1U;
    if (!tones) tones = 1;
    s->twin_mode = v[AP_TWIN] < 3 ? v[AP_TWIN] : 0;
    s->twin_len = 16;
    for (unsigned i = 0; i < 16; ++i)
        s->twin[i] = (int8_t)(pings[i] < 0 ? ARP_REST : (int)(2U * tones) + pings[i]);
    s->twin_gain = unit(v[AP_TWIN_LVL]) * (s->twin_mode == 1 ? 0.6f : 1.0f);
    STEEL_BARRIER();
    arp_song_index ^= 1U;
    if (jump) arp_jump_serial = (uint8_t)(arp_jump_serial + 1U);
}

/* ------------------------------------------------------------ state */
static unsigned base_index;                    /* the factory preset a sound comes from (CLASSIC lead) */
/* LEAD: copy the chosen sound (CLASSIC: the preset's own) into the ARP sound. */
static void apply_lead(unsigned v)
{
    uint8_t own[11];
    const uint8_t *src = leads[v < LEADS ? v : 0].v;
    if (v == LEAD_CLASSIC) {
        const unsigned b = base_index < PRESETS ? base_index : 0;
        for (unsigned k = 0; k < 11; ++k) own[k] = defaults[lead_params[k]];
        for (const uint8_t *q = presets[b].set; *q != 255 && q < presets[b].set + sizeof presets[b].set; q += 2)
            for (unsigned k = 0; k < 11; ++k) if (q[0] == lead_params[k]) own[k] = q[1];
        src = own;
    }
    for (unsigned k = 0; k < 11; ++k)
        current.value[lead_params[k]] = src[k] <= info[lead_params[k]].max ? src[k] : info[lead_params[k]].max;
}

static unsigned tempo_ready;
void arp_load(unsigned index)
{
    if (index >= PRESETS) index = 0;
    preset_index = base_index = index;
    uint8_t bpm = current.value[AP_BPM], midi = current.value[AP_MIDI_OUT];
    memcpy(current.value, defaults, sizeof current.value);
    for (const uint8_t *s = presets[index].set; *s != 255 && s < presets[index].set + sizeof presets[index].set; s += 2)
        if (s[0] < AP_COUNT && s[1] <= info[s[0]].max) current.value[s[0]] = s[1];
    strcpy(current.name, presets[index].name);
    apply_lead(current.value[AP_LEAD]); /* SINE LONG unless a preset says otherwise */
    if (tempo_ready && arp_bpm_external()) current.value[AP_BPM] = bpm; /* MIDI clock keeps its tempo */
    current.value[AP_MIDI_OUT] = midi;                                  /* a device setting */
    tempo_ready = 1;
    hand = 0;
    derive();
    publish(1);
}

/* ------------------------------------------------------------ user presets */
/* 16 user presets, one Flash sector each, in the last 64 KB of the FM1's
 * sample area (ARPHONY plays no samples; GLITCH uses the same place). A
 * preset keeps every setting and the RANDOM riff and chords it holds. */
struct user_record {
    uint32_t magic, version, check;
    uint8_t base, count;
    char name[13];
    uint8_t value[AP_COUNT];
    uint8_t riff_len;
    int8_t riff[ARP_PATTERN_MAX];
    uint8_t chords, root[ARP_MAX_CHORDS], color[ARP_MAX_CHORDS];
    int8_t bass[ARP_MAX_CHORDS];
};
enum { USER_MAGIC = 0x31505241U /* "ARP1" */, USER_VERSION = 1,
       USER_BASE = GROOVE_LIBRARY_OFFSET - ARP_USER_SLOTS * FM1_FLASH_SECTOR_BYTES };
_Static_assert(sizeof(struct user_record) <= FM1_FLASH_SECTOR_BYTES, "a user preset fits a sector");
_Static_assert(USER_BASE >= GROOVE_SAMPLE_OFFSET, "user presets inside the user partition");
static struct user_record user_scratch;
static uint32_t user_check(const struct user_record *r)
{
    const uint8_t *b = (const uint8_t *)r + 12;
    uint32_t h = 0x811c9dc5U;
    for (unsigned i = 0; i < sizeof *r - 12U; ++i) { h ^= b[i]; h *= 0x01000193U; }
    return h;
}
static unsigned user_space(void) { return fm1_flash_size() >= USER_BASE + ARP_USER_SLOTS * FM1_FLASH_SECTOR_BYTES; }
unsigned arp_user_stored(unsigned slot)
{
    uint32_t head[2];
    if (slot >= ARP_USER_SLOTS || !user_space()) return 0;
    if (fm1_flash_read(USER_BASE + slot * FM1_FLASH_SECTOR_BYTES, head, sizeof head)) return 0;
    return head[0] == USER_MAGIC && head[1] == USER_VERSION;
}
int arp_user_slot(void) { return preset_index >= PRESETS ? (int)(preset_index - PRESETS) : -1; }
int arp_user_save(unsigned slot)
{
    if (slot >= ARP_USER_SLOTS || !user_space()) return -1;
    struct user_record *r = &user_scratch;
    memset(r, 0, sizeof *r);
    r->magic = USER_MAGIC; r->version = USER_VERSION; r->count = AP_COUNT;
    r->base = (uint8_t)base_index;
    /* "U3 DORIAN SW": the slot and the preset it grew from. */
    char name[13] = "U";
    if (slot >= 9U) { name[1] = '1'; name[2] = (char)('0' + slot - 9U); name[3] = ' '; }
    else { name[1] = (char)('1' + slot); name[2] = ' '; }
    strncat(name, presets[base_index < PRESETS ? base_index : 0].name, sizeof name - strlen(name) - 1U);
    memcpy(r->name, name, sizeof r->name);
    memcpy(r->value, current.value, AP_COUNT);
    r->riff_len = random_riff.len;
    memcpy(r->riff, random_riff.step, sizeof r->riff);
    r->chords = random_prog.count;
    memcpy(r->root, random_prog.root, sizeof r->root);
    memcpy(r->color, random_prog.color, sizeof r->color);
    memcpy(r->bass, random_prog.bass, sizeof r->bass);
    r->check = user_check(r);
    uint32_t at = USER_BASE + slot * FM1_FLASH_SECTOR_BYTES;
    int e = fm1_flash_erase(at);
    if (!e) e = fm1_flash_write(at, r, sizeof *r);
    if (!e) { preset_index = PRESETS + slot; memcpy(current.name, r->name, sizeof current.name); }
    return e;
}
int arp_user_load(unsigned slot)
{
    struct user_record *r = &user_scratch;
    if (slot >= ARP_USER_SLOTS || !user_space()) return -1;
    if (fm1_flash_read(USER_BASE + slot * FM1_FLASH_SECTOR_BYTES, r, sizeof *r)) return -1;
    if (r->magic != USER_MAGIC || r->version != USER_VERSION || r->count != AP_COUNT || r->check != user_check(r)) return -1;
    uint8_t bpm = current.value[AP_BPM], midi = current.value[AP_MIDI_OUT];
    for (unsigned p = 0; p < AP_COUNT; ++p) current.value[p] = r->value[p] <= info[p].max ? r->value[p] : info[p].max;
    if (arp_bpm_external()) current.value[AP_BPM] = bpm;
    current.value[AP_MIDI_OUT] = midi;
    memcpy(current.name, r->name, sizeof current.name);
    current.name[sizeof current.name - 1] = 0;
    random_riff.len = r->riff_len >= 1 && r->riff_len <= ARP_PATTERN_MAX ? r->riff_len : 8;
    memcpy(random_riff.step, r->riff, sizeof random_riff.step);
    random_prog.count = r->chords >= 1 && r->chords <= ARP_MAX_CHORDS ? r->chords : 2;
    memcpy(random_prog.root, r->root, sizeof random_prog.root);
    memcpy(random_prog.color, r->color, sizeof random_prog.color);
    memcpy(random_prog.bass, r->bass, sizeof random_prog.bass);
    for (unsigned i = 0; i < ARP_MAX_CHORDS; ++i) { random_prog.root[i] %= 12U; if (random_prog.color[i] > COLOR_POWER) random_prog.color[i] = COLOR_ADD9; }
    base_index = r->base < PRESETS ? r->base : 0;
    preset_index = PRESETS + slot;
    hand = 0;
    derive();
    publish(1);
    return 0;
}
void arp_init(void)
{
    arp_run = 0;
    voice_cap = ARP_VOICES;
    arp_load(0);
}
unsigned arp_preset_index(void) { return preset_index; }
const struct arp_preset *arp_current(void) { return &current; }
unsigned arp_get(unsigned p) { return p < AP_COUNT ? current.value[p] : 0; }
/* Settings that change the chords restart the progression on the next beat. */
static unsigned structural(unsigned p)
{
    return p == AP_KEY || p == AP_MODE || p == AP_PROG || p == AP_COLOR;
}
void arp_edit(unsigned p, int delta)
{
    if (p >= AP_COUNT || !delta) return;
    int v = (int)current.value[p] + delta, max = info[p].max;
    v = v < 0 ? 0 : v > max ? max : v;
    if (v == current.value[p]) return;
    current.value[p] = (uint8_t)v;
    if (p == AP_LEAD) apply_lead((unsigned)v);
    if (p == AP_PROG && v < FIXED_PROGS && progs[v].mode < MODES) current.value[AP_MODE] = progs[v].mode; /* chords as written */
    if (hand && (p == AP_KEY || p == AP_MODE || p == AP_COLOR)) {
        /* The held chord keeps its root but takes the new key, mode or colour. */
        shown[0] = shown[1] = hand_chord(hand_pc, hand_pedal);
    }
    derive();
    publish(structural(p) && !hand);
}
void arp_set_key(unsigned pc)
{
    arp_edit(AP_KEY, (int)(pc % 12U) - (int)current.value[AP_KEY]);
}
void arp_transport(int run)
{
    arp_run = (uint8_t)(run < 0 ? !arp_run : run > 0);
}
unsigned arp_running(void) { return arp_now_running || arp_run; }
/* HAND keeps two song slots: the one playing and the next chord, which the
 * sequencer takes on the next beat. A stopped arpeggio starts on slot 0. */
void arp_hand_chord(unsigned pc, unsigned pedal)
{
    unsigned running = arp_run && arp_now_running;
    hand_slot = (uint8_t)(hand && running ? (arp_now_chord & 1U) ^ 1U : 0U);
    hand = 1;
    hand_pc = (uint8_t)(pc % 12U);
    hand_pedal = (uint8_t)(pedal != 0);
    shown[hand_slot] = hand_chord(pc, pedal);
    if (!running) shown[hand_slot ^ 1U] = shown[hand_slot];
    publish(1);
    arp_run = 1;
}
/* ARP button in AUTO: stay on the chord that is playing (exactly as the
 * progression has it) and go on by hand. */
void arp_hold(void)
{
    if (hand) return;
    unsigned c = arp_now_running ? arp_now_chord : 0U;
    if (c >= shown_count) c = 0;
    struct chord held = shown[c];
    hand = 1;
    hand_slot = (uint8_t)(arp_now_running && c < 2U ? c : 0U); /* the sequencer folds higher chords to 0 */
    hand_pc = held.root;
    hand_pedal = held.bass != held.root;
    shown[0] = shown[1] = held;
    publish(0); /* both slots hold the chord: nothing is struck again */
}
void arp_auto(void)
{
    if (!hand) return;
    hand = 0;
    publish(1);
}
unsigned arp_is_hand(void) { return hand; }
unsigned arp_chord_position(void) { return arp_now_chord < shown_count ? arp_now_chord : 0U; }
unsigned arp_chord_count(void) { return hand ? 1U : shown_count; }
void arp_chord_name(unsigned index, char out[16])
{
    if (hand) index = arp_now_running ? arp_now_chord : hand_slot;
    if (index >= shown_count) { out[0] = 0; return; }
    chord_text(&shown[index], out);
}
uint16_t arp_chord_pcs(unsigned index)
{
    if (hand) index = arp_now_running ? arp_now_chord : hand_slot;
    if (index >= shown_count) return 0;
    const struct chord *c = &shown[index];
    uint16_t m = 0;
    for (unsigned k = 0; k < 12; ++k)
        if (c->mask & (1U << k)) m |= (uint16_t)(1U << ((c->root + k) % 12U));
    return (uint16_t)(m | 1U << c->bass);
}
unsigned arp_chord_shade(unsigned index)
{
    if (hand) index = arp_now_running ? arp_now_chord : hand_slot;
    if (index >= shown_count) return current.value[AP_KEY] % 12U;
    const struct chord *c = &shown[index];
    unsigned minor = (c->mask & (1U << 3)) && !(c->mask & (1U << 4));
    return c->root % 12U | (minor ? 16U : 0U);
}
unsigned arp_step_position(void) { return arp_now_step; }
/* Milliseconds until the next chord of the progression (the display's fairy
 * swings her wand on it); ~0 when no change is coming (stopped, HAND, one chord). */
unsigned arp_chord_ms_left(void)
{
    const struct arp_song *s = &arp_songs[arp_song_index];
    if (!arp_now_running || hand || s->chords < 2U || s->chord_ticks == ARP_HOLD) return ~0U;
    unsigned rel = arp_now_rel, bpm = arp_bpm();
    if (rel >= s->chord_ticks || !bpm) return 0;
    return (s->chord_ticks - rel) * 60000U / (bpm * ARP_PPQN);
}
unsigned arp_pattern_length(void) { return pattern_len(current.value[AP_PATTERN] < PATTERNS ? current.value[AP_PATTERN] : 0); }
int arp_pattern_step(unsigned i) /* as published (after any nudge) */
{
    const struct arp_song *s = &arp_songs[arp_song_index];
    return i < s->pattern_len ? s->pattern[i] : ARP_REST;
}

/* ------------------------------------------------------------ RANDOM */
/* A new riff from rules, not from a table: a short motif (4 steps, 3 in
 * triplet rates), repeated, then moved up or down the chord ladder, and a
 * fresh closing figure - the way the hand-written riffs are built. Steps go
 * mostly by neighbours on the ladder, sometimes leap; a third of the riffs
 * keep returning to one inner pedal tone. Rests and ties are rare. */
static uint32_t rnd_state = 0x9e3779b9U;
static unsigned rnd(unsigned n)
{
    rnd_state ^= rnd_state << 13; rnd_state ^= rnd_state >> 17; rnd_state ^= rnd_state << 5;
    return n ? rnd_state % n : 0;
}
static int clamp_index(int v) { return v < 0 ? -v : v > 10 ? 20 - v : v; } /* bounce off the ends */
static void motif(int8_t *out, unsigned n, int start, int pedal)
{
    int v = start;
    for (unsigned i = 0; i < n; ++i) {
        if (i) {
            if (pedal >= 0 && (i & 1U)) { out[i] = (int8_t)pedal; continue; }
            unsigned r = rnd(100);
            int move = r < 50 ? 1 : r < 75 ? 2 : r < 88 ? 3 : 0;
            if (rnd(2)) move = -move;
            v = clamp_index(v + move);
        }
        out[i] = (int8_t)v;
    }
}
/* Never a copy: every 7 notes in a row of the riff, over every chord, as
 * intervals (any key), must differ from all figures of the reference
 * recordings (arp_forbidden.inc). Pedal and pulse figures with fewer than
 * three different notes are free for everyone. */
#include "arp_forbidden.inc"
enum { FIGURE = 7 };
static unsigned figure_known(uint32_t h)
{
    unsigned lo = 0, hi = sizeof forbidden / sizeof forbidden[0];
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2U;
        if (forbidden[mid] == h) return 1;
        if (forbidden[mid] < h) lo = mid + 1U; else hi = mid;
    }
    return 0;
}
/* Checks the riff as it is heard: every chord plays the riff from its first
 * step for the chord's length, then the next chord starts it again; the
 * whole progression repeats. All windows of that note stream are checked,
 * across chord changes too. */
static unsigned song_copies(const struct arp_song *s)
{
    enum { MAX_NOTES = 800 };
    static int stream[MAX_NOTES];
    unsigned n = 0;
    unsigned steps = s->chord_ticks == ARP_HOLD || !s->step_ticks ? s->pattern_len
                   : (s->chord_ticks + s->step_ticks - 1U) / s->step_ticks;
    for (unsigned c = 0; c < s->chords; ++c)
        for (unsigned i = 0; i < steps && n < MAX_NOTES; ++i) {
            int v = s->pattern[s->pattern_len ? i % s->pattern_len : 0];
            if (v >= 0) stream[n++] = (int)(s->ladder[c][v] + 0.5f);
        }
    if (n < 2) return 0;
    for (unsigned k = 0; k < n; ++k) {                       /* the progression repeats: windows wrap */
        int w[FIGURE]; unsigned distinct = 0;
        for (unsigned j = 0; j < FIGURE; ++j) {
            w[j] = stream[(k + j) % n];
            unsigned seen = 0;
            for (unsigned q = 0; q < j; ++q) seen |= w[q] == w[j];
            distinct += !seen;
        }
        if (distinct < 3) continue;
        uint32_t h = 0x811c9dc5U;
        for (unsigned j = 1; j < FIGURE; ++j) {
            int d = w[j] - w[j - 1];
            d = d < -48 ? -48 : d > 48 ? 48 : d;
            h ^= (uint32_t)(d + 64) & 0xffU;
            h *= 0x01000193U;
        }
        if (figure_known(h)) return 1;
    }
    return 0;
}
/* RANDOM chords: two or four chords in MODE, built the way the reference
 * progressions are: mostly from the tonic, roots from the mode (no
 * diminished chords) plus the borrowed bVII, bVI, bIII; moves by a tone or a
 * semitone preferred (IV - V, bVI - bVII), no V - I at the end; colours add9,
 * 7, 9, 6, sus2; now and then a chord over the tonic as a pedal. */
void arp_random_chords(uint32_t seed)
{
    rnd_state ^= seed * 2246822519U + ++random_count;
    if (!rnd_state) rnd_state = 0x9e3779b9U;
    const unsigned mode = current.value[AP_MODE];
    uint8_t roots[10]; unsigned n_roots = 0;
    for (unsigned i = 0; i < 7; ++i) {
        unsigned rel = modes[mode < MODES ? mode : 0].step[i];
        if (diatonic_in(mode, rel, COLOR_TRIAD) & (1U << 7)) roots[n_roots++] = (uint8_t)rel; /* a perfect fifth */
    }
    static const uint8_t borrowed[3] = {10, 8, 3};
    for (unsigned b = 0; b < 3; ++b) if (!in_mode(mode, borrowed[b])) roots[n_roots++] = borrowed[b];
    static const uint8_t colour_pick[10] = {COLOR_ADD9, COLOR_ADD9, COLOR_ADD9, COLOR_7TH, COLOR_7TH,
                                            COLOR_9TH, COLOR_9TH, COLOR_6TH, COLOR_SUS2, COLOR_TRIAD};
    const unsigned count = rnd(10) < 7 ? 4U : 2U;
    unsigned prev = rnd(4) ? 0U : roots[1U + rnd(n_roots - 1U)];
    for (unsigned c = 0; c < count; ++c) {
        if (c) {
            unsigned pick = prev;
            for (unsigned tries = 0; tries < 12 && pick == prev; ++tries) {
                pick = roots[rnd(n_roots)];
                unsigned d = (pick + 12U - prev) % 12U;
                if (pick != prev && !(d == 1U || d == 2U || d == 10U || d == 11U) && rnd(2)) pick = prev; /* steps twice as likely */
            }
            if (c == count - 1U && pick == 7U && random_prog.root[0] == 0U) pick = 5U;      /* no V - I */
            if (pick == prev) pick = prev == 5U ? 7U : 5U;
            prev = pick;
        }
        random_prog.root[c] = (uint8_t)prev;
        random_prog.color[c] = colour_pick[rnd(10)];
        random_prog.bass[c] = (int8_t)(c && prev != 0U && rnd(5) == 0 ? 0 : -1);
    }
    random_prog.count = (uint8_t)count;
    current.value[AP_PROG] = PROG_RANDOM;
    hand = 0;
    publish(1);
}
static void random_attempt(void);
static unsigned rejected;                     /* riffs thrown away as too close (tests) */
unsigned arp_random_rejected(void) { return rejected + varied; }
void arp_random_riff(uint32_t seed)
{
    rnd_state ^= seed * 2654435761U + ++random_count;
    if (!rnd_state) rnd_state = 0x9e3779b9U;
    for (unsigned tries = 0; tries < 32; ++tries) {
        random_attempt();
        current.value[AP_PATTERN] = PATTERNS - 1;
        publish(0);
        if (!song_copies(&arp_songs[arp_song_index])) return;
        ++rejected;
    }
}
static void random_attempt(void)
{
    const unsigned triplet = rates[current.value[AP_RATE] & 3U].swing == 0;
    const unsigned m = triplet ? 3U : 4U;
    const unsigned len = triplet ? 12U : (rnd(4) ? 16U : 8U);
    const int pedal = rnd(3) == 0 ? (int)(1U + rnd(4)) : -1;
    int8_t a[4], b[4];
    motif(a, m, (int)rnd(5), pedal);
    motif(b, m, a[0] + (int)rnd(3) - 1 < 0 ? 0 : a[0] + (int)rnd(3) - 1, pedal);
    static const int8_t shifts[4] = {1, 2, -1, 3};
    int shift = shifts[rnd(4)];
    for (unsigned i = 0; i < len; ++i) {
        unsigned part = i / m, k = i % m;
        int v;
        if (part == 0 || part == 2) v = a[k];                     /* A */
        else if (part == len / m - 1) v = b[k];                   /* the closing figure */
        else v = clamp_index(a[k] + (a[k] == pedal ? 0 : shift)); /* A moved */
        random_riff.step[i] = (int8_t)v;
    }
    /* A few rests and ties, never on the first step or twice in a row. */
    for (unsigned i = 2; i < len; ++i) {
        unsigned r = rnd(100);
        if (r < 8 && random_riff.step[i - 1] >= 0) random_riff.step[i] = ARP_REST;
        else if (r < 12 && random_riff.step[i - 1] >= 0) random_riff.step[i] = ARP_TIE;
    }
    for (unsigned i = len; i < ARP_PATTERN_MAX; ++i) random_riff.step[i] = ARP_REST;
    random_riff.len = (uint8_t)len;
}
uint32_t arp_note_count(void) { return arp_now_count; }
int arp_last_note(unsigned layer) { return layer < 3 ? arp_now_note[layer] : -1; }

/* ------------------------------------------------------------ CPU guard */
/* Under load the riff uses fewer overlapping voices (shorter tails). */
enum { GUARD_HIGH = 90, GUARD_LOW = 70, GUARD_CALM_MS = 4000 };
static uint32_t calm_at;
unsigned arp_voice_limit(void) { return voice_cap; }
unsigned arp_guard(unsigned load, unsigned overrun, uint32_t now)
{
    unsigned before = voice_cap;
    if ((overrun || load >= GUARD_HIGH) && voice_cap > 2) { --voice_cap; calm_at = now; }
    else if (load < GUARD_LOW && voice_cap < ARP_VOICES && now - calm_at >= GUARD_CALM_MS) { ++voice_cap; calm_at = now; }
    else if (load >= GUARD_LOW) calm_at = now;
    if (voice_cap != before) publish(0);
    return voice_cap != before;
}

/* ------------------------------------------------------------ tempo */
static uint32_t tap_last, tap_intervals[3];
static unsigned tap_count;
static volatile uint32_t clock_last, clock_beat_start, clock_ticks;
static volatile float clock_bpm;
static volatile uint8_t clock_running, midi_transport; /* 1 start, 2 continue, 3 stop */
static void set_bpm(float bpm)
{
    int v = (int)(bpm + 0.5f) - 40;
    v = v < 0 ? 0 : v > info[AP_BPM].max ? info[AP_BPM].max : v;
    if ((uint8_t)v == current.value[AP_BPM]) return;
    current.value[AP_BPM] = (uint8_t)v;
    derive();
}
unsigned arp_bpm(void) { return 40U + current.value[AP_BPM]; }
unsigned arp_bpm_external(void) { return clock_running; }
void arp_tap(uint32_t now)
{
    uint32_t gap = now - tap_last;
    tap_last = now;
    if (gap < 200U || gap > 2000U) { tap_count = 0; return; }
    tap_intervals[tap_count % 3U] = gap;
    if (++tap_count < 2) return;
    unsigned n = tap_count < 3 ? tap_count : 3U;
    uint32_t sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += tap_intervals[i];
    set_bpm(60000.0f * (float)n / (float)sum);
}
/* MIDI input context. */
void arp_midi_clock(uint8_t status, uint32_t now)
{
    if (status == 0xfa) { midi_transport = 1; clock_ticks = 0; return; }
    if (status == 0xfb) { midi_transport = 2; return; }
    if (status == 0xfc) { midi_transport = 3; clock_running = 0; clock_ticks = 0; return; }
    if (status != 0xf8) return;
    if (now - clock_last > 2000U) clock_ticks = 0;
    clock_last = now;
    if (clock_ticks++ == 0) { clock_beat_start = now; return; }
    if (clock_ticks == 25U) {
        uint32_t beat = now - clock_beat_start;
        if (beat >= 150U && beat <= 2000U) { clock_bpm = 60000.0f / (float)beat; clock_running = 1; }
        clock_beat_start = now;
        clock_ticks = 1;
    }
}

/* ------------------------------------------------------------ MIDI notes */
static struct { volatile unsigned read, write; uint8_t data[64][3]; } midi_in;
void arp_midi_message(const uint8_t *m, size_t n)
{
    if (!m || n < 2 || m[0] < 0x80 || m[0] >= 0xf0) return;
    unsigned w = midi_in.write;
    if (w - midi_in.read >= 64U) return;
    midi_in.data[w & 63U][0] = m[0];
    midi_in.data[w & 63U][1] = m[1];
    midi_in.data[w & 63U][2] = n > 2 ? m[2] : 0;
    STEEL_BARRIER();
    midi_in.write = w + 1U;
}
static void midi_play(const uint8_t m[3])
{
    unsigned type = m[0] & 0xf0U, note = m[1];
    if (type == 0xb0 && (m[1] == 123 || m[1] == 120)) { arp_transport(0); return; }
    if (type != 0x90 || !m[2]) return;
    /* Like the keys: below F4 the chord has its own bass, from F4 up it sits
     * over the key's tonic. */
    arp_hand_chord(note % 12U, note >= 65U);
}
void arp_poll(uint32_t now)
{
    if (clock_running && now - clock_last > 2000U) clock_running = 0;
    if (clock_running) set_bpm(clock_bpm);
    uint8_t t = midi_transport;
    if (t) {
        midi_transport = 0;
        if (t == 1) { arp_restart_serial = (uint8_t)(arp_restart_serial + 1U); arp_transport(1); }
        else if (t == 2) arp_transport(1);
        else arp_transport(0);
    }
    for (unsigned r = midi_in.read, w = midi_in.write; r != w; ++r) {
        STEEL_BARRIER();
        midi_play(midi_in.data[r & 63U]);
        midi_in.read = r + 1U;
    }
}
