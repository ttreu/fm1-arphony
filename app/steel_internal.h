/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Private GLIDER state shared by the control task and the audio owner. */
#ifndef FM1_STEEL_INTERNAL_H
#define FM1_STEEL_INTERNAL_H
#include "steel.h"

#ifdef FM1_HOST_TEST
#define STEEL_BARRIER() __asm__ volatile("" ::: "memory")
#else
#define STEEL_BARRIER() __asm__ volatile("csync" ::: "memory")
#endif
#define STEEL_INLINE static inline __attribute__((always_inline))
#define STEEL_RATE 44100.0f

/* Derived sound settings, written by control into the inactive slot. */
struct steel_patch {
    float shape;                     /* 0 sine, 1 triangle, 2 saw, 3 square */
    float detune, osc2, sub;         /* osc2 detune in cents, osc2 level, sub level */
    float detune_lfo;                /* extra cents swept by the slow detune LFO */
    float overtone[4];               /* levels of harmonics 2..5 (0 = off) */
    float undertone[2];              /* levels of 1/2 and 1/3 of the pitch */
    float float_depth, float_rate;   /* partial motion 0..1, base rate in Hz */
    float tone, tone_env;            /* voice low-pass in semitones */
    float drift, spread;             /* drift depth (semitones), stereo width */
    float attack, decay, sustain, release; /* per-sample coefficients */
    float vib_rate, vib_depth, vib_delay;  /* Hz, semitones, per-sample ramp */
    float glide_step, level;         /* glide progress per sample, output gain */
    unsigned glide_curve;            /* 0 linear, 1 smooth, 2 bar (ease out) */
    /* Effects: tape echo -> ladder filter -> ping-pong delay. */
    float cutoff, resonance, drive;  /* semitones, feedback 0..~3.9, input gain */
    float lfo_step, lfo_depth;       /* LFO cycles per block, semitones */
    unsigned filter_on;
    float tape_time, tape_feedback, tape_wow, tape_tone, tape_mix; /* time in samples */
    float delay_time, delay_feedback, delay_mix;                     /* time in samples */
    float rev_decay, rev_damp, rev_shimmer, rev_mix, rev_dry;        /* shimmer plate; dry gain */
    float bpm;
};
/* Voice commands: control writes target/gate, then bumps the serials. */
struct steel_command {
    volatile float target;           /* MIDI pitch in semitones */
    volatile float target2;          /* oscillator 2 (the interval voice) */
    volatile float pan;              /* -0.5 (left) .. 0.5, by key */
    volatile uint8_t gate, glide_serial, note_serial;
    volatile uint8_t fade;           /* 1: release within ~30 ms (CPU guard) */
};
extern struct steel_patch steel_patches[2];
extern volatile uint8_t steel_patch_index;
extern struct steel_command steel_commands[STEEL_VOICES];
/* Voices still sounding a release tail (read by the control task). */
uint16_t steel_ringing(void);
/* Effects on the stereo voice bus, in place (audio owner). */
/* `send` (mono, may be NULL) feeds the tape echo / the reverb in addition to
 * the bus: the drum track's per-slot sends. */
FM1_AUDIO_CODE void steel_fx(float *left, float *right, const struct steel_patch *p, const float *send);
/* Same, plus `post_l/post_r` (may be NULL) added after the ladder filter:
 * the voices' over- and undertones, which the filter must not darken. */
FM1_AUDIO_CODE void steel_fx_post(float *left, float *right, const struct steel_patch *p, const float *send,
                                  const float *post_l, const float *post_r);
FM1_AUDIO_CODE void steel_reverb(float *left, float *right, const struct steel_patch *p, const float *send);
enum { STEEL_FX_DECIMATE = 4, STEEL_DELAY_LINE = 16384, STEEL_TAPE_LINE = 8192 };
#define STEEL_DELAY_MAX_S ((float)(STEEL_DELAY_LINE - 64) * STEEL_FX_DECIMATE / STEEL_RATE)
#define STEEL_TAPE_MAX_S ((float)(STEEL_TAPE_LINE - 512) * STEEL_FX_DECIMATE / STEEL_RATE)

/* 2^x without libm; safe for SRAM audio code. The fraction is reduced to
 * -0.5..0.5, where a 6th-order series keeps the relative error below 3e-7,
 * so 1 - 2^-tiny (envelope coefficients) stays accurate too. */
STEEL_INLINE float steel_exp2(float x)
{
    if (x < -60.0f) return 0.0f;
    if (x > 60.0f) x = 60.0f;
    int i = (int)(x < 0.0f ? x - 0.5f : x + 0.5f);
    float f = (x - (float)i) * 0.69314718f;
    float p = 1.0f + f * (1.0f + f * (0.5f + f * (0.16666667f + f * (0.04166667f + f * (0.00833333f + f * 0.00138889f)))));
    union { float f; int32_t i; } u = {p};
    u.i += i * (1 << 23);
    return u.f;
}
STEEL_INLINE float steel_clamp(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
#endif
