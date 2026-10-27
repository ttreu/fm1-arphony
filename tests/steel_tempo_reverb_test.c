/* Native tests: shimmer reverb, tempo sync, tap tempo, MIDI clock.
 * Build: cc -DFM1_HOST_TEST tests/steel_tempo_reverb_test.c app/steel.c app/steel_dsp.c app/steel_fx.c app/steel_reverb.c */
#include "../app/steel_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static float L[FM1_BLOCK_FRAMES], R[FM1_BLOCK_FRAMES];
static const struct steel_patch *patch(void) { return &steel_patches[steel_patch_index]; }
static void set(unsigned p, unsigned v) { steel_edit(p, (int)v - (int)steel_get(p)); }
static double energy_at(const float *x, unsigned n, float hz) /* Goertzel */
{
    double w = 2 * M_PI * hz / 44100.0, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (unsigned i = 0; i < n; ++i) { double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}
int main(void)
{
    steel_init();
    /* ---------------- reverb */
    struct steel_patch p = *patch();
    p.rev_mix = 1.0f; p.rev_dry = 1.0f; p.rev_shimmer = 0.0f; p.rev_decay = 0.25f + 0.5f * 0.70f; p.rev_damp = 0.4f;
    double early = 0, late = 0;
    for (int b = 0; b < 344 * 8; ++b) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        if (b == 0) L[0] = R[0] = 1.0f;
        steel_reverb(L, R, &p, NULL);
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) {
            if (b >= 10 && b < 100) early += L[i] * L[i];
            if (b >= 344 * 7) late += L[i] * L[i];
        }
    }
    printf("     plate size 50%%: early %.2e, after 7 s %.2e\n", early, late);
    check(early > 1e-4 && late < early * 1e-6, "plate tail is dense, then decays away");
    /* Maximum size with shimmer off and fully on, via the real knob mapping:
     * bounded, finite, and the shimmer adds octaves above the played tone. */
    double ratio[2];
    for (int shim = 0; shim < 2; ++shim) {
        set(SP_REV_SIZE, 99); set(SP_REV_DAMP, 20); set(SP_REV_MIX, 99); set(SP_SHIMMER, shim ? 99 : 0);
        p = *patch();
        float peak = 0; int finite = 1;
        static float tail[44100];
        for (int b = 0; b < 344 * 30; ++b) {
            for (int i = 0; i < FM1_BLOCK_FRAMES; ++i)
                L[i] = R[i] = b < 344 ? 0.3f * sinf(2 * (float)M_PI * 440.0f * (b * 128 + i) / 44100.0f) : 0.0f;
            steel_reverb(L, R, &p, NULL);
            for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) {
                if (!isfinite(L[i])) finite = 0;
                if (fabsf(L[i]) > peak) peak = fabsf(L[i]);
                int t = (b - 344 * 2) * 128 + i;
                if (t >= 0 && t < 44100) tail[t] = L[i];
            }
        }
        double e440 = energy_at(tail, 44100, 440), e880 = energy_at(tail, 44100, 880), e1760 = energy_at(tail, 44100, 1760);
        ratio[shim] = (e880 + e1760) / (e440 + 1e-12);
        printf("     size 99, shimmer %s: peak %.2f, tail octave/fundamental %.3f\n", shim ? "99" : "0 ", peak, ratio[shim]);
        /* REV MIX 99 is now a loud hall (wet x1.6); a 1 s sine of 0.3 may ring up
         * to about 9x but must not run away. */
        check(finite && peak < 3.5f, shim ? "max size + max shimmer stays bounded" : "max size stays bounded");
    }
    check(ratio[1] > 1.0 && ratio[1] > ratio[0] * 100.0, "shimmer tail is dominated by octaves above the played tone");
    set(SP_SHIMMER, 0); set(SP_REV_MIX, 0);

    /* ---------------- tempo sync */
    set(SP_BPM, 120 - 40);
    check(steel_bpm() == 120, "BPM set to 120");
    set(SP_DELAY_SYNC, 1); set(SP_DELAY_TIME, 60);      /* index 6 = 1/4 */
    char text[12]; steel_param_text(SP_DELAY_TIME, steel_get(SP_DELAY_TIME), text);
    printf("     delay knob shows %s, time %.1f ms\n", text, patch()->delay_time * 1000.0 / STEEL_RATE);
    check(!strcmp(text, "1/4") && fabsf(patch()->delay_time - 22050.0f) < 2.0f, "synced delay: 1/4 at 120 BPM = 500 ms");
    set(SP_TAPE_SYNC, 1); set(SP_TAPE_TIME, 30);        /* index 3 = 1/8 */
    check(fabsf(patch()->tape_time - 11025.0f) < 2.0f, "synced tape: 1/8 at 120 BPM = 250 ms");
    set(SP_LFO_SYNC, 1); set(SP_LFO_RATE, 40);          /* index 3 = 1 BAR */
    float lfo_hz_value = patch()->lfo_step * STEEL_RATE / FM1_BLOCK_FRAMES;
    check(fabsf(lfo_hz_value - 0.5f) < 0.001f, "synced LFO: 1 bar at 120 BPM = 0.5 Hz");
    steel_load(3);
    check(steel_bpm() == 120, "tempo survives a preset change");

    /* ---------------- tap tempo */
    steel_tap(10000); steel_tap(10750); steel_tap(11500); steel_tap(12250);
    check(steel_bpm() == 80, "tap every 750 ms = 80 BPM");

    /* ---------------- MIDI clock: 24 ticks per beat */
    uint32_t t = 20000;
    for (int i = 0; i < 24 * 4; ++i) { steel_midi_clock(0xf8, t); t += 25; }   /* 600 ms per beat */
    steel_poll(t);
    check(steel_bpm_external() && steel_bpm() == 100, "MIDI clock at 25 ms/tick = 100 BPM, marked EXT");
    steel_poll(t + 2500);
    check(!steel_bpm_external() && steel_bpm() == 100, "clock gone: tempo stays, EXT clears");
    return fails != 0;
}
