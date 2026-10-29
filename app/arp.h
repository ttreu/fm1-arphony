/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* ARPHONY: the FM-1 as a modal arpeggiator (repeating single-note riffs
 * over slowly moving modal chords).
 *
 * Two layers play together:
 *   ARP    single notes, a repeating pattern over the current chord's tones
 *          (add9 / maj7 / m9 / 6 colours), in the upper register;
 *   HARM   the harmony below: a bass note plus up to two chord tones
 *          (at most three notes), held or pulsing.
 * Chords come from a progression (AUTO) or from the keys (HAND). In LEAD
 * mode the riff keeps its shape and only moves to the nearest chord tones,
 * so tones the chords share stay put while the bass moves underneath. */
#ifndef FM1_ARP_H
#define FM1_ARP_H
#include "fm1.h"

/* This firmware is the ARPHONY instrument only. */
#define FM1_ARP 1

/* Editable settings. All values are 0..max integers. */
enum arp_param {
    /* Harmony and riff */
    AP_KEY, AP_MODE, AP_PROG, AP_CHORD_LEN, AP_COLOR,
    AP_PATTERN, AP_RATE, AP_HARMONY, AP_ARP_OCT, AP_GATE, AP_SWING, AP_ACCENT, AP_MUTATE,
    AP_BASS_RHY, AP_NOTES, AP_BASS_OCT, AP_BASS_LVL, AP_CHORD_LVL, AP_HARM_FX,
    /* ARP sound */
    AP_A_SHAPE, AP_A_DETUNE, AP_A_OSC2, AP_A_FM, AP_A_RATIO, AP_A_TONE, AP_A_TONE_ENV,
    AP_A_ATTACK, AP_A_DECAY, AP_A_SUSTAIN, AP_A_RELEASE, AP_A_LEVEL,
    /* HARM sound */
    AP_H_SHAPE, AP_H_DETUNE, AP_H_OSC2, AP_H_SUB, AP_H_TONE, AP_H_TONE_ENV,
    AP_H_ATTACK, AP_H_DECAY, AP_H_SUSTAIN, AP_H_RELEASE,
    /* Shared voice motion */
    AP_DRIFT, AP_SPREAD, AP_VIB_RATE, AP_VIB_DEPTH,
    /* Effects (GLIDER's tape echo, ladder filter, delay, shimmer reverb) */
    AP_FILTER, AP_RESO, AP_DRIVE, AP_LFO_RATE, AP_LFO_DEPTH,
    AP_DELAY_TIME, AP_DELAY_FEEDBACK, AP_DELAY_MIX,
    AP_TAPE_TIME, AP_TAPE_FEEDBACK, AP_TAPE_WOW, AP_TAPE_TONE, AP_TAPE_MIX,
    AP_TAPE_SYNC, AP_DELAY_SYNC, AP_LFO_SYNC,
    AP_REV_SIZE, AP_REV_DAMP, AP_SHIMMER, AP_REV_MIX,
    AP_BPM, AP_LEVEL,
    /* TWIN: a second riff layer above (octave doubling or sparse high pings) */
    AP_TWIN, AP_TWIN_LVL,
    /* LEAD: a ready sound for the ARP layer (SINE LONG ... CLASSIC) */
    AP_LEAD,
    AP_MIDI_OUT,  /* riff, bass and chord notes to MIDI channels 1, 2, 3 */
    AP_COUNT
};
struct arp_param_info { const char *name; uint8_t max; };
const struct arp_param_info *arp_param_info(unsigned parameter);
/* Display text for a value (note names, modes, note values, ms, ...). */
void arp_param_text(unsigned parameter, unsigned value, char out[16]);

struct arp_preset {
    char name[13];
    uint8_t value[AP_COUNT];
};
unsigned arp_preset_count(void);           /* factory presets; user presets follow */
enum { ARP_USER_SLOTS = 16 };
unsigned arp_user_stored(unsigned slot);  /* 0..15 */
int arp_user_save(unsigned slot);         /* 0 = saved */
int arp_user_load(unsigned slot);         /* 0 = loaded */
int arp_user_slot(void);                  /* the user preset playing, -1 = a factory preset */
const char *arp_preset_name(unsigned index);

