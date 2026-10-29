/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* ARPHONY audio path: the tick sequencer and twelve voices.
 * Runs in the audio owner from SRAM: no libc, no integer division by
 * variables (that would call Flash helpers), only counters and compares. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "arp_internal.h"

enum { BLOCK = FM1_BLOCK_FRAMES };

struct arp_patch arp_patches[2];
struct arp_song arp_songs[2];
volatile uint8_t arp_patch_index, arp_song_index;
volatile uint8_t arp_run, arp_jump_serial, arp_restart_serial;
volatile uint8_t arp_now_chord, arp_now_step, arp_now_running;
volatile uint16_t arp_now_rel;            /* ticks since the chord started (display) */
volatile int8_t arp_now_note[3] = {-1, -1, -1};
volatile uint32_t arp_now_count;

struct voice {
    float pitch, p1, p2, ps, pm, ic1, ic2, env, gain, pan, drift, drift_to;
    int32_t gate_left;             /* samples until the gate closes (<= 0: closed) */
    int8_t midi;                   /* MIDI note sounding on the output, -1 none */
    uint32_t random;
    uint8_t stage, layer;          /* stage: 0 off, 1 attack, 2 decay/sustain, 3 release */
};
static struct voice voices[ARP_ALL_VOICES];
static uint8_t voices_ready;
static float left[BLOCK], right[BLOCK], harm_l[BLOCK], harm_r[BLOCK], send[BLOCK], vib_phase;
static unsigned blocks;

/* ------------------------------------------------------------ sequencer */
static struct {
    uint8_t running, restart_seen, jump_seen, jump_pending, chord, rr, bass_rr, chord_set, last_arp;
    uint32_t tick;                 /* next tick to play */
    float position;                /* ticks played so far, fractional */
    uint16_t rel, next_step, step; /* ticks since the chord started; next riff step; steps played */
    uint8_t beat, sub, slot;       /* tick in the beat (0..95), in the 16th (0..23); 16th in the bar */
    uint32_t random;
} seq = {.random = 0x2468aceU};

