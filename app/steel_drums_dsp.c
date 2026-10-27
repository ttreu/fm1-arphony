/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLITCH DRUMS audio path: a sample-accurate step sequencer with an event
 * queue (ratchets, micro-timing, random scatter) and twelve voices of
 * synthesized sounds: classic kick/snare/hat, glitch generators and eight
 * sounds from GLITCH. No samples. Audio owner, SRAM, no libc or Flash code.
 * No division or modulo by variables here: those may call Flash helpers. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "steel_drums_internal.h"

enum { BLOCK = FM1_BLOCK_FRAMES, EVENTS = 128 };

struct drum_slot drum_slots[DRUM_SLOTS];
volatile uint8_t drum_run, drum_step_now;
volatile uint32_t drum_activity_bits;
volatile float drum_level_gain = 0.6f, drum_hiss_gain;
volatile uint32_t drum_pattern_seed = 1U;
volatile float drum_blip_hz[8] = {1568.0f, 1760.0f, 1976.0f, 2093.0f, 2349.0f, 2637.0f, 3136.0f, 3520.0f};
static float cut_depth, cut_level, hiss_hp[2];
static int cut_hold;
static uint32_t hiss_noise = 0x1234567U;
struct drum_queue drum_auditions;

struct event { uint32_t time; uint8_t slot, velocity; };
static struct event events[EVENTS];
static unsigned event_count;
struct voice {
    uint8_t active, type, slot, crush;
    int delay;                       /* samples until the hit starts */
    float env, decay, amp, gl, gr, tape, reverb;
    float phase, freq, freq_end, sweep, tone_env, tone_decay;
    float lp, hp, y1, y2, a1, a2;    /* filter / resonator state */
    float lowcut;                    /* high-pass state for glitch slots */
    uint8_t highpass;
    uint32_t noise, seed;
    int length, count, repeats, hold, held_left;
    float held;
    /* The GLITCH sounds */
    float color, ratio, spacing, inv_length;
    uint32_t lfsr;
};
static struct voice voices[DRUM_VOICES];
static uint32_t now, random_state = 0x9e3779b9U;
static float tick_phase, bar_lfo;
static unsigned tick, was_running;   /* 1/64 ticks: 64 per bar */
static uint8_t pingpong[DRUM_SLOTS];

STEEL_INLINE uint32_t rnd(void) { random_state = random_state * 1664525U + 1013904223U; return random_state; }
STEEL_INLINE float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */
STEEL_INLINE unsigned pick(unsigned n) /* 0..n-1 without a division helper */
{
    unsigned i = (unsigned)(frand() * (float)n);
    return i < n ? i : n - 1U;
}
STEEL_INLINE float bipolar(uint32_t *s) { *s = *s * 1664525U + 1013904223U; return (float)(int32_t)*s * (1.0f / 2147483648.0f); }
STEEL_INLINE float sine(float t)
{
    float x = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t;
    x *= 1.5707963f;
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666667f - x2 * (0.0083333f - x2 * 0.000198f)));
}
STEEL_INLINE float wrap1(float t) { return t >= 1.0f ? t - (float)(int)t : t; }
STEEL_INLINE float soft(float x)
{
    x = steel_clamp(x, -1.5f, 1.5f);
    return x - x * x * x * (4.0f / 27.0f);
}
/* Per-sample multiplier reaching -60 dB after `seconds`. */
STEEL_INLINE float decay_for(float seconds) { return steel_exp2(-9.965784f / (seconds * STEEL_RATE)); }

