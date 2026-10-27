/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLITCH DRUMS control side: the default kit, editing and random beats. */
#include "steel_drums_internal.h"
#include <string.h>


static const struct { char name[10]; uint8_t max; } params[DP_COUNT] = {
    {"SOURCE", 0}, {"PITCH", 48}, {"DECAY", 99}, {"LEVEL", 99},
    {"PROB", 100}, {"RATCHET", 4}, {"MICRO", 99}, {"SCATTER", 32},
    {"PAN MODE", 3}, {"PAN", 99}, {"TAPE", 99}, {"SHIMMER", 99}, {"RANDOM", 99}, {"RATE", 2},
    {"COLOR", 99}};
static const char source_names[DS_COUNT][10] = {
    "KICK", "SNARE", "HIHAT", "IMPULSE", "TRAIN", "BLIP", "TICK", "CUT",
    "CRUNCH", "CHIRP", "SNAP", "DUST", "BITS", "THUMP", "POP", "HIGH"};
static unsigned level = 75;
static uint32_t activity_seen;

static unsigned fixed_slot(unsigned slot)
{
    return slot == DRUM_KEY_HAT;
}
static void put_number(char *out, int v)
{
    char tmp[8]; unsigned n = 0, u = (unsigned)(v < 0 ? -v : v);
    if (v < 0) *out++ = '-';
    do { tmp[n++] = (char)('0' + u % 10U); u /= 10U; } while (u && n < 7);
    while (n) *out++ = tmp[--n];
    *out = 0;
}
unsigned steel_drums_slot_of_key(unsigned key) { return key < DRUM_SLOTS ? key : 0; }
unsigned steel_drums_max(unsigned p)
{
    if (p != DP_SOURCE) return p < DP_COUNT ? params[p].max : 0;
    return DS_COUNT - 1U;
}
const char *steel_drums_param_name(unsigned p) { return p < DP_COUNT ? params[p].name : ""; }
unsigned steel_drums_get(unsigned slot, unsigned p) { return slot < DRUM_SLOTS && p < DP_COUNT ? drum_slots[slot].value[p] : 0; }
uint16_t steel_drums_steps(unsigned slot) { return slot < DRUM_SLOTS ? drum_slots[slot].steps : 0; }
static void source_name(unsigned source, char out[12]) { strcpy(out, source_names[source < DS_COUNT ? source : DS_IMPULSE]); }
void steel_drums_slot_name(unsigned slot, char out[12]) { source_name(steel_drums_get(slot, DP_SOURCE), out); }
void steel_drums_param_text(unsigned slot, unsigned p, char out[12])
{
    static const char ratchets[5][5] = {"OFF", "X2", "X3", "X4", "ROLL"};
    static const char pans[4][10] = {"FIXED", "RANDOM", "PINGPONG", "LFO"};
    unsigned v = steel_drums_get(slot, p);
    switch (p) {
    case DP_SOURCE: source_name(v, out); return;
    case DP_PITCH: put_number(out, (int)v - 24); return;
    case DP_PROB: put_number(out, (int)v); strcat(out, "%"); return;
    case DP_RATCHET: strcpy(out, ratchets[v < 5 ? v : 0]); return;
    case DP_MICRO: put_number(out, (int)(v * 10U / 99U)); strcat(out, "MS"); return;
    case DP_SCATTER: if (!v) { strcpy(out, "OFF"); return; } put_number(out, (int)v); strcat(out, "/BAR"); return;
    case DP_PAN_MODE: strcpy(out, pans[v < 4 ? v : 0]); return;
    case DP_RATE: strcpy(out, v == 0 ? "1/16" : v == 1 ? "1/32" : "1/64"); return;
    case DP_PAN:
        if (steel_drums_get(slot, DP_PAN_MODE) == PAN_FIXED) {
            int pan = (int)v * 2 - 99;
            if (pan > -3 && pan < 3) { strcpy(out, "C"); return; }
            out[0] = pan < 0 ? 'L' : 'R'; put_number(out + 1, (pan < 0 ? -pan : pan) / 2); return;
        }
        put_number(out, (int)v); strcat(out, "%"); return;
    default: break;
    }
    put_number(out, (int)v);
}
void steel_drums_edit(unsigned slot, unsigned p, int delta)
{
    if (slot >= DRUM_SLOTS || p >= DP_COUNT || !delta) return;
    if (p == DP_SOURCE && fixed_slot(slot)) return; /* the hi-hat on A stays classic */
    int v = (int)drum_slots[slot].value[p] + delta, max = (int)steel_drums_max(p);
    int min = p == DP_SOURCE ? DS_IMPULSE : 0;        /* no kick, snare or hi-hat elsewhere */
    drum_slots[slot].value[p] = (uint8_t)(v < min ? min : v > max ? max : v);
}
void steel_drums_toggle_step(unsigned slot, unsigned s)
{
    if (slot < DRUM_SLOTS && s < DRUM_STEPS) drum_slots[slot].steps ^= (uint16_t)(1U << s);
}
void steel_drums_transport(int run)
{
    drum_run = (uint8_t)(run < 0 ? !drum_run : run != 0);
}
unsigned steel_drums_running(void) { return drum_run; }
unsigned steel_drums_step(void) { return drum_step_now; }
uint32_t steel_drums_activity(void)
{
    uint32_t bits = drum_activity_bits;
    drum_activity_bits = 0;
    uint32_t shown = bits | activity_seen;
    activity_seen = bits;
    return shown;
}
static unsigned hiss = 12;
unsigned steel_drums_hiss(void) { return hiss; }
void steel_drums_hiss_edit(int delta)
{
    int v = (int)hiss + delta;
    hiss = (unsigned)(v < 0 ? 0 : v > 99 ? 99 : v);
    drum_hiss_gain = (float)hiss * (float)hiss * (0.02f / (99.0f * 99.0f));
}
unsigned steel_drums_level(void) { return level; }
void steel_drums_level_edit(int delta)
{
    int v = (int)level + delta;
    level = (unsigned)(v < 0 ? 0 : v > 99 ? 99 : v);
    drum_level_gain = (float)level * (float)level / (99.0f * 99.0f);
}