STEEL_INLINE float sine(float t) /* sin(2 pi t) for t in [0,1) */
{
    float x = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
    x *= 1.5707963f;
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666667f - x2 * (0.0083333f - x2 * 0.000198f)));
}
STEEL_INLINE float wrap(float t) { return t >= 1.0f ? t - 1.0f : t; }
STEEL_INLINE float wrap_any(float t)
{
    t -= (float)(int)t;
    return t < 0.0f ? t + 1.0f : t;
}
static uint8_t midi_on;            /* MIDI out enabled (from the song) */
STEEL_INLINE unsigned midi_channel(unsigned index) { return index < BASS_VOICE ? 0U : index < CHORD_VOICE ? 1U : 2U; }
static FM1_AUDIO_CODE void midi_off(unsigned index)
{
    struct voice *v = &voices[index];
    if (v->midi >= 0) { arp_midi_note(midi_channel(index), (unsigned)v->midi, 0); v->midi = -1; }
}
static FM1_AUDIO_CODE void voice_start(unsigned index, unsigned layer, float pitch, float gain, float pan, float gate_samples)
{
    struct voice *v = &voices[index];
    midi_off(index);
    if (midi_on) {
        int note = (int)(pitch + 0.5f), vel = (int)(40.0f + 87.0f * steel_clamp(gain, 0.0f, 1.0f));
        if (note >= 0 && note < 128) { v->midi = (int8_t)note; arp_midi_note(midi_channel(index), (unsigned)note, (unsigned)vel); }
    }
    if (v->stage == 0 || v->env < 0.001f) {
        v->p1 = 0.0f; v->p2 = 0.31f; v->ps = 0.0f; v->pm = 0.0f; v->env = 0.0f;
    }
    v->layer = (uint8_t)layer;
    v->pitch = pitch; v->gain = gain; v->pan = pan;
    v->gate_left = (int32_t)gate_samples;
    if (v->gate_left < BLOCK) v->gate_left = BLOCK;
    v->stage = 1;
}
static FM1_AUDIO_CODE void release_all(void)
{
    for (unsigned i = 0; i < ARP_ALL_VOICES; ++i) {
        if (voices[i].stage == 1 || voices[i].stage == 2) { voices[i].stage = 3; voices[i].gate_left = 0; }
        midi_off(i);
    }
}
STEEL_INLINE uint32_t next_random(void)
{
    seq.random = seq.random * 1664525U + 1013904223U;
    return seq.random;
}
/* Ticks until the next bass hit after 16th-slot `slot` (at most a bar). */
static FM1_AUDIO_CODE unsigned bass_span(const struct arp_song *s, unsigned slot)
{
    unsigned n = 1;
    for (unsigned k = slot + 1U; n < 16U; ++k, ++n) {
        if (k >= 16U) k = 0;
        if (s->bass_rhythm[k]) break;
    }
    return n * (ARP_PPQN / 4);
}
static FM1_AUDIO_CODE void bass_hit(const struct arp_song *s, unsigned code, float ticks, float samples_per_tick, const struct arp_patch *p)
{
    unsigned c = seq.chord;
    float pitch = s->harm[c][0];
    if (code == 2) pitch += 12.0f;
    else if (code == 3) pitch = s->fifth[c];
    seq.bass_rr ^= 1U;
    voice_start(BASS_VOICE + seq.bass_rr, 1, pitch, p->bass_level, 0.0f, ticks * samples_per_tick);
    arp_now_note[1] = (int8_t)(pitch + 0.5f);
}
static FM1_AUDIO_CODE void chord_change(const struct arp_song *s, float samples_per_tick, const struct arp_patch *p)
{
    unsigned c = seq.chord;
    seq.rel = 0;
    seq.next_step = 0;
    seq.step = 0;
    arp_now_chord = (uint8_t)c;
    float hold = s->chord_ticks == ARP_HOLD ? 1e9f / samples_per_tick : (float)s->chord_ticks * 0.97f;
    /* Upper chord tones ring for the whole chord; the old set rings out. */
    seq.chord_set ^= 1U;
    for (unsigned k = 1; k < 3; ++k) {
        if (k >= s->notes) break;
        float pan = (k == 1 ? -0.35f : 0.35f) * p->spread;
        voice_start(CHORD_VOICE + seq.chord_set * 2U + (k - 1U), 1, s->harm[c][k], p->chord_level, pan, hold * samples_per_tick);
    }
    /* The bass always marks a new chord; it lasts until its next hit. */
    unsigned slot = seq.slot;
    unsigned any = 0;
    for (unsigned k = 0; k < 16; ++k) any |= s->bass_rhythm[k];
    float span = any ? (float)bass_span(s, slot) * 0.85f : hold;
    if (span > hold) span = hold;
    bass_hit(s, s->bass_rhythm[slot] ? s->bass_rhythm[slot] : 1U, span, samples_per_tick, p);
}
static FM1_AUDIO_CODE void arp_note(float pitch, float gain, float pan, float gate_samples, const struct arp_song *s)
{
    unsigned limit = s->voices >= 2U && s->voices <= ARP_VOICES ? s->voices : ARP_VOICES;
    if (++seq.rr >= limit) seq.rr = 0;
    voice_start(seq.rr, 0, pitch, gain, pan, gate_samples);
}
static FM1_AUDIO_CODE void riff_step(const struct arp_song *s, float samples_per_tick, const struct arp_patch *p)
{
    unsigned len = s->pattern_len ? s->pattern_len : 1U;
    unsigned i = seq.step;
    while (i >= len) i -= len;                         /* no division helper */
    int v = s->pattern[i];
    arp_now_step = (uint8_t)i;
    float step_samples = (float)s->step_ticks * samples_per_tick;
    float accent = seq.beat == 0 ? 1.0f : 1.0f - 0.55f * s->accent;
    /* TWIN PINGS: a second, sparse riff high above, on its own 16-step cycle. */
    if (s->twin_mode == 2 && s->twin_len) {
        unsigned k = seq.step;
        while (k >= s->twin_len) k -= s->twin_len;
        int t = s->twin[k];
        if (t >= 0 && t < ARP_LADDER) {
            arp_now_note[2] = (int8_t)(s->ladder[seq.chord][t] + 0.5f);
            arp_note(s->ladder[seq.chord][t], s->twin_gain * p->arp_level, 0.45f * p->spread,
                     step_samples * (s->gate > 0.6f ? s->gate : 0.6f) * 2.0f, s);
        }
    }
    /* No silent gaps: a left-out step (rest or tie) holds the note before it,
     * so it is skipped here; the note was started long enough (below). */
    if (v < 0) return;
    if (s->mutate > 0.0f && (float)(next_random() >> 8) * (1.0f / 16777216.0f) < s->mutate) {
        v += (next_random() & 0x100U) ? 1 : -1;
        if (v < 0) v = 1;
        if (v >= ARP_LADDER) v = ARP_LADDER - 2;
    }
    float pitch = s->ladder[seq.chord][v];
    /* Higher riff notes lean right, lower ones left. */
    float pan = ((float)(v % 5) - 2.0f) * 0.2f * p->spread;
    /* Hold through the left-out steps that follow, up to the next note or the
     * next chord (where the riff starts again). */
    unsigned held = 0;
    for (unsigned k = 1; k < len; ++k) {
        unsigned j = i + k;
        if (j >= len) j -= len;
        if (s->pattern[j] >= 0) break;
        if (s->chord_ticks != ARP_HOLD && (unsigned)(seq.step + k) * s->step_ticks >= s->chord_ticks) break;
        ++held;
    }
    float length = held ? step_samples * (float)(held + 1U) : step_samples * s->gate; /* held: right up to the next note */
    arp_note(pitch, accent * p->arp_level, pan, length, s);
    seq.last_arp = seq.rr;
    arp_now_note[0] = (int8_t)(pitch + 0.5f);
    ++arp_now_count;
    /* TWIN OCTAVE: the same note an octave up, softer. */
    if (s->twin_mode == 1) arp_now_note[2] = (int8_t)(pitch + 12.5f);
    if (s->twin_mode == 1) arp_note(pitch + 12.0f, accent * s->twin_gain * p->arp_level, -pan, length, s);
}
static FM1_AUDIO_CODE void play_tick(const struct arp_song *s, float samples_per_tick, const struct arp_patch *p)
{
    unsigned changed = 0;
    if (seq.jump_pending && seq.beat == 0) {          /* start, HAND chord: on the beat */
        seq.jump_pending = 0;
        seq.chord = s->jump_chord;
        changed = 1;
    } else if (s->chord_ticks != ARP_HOLD && seq.rel >= s->chord_ticks) {
        seq.chord = (uint8_t)(seq.chord + 1U);
        if (seq.chord >= s->chords) seq.chord = 0;
        changed = 1;
    }
    if (seq.chord >= s->chords) { seq.chord = 0; arp_now_chord = 0; }
    if (changed) chord_change(s, samples_per_tick, p);
    else if (seq.sub == 0 && s->bass_rhythm[seq.slot]) {
        float span = (float)bass_span(s, seq.slot) * 0.85f;
        float rest = s->chord_ticks == ARP_HOLD ? span : (float)s->chord_ticks - (float)seq.rel;
        bass_hit(s, s->bass_rhythm[seq.slot], span < rest ? span : rest, samples_per_tick, p);
    }
    if (seq.rel == seq.next_step) {
        riff_step(s, samples_per_tick, p);
        seq.step = (uint16_t)(seq.step + 1U);
        /* Odd steps come late by SWING. */
        seq.next_step = (uint16_t)(seq.step * s->step_ticks + ((seq.step & 1U) ? s->swing_ticks : 0U));
    }
    if (seq.rel < 0xfffeU) ++seq.rel;
    arp_now_rel = seq.rel;
    if (++seq.beat >= ARP_PPQN) seq.beat = 0;
    if (++seq.sub >= ARP_PPQN / 4) {
        seq.sub = 0;
        if (++seq.slot >= 16U) seq.slot = 0;
    }
}
static FM1_AUDIO_CODE void sequencer(const struct arp_patch *p)
{
    const struct arp_song *s = &arp_songs[arp_song_index];
    if (midi_on && !s->midi_out) for (unsigned i = 0; i < ARP_ALL_VOICES; ++i) midi_off(i); /* switched off: no hanging notes */
    midi_on = s->midi_out;
    if (arp_restart_serial != seq.restart_seen) { seq.restart_seen = arp_restart_serial; seq.running = 0; }
    if (arp_run && !seq.running) {
        seq.running = 1; seq.tick = 0; seq.position = 0.0f; seq.beat = seq.sub = seq.slot = 0;
        seq.rel = 0; seq.jump_pending = 1; seq.jump_seen = arp_jump_serial; /* the first tick starts the chord */
    } else if (!arp_run && seq.running) {
        seq.running = 0;
        release_all();
    }
    arp_now_running = seq.running;
    if (!seq.running) return;
    if (arp_jump_serial != seq.jump_seen) { seq.jump_seen = arp_jump_serial; seq.jump_pending = 1; }
    float ticks_per_sample = p->fx.bpm * (float)ARP_PPQN / (60.0f * STEEL_RATE);
    float samples_per_tick = 1.0f / ticks_per_sample;
    while ((float)seq.tick <= seq.position) {
        play_tick(s, samples_per_tick, p);
        ++seq.tick;
        if (seq.tick >= 0x40000000U) { seq.tick -= 0x3fffff00U; seq.position -= (float)0x3fffff00U; }
    }
    seq.position += (float)BLOCK * ticks_per_sample;
}

