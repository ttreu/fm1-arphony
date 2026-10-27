/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLIDER control side: scales, presets, latching keys and glides. */
#include "steel_internal.h"
#include "steel_drums.h"
#include <string.h>

/* ------------------------------------------------------------ scales */
static const struct scale {
    char name[10];
    uint8_t count, equal[7];
    float just[7]; /* cents above the root; 5-limit; 7/4 in mixolydian, 7/5 in the diminished chords */
} scales[] = {
    {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}, {0.00f, 203.91f, 386.31f, 498.04f, 701.96f, 884.36f, 1088.27f}},
    {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}, {0.00f, 203.91f, 315.64f, 498.04f, 701.96f, 813.69f, 1017.60f}},
    {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}, {0.00f, 203.91f, 315.64f, 498.04f, 701.96f, 884.36f, 1017.60f}},
    {"MIXOLYD", 7, {0, 2, 4, 5, 7, 9, 10}, {0.00f, 203.91f, 386.31f, 498.04f, 701.96f, 884.36f, 968.83f}},
    {"LYDIAN", 7, {0, 2, 4, 6, 7, 9, 11}, {0.00f, 203.91f, 386.31f, 590.22f, 701.96f, 884.36f, 1088.27f}},
    {"PHRYGIAN", 7, {0, 1, 3, 5, 7, 8, 10}, {0.00f, 111.73f, 315.64f, 498.04f, 701.96f, 813.69f, 1017.60f}},
    {"HIJAZ", 7, {0, 1, 4, 5, 7, 8, 10}, {0.00f, 111.73f, 386.31f, 498.04f, 701.96f, 813.69f, 1017.60f}},
    {"PENTA MAJ", 5, {0, 2, 4, 7, 9}, {0.00f, 203.91f, 386.31f, 701.96f, 884.36f}},
    {"PENTA MIN", 5, {0, 3, 5, 7, 10}, {0.00f, 315.64f, 498.04f, 701.96f, 1017.60f}},
    {"HARM MIN", 7, {0, 2, 3, 5, 7, 8, 11}, {0.00f, 203.91f, 315.64f, 498.04f, 701.96f, 813.69f, 1088.27f}},
    {"MEL MIN", 7, {0, 2, 3, 5, 7, 9, 11}, {0.00f, 203.91f, 315.64f, 498.04f, 701.96f, 884.36f, 1088.27f}},
    {"WHOLETONE", 6, {0, 2, 4, 6, 8, 10}, {0.00f, 203.91f, 386.31f, 590.22f, 813.69f, 1017.60f}},
    {"HIRAJOSHI", 5, {0, 2, 3, 7, 8}, {0.00f, 203.91f, 315.64f, 701.96f, 813.69f}},
    {"IN SEN", 5, {0, 1, 5, 7, 10}, {0.00f, 111.73f, 498.04f, 701.96f, 1017.60f}},
    {"DBL HARM", 7, {0, 1, 4, 5, 7, 8, 11}, {0.00f, 111.73f, 386.31f, 498.04f, 701.96f, 813.69f, 1088.27f}},
    /* Chords as scales: the white keys stack only the chord's tones, so
     * every key and every glide stays inside the chord. */
    {"SUS2", 3, {0, 2, 7}, {0.00f, 203.91f, 701.96f}},
    {"SUS4", 3, {0, 5, 7}, {0.00f, 498.04f, 701.96f}},
    {"DIM", 3, {0, 3, 6}, {0.00f, 315.64f, 582.51f}},
    {"DIM7", 4, {0, 3, 6, 9}, {0.00f, 315.64f, 582.51f, 884.36f}},
    {"AUG", 3, {0, 4, 8}, {0.00f, 386.31f, 772.63f}},
    {"MAJ7", 4, {0, 4, 7, 11}, {0.00f, 386.31f, 701.96f, 1088.27f}},
    {"MIN7", 4, {0, 3, 7, 10}, {0.00f, 315.64f, 701.96f, 1017.60f}},
    {"MAJ9", 5, {0, 2, 4, 7, 11}, {0.00f, 203.91f, 386.31f, 701.96f, 1088.27f}},
    {"MIN9", 5, {0, 2, 3, 7, 10}, {0.00f, 203.91f, 315.64f, 701.96f, 1017.60f}},
    {"ADD9", 4, {0, 2, 4, 7}, {0.00f, 203.91f, 386.31f, 701.96f}},
};
enum { SCALES = sizeof scales / sizeof scales[0] };
const char *steel_scale_name(unsigned scale) { return scale < SCALES ? scales[scale].name : ""; }

