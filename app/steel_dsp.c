/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLIDER audio path: sixteen latched voices, one per white key.
 * Runs in the audio owner from SRAM; no libc or Flash-resident helpers. */
#ifndef FM1_HOST_TEST
#pragma code_seg(".volatile_ram_code")
#pragma const_seg(".volatile_ram.fm1")
#endif
#include "steel_internal.h"
#include "steel_drums.h"

enum { BLOCK = FM1_BLOCK_FRAMES };

struct steel_patch steel_patches[2];
volatile uint8_t steel_patch_index;
struct steel_command steel_commands[STEEL_VOICES];

struct voice {
    float pitch, from, to, progress;   /* glide in semitones, progress 0..1 */
    float pitch2, from2, to2;          /* oscillator 2 (interval), same glide */
    float p1, p2, ps, ic1, ic2, env, vibrato, drift, drift_to;
    float partial[6];                  /* phases of harmonics 2..5 and of 1/2, 1/3 */
    uint32_t random;
    uint8_t stage, note_seen, glide_seen;  /* stage: 0 off, 1 attack, 2 hold, 3 release */
};
static struct voice voices[STEEL_VOICES];
static float left[BLOCK], right[BLOCK], lfo, detune_lfo, float_clock;
/* Partials: frequency ratios, and incommensurate motion speeds for FLOAT. */
static const float partial_ratio[6] = {2.0f, 3.0f, 4.0f, 5.0f, 0.5f, 1.0f / 3.0f};
static const float partial_speed[6] = {1.0f, 1.37f, 0.73f, 1.61f, 0.53f, 0.89f};
static float drum_l[BLOCK], drum_r[BLOCK], tape_send[BLOCK], rev_send[BLOCK], cut_lp[2];
static float part_l[BLOCK], part_r[BLOCK]; /* over/undertones, bypassing the FX filter */
static unsigned blocks;