/* ------------------------------------------------------------ default kit */
static void set(unsigned slot, unsigned source, uint16_t steps, unsigned prob, unsigned ratchet, unsigned micro,
                unsigned scatter, unsigned pan_mode, unsigned pan, unsigned tape, unsigned shimmer)
{
    uint8_t *v = drum_slots[slot].value;
    v[DP_SOURCE] = (uint8_t)source; v[DP_PITCH] = 24; v[DP_DECAY] = 40; v[DP_LEVEL] = 80;
    v[DP_PROB] = (uint8_t)prob; v[DP_RATCHET] = (uint8_t)ratchet; v[DP_MICRO] = (uint8_t)micro;
    v[DP_SCATTER] = (uint8_t)scatter; v[DP_PAN_MODE] = (uint8_t)pan_mode; v[DP_PAN] = (uint8_t)pan;
    v[DP_TAPE] = (uint8_t)tape; v[DP_SHIMMER] = (uint8_t)shimmer;
    v[DP_COLOR] = 50;
    /* Glitch slots: plenty of per-hit variation. All slots start
     * at 1/16; RATE (DRUM 4/5) switches a slot to 1/32 or 1/64. */
    v[DP_RANDOM] = source >= DS_IMPULSE ? 30 : 15;
    v[DP_RATE] = 0;
    drum_slots[slot].steps = steps;
}
static void default_kit(void)
{
    memset(drum_slots, 0, sizeof drum_slots);
    drum_pattern_seed = 1U;
    /* Microsound kit after the keins references: about 5-15 tiny clicks per
     * second, a third of them in short trains, quiet, panned +-30..60 %, a few
     * sine blips in the synth's key, rare dropouts of the synth layer. */
    set(0, DS_IMPULSE, 0, 100, 0, 0, 0, PAN_RANDOM, 40, 0, 20);              /* F: play a click */
    set(2, DS_TRAIN, 0, 100, 0, 0, 0, PAN_RANDOM, 40, 5, 20);                 /* G: play a rasp  */
    set(DRUM_KEY_HAT, DS_HAT, 0x5555, 95, 0, 5, 0, PAN_RANDOM, 35, 0, 10);   /* A: hi-hat       */
    drum_slots[DRUM_KEY_HAT].value[DP_LEVEL] = 45;
    set(1, DS_IMPULSE, 0x1111, 95, 0, 10, 0, PAN_RANDOM, 30, 5, 20); /* pulse on quarters  */
    set(3, DS_IMPULSE, 0x4924, 95, 0, 5, 0, PAN_RANDOM, 45, 0, 15);  /* syncopated clicks  */
    set(5, DS_TRAIN, 0x0808, 95, 0, 5, 0, PAN_RANDOM, 50, 10, 20);   /* click trains       */
    set(6, DS_BLIP, 0x0420, 80, 0, 5, 0, PAN_RANDOM, 45, 20, 45);    /* sine blips in key  */
    set(7, DS_TICK, 0, 85, 0, 10, 4, PAN_RANDOM, 45, 5, 20);         /* ticks, repeating   */
    set(8, DS_CUT, 0x0100, 35, 0, 0, 0, PAN_FIXED, 50, 0, 0);        /* rare dropouts      */
    for (unsigned slot = 1; slot <= 8; ++slot) drum_slots[slot].value[DP_LEVEL] = 50;
    drum_slots[6].value[DP_LEVEL] = 40; drum_slots[8].value[DP_LEVEL] = 70; drum_slots[8].value[DP_DECAY] = 25;
    drum_slots[5].value[DP_DECAY] = 20; drum_slots[1].value[DP_DECAY] = 30;
    /* The other keys: the GLITCH sounds first, then microsound variations,
     * to play and build patterns from. */
    static const uint8_t cycle[18] = {DS_CRUNCH, DS_CHIRP, DS_SNAP, DS_DUST, DS_BITS, DS_THUMP, DS_POP, DS_HIGH,
                                      DS_IMPULSE, DS_TRAIN, DS_BLIP, DS_TICK, DS_CHIRP, DS_SNAP, DS_BITS,
                                      DS_DUST, DS_HIGH, DS_TICK};
    for (unsigned slot = 9, n = 0; slot < DRUM_SLOTS; ++slot, ++n)
        if (!fixed_slot(slot)) {
            set(slot, cycle[n < 18U ? n : 0U], 0, 100, 0, 20, 0, PAN_RANDOM, 45, 10, 25);
            drum_slots[slot].value[DP_PITCH] = (uint8_t)(18U + (n * 7U) % 13U);
            drum_slots[slot].value[DP_DECAY] = (uint8_t)(20U + (n * 17U) % 60U);
            drum_slots[slot].value[DP_COLOR] = (uint8_t)(25U + (n * 29U) % 50U);
            drum_slots[slot].value[DP_LEVEL] = 50;
        }
    drum_slots[14].value[DP_PITCH] = 24; /* THUMP on its 42 Hz */
}

