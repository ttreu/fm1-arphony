/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Private GLITCH DRUMS state shared by the control task and the audio owner. */
#ifndef FM1_STEEL_DRUMS_INTERNAL_H
#define FM1_STEEL_DRUMS_INTERNAL_H
#include "steel_drums.h"
#include "steel_internal.h"

/* Slot settings: control writes single bytes/words; audio reads them. */
extern struct drum_slot drum_slots[DRUM_SLOTS];
extern volatile uint8_t drum_run, drum_step_now;
extern volatile uint32_t drum_activity_bits;
extern volatile float drum_level_gain, drum_hiss_gain;
extern volatile float drum_blip_hz[8];
/* Seed of the scatter positions: they repeat every bar until it changes. */
extern volatile uint32_t drum_pattern_seed;
/* Auditions from control: one producer, the audio owner consumes. */
struct drum_queue { volatile unsigned read, write; uint8_t slot[16]; };
extern struct drum_queue drum_auditions;
#endif
