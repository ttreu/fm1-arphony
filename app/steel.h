/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLIDER: the FM-1 as a latching drone / steel-guitar instrument.
 *
 * White keys latch scale tones (press = on, press again = off). Each black
 * key glides the white key(s) it owns by 1-4 scale steps and back. The
 * keyboard is tuned to the preset's key and scale, in equal or just
 * intonation, so every tone and every glide target stays in the scale. */
#ifndef FM1_STEEL_H
#define FM1_STEEL_H
#include "fm1.h"

/* This firmware is the GLIDER instrument only. */
#define FM1_STEEL 1

enum { STEEL_WHITE = 16, STEEL_BLACK = 11, STEEL_VOICES = STEEL_WHITE };

/* Editable sound and play settings. All values are 0..max integers. */
enum steel_param {
    /* PLAY */
    SP_ROOT, SP_SCALE, SP_TUNING, SP_GLIDE_STEPS, SP_GLIDE_TIME, SP_GLIDE_CURVE,
    /* SOUND */
    SP_SHAPE, SP_DETUNE, SP_OSC2, SP_SUB, SP_TONE, SP_TONE_ENV, SP_DRIFT, SP_SPREAD,
    /* ENVELOPE */
    SP_ATTACK, SP_DECAY, SP_SUSTAIN, SP_RELEASE,
    /* VIBRATO */
    SP_VIB_RATE, SP_VIB_DEPTH, SP_VIB_DELAY, SP_LEVEL,
    /* EFFECTS (filter, delay, tape echo) */
    SP_FILTER, SP_RESO, SP_DRIVE, SP_LFO_RATE, SP_LFO_DEPTH,
    SP_DELAY_TIME, SP_DELAY_FEEDBACK, SP_DELAY_MIX,
    SP_TAPE_TIME, SP_TAPE_FEEDBACK, SP_TAPE_WOW, SP_TAPE_TONE, SP_TAPE_MIX,
    /* TEMPO SYNC (off/on: TIME/RATE knobs then select note values) */
    SP_TAPE_SYNC, SP_DELAY_SYNC, SP_LFO_SYNC,
    /* SHIMMER REVERB */
    SP_REV_SIZE, SP_REV_DAMP, SP_SHIMMER, SP_REV_MIX,
    /* Tempo is global: loading a preset keeps the running BPM. */
    SP_BPM,
    /* OVERTONES: pure sine harmonics 2..5 per tone; detune moved by a slow LFO */
    SP_OVERTONES, SP_OVERTONE_TILT, SP_DETUNE_LFO,
    /* FLOAT: undertones (octave and twelfth below) and slow independent motion
     * of every partial's level and pitch, so even one note hovers */
    SP_UNDERTONES, SP_FLOAT, SP_FLOAT_RATE,
    /* INTERVAL: oscillator 2 plays an interval above or below oscillator 1,
     * either in the key (it follows the scale and glides along) or pure */
    SP_INTERVAL, SP_IV_MODE,
    SP_COUNT
};
struct steel_param_info { const char *name; uint8_t max; };
const struct steel_param_info *steel_param_info(unsigned parameter);
/* Display text for a value (note names, scale names, milliseconds, ...). */
void steel_param_text(unsigned parameter, unsigned value, char out[12]);

struct steel_preset {
    char name[13];
    uint8_t value[SP_COUNT];
};
enum { STEEL_PRESETS = 22 };
const struct steel_preset *steel_factory(unsigned index);

/* Control task. */
void steel_init(void);
void steel_load(unsigned preset);
/* Key selector: set the key's root to pitch class pc (0 = C .. 11 = B). */
void steel_set_key(unsigned pc);
unsigned steel_preset_index(void);
const struct steel_preset *steel_current(void);
unsigned steel_get(unsigned parameter);
void steel_edit(unsigned parameter, int delta);
void steel_white(unsigned white);           /* toggle a white key */
/* Black key click: once = glide up, twice quickly = glide down, the next
 * click = back home. Single clicks take effect after 350 ms (steel_poll). */
void steel_black(unsigned black, uint32_t now_ms);
void steel_all_off(void);
/* Tempo: BPM = 40 + value. Tap from the control task; MIDI clock bytes from
 * the MIDI input path (0xF8 ticks are timed; 0xFC or 2 s silence ends sync). */
unsigned steel_bpm(void);
unsigned steel_bpm_external(void);
void steel_tap(uint32_t now_ms);
void steel_midi_clock(uint8_t status, uint32_t now_ms);
void steel_poll(uint32_t now_ms); /* control task: adopt external tempo, play queued MIDI */
/* MIDI input context: queue a channel message. Channels 1-9 and 11-16 play
 * the keyboard (F3..G5, other octaves folded in; notes held, not latched),
 * channel 10 plays the drum slots from C1 upwards. */
void steel_midi_message(const uint8_t *message, size_t bytes);
/* OCT -/+: shift the keyboard (-2..+2 octaves); returns the new octave.
 * Tones already sounding keep their pitch. */
int steel_transpose(int octaves);
int steel_octave(void);
uint16_t steel_white_state(void);           /* latched white keys in the current octave */
uint16_t steel_black_state(void);           /* engaged black keys */
uint16_t steel_white_glided(void);          /* white keys currently off their home pitch */
const char *steel_scale_name(unsigned scale);
/* Black key b owns white keys first..first+count-1 (count 0: none). */
void steel_black_owner(unsigned black, unsigned *first, unsigned *count);
/* Physical key 0..26 -> white index 0..15, black index 16+0..10. */
unsigned steel_key_role(unsigned key);

/* CPU guard (control task, about every 500 ms): `load` is the audio load in
 * percent, `overrun` is set when audio blocks were missed. Under load,
 * release tails fade quickly first, then the oldest tone gives way and the
 * tone limit drops; it rises again after 4 s of calm. Returns 1 when the
 * limit changed. */
unsigned steel_guard(unsigned load, unsigned overrun, uint32_t now_ms);
unsigned steel_tone_limit(void);

/* Audio owner: add one stereo Q23 block (FM1_BLOCK_FRAMES frames). */
FM1_AUDIO_CODE void steel_render(int32_t *output);
#endif