/* ------------------------------------------------------------ parameters */
static const struct steel_param_info info[SP_COUNT] = {
    [SP_ROOT] = {"KEY", 35},        [SP_SCALE] = {"SCALE", SCALES - 1}, [SP_TUNING] = {"TUNING", 1},
    [SP_GLIDE_STEPS] = {"STEPS", 3}, [SP_GLIDE_TIME] = {"GLIDE", 99},   [SP_GLIDE_CURVE] = {"CURVE", 2},
    [SP_SHAPE] = {"SHAPE", 99},     [SP_DETUNE] = {"DETUNE", 99},      [SP_OSC2] = {"OSC 2", 99},
    [SP_SUB] = {"SUB", 99},         [SP_TONE] = {"TONE", 99},          [SP_TONE_ENV] = {"TONE ENV", 99},
    [SP_DRIFT] = {"DRIFT", 99},     [SP_SPREAD] = {"WIDTH", 99},
    [SP_ATTACK] = {"ATTACK", 99},   [SP_DECAY] = {"DECAY", 99},        [SP_SUSTAIN] = {"SUSTAIN", 99},
    [SP_RELEASE] = {"RELEASE", 99},
    [SP_VIB_RATE] = {"VIB RATE", 99}, [SP_VIB_DEPTH] = {"VIBRATO", 99}, [SP_VIB_DELAY] = {"VIB DLY", 99},
    [SP_LEVEL] = {"LEVEL", 99},
    [SP_FILTER] = {"CUTOFF", 99},   [SP_RESO] = {"RESO", 99},          [SP_DRIVE] = {"DRIVE", 99},
    [SP_LFO_RATE] = {"LFO RATE", 99}, [SP_LFO_DEPTH] = {"LFO AMT", 99},
    [SP_DELAY_TIME] = {"DLY TIME", 99}, [SP_DELAY_FEEDBACK] = {"DLY FB", 99}, [SP_DELAY_MIX] = {"DLY MIX", 99},
    [SP_TAPE_TIME] = {"TAPE TM", 99}, [SP_TAPE_FEEDBACK] = {"TAPE FB", 120}, [SP_TAPE_WOW] = {"WOW", 99},
    [SP_TAPE_TONE] = {"TAPE TONE", 99}, [SP_TAPE_MIX] = {"TAPE MIX", 99},
    [SP_TAPE_SYNC] = {"TAPE SYNC", 1}, [SP_DELAY_SYNC] = {"DLY SYNC", 1}, [SP_LFO_SYNC] = {"LFO SYNC", 1},
    [SP_REV_SIZE] = {"SIZE", 99}, [SP_REV_DAMP] = {"DAMP", 99}, [SP_SHIMMER] = {"SHIMMER", 99},
    [SP_REV_MIX] = {"REV MIX", 99}, [SP_BPM] = {"BPM", 200},
    [SP_OVERTONES] = {"OVERTONE", 99}, [SP_OVERTONE_TILT] = {"OT BRIGHT", 99}, [SP_DETUNE_LFO] = {"DET LFO", 99},
    [SP_UNDERTONES] = {"UNDERTONE", 99}, [SP_FLOAT] = {"FLOAT", 99}, [SP_FLOAT_RATE] = {"FLOAT RT", 99},
    [SP_INTERVAL] = {"INTERVAL", 17}, [SP_IV_MODE] = {"IV MODE", 1},
};
/* Oscillator 2 intervals. IN KEY: 7-tone scales move by `steps` scale
 * degrees (a third is major or minor as the key says); pentatonic scales
 * take the scale tone nearest to `cents`. PURE: exactly `cents`. */
static const struct { char name[7]; int8_t steps; float cents; } intervals[] = {
    {"UNISON", 0, 0.0f}, {"+2ND", 1, 203.91f}, {"+3RD", 2, 386.31f}, {"+4TH", 3, 498.04f},
    {"+5TH", 4, 701.96f}, {"+6TH", 5, 884.36f}, {"+7TH", 6, 1017.60f}, {"+OCT", 7, 1200.0f},
    {"+9TH", 8, 1403.91f}, {"+10TH", 9, 1586.31f}, {"+12TH", 11, 1901.96f}, {"+2 OCT", 14, 2400.0f},
    {"-2ND", -1, -203.91f}, {"-3RD", -2, -386.31f}, {"-4TH", -3, -498.04f}, {"-5TH", -4, -701.96f},
    {"-6TH", -5, -884.36f}, {"-OCT", -7, -1200.0f}};
_Static_assert(sizeof intervals / sizeof intervals[0] == 18, "interval names");
/* Tempo-synced note values, in beats (quarter notes). */
static const struct { char name[6]; float beats; } echo_notes[] = {
    {"1/16", 0.25f}, {"1/8T", 1.0f / 3.0f}, {"1/16D", 0.375f}, {"1/8", 0.5f}, {"1/4T", 2.0f / 3.0f},
    {"1/8D", 0.75f}, {"1/4", 1.0f}, {"1/2T", 4.0f / 3.0f}, {"1/4D", 1.5f}, {"1/2", 2.0f}, {"1/1", 4.0f}};
static const struct { char name[7]; float beats; } lfo_notes[] = {
    {"8 BARS", 32.0f}, {"4 BARS", 16.0f}, {"2 BARS", 8.0f}, {"1 BAR", 4.0f},
    {"1/2", 2.0f}, {"1/4", 1.0f}, {"1/8", 0.5f}, {"1/16", 0.25f}};
enum { ECHO_NOTES = sizeof echo_notes / sizeof echo_notes[0], LFO_NOTES = sizeof lfo_notes / sizeof lfo_notes[0] };
static unsigned note_index(unsigned v, unsigned count) { unsigned i = v * count / 100U; return i < count ? i : count - 1U; }
const struct steel_param_info *steel_param_info(unsigned p) { return p < SP_COUNT ? info + p : 0; }

static float unit(unsigned v) { return (float)v / 99.0f; }
static float glide_ms(unsigned v) { float u = unit(v); return 20.0f + u * u * 5980.0f; }
static float attack_s(unsigned v) { float u = unit(v); return 0.002f + u * u * u * 8.0f; }
static float decay_s(unsigned v) { float u = unit(v); return 0.02f + u * u * u * 10.0f; }
static float release_s(unsigned v) { float u = unit(v); return 0.02f + u * u * u * 12.0f; }
static float vib_hz(unsigned v) { return 0.5f + unit(v) * 8.5f; }
static float lfo_hz(unsigned v) { return 0.03f * steel_exp2(unit(v) * 7.6f); }
static float tape_s(unsigned v) { float u = unit(v); return 0.04f + u * u * (STEEL_TAPE_MAX_S - 0.04f); }
static float delay_s(unsigned v) { float u = unit(v); return 0.02f + u * u * (STEEL_DELAY_MAX_S - 0.02f); }
static float float_hz(unsigned v) { return 0.02f * steel_exp2(unit(v) * 5.0f); } /* 0.02..0.64 Hz */
static float one_pole(float hz) { return 1.0f - steel_exp2(-6.2831853f * 1.4426950f / STEEL_RATE * hz); }