static FM1_AUDIO_CODE void schedule(uint32_t time, unsigned slot, unsigned velocity)
{
    if (event_count >= EVENTS) return;
    events[event_count].time = time;
    events[event_count].slot = (uint8_t)slot;
    events[event_count].velocity = (uint8_t)velocity;
    ++event_count;
}
static FM1_AUDIO_CODE void trigger(unsigned slot, unsigned velocity, int delay)
{
    if (slot >= DRUM_SLOTS) return;
    const uint8_t *s = drum_slots[slot].value;
    unsigned source = s[DP_SOURCE];
    float level = (float)s[DP_LEVEL] * (1.0f / 99.0f);
    if (level <= 0.0f) return;
    /* Pick a voice: free first, otherwise the quietest. */
    struct voice *v = &voices[0];
    for (unsigned i = 0; i < DRUM_VOICES; ++i) {
        if (!voices[i].active) { v = &voices[i]; break; }
        if (voices[i].env * voices[i].amp < v->env * v->amp) v = &voices[i];
    }
    float random = (float)s[DP_RANDOM] * (1.0f / 99.0f);
    /* RANDOM varies each hit a little (up to +-3 semitones, +-12 % length),
     * so a pattern keeps its shape from bar to bar. */
    float pitch = steel_exp2(((float)s[DP_PITCH] - 24.0f + random * 3.0f * (2.0f * frand() - 1.0f)) * (1.0f / 12.0f));
    float length = 0.02f + 2.0f * ((float)s[DP_DECAY] * (1.0f / 99.0f)) * ((float)s[DP_DECAY] * (1.0f / 99.0f));
    length *= 1.0f + random * 0.12f * (2.0f * frand() - 1.0f);
    /* Pan per hit. */
    float amount = (float)s[DP_PAN] * (1.0f / 99.0f), pan = 0.0f;
    switch (s[DP_PAN_MODE]) {
    case PAN_FIXED: pan = 2.0f * amount - 1.0f; break;
    case PAN_RANDOM: pan = (rnd() & 0x100U ? 1.0f : -1.0f) * amount * (0.8f + 0.2f * frand()); break;
    case PAN_PINGPONG: pingpong[slot] ^= 1U; pan = (pingpong[slot] ? 1.0f : -1.0f) * amount; break;
    default: pan = sine(bar_lfo) * amount; break;
    }
    /* Loudness of the GLITCH sounds (A-weighted, measured in GLITCH): the
     * fine, short ones get more gain so they are not lost. */
    static const float loudness[DS_COUNT] = {
        [DS_KICK] = 1.0f, [DS_SNARE] = 1.0f, [DS_HAT] = 1.0f, [DS_IMPULSE] = 1.0f, [DS_TRAIN] = 1.0f,
        [DS_BLIP] = 1.0f, [DS_TICK] = 1.0f, [DS_CUT] = 1.0f, [DS_CRUNCH] = 2.2f, [DS_CHIRP] = 1.0f,
        [DS_SNAP] = 0.7f, [DS_DUST] = 2.5f, [DS_BITS] = 3.5f, [DS_THUMP] = 1.4f, [DS_POP] = 3.5f, [DS_HIGH] = 1.0f};
    if (source >= DS_COUNT) source = DS_IMPULSE;
    float gain = level * level * (float)velocity * (1.0f / 127.0f) * loudness[source];
    v->active = 1; v->slot = (uint8_t)slot; v->delay = delay;
    v->gl = gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
    v->gr = gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
    v->tape = (float)s[DP_TAPE] * (1.0f / 99.0f);
    v->reverb = (float)s[DP_SHIMMER] * (1.0f / 99.0f);
    v->env = 1.0f; v->amp = 1.0f; v->phase = 0.0f; v->lp = v->hp = v->y1 = v->y2 = 0.0f;
    v->noise = rnd() | 1U; v->seed = v->noise; v->count = 0; v->crush = 0; v->hold = 1; v->held_left = 0; v->held = 0.0f;
    v->tone_env = 1.0f;
    v->lowcut = 0.0f;
    /* Glitch slots stay out of the bass; THUMP and POP live there. */
    v->highpass = source >= DS_IMPULSE && source != DS_THUMP && source != DS_POP;
    v->color = (float)s[DP_COLOR] * (1.0f / 99.0f);
    drum_activity_bits |= 1U << slot;
    v->type = (uint8_t)source;
    float shape = (float)s[DP_DECAY] * (1.0f / 99.0f);        /* DECAY scales each sound's length */
    shape = steel_clamp(shape * (1.0f + random * 0.24f * (frand() - 0.5f)), 0.0f, 1.0f);
    switch (source) {
    case DS_KICK:
        v->freq = 48.0f * pitch; v->freq_end = v->freq; v->phase = 0.25f;
        v->sweep = decay_for(0.05f); v->tone_env = 3.0f;    /* pitch starts 4x higher */
        v->decay = decay_for(0.15f + length * 0.6f); break;
    case DS_SNARE:
        v->freq = 185.0f * pitch; v->decay = decay_for(0.08f + length * 0.3f);
        v->tone_decay = decay_for(0.06f); break;
    case DS_HAT:
        v->decay = decay_for(0.02f + length * 0.15f); v->a1 = steel_clamp(0.35f * pitch, 0.05f, 0.95f); break;
    case DS_IMPULSE:
        /* 4..66 samples (0.1-1.5 ms), random polarity; PITCH shortens it. */
        v->length = 4 + (int)((62.0f * shape) / pitch);
        v->held = rnd() & 0x400U ? 0.9f : -0.9f;
        v->decay = 1.0f; break;
    case DS_TRAIN:
        v->repeats = 3 + (int)(17.0f * shape);                          /* 3..20 impulses (DECAY) */
        v->hold = (int)((0.002f + 0.010f * (1.0f - 0.7f * shape) + 0.002f * random * frand()) * STEEL_RATE / pitch);
        v->length = 2 + (int)pick(4);                                   /* impulse width */
        v->held = rnd() & 0x400U ? 0.8f : -0.8f;
        v->decay = 1.0f; break;
    case DS_BLIP:
        /* A pure sine on one of eight high notes of the synth's scale. */
        v->freq = steel_clamp(drum_blip_hz[pick(8)] * pitch, 200.0f, 16000.0f);
        v->length = (int)((0.015f + 0.065f * shape) * STEEL_RATE);
        v->decay = 1.0f; v->amp = 0.5f; break;
    case DS_TICK:
        v->length = (int)((0.002f + 0.008f * shape) * STEEL_RATE);
        v->decay = decay_for((float)v->length / STEEL_RATE * 1.5f); break;
    /* The GLITCH sounds; COLOR shapes each one. */
    case DS_CRUNCH:  /* PITCH: hold rate; COLOR: smooth steps .. harsh, jittery crunch */
        v->freq = steel_clamp(2500.0f * pitch, 200.0f, 20000.0f);
        v->decay = decay_for(0.006f + 0.18f * shape);
        v->length = (int)((0.006f + 0.18f * shape) * 0.7f * STEEL_RATE);
        v->phase = 1.0f; v->spacing = 1.0f; v->held = 0.0f;
        v->amp = 0.55f; break;
    case DS_CHIRP:   /* COLOR: -4 octaves (down) .. +4 octaves (up) */
        v->freq = steel_clamp(700.0f * pitch, 60.0f, 12000.0f);
        v->length = (int)((0.005f + 0.08f * shape) * STEEL_RATE);
        v->sweep = steel_exp2((v->color - 0.5f) * 8.0f / (float)v->length);
        v->decay = 1.0f; v->amp = 0.6f; break;
    case DS_SNAP:    /* COLOR: dark .. bright band of noise; DECAY: length of the gate */
        v->a1 = steel_clamp((0.12f + 0.8f * v->color) * pitch, 0.02f, 0.95f); /* upper edge of the band */
        v->a2 = v->a1 * 0.12f;                                                 /* lower edge */
        v->length = (int)((0.004f + 0.07f * shape) * STEEL_RATE);
        v->held = rnd() & 0x400U ? 1.0f : -1.0f;
        v->decay = 1.0f; v->amp = 1.5f; break;
    case DS_DUST:    /* COLOR: density of the crackle */
        v->length = (int)((0.05f + 0.6f * shape) * STEEL_RATE);
        v->ratio = (60.0f + 3000.0f * v->color * v->color) * (1.0f / STEEL_RATE);
        v->held = 0.0f; v->decay = 1.0f; break;
    case DS_BITS:    /* COLOR < 50: short, tonal 7-bit LFSR; above: 15-bit noise */
        v->freq = steel_clamp(1500.0f * pitch, 100.0f, 22000.0f);
        v->lfsr = v->color < 0.5f ? 0x7fU : 0x7fffU;
        v->length = (int)((0.005f + 0.1f * shape) * STEEL_RATE);
        v->decay = decay_for((float)v->length * (1.2f / STEEL_RATE));
        v->amp = 0.5f; break;
    case DS_THUMP:   /* PITCH: 18-200 Hz; COLOR: pure sine .. click and drive; DECAY: a cycle .. a boom */
        v->freq = steel_clamp(42.0f * pitch, 18.0f, 200.0f);
        v->ratio = 1.4f + 2.6f * v->color;                 /* the start of the fall, times the end */
        v->tone_env = 1.0f;
        v->sweep = decay_for(0.015f + 0.03f * shape);
        v->length = (int)((0.035f + 0.45f * shape * shape) * STEEL_RATE);
        v->phase = 0.25f;                                  /* sine() is -cos: start at the zero crossing */
        v->held = v->color * (rnd() & 0x400U ? 0.8f : -0.8f);
        v->decay = 1.0f; v->amp = 0.9f; break;
    case DS_POP:     /* COLOR: soft .. sharp edge; DECAY: how slowly it slides back */
        v->held = rnd() & 0x400U ? 1.0f : -1.0f;
        v->a1 = steel_clamp((0.01f + 0.9f * v->color * v->color) * pitch, 0.003f, 1.0f);
        v->decay = decay_for(0.003f + 0.12f * shape * shape);
        v->length = (int)((0.003f + 0.12f * shape * shape) * 0.7f * STEEL_RATE) + 64;
        v->amp = 0.8f; break;
    case DS_HIGH:    /* PITCH: 7.7 kHz at 0, 4-18 kHz; COLOR: pure .. a beating second tone; DECAY: 5-200 ms */
        v->freq = steel_clamp(7700.0f * pitch, 4000.0f, 18000.0f);
        v->ratio = 1.0f + 0.05f * v->color;                /* the second tone, a little higher */
        v->length = (int)((0.005f + 0.2f * shape * shape) * STEEL_RATE);
        v->decay = 1.0f; v->amp = 0.3f; break;
    default: /* DS_CUT: no sound; the synth layer dips for 20-400 ms */
        cut_depth = level;
        cut_hold = delay + (int)((0.02f + 0.38f * shape) * STEEL_RATE);
        v->active = 0; break;
    }
    if (v->length < 2) v->length = 2;
    v->inv_length = 1.0f / (float)v->length;
}
static FM1_AUDIO_CODE float voice_sample(struct voice *v)
{
    float y = 0.0f;
    switch (v->type) {
    case DS_KICK: {
        float f = v->freq * (1.0f + v->tone_env);
        v->tone_env *= v->sweep;
        v->phase = wrap1(v->phase + f / STEEL_RATE);
        y = sine(v->phase);
        if (v->count < 90) { y += 0.3f * bipolar(&v->noise) * (1.0f - (float)v->count / 90.0f); ++v->count; }
        break;
    }
    case DS_SNARE: {
        v->phase = wrap1(v->phase + v->freq / STEEL_RATE);
        float n = bipolar(&v->noise);
        v->hp += (n - v->hp) * 0.25f;                 /* keep the noise bright */
        y = 0.55f * (n - v->hp) + 0.6f * v->tone_env * sine(v->phase);
        v->tone_env *= v->tone_decay;
        break;
    }
    case DS_HAT: {
        float n = bipolar(&v->noise);
        float d1 = n - v->y1; v->y1 = n;              /* two differentiators */
        float d2 = d1 - v->y2; v->y2 = d1;
        v->lp += (d2 - v->lp) * v->a1;
        y = 1.4f * v->lp;
        break;
    }
    case DS_IMPULSE:
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        y = v->count < v->length / 2 ? v->held : -0.5f * v->held; /* a short doublet */
        ++v->count;
        break;
    case DS_TRAIN:
        if (v->count < v->length) y = v->held;
        if (++v->count >= v->hold) {
            v->count = 0;
            v->held *= -0.85f;                           /* alternate and fade */
            if (--v->repeats <= 0) { v->active = 0; return 0.0f; }
        }
        break;
    case DS_BLIP: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        v->phase = wrap1(v->phase + v->freq / STEEL_RATE);
        /* 1 ms fade-in, 5 ms fade-out: clean edges, no click of its own. */
        float edge = steel_clamp(steel_clamp((float)v->count * (1.0f / 44.0f), 0.0f, 1.0f),
                                 0.0f, (float)(v->length - v->count) * (1.0f / 220.0f));
        y = v->amp * edge * sine(v->phase);
        ++v->count;
        break;
    }
    case DS_TICK: {
        if (v->count++ >= v->length) { v->active = 0; return 0.0f; }
        float n = bipolar(&v->noise);
        y = 0.7f * (n - v->y1);                          /* differentiated: bright */
        v->y1 = n;
        break;
    }
    case DS_CRUNCH: {
        if (v->count++ >= v->length) { v->active = 0; return 0.0f; }
        v->phase += v->freq * v->spacing * (1.0f / STEEL_RATE);
        if (v->phase >= 1.0f) {                      /* a new random level, fewer of them with COLOR */
            v->phase -= (float)(int)v->phase;
            float levels = 2.0f + 14.0f * (1.0f - v->color);
            v->held = (float)(int)(bipolar(&v->noise) * levels) / levels;
            v->spacing = steel_clamp(1.0f + 1.6f * v->color * bipolar(&v->noise), 0.2f, 2.6f);
        }
        v->lp += (v->held - v->lp) * (0.3f + 0.7f * v->color);
        y = v->amp * v->lp;
        break;
    }
    case DS_CHIRP: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        float x = (float)v->count * v->inv_length;
        v->phase = wrap1(v->phase + v->freq * (1.0f / STEEL_RATE));
        v->freq = steel_clamp(v->freq * v->sweep, 30.0f, 18000.0f);
        y = v->amp * steel_clamp((float)v->count * (1.0f / 22.0f), 0.0f, 1.0f) * (1.0f - x) * sine(v->phase);
        ++v->count;
        break;
    }
    case DS_SNAP: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        float x = (float)v->count * v->inv_length, n = bipolar(&v->noise);
        v->lp += (n - v->lp) * v->a1;
        v->y1 += (v->lp - v->y1) * v->a2;
        y = (v->lp - v->y1) * v->amp * (1.0f - x * x) + (v->count < 3 ? v->held : 0.0f); /* flat, then a quick close */
        ++v->count;
        break;
    }
    case DS_DUST: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        float x = (float)v->count * v->inv_length, n = bipolar(&v->noise);
        if ((float)(v->noise >> 8) * (1.0f / 16777216.0f) < v->ratio) v->held = n > 0.0f ? 0.5f + 0.5f * n : -0.5f + 0.5f * n;
        y = v->held * (1.0f - x);
        v->held *= 0.35f;
        ++v->count;
        break;
    }
    case DS_BITS:
        if (v->count++ >= v->length) { v->active = 0; return 0.0f; }
        v->phase += v->freq * (1.0f / STEEL_RATE);
        if (v->phase >= 1.0f) {
            v->phase -= (float)(int)v->phase;
            uint32_t l = v->lfsr, bit = (l ^ (l >> 1)) & 1U;
            v->lfsr = v->color < 0.5f ? (l >> 1) | (bit << 6) : (l >> 1) | (bit << 14);
        }
        y = v->amp * ((v->lfsr & 1U) ? 1.0f : -1.0f);
        break;
    case DS_THUMP: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        float x = (float)v->count * v->inv_length, g = 1.0f - x * x;
        float f = v->freq * (1.0f + (v->ratio - 1.0f) * v->tone_env);
        v->tone_env = steel_clamp(v->tone_env * v->sweep, 0.0f, 1.0f);
        v->phase = wrap1(v->phase + f * (1.0f / STEEL_RATE));
        y = v->amp * g * g * soft(sine(v->phase) * (1.0f + 1.5f * v->color));
        if (v->count < 24) y += v->held * bipolar(&v->noise) * (1.0f - (float)v->count * (1.0f / 24.0f)); /* click */
        ++v->count;
        break;
    }
    case DS_POP:
        if (v->count++ >= v->length) { v->active = 0; return 0.0f; }
        v->lp += (v->held - v->lp) * v->a1;              /* the edge of the jump */
        y = v->amp * v->lp;
        break;
    case DS_HIGH: {
        if (v->count >= v->length) { v->active = 0; return 0.0f; }
        v->phase = wrap1(v->phase + v->freq * (1.0f / STEEL_RATE));
        v->y1 = wrap1(v->y1 + v->freq * v->ratio * (1.0f / STEEL_RATE)); /* phase of the second tone */
        float gate = steel_clamp((float)v->count * (1.0f / 60.0f), 0.0f, 1.0f) *
                     steel_clamp((float)(v->length - v->count) * (1.0f / 130.0f), 0.0f, 1.0f);
        y = v->amp * gate * (sine(v->phase) + 0.7f * v->color * sine(v->y1));
        ++v->count;
        break;
    }
    default:
        v->active = 0; return 0.0f;
    }
    if (v->highpass) { v->lowcut += (y - v->lowcut) * 0.04f; y -= v->lowcut; } /* about 280 Hz */
    y *= v->env;
    v->env *= v->decay;
    if (v->env < 0.0005f) v->active = 0;
    return y;
}
/* One 1/64 tick. A slot at RATE 1/16 advances every 4 ticks, 1/32 every 2,
 * 1/64 every tick; its 16-step pattern therefore loops 1, 2 or 4 times a bar. */