/* sin(2 pi t) for t in [0,1), parabolic with one refinement (error < 0.1%). */
STEEL_INLINE float sine(float t)
{
    float x = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t; /* triangle -1..1 */
    x *= 1.5707963f;                                         /* -pi/2..pi/2 */
    float x2 = x * x;
    return x * (1.0f - x2 * (0.16666667f - x2 * (0.0083333f - x2 * 0.000198f)));
}
STEEL_INLINE float wrap(float t) { return t >= 1.0f ? t - 1.0f : t; }
/* Band-limited ramp: naive saw minus a polynomial BLEP residual. */
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
STEEL_INLINE float curve(float t, unsigned kind)
{
    if (kind == 1) return t * t * (3.0f - 2.0f * t);  /* smooth start and landing */
    if (kind == 2) { float u = 1.0f - t; return 1.0f - u * u * u; } /* bar: fast, then settles */
    return t;
}
static FM1_AUDIO_CODE void update(struct voice *v, unsigned index, const struct steel_patch *p)
{
    struct steel_command *c = &steel_commands[index];
    if (c->note_serial != v->note_seen) {
        v->note_seen = c->note_serial;
        STEEL_BARRIER();
        v->pitch = v->to = c->target;
        v->pitch2 = v->to2 = c->target2;
        v->progress = 1.0f;
        if (v->stage == 0 || v->env < 0.001f) {
            v->vibrato = 0.0f; v->p1 = 0.0f; v->p2 = 0.31f; v->ps = 0.0f;
            for (unsigned k = 0; k < 6; ++k) v->partial[k] = 0.0f;
        }
        v->stage = 1;
    }
    if (c->glide_serial != v->glide_seen) {
        v->glide_seen = c->glide_serial;
        STEEL_BARRIER();
        v->from = v->pitch;
        v->to = c->target;
        v->from2 = v->pitch2;
        v->to2 = c->target2;
        v->progress = 0.0f;
        if (v->stage == 0) { v->pitch = v->to; v->pitch2 = v->to2; v->progress = 1.0f; }
    }
    if (!c->gate && (v->stage == 1 || v->stage == 2)) v->stage = 3;
    if (v->progress < 1.0f) {
        v->progress += p->glide_step * BLOCK;
        if (v->progress > 1.0f) v->progress = 1.0f;
        float shape = curve(v->progress, p->glide_curve);
        v->pitch = v->from + (v->to - v->from) * shape;
        v->pitch2 = v->from2 + (v->to2 - v->from2) * shape;
    }
    /* Analog drift: a slow random walk per voice. */
    if (((blocks + index * 7U) & 63U) == 0) {
        v->random = v->random * 1664525U + 1013904223U + index;
        v->drift_to = (float)(int32_t)v->random * (1.0f / 2147483648.0f);
    }
    v->drift += (v->drift_to - v->drift) * 0.01f;
}
static FM1_AUDIO_CODE void render_voice(struct voice *v, unsigned index, const struct steel_patch *p, float vib)
{
    /* Oscillator 2 detune: fixed cents plus a slow sweep, phase-shifted per key. */
    float sweep = detune_lfo + (float)index * 0.37f;
    sweep -= (float)(int)sweep;
    float detune = steel_exp2((p->detune + p->detune_lfo * sine(sweep)) * (1.0f / 1200.0f));
    float mod = v->drift * p->drift + vib * v->vibrato * p->vib_depth;
    float pitch = v->pitch + mod;
    float dt = steel_clamp(440.0f / STEEL_RATE * steel_exp2((pitch - 69.0f) * (1.0f / 12.0f)), 1e-5f, 0.45f);
    /* Oscillator 2 sounds its own interval pitch (unison when the interval is off). */
    float dt2 = v->pitch2 == v->pitch ? dt * detune
              : 440.0f / STEEL_RATE * steel_exp2((v->pitch2 + mod - 69.0f) * (1.0f / 12.0f)) * detune;
    dt2 = steel_clamp(dt2, 1e-5f, 0.45f);
    float dts = dt * 0.5f;
    float inv = 1.0f / dt, inv2 = 1.0f / dt2;
    unsigned shape = (unsigned)p->shape;
    if (shape > 2U) shape = 2U;
    float blend = p->shape - (float)shape;
    float cutoff = p->tone + p->tone_env * v->env + 0.5f * (pitch - 60.0f);
    float hz = steel_clamp(440.0f * steel_exp2((cutoff - 69.0f) * (1.0f / 12.0f)), 30.0f, 16000.0f);
    float g = 3.14159265f / STEEL_RATE * hz;
    g = g * (15.0f - g * g) / (15.0f - 6.0f * g * g); /* tan prewarp */
    const float k = 1.5f, a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    const float osc2 = p->osc2, sub = 0.7f * p->sub;
    const float norm = 0.22f / (1.0f + osc2 + sub);
    /* Partials: harmonics 2..5 and undertones 1/2, 1/3 as pure sines with their
     * own phases. FLOAT moves each one's level and pitch slowly and
     * independently (per key, per partial), so a single note hovers.
     * Partials above about 19 kHz are skipped (no aliasing). */
    float level[6], step[6];
    unsigned partials_on = 0;
    for (unsigned k = 0; k < 6; ++k) {
        float base = k < 4 ? p->overtone[k] : p->undertone[k - 4];
        level[k] = 0.0f; step[k] = 0.0f;
        if (base <= 0.0f || partial_ratio[k] * dt >= 0.43f) continue;
        float motion = float_clock * p->float_rate * partial_speed[k] + (float)index * 0.37f + (float)k * 0.23f;
        motion -= (float)(int)motion;
        float drift = motion + 0.31f;
        drift -= (float)(int)drift;
        level[k] = base * (1.0f - p->float_depth * (0.5f - 0.5f * sine(motion)));
        step[k] = dt * partial_ratio[k] * steel_exp2(p->float_depth * 8.0f * sine(drift) * (1.0f / 1200.0f));
        partials_on = 1;
    }
    /* Keys spread across the stereo field from low (left) to high (right). */
    float pan = steel_commands[index].pan * p->spread;
    float gl = norm * (1.0f - pan), gr = norm * (1.0f + pan);
    float p1 = v->p1, p2 = v->p2, ps = v->ps, ic1 = v->ic1, ic2 = v->ic2, env = v->env;
    unsigned stage = v->stage;
    const float attack = p->attack, decay = p->decay, sustain = p->sustain;
    const float release = steel_commands[index].fade ? 0.007f : p->release; /* guard: ~30 ms */
    for (unsigned i = 0; i < BLOCK; ++i) {
        float x = oscillator(p1, dt, inv, shape, blend);
        /* Pure sine partials bypass the voice filter. */
        float partials = 0.0f;
        if (partials_on)
            for (unsigned k = 0; k < 6; ++k)
                if (level[k] > 0.0f) {
                    partials += level[k] * sine(v->partial[k]);
                    v->partial[k] = wrap(v->partial[k] + step[k]);
                }
        if (osc2 > 0.0f) x += osc2 * oscillator(p2, dt2, inv2, shape, blend);
        if (sub > 0.0f) x += sub * (ps < 0.5f ? 4.0f * ps - 1.0f : 3.0f - 4.0f * ps);
        p1 = wrap(p1 + dt); p2 = wrap(p2 + dt2); ps = wrap(ps + dts);
        float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        if (stage == 1) { env += attack; if (env >= 1.0f) { env = 1.0f; stage = 2; } }
        else if (stage == 2) env += (sustain - env) * decay;
        else env -= env * release;
        float y = v2 * env, z = partials * env;
        left[i] += y * gl;
        right[i] += y * gr;
        part_l[i] += z * gl;
        part_r[i] += z * gr;
    }
    if (stage == 3 && env < 1e-4f) stage = 0;
    if (!(ic1 > -1e4f && ic1 < 1e4f) || !(ic2 > -1e4f && ic2 < 1e4f)) ic1 = ic2 = 0.0f;
    v->vibrato = steel_clamp(v->vibrato + p->vib_delay * BLOCK, 0.0f, 1.0f);
    v->p1 = p1; v->p2 = p2; v->ps = ps; v->ic1 = ic1; v->ic2 = ic2; v->env = env; v->stage = (uint8_t)stage;
}
uint16_t steel_ringing(void)
{
    uint16_t bits = 0;
    for (unsigned i = 0; i < STEEL_VOICES; ++i)
        if (voices[i].stage == 3 && voices[i].env > 0.001f) bits |= (uint16_t)(1U << i);
    return bits;
}
/* Gentle tape-like saturation of the voice bus. */
STEEL_INLINE float saturate(float x)
{
    x = steel_clamp(x, -1.5f, 1.5f);
    return x - x * x * x * (4.0f / 27.0f);
}
FM1_AUDIO_CODE void steel_render(int32_t *output)
{
    const struct steel_patch *p = &steel_patches[steel_patch_index];
    ++blocks;
    lfo += p->vib_rate * BLOCK / STEEL_RATE;
    if (lfo >= 1.0f) lfo -= 1.0f;
    detune_lfo += 0.15f * BLOCK / STEEL_RATE; /* about 0.15 Hz, like the slow swells in keins */
    if (detune_lfo >= 1.0f) detune_lfo -= 1.0f;
    float_clock += BLOCK / STEEL_RATE;        /* seconds; wraps after 10 minutes */
    if (float_clock >= 600.0f) float_clock -= 600.0f;
    float vib = sine(lfo);
    unsigned active = 0;
    for (unsigned i = 0; i < STEEL_VOICES; ++i) {
        update(&voices[i], i, p);
        active |= voices[i].stage;
    }
    volatile float *clear_l = left, *clear_r = right, *clear_dl = drum_l, *clear_dr = drum_r,
                   *clear_t = tape_send, *clear_s = rev_send, *clear_pl = part_l, *clear_pr = part_r; /* no Flash memset */
    for (unsigned i = 0; i < BLOCK; ++i)
        clear_l[i] = clear_r[i] = clear_dl[i] = clear_dr[i] = clear_t[i] = clear_s[i] = clear_pl[i] = clear_pr[i] = 0.0f;
    if (active)
        for (unsigned i = 0; i < STEEL_VOICES; ++i)
            if (voices[i].stage) render_voice(&voices[i], i, p, vib);
    /* Drums: dry stays crisp (outside the drone filter); sends feed tape/reverb. */
    steel_drums_render(drum_l, drum_r, tape_send, rev_send, p->bpm);
    /* Effects run even when all keys are off: echoes and tape feedback ring on. */
    steel_fx_post(left, right, p, tape_send, part_l, part_r);
    steel_reverb(left, right, p, rev_send);
    /* A drum CUT darkens and dips the synth layer (with its effects) briefly. */
    float cut = steel_drums_cut();
    if (cut > 0.001f || cut_lp[0] != 0.0f) {
        float coef = 1.0f - 0.96f * cut, gain = 1.0f - 0.7f * cut;
        for (unsigned i = 0; i < BLOCK; ++i) {
            cut_lp[0] += (left[i] - cut_lp[0]) * coef; cut_lp[1] += (right[i] - cut_lp[1]) * coef;
            left[i] = cut_lp[0] * gain; right[i] = cut_lp[1] * gain;
        }
        if (cut <= 0.001f) cut_lp[0] = cut_lp[1] = 0.0f;
    }
    /* Tones (with all effects) follow LEVEL; the dry drums have their own
     * level (DRUM LVL) and a separate soft limiter. */
    const float level = p->level * 8388607.0f, drums = 0.7f * 8388607.0f;
    for (unsigned i = 0; i < BLOCK; ++i) {
        output[2 * i] += (int32_t)(saturate(left[i]) * level + saturate(drum_l[i]) * drums);
        output[2 * i + 1] += (int32_t)(saturate(right[i]) * level + saturate(drum_r[i]) * drums);
    }
}
