/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLITCH DRUMS: one drum track for GLIDER.
 *
 * Each of the 27 keys is a slot. A is a fixed classic hi-hat; the other 26
 * slots play synthesized microsounds (impulses, click trains, sine blips in
 * the preset's key, noise ticks, cuts of the synth layer, and eight sounds
 * from GLITCH). There are no samples. A quiet hiss bed lies underneath.
 * Every slot has 16 steps plus probability, ratchets, micro-timing, random
 * scatter, auto-panning and sends into the tape echo and shimmer reverb. */
#ifndef FM1_STEEL_DRUMS_H
#define FM1_STEEL_DRUMS_H
#include "fm1.h"

enum { DRUM_SLOTS = 27, DRUM_STEPS = 16, DRUM_VOICES = 12 };
/* Physical key of the one fixed classic sound: A (third white key). */
enum { DRUM_KEY_HAT = 4 };

/* Sound sources. DS_KICK/DS_SNARE stay in the engine but no slot offers
 * them; slots other than the hi-hat choose from DS_IMPULSE upwards.
 * COLOR shapes the eight sounds from GLITCH (CRUNCH .. HIGH). */
enum drum_source {
    DS_KICK, DS_SNARE, DS_HAT,
    DS_IMPULSE,  /* a single 0.1-1.5 ms digital click */
    DS_TRAIN,    /* 3-20 impulses, 2-12 ms apart: a short rasp */
    DS_BLIP,     /* a pure sine, 15-80 ms, on a high note of the preset's scale */
    DS_TICK,     /* a 2-10 ms burst of bright noise */
    DS_CUT,      /* silent: briefly dips and darkens the synth layer (a dropout) */
    DS_CRUNCH,   /* sample-and-hold digital noise; COLOR smooth .. harsh */
    DS_CHIRP,    /* a fast sine sweep; COLOR down .. up */
    DS_SNAP,     /* a dry, gated band of noise with a click; COLOR dark .. bright */
    DS_DUST,     /* a crackle of random clicks; COLOR sparse .. dense */
    DS_BITS,     /* LFSR noise; COLOR tonal .. noisy */
    DS_THUMP,    /* a micro kick, 18-200 Hz; COLOR pure .. click and drive */
    DS_POP,      /* a speaker pop: a jump that slides back; COLOR soft .. sharp */
    DS_HIGH,     /* a very high short sine beep; COLOR pure .. beating */
    DS_COUNT
};
enum drum_param {
    DP_SOURCE, DP_PITCH, DP_DECAY, DP_LEVEL,      /* SOUND  */
    DP_PROB, DP_RATCHET, DP_MICRO, DP_SCATTER,    /* RHYTHM */
    DP_PAN_MODE, DP_PAN, DP_TAPE, DP_SHIMMER,     /* SPACE  */
    DP_RANDOM,                                    /* per-hit pitch/decay variation */
    DP_RATE,                                      /* pattern speed: 0 = 1/16, 1 = 1/32, 2 = 1/64 */
    DP_COLOR,                                     /* tone of the GLITCH sounds (CRUNCH .. HIGH) */
    DP_COUNT
};
enum { PAN_FIXED, PAN_RANDOM, PAN_PINGPONG, PAN_LFO };
struct drum_slot {
    uint8_t value[DP_COUNT];
    uint16_t steps; /* bit n: step n plays */
};

/* Control task. */
void steel_drums_init(void);
unsigned steel_drums_slot_of_key(unsigned key);  /* 0..26 */
void steel_drums_hit(unsigned slot);             /* audition */
void steel_drums_toggle_step(unsigned slot, unsigned step);
void steel_drums_edit(unsigned slot, unsigned parameter, int delta);
unsigned steel_drums_get(unsigned slot, unsigned parameter);
uint16_t steel_drums_steps(unsigned slot);
unsigned steel_drums_max(unsigned parameter);
const char *steel_drums_param_name(unsigned parameter);
void steel_drums_param_text(unsigned slot, unsigned parameter, char out[12]);
void steel_drums_slot_name(unsigned slot, char out[12]);
void steel_drums_transport(int run);             /* 1 start, 0 stop, -1 toggle */
/* A completely new random microsound beat (all slots; A stays the hi-hat).
 * Density is kept in the keins range (about 4-14 hits per second). */
void steel_drums_randomize(uint32_t seed, unsigned bpm);
unsigned steel_drums_running(void);
unsigned steel_drums_step(void);                 /* current step 0..15 */
uint32_t steel_drums_activity(void);             /* bit per slot: hit in the last ~100 ms */
unsigned steel_drums_level(void);
void steel_drums_level_edit(int delta);
unsigned steel_drums_hiss(void);                 /* hiss bed 0..99 */
void steel_drums_hiss_edit(int delta);
/* BLIP notes follow the synth: MIDI pitches of 8 high scale tones. */
void steel_drums_set_blips(const float midi[8]);
/* Audio owner: the current CUT depth for the synth layer (0 = none). */
FM1_AUDIO_CODE float steel_drums_cut(void);

/* Audio owner: render one block. Dry stereo is added to dry_l/dry_r, sends
 * (mono) to tape/reverb. BPM drives the sequencer. */
FM1_AUDIO_CODE void steel_drums_render(float *dry_l, float *dry_r, float *tape, float *reverb, float bpm);
#endif