/* ------------------------------------------------------------ voices */
STEEL_INLINE float saw(float t, float dt, float inv)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) { float x = t * inv; y -= x + x - x * x - 1.0f; }
    else if (t > 1.0f - dt) { float x = (t - 1.0f) * inv; y -= x * x + x + x + 1.0f; }
    return y;
}
STEEL_INLINE float wave(unsigned shape, float t, float dt, float inv)
{
    switch (shape) {
    case 0: return sine(t);
    case 1: return t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
    case 2: return saw(t, dt, inv);
    default: return 0.5f * (saw(t, dt, inv) - saw(wrap(t + 0.5f), dt, inv));
    }
}
STEEL_INLINE float oscillator(float t, float dt, float inv, unsigned shape, float blend)
{
    float y = wave(shape, t, dt, inv);
    return blend > 0.0f ? y + blend * (wave(shape + 1U, t, dt, inv) - y) : y;
}
static FM1_AUDIO_CODE void render_voice(struct voice *v, unsigned index, const struct arp_patch *p, float vib)
{
    const struct arp_layer *L = &p->layer[v->layer];
    if (((blocks + index * 7U) & 63U) == 0) {     /* analog drift: slow random walk */
        v->random = v->random * 1664525U + 1013904223U + index;
        v->drift_to = (float)(int32_t)v->random * (1.0f / 2147483648.0f);
    }
    v->drift += (v->drift_to - v->drift) * 0.01f;
    float pitch = v->pitch + v->drift * p->drift + vib * p->vib_depth;
    float dt = steel_clamp(440.0f / STEEL_RATE * steel_exp2((pitch - 69.0f) * (1.0f / 12.0f)), 1e-5f, 0.45f);
    float dt2 = steel_clamp(dt * L->detune, 1e-5f, 0.45f), dts = dt * 0.5f, dtm = dt * L->fm_ratio;
    if (dtm > 0.45f) dtm = 0.0f;
    float inv = 1.0f / dt, inv2 = 1.0f / dt2;
    unsigned shape = (unsigned)L->shape;
    if (shape > 2U) shape = 2U;
    float blend = L->shape - (float)shape;
    float cutoff = L->tone + L->tone_env * v->env + 0.5f * (pitch - 60.0f);
    float hz = steel_clamp(440.0f * steel_exp2((cutoff - 69.0f) * (1.0f / 12.0f)), 30.0f, 16000.0f);
    float g = 3.14159265f / STEEL_RATE * hz;
    g = g * (15.0f - g * g) / (15.0f - 6.0f * g * g); /* tan prewarp */
    const float k = 1.4f, a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float osc2 = L->osc2, sub = 0.7f * L->sub, fm = L->fm;
    const float norm = 0.24f / (1.0f + osc2 + sub + fm);
    const float fm_index = L->fm_index;
    float pan = v->pan;
    float gl = norm * v->gain * (1.0f - pan), gr = norm * v->gain * (1.0f + pan);
    float *out_l = v->layer ? harm_l : left, *out_r = v->layer ? harm_r : right;
    float p1 = v->p1, p2 = v->p2, ps = v->ps, pm = v->pm, ic1 = v->ic1, ic2 = v->ic2, env = v->env;
    unsigned stage = v->stage;
    const float attack = L->attack, decay = L->decay, sustain = L->sustain, release = L->release;
    for (unsigned i = 0; i < BLOCK; ++i) {
        float x = oscillator(p1, dt, inv, shape, blend);
        if (osc2 > 0.0f) x += osc2 * oscillator(p2, dt2, inv2, shape, blend);
        if (sub > 0.0f) x += sub * (ps < 0.5f ? 4.0f * ps - 1.0f : 3.0f - 4.0f * ps);
        if (fm > 0.0f && dtm > 0.0f) {
            /* FM bell: a sine carrier whose brightness falls with the envelope. */
            x += fm * sine(wrap_any(p1 + fm_index * env * sine(pm)));
            pm = wrap(pm + dtm);
        }
        p1 = wrap(p1 + dt); p2 = wrap(p2 + dt2); ps = wrap(ps + dts);
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        if (stage == 1) { env += attack; if (env >= 1.0f) { env = 1.0f; stage = 2; } }
        else if (stage == 2) env += (sustain - env) * decay;
        else env -= env * release;
        float y = v2 * env;
        out_l[i] += y * gl;
        out_r[i] += y * gr;
    }
    if (stage == 3 && env < 1e-4f) stage = 0;
    if (!(ic1 > -1e4f && ic1 < 1e4f) || !(ic2 > -1e4f && ic2 < 1e4f)) ic1 = ic2 = 0.0f;
    v->p1 = p1; v->p2 = p2; v->ps = ps; v->pm = pm; v->ic1 = ic1; v->ic2 = ic2; v->env = env; v->stage = (uint8_t)stage;
}
STEEL_INLINE float saturate(float x)
{
    x = steel_clamp(x, -1.5f, 1.5f);
    return x - x * x * x * (4.0f / 27.0f);
}
FM1_AUDIO_CODE void arp_render(int32_t *output)
{
    const struct arp_patch *p = &arp_patches[arp_patch_index];
    if (!voices_ready) { for (unsigned i = 0; i < ARP_ALL_VOICES; ++i) voices[i].midi = -1; voices_ready = 1; }
    ++blocks;
    sequencer(p);
    vib_phase += p->vib_rate * BLOCK / STEEL_RATE;
    if (vib_phase >= 1.0f) vib_phase -= 1.0f;
    float vib = sine(vib_phase);
    volatile float *cl = left, *cr = right, *chl = harm_l, *chr = harm_r, *cs = send; /* no Flash memset */
    for (unsigned i = 0; i < BLOCK; ++i) cl[i] = cr[i] = chl[i] = chr[i] = cs[i] = 0.0f;
    for (unsigned i = 0; i < ARP_ALL_VOICES; ++i) {
        struct voice *v = &voices[i];
        if (v->gate_left > 0) {
            v->gate_left -= BLOCK;
            if (v->gate_left <= 0 && (v->stage == 1 || v->stage == 2)) v->stage = 3;
            if (v->gate_left <= 0) midi_off(i);
        }
        if (v->stage) render_voice(v, i, p, vib);
    }
    /* The harmony feeds the tape echo and the reverb by HARM FX; dry it stays
     * outside the filter and the delay, so the bass keeps its weight. */
    const float hfx = p->harm_fx;
    for (unsigned i = 0; i < BLOCK; ++i) send[i] = 0.5f * (harm_l[i] + harm_r[i]) * hfx;
    steel_fx_post(left, right, &p->fx, send, 0, 0);
    steel_reverb(left, right, &p->fx, send);
    const float level = p->level * 8388607.0f;
    for (unsigned i = 0; i < BLOCK; ++i) {
        output[2 * i] += (int32_t)(saturate(left[i] + harm_l[i]) * level);
        output[2 * i + 1] += (int32_t)(saturate(right[i] + harm_r[i]) * level);
    }
}
/* ARP voices with an open gate (tests: no silent gaps). */
unsigned arp_riff_gated(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < ARP_VOICES; ++i) n += (voices[i].stage == 1 || voices[i].stage == 2) && voices[i].layer == 0;
    return n;
}
/* Voices still sounding (CPU guard). */
unsigned arp_voices_sounding(void)
{
    unsigned n = 0;
    for (unsigned i = 0; i < ARP_ALL_VOICES; ++i) n += voices[i].stage != 0;
    return n;
}