/* Control task. */
void arp_init(void);
void arp_load(unsigned preset);
unsigned arp_preset_index(void);
const struct arp_preset *arp_current(void);
unsigned arp_get(unsigned parameter);
void arp_edit(unsigned parameter, int delta);
/* Key selector: the key's tonic becomes pitch class pc (0 = C .. 11 = B). */
void arp_set_key(unsigned pc);

/* Transport. Start/stop (toggle when `run` < 0). Starting begins at the
 * first chord of the progression (AUTO) or at the held chord (HAND). */
void arp_transport(int run);
unsigned arp_running(void);
/* HAND mode: chord on pitch class pc (0 = C). `pedal`: the bass stays on
 * the key's tonic (a slash chord over a pedal point). The change happens on
 * the next beat; a stopped arpeggio starts. */
void arp_hand_chord(unsigned pc, unsigned pedal);
/* Stay on the chord that is playing (HAND), or back to the progression (AUTO). */
void arp_hold(void);
/* RANDOM riff: a new riff from rules (motif, repeat, move, closing figure);
 * RIFF switches to RANDOM. */
void arp_random_riff(uint32_t seed);
unsigned arp_random_rejected(void);
/* RANDOM chords: a new progression in MODE (CHORDS switches to RANDOM). */
void arp_random_chords(uint32_t seed);     /* riffs thrown away as too close to the references */
void arp_auto(void);
unsigned arp_is_hand(void);

/* Display: the chord the sequencer is on, its name ("DM9", "G/D"), the
 * progression's chord names and the sequencer position. */
unsigned arp_chord_position(void);      /* index into the progression (HAND: 0) */
unsigned arp_chord_count(void);
void arp_chord_name(unsigned index, char out[16]);
unsigned arp_step_position(void);       /* riff step 0..15 */
unsigned arp_chord_ms_left(void);       /* until the next chord; ~0 = none coming */
unsigned arp_chord_shade(unsigned index); /* display colour: root pitch class, +16 for minor */
unsigned arp_pattern_length(void);
int arp_pattern_step(unsigned step);    /* ladder index, -1 rest, -2 tie */
uint16_t arp_chord_pcs(unsigned index); /* pitch classes of chord `index` (bit 0 = C) */
unsigned arp_riff_gated(void);         /* riff voices with an open gate (tests) */
uint32_t arp_note_count(void);          /* riff notes started so far */
int arp_last_note(unsigned layer);      /* last MIDI note started: 0 ARP, 1 bass, 2 TWIN */

/* Tempo: BPM = 40 + value. Tap from the control task; MIDI clock bytes from
 * the MIDI input path (0xF8 ticks are timed, 0xFA/0xFB start, 0xFC stops). */
unsigned arp_bpm(void);
unsigned arp_bpm_external(void);
void arp_tap(uint32_t now_ms);
void arp_midi_clock(uint8_t status, uint32_t now_ms);
/* MIDI input context: queue a channel message. Note-ons choose HAND chords
 * like the keys (F3..E4 and below: own bass, F4 and above: over the pedal);
 * CC 123 stops. */
void arp_midi_message(const uint8_t *message, size_t bytes);
void arp_poll(uint32_t now_ms);         /* control task: tempo, queued MIDI */

/* CPU guard (control task, about every 500 ms). Under load the ARP uses
 * fewer overlapping voices (shorter tails); returns 1 when that changed. */
unsigned arp_guard(unsigned load, unsigned overrun, uint32_t now_ms);
unsigned arp_voice_limit(void);

/* MIDI out (app/midi_router.c, audio owner): note on (velocity > 0) or off.
 * Channels: 1 riff and TWIN, 2 bass, 3 chord tones. */
FM1_AUDIO_CODE void arp_midi_note(unsigned channel, unsigned note, unsigned velocity);

/* Audio owner: add one stereo Q23 block (FM1_BLOCK_FRAMES frames). */
FM1_AUDIO_CODE void arp_render(int32_t *output);
#endif