static void put(char *out, const char *s) { strcpy(out, s); }
static struct steel_preset current;
static float beat_s(void) { return 60.0f / (float)(40U + current.value[SP_BPM]); }
/* Echo and LFO times follow the tempo when their SYNC is on. */
static float tape_time_s(void)
{
    return current.value[SP_TAPE_SYNC] ? echo_notes[note_index(current.value[SP_TAPE_TIME], ECHO_NOTES)].beats * beat_s()
                                       : tape_s(current.value[SP_TAPE_TIME]);
}
static float delay_time_s(void)
{
    return current.value[SP_DELAY_SYNC] ? echo_notes[note_index(current.value[SP_DELAY_TIME], ECHO_NOTES)].beats * beat_s()
                                        : delay_s(current.value[SP_DELAY_TIME]);
}
static float lfo_rate_hz(void)
{
    return current.value[SP_LFO_SYNC] ? 1.0f / (lfo_notes[note_index(current.value[SP_LFO_RATE], LFO_NOTES)].beats * beat_s())
                                      : lfo_hz(current.value[SP_LFO_RATE]);
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
void steel_param_text(unsigned p, unsigned v, char out[12])
{
    static const char notes[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char curves[3][5] = {"LIN", "SOFT", "BAR"};
    switch (p) {
    case SP_ROOT: { unsigned midi = 24U + v; put(out, notes[midi % 12U]); number(out + strlen(out), midi / 12U - 1U); return; }
    case SP_SCALE: put(out, steel_scale_name(v)); return;
    case SP_TUNING: put(out, v ? "JUST" : "EQUAL"); return;
    case SP_GLIDE_STEPS: put(out, "+"); number(out + 1, v + 1U); return;
    case SP_GLIDE_TIME: seconds(out, glide_ms(v) / 1000.0f); return;
    case SP_GLIDE_CURVE: put(out, curves[v < 3 ? v : 0]); return;
    case SP_ATTACK: seconds(out, attack_s(v)); return;
    case SP_DECAY: seconds(out, decay_s(v)); return;
    case SP_RELEASE: seconds(out, release_s(v)); return;
    case SP_VIB_RATE: { unsigned t = (unsigned)(vib_hz(v) * 10.0f + 0.5f); number(out, t / 10U); strcat(out, ".");
        number(out + strlen(out), t % 10U); strcat(out, "HZ"); return; }
    case SP_TAPE_FEEDBACK: number(out, v); strcat(out, "%"); return;
    case SP_TAPE_TIME:
        if (current.value[SP_TAPE_SYNC]) { put(out, echo_notes[note_index(v, ECHO_NOTES)].name); return; }
        seconds(out, tape_s(v)); return;
    case SP_DELAY_TIME:
        if (current.value[SP_DELAY_SYNC]) { put(out, echo_notes[note_index(v, ECHO_NOTES)].name); return; }
        seconds(out, delay_s(v)); return;
    case SP_TAPE_SYNC: case SP_DELAY_SYNC: case SP_LFO_SYNC: put(out, v ? "SYNC" : "FREE"); return;
    case SP_BPM: number(out, 40U + v); return;
    case SP_INTERVAL: put(out, intervals[v < 18U ? v : 0].name); return;
    case SP_IV_MODE: put(out, v ? "PURE" : "IN KEY"); return;
    case SP_DETUNE: number(out, v * 40U / 99U); strcat(out, "CT"); return;
    case SP_DETUNE_LFO: number(out, v * 30U / 99U); strcat(out, "CT"); return;
    case SP_FLOAT_RATE: { unsigned t = (unsigned)(float_hz(v) * 100.0f + 0.5f); number(out, t / 100U); strcat(out, ".");
        if (t % 100U < 10U) strcat(out, "0");
        number(out + strlen(out), t % 100U); strcat(out, "HZ"); return; }
    case SP_LFO_RATE: if (current.value[SP_LFO_SYNC]) { put(out, lfo_notes[note_index(v, LFO_NOTES)].name); return; }
    { unsigned t = (unsigned)(lfo_hz(v) * 100.0f + 0.5f); number(out, t / 100U); strcat(out, ".");
        if (t % 100U < 10U) strcat(out, "0");
        number(out + strlen(out), t % 100U); strcat(out, "HZ"); return; }
    default: number(out, v); return;
    }
}

/* ------------------------------------------------------------ presets */
#define P(name, ...) {name, {__VA_ARGS__}}
/*  KEY SCL TUN STP GLT CRV | SHP DET OS2 SUB TON TEN DRF WID | ATK DEC SUS REL | VRT VDP VDL LVL |
 *  CUT RES DRV LFR LFA | DTM DFB DMX | TTM TFB WOW TTN TMX |
 *  TSY DSY LSY | RSZ RDP SHM RMX | BPM-40 | OVT OTB DLF | UND FLT FRT | IVL IVM
 *  (IVL/IVM left out = unison, in key) */
static const struct steel_preset presets[STEEL_PRESETS] = {
    P("SYNTHISCAPES",
      24, 0, 1, 0, 75, 1,  55, 38, 60, 25, 30, 10, 30, 85,  60, 80, 90, 80,  20, 5, 50, 70,
      45, 25, 20, 30, 45,  93, 55, 40,  50, 35, 30, 35, 20,
      0, 0, 1,  85, 45, 35, 35,  50,  0, 0, 35,  0, 15, 25),
    P("AJAM DRONE",
      14, 0, 1, 0, 60, 0,  70, 32, 55, 45, 28, 20, 25, 30,  70, 80, 95, 85,  10, 3, 50, 70,
      40, 30, 35, 22, 60,  70, 35, 20,  60, 55, 25, 35, 25,
      0, 0, 0,  70, 55, 15, 25,  50,  0, 0, 25,  10, 10, 20),
    P("DORIAN DRIFT",
      26, 2, 1, 1, 65, 1,  60, 45, 60, 20, 38, 25, 70, 60,  50, 70, 80, 75,  25, 8, 60, 70,
      55, 35, 40, 20, 25,  55, 45, 25,  55, 95, 45, 40, 40,
      0, 0, 0,  75, 50, 25, 30,  50,  0, 0, 45,  0, 20, 30),
    P("TAPE FOG",
      17, 3, 1, 2, 85, 1,  40, 38, 50, 30, 25, 10, 40, 70,  75, 85, 95, 90,  15, 6, 80, 65,
      30, 40, 50, 15, 30,  75, 50, 20,  75, 110, 55, 30, 50,
      0, 0, 0,  90, 65, 20, 35,  50,  10, 30, 40,  15, 25, 20),
    P("DEEP ROOT",
      12, 1, 1, 3, 80, 0,  50, 28, 40, 70, 22, 15, 20, 40,  55, 80, 95, 85,  10, 3, 90, 75,
      35, 20, 45, 10, 20,  65, 35, 15,  50, 50, 20, 30, 20,
      0, 0, 0,  80, 70, 10, 25,  50,  0, 0, 20,  25, 10, 15),
    P("MINOR DRONE",
      21, 1, 1, 1, 70, 1,  35, 32, 45, 20, 40, 15, 20, 50,  45, 70, 85, 75,  35, 15, 70, 75,
      60, 15, 25, 15, 10,  60, 40, 25,  40, 40, 30, 40, 25,
      0, 0, 0,  80, 55, 20, 30,  50,  0, 0, 30,  10, 15, 25),
    P("PENTA GLIDE",
      21, 7, 1, 0, 55, 2,  15, 28, 50, 5, 50, 20, 20, 45,  15, 70, 70, 70,  55, 35, 30, 75,
      75, 10, 20, 10, 0,  50, 40, 25,  35, 45, 40, 45, 30,
      0, 0, 0,  70, 40, 40, 30,  50,  15, 60, 20,  0, 20, 30),
    P("PENTA BELLS",
      31, 7, 1, 0, 25, 1,  85, 22, 35, 0, 65, 45, 10, 60,  2, 45, 30, 60,  50, 6, 20, 70,
      75, 20, 15, 0, 0,  45, 45, 35,  35, 35, 20, 50, 20,
      0, 0, 0,  75, 35, 60, 35,  50,  40, 80, 15,  0, 25, 35),
    /* Extras: steel-guitar flavoured glides (names only, not a guitar model). */
    P("LAP STEEL",
      24, 0, 1, 0, 35, 1,  22, 18, 40, 10, 55, 25, 15, 40,  30, 60, 85, 55,  45, 18, 50, 75,
      80, 10, 20, 20, 0,  40, 25, 15,  25, 30, 25, 50, 25,
      0, 0, 0,  55, 50, 10, 20,  50,  0, 0, 10,  0, 0, 25),
    P("PEDAL STEEL",
      28, 0, 1, 0, 45, 1,  30, 20, 35, 0, 60, 30, 10, 50,  20, 55, 80, 60,  50, 10, 60, 75,
      85, 5, 15, 15, 0,  45, 30, 20,  30, 35, 20, 55, 20,
      0, 0, 0,  60, 45, 20, 22,  50,  10, 40, 15,  0, 10, 25),
    /* After keins/2: tones with pure upper partials, gently detuned and moving. */
    P("GLASS PARTS",
      13, 0, 1, 0, 70, 1,  15, 35, 40, 15, 45, 10, 15, 60,  70, 80, 90, 80,  15, 4, 60, 70,
      70, 10, 15, 12, 15,  70, 35, 25,  45, 30, 20, 50, 15,  0, 0, 1,  80, 50, 25, 30,  50,  75, 75, 45,  0, 45, 30),
    P("DB MAJOR 9",
      13, 0, 1, 0, 65, 1,  45, 30, 55, 20, 38, 15, 20, 55,  60, 80, 90, 80,  12, 3, 60, 70,
      55, 15, 20, 10, 20,  70, 35, 20,  45, 30, 20, 45, 15,  0, 0, 1,  80, 55, 20, 30,  50,  45, 55, 40,  10, 35, 25),
    P("C MINOR 9",
      12, 1, 1, 0, 60, 1,  55, 35, 60, 25, 32, 15, 25, 50,  55, 80, 90, 80,  10, 3, 60, 70,
      48, 20, 25, 12, 25,  65, 35, 20,  45, 30, 20, 40, 15,  0, 0, 1,  80, 55, 15, 30,  50,  30, 40, 35,  20, 30, 20),
    P("DARK CLUSTER",
      14, 2, 1, 0, 75, 1,  60, 85, 70, 30, 26, 10, 45, 65,  70, 85, 95, 85,  8, 3, 70, 70,
      38, 25, 35, 10, 20,  70, 40, 20,  55, 45, 30, 35, 20,  0, 0, 1,  85, 60, 10, 30,  50,  20, 30, 70,  35, 40, 20),
    /* Hovering: undertones and slowly floating partials, even on one note. */
    P("FLOAT GLASS",
      13, 0, 1, 0, 70, 1,  10, 25, 40, 0, 50, 5, 10, 70,  70, 85, 95, 85,  10, 2, 60, 70,
      70, 10, 10, 10, 10,  70, 35, 25,  45, 30, 20, 50, 15,  0, 0, 1,  85, 50, 30, 35,  50,  75, 80, 40,  0, 85, 35),
    P("UNDERTOW",
      12, 1, 1, 0, 70, 1,  20, 30, 45, 0, 35, 10, 15, 60,  75, 85, 95, 85,  8, 2, 70, 70,
      45, 15, 15, 10, 15,  70, 35, 20,  50, 35, 25, 40, 15,  0, 0, 1,  85, 60, 15, 30,  50,  25, 30, 35,  75, 70, 20),
    P("HOVER",
      17, 4, 1, 0, 70, 1,  15, 30, 50, 0, 45, 10, 20, 75,  70, 85, 95, 85,  10, 2, 60, 70,
      60, 10, 10, 15, 10,  75, 40, 25,  50, 35, 20, 45, 15,  0, 0, 1,  90, 50, 25, 35,  50,  55, 60, 45,  40, 95, 45),
    P("HALO",
      19, 7, 1, 0, 65, 1,  0, 20, 35, 0, 55, 10, 10, 70,  65, 80, 95, 85,  12, 3, 50, 70,
      70, 10, 10, 10, 10,  65, 35, 25,  45, 30, 20, 50, 15,  0, 0, 1,  85, 45, 40, 35,  50,  65, 70, 30,  30, 75, 25),
    /* Two oscillators in an interval: every key sounds a dyad. */
    P("FIFTH DRONE",
      24, 0, 1, 0, 75, 1,  40, 15, 80, 30, 32, 10, 25, 70,  60, 80, 90, 80,  15, 4, 50, 70,
      50, 20, 20, 20, 30,  90, 50, 35,  50, 40, 30, 35, 20,  0, 0, 1,  85, 50, 30, 35,  50,  0, 0, 20,  0, 10, 25,  4, 1),
    P("THIRD STEEL",
      28, 0, 1, 0, 45, 1,  30, 12, 70, 0, 60, 30, 10, 55,  20, 55, 80, 60,  50, 10, 60, 75,
      85, 5, 15, 15, 0,  45, 30, 20,  30, 35, 20, 55, 20,  0, 0, 0,  60, 45, 20, 22,  50,  10, 40, 10,  0, 10, 25,  2, 0),
    P("SIXTH SWELL",
      21, 1, 1, 1, 70, 1,  35, 18, 65, 15, 40, 15, 20, 60,  70, 80, 90, 80,  35, 10, 70, 72,
      60, 15, 25, 15, 10,  60, 40, 25,  40, 40, 30, 40, 25,  0, 0, 0,  85, 55, 25, 35,  50,  15, 30, 30,  10, 30, 25,  5, 0),
    P("TENTH GLASS",
      13, 0, 1, 0, 70, 1,  10, 10, 55, 0, 45, 10, 15, 65,  70, 80, 90, 80,  15, 4, 60, 70,
      70, 10, 15, 12, 15,  70, 35, 25,  45, 30, 20, 50, 15,  0, 0, 1,  85, 50, 30, 35,  50,  50, 65, 30,  0, 40, 30,  9, 0),
};
#undef P
_Static_assert(SP_COUNT == 51, "preset columns");
const struct steel_preset *steel_factory(unsigned i) { return i < STEEL_PRESETS ? presets + i : 0; }

/* ------------------------------------------------------------ keyboard */
/* Physical keys 0..26 are F3..G5. A black key owns the white key to its
 * left; Fis also owns E (E-F has no black key) and Cis also owns H (B). */
static const uint8_t white_key[STEEL_WHITE] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t black_key[STEEL_BLACK] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static const uint8_t black_first[STEEL_BLACK] = {0, 1, 2, 3, 5, 6, 8, 9, 10, 12, 13};
static const uint8_t black_count[STEEL_BLACK] = {1, 1, 1, 2, 1, 2, 1, 1, 2, 1, 2};
void steel_black_owner(unsigned b, unsigned *first, unsigned *count)
{
    *first = b < STEEL_BLACK ? black_first[b] : 0;
    *count = b < STEEL_BLACK ? black_count[b] : 0;
}
unsigned steel_key_role(unsigned key)
{
    for (unsigned i = 0; i < STEEL_WHITE; ++i) if (white_key[i] == key) return i;
    for (unsigned i = 0; i < STEEL_BLACK; ++i) if (black_key[i] == key) return 16U + i;
    return 255;
}
static unsigned owner_of(unsigned white)
{
    for (unsigned b = 0; b < STEEL_BLACK; ++b)
        if (white >= black_first[b] && white < black_first[b] + black_count[b]) return b;
    return STEEL_BLACK; /* the top G has no black key */
}

/* ------------------------------------------------------------ state */
static unsigned preset_index;
/* OCT -/+ shift the keyboard by octaves (layers -2..+2); tones already
 * sounding keep their octave. Each latched tone takes one of the 16 voices;
 * every layer has its own black-key glide state. */
enum { LAYERS = 5, LAYER0 = 2 };
static int kb_layer = LAYER0;
/* Black keys: one click glides up, a double click glides down, the next
 * click glides back. A first click waits DOUBLE_MS for a second one. */
enum { DOUBLE_MS = 350 };
static uint16_t black_on[LAYERS], black_down[LAYERS], black_pending;
static uint32_t pending_at[STEEL_BLACK];
static int8_t pending_layer[STEEL_BLACK];
static struct { int8_t white, layer; uint32_t age; } slot[STEEL_VOICES]; /* white < 0: free */
static uint32_t slot_clock;
static float last_target[STEEL_VOICES], last_target2[STEEL_VOICES];

static float degree_pitch(int degree)
{
    const struct scale *s = &scales[current.value[SP_SCALE] < SCALES ? current.value[SP_SCALE] : 0];
    int n = s->count, octave = degree >= 0 ? degree / n : -((-degree + n - 1) / n);
    int step = degree - octave * n;
    float cents = current.value[SP_TUNING] ? s->just[step] : 100.0f * s->equal[step];
    return (float)(24 + current.value[SP_ROOT] + 12 * octave) + cents * 0.01f;
}
static int white_degree(unsigned w, uint16_t blacks, uint16_t downs)
{
    int degree = (int)w;
    unsigned b = owner_of(w);
    if (b < STEEL_BLACK && (blacks & (1U << b))) {
        int steps = (int)current.value[SP_GLIDE_STEPS] + 1;
        degree += (downs & (1U << b)) ? -steps : steps;
    }
    return degree;
}
/* Oscillator 2's pitch for a key sounding scale degree `degree`. */
static float interval_pitch(int degree)
{
    unsigned i = current.value[SP_INTERVAL] < 18U ? current.value[SP_INTERVAL] : 0U;
    float base = degree_pitch(degree), want = intervals[i].cents * 0.01f;
    if (!i) return base;
    if (current.value[SP_IV_MODE]) return base + want;
    const struct scale *s = &scales[current.value[SP_SCALE] < SCALES ? current.value[SP_SCALE] : 0];
    if (s->count == 7) return degree_pitch(degree + intervals[i].steps);
    int best = 0; float best_error = 1e9f;
    for (int d = -12; d <= 12; ++d) {
        if (!d) continue;
        float e = degree_pitch(degree + d) - base - want;
        if (e < 0.0f) e = -e;
        if (e < best_error - 0.01f) { best_error = e; best = d; }
    }
    return degree_pitch(degree + best);
}
static void derive(void)
{
    struct steel_patch *p = &steel_patches[steel_patch_index ^ 1U];
    const uint8_t *v = current.value;
    p->shape = unit(v[SP_SHAPE]) * 3.0f;
    /* Detune of oscillator 2: up to 40 cents, plus up to 30 cents of slow LFO
     * sweep (about 0.15 Hz, a different phase per key) for a living chorus. */
    p->detune = unit(v[SP_DETUNE]) * 40.0f;
    p->detune_lfo = unit(v[SP_DETUNE_LFO]) * 30.0f;
    /* Overtones: harmonics 2..5 as pure sines, from dark (low ones) to glassy (high ones). */
    {
        static const float dark[4] = {1.0f, 0.5f, 0.3f, 0.2f}, bright[4] = {0.35f, 0.6f, 1.0f, 0.8f};
        float t = unit(v[SP_OVERTONE_TILT]), amount = unit(v[SP_OVERTONES]) * 0.45f;
        for (unsigned k = 0; k < 4; ++k) p->overtone[k] = amount * (dark[k] + t * (bright[k] - dark[k]));
        float under = unit(v[SP_UNDERTONES]) * 0.6f;
        p->undertone[0] = under; p->undertone[1] = under * 0.6f;
        p->float_depth = unit(v[SP_FLOAT]);
        p->float_rate = float_hz(v[SP_FLOAT_RATE]);
    }
    p->osc2 = unit(v[SP_OSC2]);
    p->sub = unit(v[SP_SUB]);
    p->tone = 30.0f + (float)v[SP_TONE];
    p->tone_env = unit(v[SP_TONE_ENV]) * 48.0f;
    p->drift = unit(v[SP_DRIFT]) * unit(v[SP_DRIFT]) * 0.25f;
    p->spread = unit(v[SP_SPREAD]);
    p->attack = 1.0f / (attack_s(v[SP_ATTACK]) * STEEL_RATE);
    p->decay = 1.0f - steel_exp2(-9.965784f / (decay_s(v[SP_DECAY]) * STEEL_RATE));
    p->sustain = unit(v[SP_SUSTAIN]);
    p->release = 1.0f - steel_exp2(-9.965784f / (release_s(v[SP_RELEASE]) * STEEL_RATE));
    p->vib_rate = vib_hz(v[SP_VIB_RATE]);
    p->vib_depth = unit(v[SP_VIB_DEPTH]) * unit(v[SP_VIB_DEPTH]) * 0.6f;
    p->vib_delay = 1.0f / ((0.01f + unit(v[SP_VIB_DELAY]) * 4.0f) * STEEL_RATE);
    p->glide_step = 1000.0f / (glide_ms(v[SP_GLIDE_TIME]) * STEEL_RATE);
    p->glide_curve = v[SP_GLIDE_CURVE];
    p->level = unit(v[SP_LEVEL]) * unit(v[SP_LEVEL]) * 0.5f;
    p->filter_on = v[SP_FILTER] < 99 || v[SP_RESO] || v[SP_DRIVE];
    p->cutoff = 20.0f + (float)v[SP_FILTER] * 1.17f;
    p->resonance = unit(v[SP_RESO]) * 3.9f;
    p->drive = 1.0f + unit(v[SP_DRIVE]) * 4.0f;
    p->lfo_step = lfo_rate_hz() * FM1_BLOCK_FRAMES / STEEL_RATE;
    p->lfo_depth = unit(v[SP_LFO_DEPTH]) * 36.0f;
    p->tape_time = steel_clamp(tape_time_s(), 0.01f, STEEL_TAPE_MAX_S) * STEEL_RATE;
    p->tape_feedback = (float)v[SP_TAPE_FEEDBACK] * 0.01f;
    p->tape_wow = unit(v[SP_TAPE_WOW]);
    p->tape_tone = one_pole(600.0f * steel_exp2(unit(v[SP_TAPE_TONE]) * 3.5f));
    p->tape_mix = unit(v[SP_TAPE_MIX]) * 0.9f;
    p->delay_time = steel_clamp(delay_time_s(), 0.01f, STEEL_DELAY_MAX_S) * STEEL_RATE;
    p->delay_feedback = unit(v[SP_DELAY_FEEDBACK]) * 0.92f;
    p->delay_mix = unit(v[SP_DELAY_MIX]) * 0.8f;
    p->rev_decay = 0.25f + unit(v[SP_REV_SIZE]) * 0.70f; /* up to 0.95: long tails */
    p->rev_damp = one_pole(1200.0f * steel_exp2((1.0f - unit(v[SP_REV_DAMP])) * 3.5f) * 0.5f);
    p->rev_shimmer = unit(v[SP_SHIMMER]) * 0.95f; /* share of the tank recirculation shifted up */
    /* REV MIX: the wet level rises to about +12 dB over the old scale; above
     * half the dry signal steps back (to 55 % at full), so 99 is mostly hall. */
    p->rev_mix = unit(v[SP_REV_MIX]) * 1.6f;
    p->rev_dry = 1.0f - 0.9f * steel_clamp(unit(v[SP_REV_MIX]) - 0.5f, 0.0f, 0.5f);
    p->bpm = (float)(40U + v[SP_BPM]);
    STEEL_BARRIER();
    steel_patch_index ^= 1U;
}
/* The drum track's BLIP notes: every other scale tone, folded into
 * roughly 1.5-3 kHz (first four) and 3-6 kHz (last four). */
static void publish_blips(void)
{
    float midi[8];
    for (unsigned i = 0; i < 8; ++i) {
        float p = degree_pitch((int)(i * 2U));
        float low = i < 4 ? 90.0f : 102.0f;
        while (p < low) p += 12.0f;
        while (p >= low + 12.0f) p -= 12.0f;
        midi[i] = p;
    }
    steel_drums_set_blips(midi);
}
/* Publish pitches. New notes jump to their pitch; sounding ones glide. */
static void publish(uint16_t started)
{
    publish_blips();
    for (unsigned w = 0; w < STEEL_VOICES; ++w) {
        struct steel_command *c = &steel_commands[w];
        if (slot[w].white < 0) { c->gate = 0; continue; } /* free voice: release where it is */
        unsigned key = (unsigned)slot[w].white;
        int layer = slot[w].layer, degree = white_degree(key, black_on[layer], black_down[layer]);
        float shift = 12.0f * (float)(layer - LAYER0);
        float target = degree_pitch(degree) + shift, target2 = interval_pitch(degree) + shift;
        unsigned changed = target != last_target[w] || target2 != last_target2[w];
        last_target[w] = target;
        last_target2[w] = target2;
        if (started & (1U << w)) {
            c->target = target;
            c->target2 = target2;
            c->pan = (float)key * (1.0f / 15.0f) - 0.5f; /* low keys left, high keys right */
            c->fade = 0;
            STEEL_BARRIER();
            c->gate = 1;
            c->note_serial = (uint8_t)(c->note_serial + 1U);
        } else {
            if (changed) {
                c->target = target;
                c->target2 = target2;
                STEEL_BARRIER();
                c->glide_serial = (uint8_t)(c->glide_serial + 1U);
            }
            c->gate = 1;
        }
    }
}
static unsigned tempo_ready;
void steel_load(unsigned index)
{
    if (index >= STEEL_PRESETS) index = 0;
    preset_index = index;
    uint8_t bpm = current.value[SP_BPM];
    current = presets[index];
    if (tempo_ready) current.value[SP_BPM] = bpm; /* tempo survives preset changes */
    tempo_ready = 1;
    derive();
    publish(0);
}
void steel_init(void)
{
    for (unsigned w = 0; w < STEEL_VOICES; ++w) { slot[w].white = -1; slot[w].age = 0; last_target[w] = last_target2[w] = -1.0f; }
    for (unsigned l = 0; l < LAYERS; ++l) black_on[l] = black_down[l] = 0;
    black_pending = 0;
    kb_layer = LAYER0;
    steel_load(0);
}
unsigned steel_preset_index(void) { return preset_index; }
const struct steel_preset *steel_current(void) { return &current; }
unsigned steel_get(unsigned p) { return p < SP_COUNT ? current.value[p] : 0; }
void steel_edit(unsigned p, int delta)
{
    if (p >= SP_COUNT || !delta) return;
    int v = (int)current.value[p] + delta, max = info[p].max;
    v = v < 0 ? 0 : v > max ? max : v;
    if (v == current.value[p]) return;
    current.value[p] = (uint8_t)v;
    derive();
    publish(0); /* key, scale, tuning and steps changes glide to the new pitches */
}
/* Key selector: the root nearest to the current one with pitch class `pc`
 * (0 = C). The key's scale and the glides follow; sounding tones glide. */
void steel_set_key(unsigned pc)
{
    int now = (int)current.value[SP_ROOT], best = now, distance = 99;
    for (int v = 0; v <= (int)info[SP_ROOT].max; ++v) {
        int d = v > now ? v - now : now - v;
        if ((unsigned)(24 + v) % 12U == pc % 12U && d < distance) { distance = d; best = v; }
    }
    steel_edit(SP_ROOT, best - now);
}
static int find_slot(unsigned w, int layer)
{
    for (unsigned v = 0; v < STEEL_VOICES; ++v)
        if (slot[v].white == (int)w && slot[v].layer == layer) return (int)v;
    return -1;
}
/* CPU guard: the number of tones that may sound at once (16 unless the
 * audio load was too high). */
enum { GUARD_HIGH = 92, GUARD_LOW = 75, GUARD_MIN = 4, GUARD_CALM_MS = 4000, GUARD_WAIT_MS = 1000 };
static unsigned tone_cap = STEEL_VOICES;
static uint32_t guard_at, calm_at;
static unsigned latched(void)
{
    unsigned n = 0;
    for (unsigned v = 0; v < STEEL_VOICES; ++v) n += slot[v].white >= 0;
    return n;
}
/* Release the oldest latched tone with a quick fade. */
static void shed_oldest(void)
{
    int oldest = -1;
    for (unsigned v = 0; v < STEEL_VOICES; ++v)
        if (slot[v].white >= 0 && (oldest < 0 || slot[v].age < slot[oldest].age)) oldest = (int)v;
    if (oldest < 0) return;
    slot[oldest].white = -1;
    slot[oldest].age = ++slot_clock;
    steel_commands[oldest].fade = 1;
}
/* Latch or release white key w in keyboard layer `layer`. A new tone takes
 * the voice released longest ago; at the tone limit the oldest tone gives way. */
static void toggle_white(unsigned w, int layer)
{
    int v = find_slot(w, layer);
    if (v >= 0) { slot[v].white = -1; slot[v].age = ++slot_clock; publish(0); return; }
    while (latched() >= tone_cap) shed_oldest();
    int best = -1;
    for (unsigned i = 0; i < STEEL_VOICES; ++i)
        if (slot[i].white < 0 && (best < 0 || slot[i].age < slot[best].age)) best = (int)i;
    if (best < 0)
        for (unsigned i = 0; i < STEEL_VOICES; ++i)
            if (best < 0 || slot[i].age < slot[best].age) best = (int)i;
    slot[best].white = (int8_t)w; slot[best].layer = (int8_t)layer; slot[best].age = ++slot_clock;
    publish((uint16_t)(1U << best));
}
void steel_white(unsigned w)
{
    if (w < STEEL_WHITE) toggle_white(w, kb_layer);
}
static void black_glide(unsigned b, int layer, unsigned on, unsigned down)
{
    uint16_t bit = (uint16_t)(1U << b);
    black_on[layer] = on ? black_on[layer] | bit : black_on[layer] & (uint16_t)~bit;
    black_down[layer] = on && down ? black_down[layer] | bit : black_down[layer] & (uint16_t)~bit;
    publish(0);
}
void steel_black(unsigned b, uint32_t now)
{
    if (b >= STEEL_BLACK) return;
    uint16_t bit = (uint16_t)(1U << b);
    if (black_pending & bit) {                      /* second click in time: down */
        black_pending &= (uint16_t)~bit;
        black_glide(b, pending_layer[b], 1, 1);
    } else if (black_on[kb_layer] & bit) {          /* gliding: back home */
        black_glide(b, kb_layer, 0, 0);
    } else {                                        /* wait for a possible second click */
        black_pending |= bit;
        pending_at[b] = now;
        pending_layer[b] = (int8_t)kb_layer;
    }
}
/* Control task: a single click whose double-click window has passed glides up. */
static void black_timeouts(uint32_t now)
{
    for (unsigned b = 0; black_pending && b < STEEL_BLACK; ++b)
        if ((black_pending & (1U << b)) && now - pending_at[b] >= DOUBLE_MS) {
            black_pending &= (uint16_t)~(1U << b);
            black_glide(b, pending_layer[b], 1, 0);
        }
}
void steel_all_off(void)
{
    for (unsigned v = 0; v < STEEL_VOICES; ++v) if (slot[v].white >= 0) { slot[v].white = -1; slot[v].age = ++slot_clock; }
    for (unsigned l = 0; l < LAYERS; ++l) black_on[l] = black_down[l] = 0;
    black_pending = 0;
    publish(0);
}
unsigned steel_tone_limit(void) { return tone_cap; }
unsigned steel_guard(unsigned load, unsigned overrun, uint32_t now)
{
    unsigned before = tone_cap;
    if (overrun || load >= GUARD_HIGH) {
        calm_at = now;
        if (!overrun && now - guard_at < GUARD_WAIT_MS) return 0; /* the load reading falls slowly */
        guard_at = now;
        /* First fade the release tails; if none ring, drop the oldest tone. */
        uint16_t tails = steel_ringing(), faded = 0;
        for (unsigned v = 0; v < STEEL_VOICES; ++v)
            if ((tails & (1U << v)) && slot[v].white < 0 && !steel_commands[v].fade) { steel_commands[v].fade = 1; faded = 1; }
        unsigned n = latched();
        if (!faded && n > GUARD_MIN) { tone_cap = n - 1U; shed_oldest(); publish(0); }
    } else if (load < GUARD_LOW && tone_cap < STEEL_VOICES) {
        if (now - calm_at >= GUARD_CALM_MS) { ++tone_cap; calm_at = now; }
    } else calm_at = now;
    return tone_cap != before;
}
/* OCT -/+: the keys reach another octave; sounding tones stay where they are. */
int steel_transpose(int octaves)
{
    int layer = kb_layer + octaves;
    kb_layer = layer < 0 ? 0 : layer >= LAYERS ? LAYERS - 1 : layer;
    return kb_layer - LAYER0;
}
int steel_octave(void) { return kb_layer - LAYER0; }
/* ------------------------------------------------------------ tempo */
static uint32_t tap_last, tap_intervals[3];
static unsigned tap_count;
static volatile uint32_t clock_last, clock_beat_start, clock_ticks;
static volatile float clock_bpm;
static volatile uint8_t clock_running;
static void set_bpm(float bpm)
{
    int v = (int)(bpm + 0.5f) - 40;
    v = v < 0 ? 0 : v > info[SP_BPM].max ? info[SP_BPM].max : v;
    if ((uint8_t)v == current.value[SP_BPM]) return;
    current.value[SP_BPM] = (uint8_t)v;
    derive();
}
unsigned steel_bpm(void) { return 40U + current.value[SP_BPM]; }
unsigned steel_bpm_external(void) { return clock_running; }
void steel_tap(uint32_t now)
{
    uint32_t gap = now - tap_last;
    tap_last = now;
    if (gap < 200U || gap > 2000U) { tap_count = 0; return; } /* 30..300 BPM */
    tap_intervals[tap_count % 3U] = gap;
    if (++tap_count < 2) return;
    unsigned n = tap_count < 3 ? tap_count : 3U;
    uint32_t sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += tap_intervals[i];
    set_bpm(60000.0f * (float)n / (float)sum);
}
/* MIDI input context: time 24 ticks per beat; the control task applies it. */
void steel_midi_clock(uint8_t status, uint32_t now)
{
    if (status == 0xfc) { clock_running = 0; clock_ticks = 0; return; }
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
void steel_midi_message(const uint8_t *m, size_t n)
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
static void white_set(unsigned w, unsigned on, int layer)
{
    if (w >= STEEL_WHITE || (find_slot(w, layer) >= 0) == (int)on) return;
    toggle_white(w, layer);
}
static void black_set(unsigned b, unsigned on, int layer)
{
    if (b >= STEEL_BLACK || ((black_on[layer] >> b) & 1U) == on) return;
    black_glide(b, layer, on, 0); /* MIDI: held black key glides up */
}
static void midi_play(const uint8_t m[3])
{
    unsigned type = m[0] & 0xf0U, channel = m[0] & 15U, note = m[1];
    unsigned on = type == 0x90 && m[2], off = type == 0x80 || (type == 0x90 && !m[2]);
    if (type == 0xb0 && (m[1] == 123 || m[1] == 120)) { steel_all_off(); return; }
    if (!on && !off) return;
    if (channel == 9) { /* drums: C1 = the F key, then upwards */
        if (on && note >= 36) steel_drums_hit((note - 36U) % 27U);
        return;
    }
    int key = (int)note - 53, layer = LAYER0; /* F3 = the first key at OCT 0 */
    while (key < 0) { key += 12; --layer; }
    while (key > 26) { key -= 12; ++layer; }
    while (layer < 0) { layer++; } /* beyond -2/+2 octaves: fold in */
    while (layer >= LAYERS) { layer--; }
    unsigned role = steel_key_role((unsigned)key);
    if (role < 16) white_set(role, on, layer);
    else if (role < 27) black_set(role - 16U, on, layer);
}
void steel_poll(uint32_t now)
{
    if (clock_running && now - clock_last > 2000U) clock_running = 0;
    if (clock_running) set_bpm(clock_bpm);
    black_timeouts(now);
    for (unsigned r = midi_in.read, w = midi_in.write; r != w; ++r) {
        STEEL_BARRIER();
        midi_play(midi_in.data[r & 63U]);
        midi_in.read = r + 1U;
    }
}
uint16_t steel_white_state(void)
{
    uint16_t m = 0;
    for (unsigned v = 0; v < STEEL_VOICES; ++v)
        if (slot[v].white >= 0 && slot[v].layer == kb_layer) m |= (uint16_t)(1U << slot[v].white);
    return m;
}
uint16_t steel_black_state(void) { return black_on[kb_layer]; }
uint16_t steel_white_glided(void)
{
    uint16_t glided = 0;
    for (unsigned w = 0; w < STEEL_WHITE; ++w) {
        unsigned b = owner_of(w);
        if (b < STEEL_BLACK && (black_on[kb_layer] & (1U << b))) glided |= (uint16_t)(1U << w);
    }
    return glided;
}
