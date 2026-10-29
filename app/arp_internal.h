/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* Private ARPHONY state shared by the control task and the audio owner.
 *
 * The control task does all the music theory: for every chord of the
 * progression it computes the riff's pitch ladder and the harmony notes and
 * publishes them as a `song` (double buffered, like the sound `patch`).
 * The audio owner only counts ticks and starts and stops voices, so the
 * timing is exact to one audio block (2.9 ms) and needs no Flash code. */
#ifndef FM1_ARP_INTERNAL_H
#define FM1_ARP_INTERNAL_H
#include "arp.h"
#include "steel_internal.h"

enum {
    ARP_PPQN = 96,                 /* ticks per quarter note */
    ARP_BAR = 4 * ARP_PPQN,
    ARP_MAX_CHORDS = 8,
    ARP_LADDER = 24,               /* riff pitches per chord, low to high */
    ARP_PATTERN_MAX = 16,
    ARP_REST = -1, ARP_TIE = -2, ARP_HOLD = 0xffff,
    /* Voices: ARP 0..7 (round robin, so tails overlap; TWIN shares them), bass 8..9 (two, so a
     * pulsing bass never clicks), upper chord tones 10..13 (two sets of two:
     * the old chord rings out while the new one starts). */
    ARP_VOICES = 8, BASS_VOICE = 8, CHORD_VOICE = 10, ARP_ALL_VOICES = 14,
};

/* One layer's sound, derived from the parameters. */
struct arp_layer {
    float shape;                   /* 0 sine .. 1 triangle .. 2 saw .. 3 square */
    float detune, osc2, sub;       /* osc 2 detune (ratio), levels */
    float fm, fm_index, fm_ratio;  /* FM bell: level, modulation index, ratio */
    float tone, tone_env;          /* voice low-pass in semitones */
    float attack, decay, sustain, release; /* per-sample coefficients */
};
struct arp_patch {
    struct steel_patch fx;         /* effects and tempo */
    struct arp_layer layer[2];     /* 0 ARP, 1 HARM */
    float arp_level, bass_level, chord_level, harm_fx;
    float drift, spread, vib_rate, vib_depth;
    float level;                   /* master */
};
struct arp_song {
    float ladder[ARP_MAX_CHORDS][ARP_LADDER]; /* riff pitches (MIDI) per chord */
    float harm[ARP_MAX_CHORDS][3];            /* bass, upper 1, upper 2 */
    float fifth[ARP_MAX_CHORDS];              /* the bass's fifth (walking rhythms) */
    int8_t pattern[ARP_PATTERN_MAX];          /* ladder index, ARP_REST, ARP_TIE */
    uint8_t bass_rhythm[16];                  /* per 16th of a bar: 0 -, 1 root, 2 octave, 3 fifth */
    uint8_t pattern_len, chords, notes, step_ticks, swing_ticks, voices;
    int8_t twin[ARP_PATTERN_MAX];             /* TWIN PINGS: ladder index or ARP_REST */
    uint8_t twin_mode, twin_len;              /* 0 off, 1 octave, 2 pings */
    uint8_t midi_out;                         /* send the notes to MIDI */
    float twin_gain;
    uint8_t jump_chord;                       /* where a jump (start, HAND chord) lands */
    uint16_t chord_ticks;                     /* ARP_HOLD: stay on the chord (HAND) */
    float gate, accent, mutate;               /* gate 0..1 of a step, accent 0..1, mutate chance */
};
extern struct arp_patch arp_patches[2];
extern struct arp_song arp_songs[2];
extern volatile uint8_t arp_patch_index, arp_song_index;
/* Requests from the control task (serials: the audio owner acts once). */
extern volatile uint8_t arp_run;          /* 1 = play */
extern volatile uint8_t arp_jump_serial;  /* HAND chord: switch on the next beat */
extern volatile uint8_t arp_restart_serial; /* MIDI start: back to the first beat */
/* Read back by the control task. */
extern volatile uint8_t arp_now_chord, arp_now_step, arp_now_running;
extern volatile uint16_t arp_now_rel;
extern volatile int8_t arp_now_note[3];
extern volatile uint32_t arp_now_count;   /* riff notes started (tests) */
#endif