/* ------------------------------------------------------------ random beat */
static uint32_t dice;
static unsigned roll(unsigned n) { dice = dice * 1664525U + 1013904223U; return (dice >> 8) % n; }
static unsigned between(unsigned lo, unsigned hi) { return lo + roll(hi - lo + 1U); }
static unsigned free_slot(uint32_t used)
{
    for (unsigned tries = 0; tries < 64; ++tries) {
        unsigned slot = roll(DRUM_SLOTS);
        if (!fixed_slot(slot) && !((used >> slot) & 1U)) return slot;
    }
    for (unsigned slot = 0; slot < DRUM_SLOTS; ++slot)
        if (!fixed_slot(slot) && !((used >> slot) & 1U)) return slot;
    return DRUM_SLOTS;
}
/* Exactly `hits` steps spread as evenly as possible (Euclid), rotated. */
static uint16_t euclid(unsigned hits, unsigned rotate)
{
    uint16_t mask = 0;
    for (unsigned i = 0; i < DRUM_STEPS; ++i)
        if ((i * hits) % DRUM_STEPS < hits) mask |= (uint16_t)(1U << i);
    rotate %= DRUM_STEPS;
    return (uint16_t)((mask << rotate) | (mask >> ((DRUM_STEPS - rotate) % DRUM_STEPS)));
}
enum { ROLE_PULSE, ROLE_GROOVE, ROLE_COUNTER, ROLE_BLIP, ROLE_DUST, ROLE_CUT, ROLE_PLAY };
static void random_voice(unsigned slot, unsigned role)
{
    static const uint8_t pans[10] = {PAN_RANDOM, PAN_RANDOM, PAN_RANDOM, PAN_RANDOM, PAN_RANDOM, PAN_RANDOM,
                                     PAN_RANDOM, PAN_PINGPONG, PAN_PINGPONG, PAN_LFO};
    static const uint8_t play[12] = {DS_IMPULSE, DS_TRAIN, DS_BLIP, DS_TICK, DS_CRUNCH, DS_CHIRP,
                                     DS_SNAP, DS_DUST, DS_BITS, DS_THUMP, DS_POP, DS_HIGH};
    static const uint8_t counter[4] = {DS_TICK, DS_IMPULSE, DS_SNAP, DS_BITS};
    static const uint8_t dust[4] = {DS_IMPULSE, DS_TICK, DS_POP, DS_CRUNCH}; /* DUST would crackle too densely here */
    unsigned source = DS_IMPULSE, prob = 100, scatter = 0, ratchet = 0, micro = between(0, 25);
    unsigned level = between(40, 55), decay = between(10, 50), shimmer = between(5, 30), tape = between(0, 15);
    uint16_t steps = 0;
    switch (role) {
    case ROLE_PULSE:   /* the anchor: quarters or eighths, on the beat */
        source = roll(3) ? DS_IMPULSE : DS_TICK;
        steps = roll(2) ? 0x1111 : 0x5555; prob = between(95, 100); micro = between(0, 8); break;
    case ROLE_GROOVE:  /* click trains on a Euclid 3 or 5 */
        source = DS_TRAIN; steps = euclid(roll(2) ? 3 : 5, roll(16)); prob = between(90, 100);
        decay = between(10, 35); break;
    case ROLE_COUNTER: /* a syncopated line against the pulse */
        source = counter[roll(4)]; steps = euclid(between(5, 7), roll(16));
        prob = between(80, 95); ratchet = roll(6) ? 0 : 1U; break;
    case ROLE_BLIP:
        source = DS_BLIP; steps = euclid(between(1, 3), roll(16)); prob = between(70, 90);
        level = between(30, 45); shimmer = between(30, 60); tape = between(10, 30); break;
    case ROLE_DUST:    /* quiet, repeating scatter on the 1/16 grid */
        source = dust[roll(4)]; scatter = between(3, 7); prob = between(70, 90);
        level = between(30, 42); break;
    case ROLE_CUT:
        source = DS_CUT; steps = euclid(between(1, 2), roll(16)); prob = between(40, 70);
        level = between(50, 80); decay = between(10, 40); break;
    default:           /* a sound to play, without a pattern */
        source = play[roll(12)]; break;
    }
    set(slot, source, steps, prob, ratchet, micro, scatter, pans[roll(10)], between(25, 60), tape, shimmer);
    uint8_t *v = drum_slots[slot].value;
    v[DP_LEVEL] = (uint8_t)level; v[DP_DECAY] = (uint8_t)decay;
    v[DP_PITCH] = (uint8_t)between(19, 29); v[DP_RANDOM] = (uint8_t)between(10, 40);
    v[DP_COLOR] = (uint8_t)between(20, 80);
    v[DP_RATE] = 0;
}
/* Expected hits per bar of one slot (trains count once). */
static float slot_hits(const struct drum_slot *d)
{
    static const uint8_t ratchets[5] = {1, 2, 3, 4, 6};
    const uint8_t *v = d->value;
    unsigned steps = 0;
    for (unsigned i = 0; i < DRUM_STEPS; ++i) steps += (d->steps >> i) & 1U;
    float per_bar = (float)(steps * ratchets[v[DP_RATCHET] < 5 ? v[DP_RATCHET] : 0]) * (float)(1U << v[DP_RATE]) +
                    (float)((unsigned)v[DP_SCATTER] << v[DP_RATE]);
    return v[DP_SOURCE] == DS_CUT ? 0.0f : per_bar * (float)v[DP_PROB] / 100.0f;
}
void steel_drums_randomize(uint32_t seed, unsigned bpm)
{
    dice = seed | 1U;
    memset(drum_slots, 0, sizeof drum_slots);
    drum_pattern_seed = seed * 2654435761U + 1U;
    set(DRUM_KEY_HAT, DS_HAT, roll(3) ? 0x5555 : euclid(between(5, 8), roll(16)), between(88, 100), 0,
        between(0, 20), 0, PAN_RANDOM, between(20, 45), 0, between(0, 20));
    drum_slots[DRUM_KEY_HAT].value[DP_LEVEL] = (uint8_t)between(28, 40);
    uint32_t used = 1U << DRUM_KEY_HAT;
    uint8_t roles[7];
    unsigned count = 0;
    roles[count++] = ROLE_PULSE;
    roles[count++] = ROLE_GROOVE;
    roles[count++] = ROLE_COUNTER;
    if (roll(10) < 8) roles[count++] = ROLE_BLIP;
    if (roll(10) < 5) roles[count++] = ROLE_DUST;
    if (roll(10) < 4) roles[count++] = ROLE_CUT;
    for (unsigned i = 0; i < count; ++i) {
        unsigned slot = free_slot(used);
        if (slot >= DRUM_SLOTS) break;
        used |= 1U << slot;
        random_voice(slot, roles[i]);
    }
    /* Keep the beat in the keins range: thin out the busiest line until it fits. */
    float bar = 240.0f / (float)(bpm ? bpm : 90U), target = 14.0f * bar;
    for (unsigned guard = 0; guard < 64; ++guard) {
        float total = 0.0f, worst = 0.0f;
        unsigned busiest = DRUM_SLOTS;
        for (unsigned slot = 0; slot < DRUM_SLOTS; ++slot) {
            float hits = slot_hits(&drum_slots[slot]);
            total += hits;
            if (hits > worst) { worst = hits; busiest = slot; }
        }
        if (total <= target || busiest >= DRUM_SLOTS) break;
        uint8_t *v = drum_slots[busiest].value;
        if (v[DP_SCATTER] > 1) v[DP_SCATTER] = (uint8_t)(v[DP_SCATTER] * 2U / 3U);
        else if (v[DP_RATCHET]) v[DP_RATCHET] = 0;
        else drum_slots[busiest].steps &= (uint16_t)(drum_slots[busiest].steps - 1U); /* drop a step */
    }
    for (unsigned slot = 0; slot < DRUM_SLOTS; ++slot)
        if (!fixed_slot(slot) && !((used >> slot) & 1U)) random_voice(slot, ROLE_PLAY);
}

void steel_drums_init(void)
{
    steel_drums_level_edit(0);
    steel_drums_hiss_edit(0);
    default_kit();
}