static FM1_AUDIO_CODE void on_tick(unsigned t, uint32_t time, float samples_per_tick)
{
    drum_step_now = (uint8_t)(t >> 2);
    for (unsigned slot = 0; slot < DRUM_SLOTS; ++slot) {
        const struct drum_slot *d = &drum_slots[slot];
        const uint8_t *v = d->value;
        unsigned rate = v[DP_RATE] < 3 ? v[DP_RATE] : 0, shift = 2U - rate;
        unsigned prob = v[DP_PROB];
        float micro = (float)v[DP_MICRO] * (0.010f / 99.0f) * STEEL_RATE; /* up to 10 ms late */
        if (!(t & ((1U << shift) - 1U)) && ((d->steps >> ((t >> shift) & 15U)) & 1U)) {
            if (frand() * 100.0f < (float)prob) {
                static const uint8_t hits[5] = {1, 2, 3, 4, 6}, fade[5] = {0, 30, 20, 15, 10};
                static const float gaps[5] = {1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f};
                unsigned r = v[DP_RATCHET] < 5 ? v[DP_RATCHET] : 0;
                float gap = samples_per_tick * (float)(1U << shift) * gaps[r];
                for (unsigned i = 0; i < hits[r]; ++i)
                    schedule(time + (uint32_t)(gap * (float)i + micro * frand()), slot, 127U - i * fade[r]);
            }
        }
        /* Scatter: N extra hits per bar on the slot's own grid (1/16, 1/32 or
         * 1/64). The positions come from a hash of the pattern seed, so they
         * repeat every bar like a hidden pattern; PROB still drops some. */
        if (t == 0 && v[DP_SCATTER]) {
            unsigned count = (unsigned)v[DP_SCATTER] << rate, grid = 16U << rate;
            for (unsigned i = 0; i < count && i < 96U; ++i) {
                uint32_t h = (slot * 73856093U) ^ (i * 19349663U) ^ (drum_pattern_seed * 83492791U);
                h = (h ^ (h >> 13)) * 2654435761U;
                unsigned position = (h >> 7) & (grid - 1U);
                if (frand() * 100.0f < (float)prob)
                    schedule(time + (uint32_t)(samples_per_tick * (float)(position << shift) + micro * frand()),
                             slot, 70U + ((h >> 3) & 31U) + ((h >> 9) & 15U));
            }
        }
    }
}
FM1_AUDIO_CODE float steel_drums_cut(void) { return cut_level; }
/* Control task: queue an audition (panel key or MIDI channel 10). */
void steel_drums_hit(unsigned slot)
{
    struct drum_queue *q = &drum_auditions;
    unsigned w = q->write;
    if (slot >= DRUM_SLOTS || w - q->read >= 16U) return;
    q->slot[w & 15U] = (uint8_t)slot;
    STEEL_BARRIER();
    q->write = w + 1U;
}
/* Control task: BLIP notes follow the synth's key and scale (single float writes). */
void steel_drums_set_blips(const float midi[8])
{
    for (unsigned i = 0; i < 8; ++i) drum_blip_hz[i] = 440.0f * steel_exp2((midi[i] - 69.0f) * (1.0f / 12.0f));
}
FM1_AUDIO_CODE void steel_drums_render(float *dry_l, float *dry_r, float *tape, float *reverb, float bpm)
{
    /* Auditions from the panel play at once. */
    for (unsigned r = drum_auditions.read, w = drum_auditions.write; r != w; ++r) {
        STEEL_BARRIER();
        trigger(drum_auditions.slot[r & 15U], 110U, 0);
        drum_auditions.read = r + 1U;
    }
    /* Sequencer: find step boundaries inside this block. */
    float samples_per_tick = 60.0f / steel_clamp(bpm, 20.0f, 300.0f) / 16.0f * STEEL_RATE;
    if (drum_run && !was_running) { tick = 0; tick_phase = samples_per_tick; event_count = 0; }
    was_running = drum_run;
    if (drum_run) {
        float offset = 0.0f;
        while (offset < (float)BLOCK) {
            float remaining = samples_per_tick - tick_phase;
            if (remaining <= (float)BLOCK - offset) {
                offset += remaining > 0.0f ? remaining : 0.0f;
                on_tick(tick, now + (uint32_t)offset, samples_per_tick);
                tick = (tick + 1U) & 63U;
                tick_phase = 0.0f;
            } else { tick_phase += (float)BLOCK - offset; break; }
        }
    } else event_count = 0;
    bar_lfo += (float)BLOCK / (samples_per_tick * 64.0f);
    if (bar_lfo >= 1.0f) bar_lfo -= 1.0f;
    /* Due events start voices with their exact offset in this block. */
    for (unsigned i = 0; i < event_count;) {
        int32_t due = (int32_t)(events[i].time - now);
        if (due < BLOCK) {
            trigger(events[i].slot, events[i].velocity, due > 0 ? due : 0);
            events[i] = events[--event_count];
        } else ++i;
    }
    /* CUT: hold the dip, then let it recover over about 15 ms. */
    if (cut_hold > 0) { cut_hold -= BLOCK; cut_level = cut_depth; }
    else cut_level *= 0.75f;
    /* Hiss bed: quiet, bright, decorrelated left/right; part of the running track. */
    const float hiss = drum_run ? drum_hiss_gain : 0.0f;
    if (hiss > 0.0f)
        for (unsigned i = 0; i < BLOCK; ++i) {
            float a = bipolar(&hiss_noise), b = bipolar(&hiss_noise);
            hiss_hp[0] += (a - hiss_hp[0]) * 0.2f; hiss_hp[1] += (b - hiss_hp[1]) * 0.2f;
            dry_l[i] += (a - hiss_hp[0]) * hiss;
            dry_r[i] += (b - hiss_hp[1]) * hiss;
        }
    const float level = drum_level_gain;
    for (unsigned k = 0; k < DRUM_VOICES; ++k) {
        struct voice *v = &voices[k];
        if (!v->active) continue;
        for (unsigned i = 0; i < BLOCK && v->active; ++i) {
            if (v->delay > 0) { --v->delay; continue; }
            float y = voice_sample(v) * level;
            dry_l[i] += y * v->gl;
            dry_r[i] += y * v->gr;
            float mono = 0.5f * (v->gl + v->gr) * y;
            tape[i] += mono * v->tape;
            reverb[i] += mono * v->reverb;
        }
    }
    now += BLOCK;
}
