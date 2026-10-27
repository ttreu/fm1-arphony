/* Native unit test of the GLITCH DRUMS voices (all synthesized, no samples).
 * Build: cc -DFM1_HOST_TEST -Iapp -Isdk/include tests/steel_drums_dsp_test.c app/steel_drums_dsp.c */
#include "../app/steel_drums_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static float L[128], R[128], T[128], V[128];
static double render(int blocks, double *corr)
{
    double e = 0, l = 0, r = 0, lr = 0;
    for (int b = 0; b < blocks; ++b) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
        steel_drums_render(L, R, T, V, 120.0f);
        for (int i = 0; i < 128; ++i) { e += L[i] * L[i] + R[i] * R[i]; l += L[i] * L[i]; r += R[i] * R[i]; lr += L[i] * R[i]; }
    }
    if (corr) *corr = lr / sqrt(l * r + 1e-20);
    return sqrt(e / (blocks * 256.0));
}
static void audition(unsigned slot)
{
    drum_auditions.slot[drum_auditions.write & 15U] = (uint8_t)slot;
    drum_auditions.write++;
}
int main(void)
{
    for (unsigned s = 0; s < DRUM_SLOTS; ++s) {
        uint8_t *v = drum_slots[s].value;
        v[DP_PITCH] = 24; v[DP_DECAY] = 99; v[DP_LEVEL] = 99; v[DP_PROB] = 100; v[DP_PAN] = 50;
    }
    drum_level_gain = 1.0f;
    /* Every sounding source: finite, bounded, and silent again afterwards. */
    for (unsigned src = DS_KICK; src < DS_COUNT; ++src) {
        if (src == DS_CUT) continue;
        drum_slots[12].value[DP_SOURCE] = (uint8_t)src;
        drum_slots[12].value[DP_COLOR] = 50;
        drum_slots[12].value[DP_DECAY] = 60;
        audition(12);
        double a = render(40, 0);
        render(800, 0);
        double tail = render(40, 0);
        char what[64]; snprintf(what, sizeof what, "source %u sounds (rms %.4f) and ends", src, a);
        check(a > 0.0002 && a < 1.0 && isfinite(a) && tail < 1e-4, what);
    }
    /* Random panning: every hit lands hard left or right, both sides occur. */
    drum_slots[13].value[DP_SOURCE] = DS_IMPULSE; drum_slots[13].value[DP_PAN_MODE] = PAN_RANDOM; drum_slots[13].value[DP_PAN] = 99;
    int lefts = 0, rights = 0, centred = 0;
    for (int i = 0; i < 40; ++i) {
        audition(13);
        double l = 0, r = 0;
        for (int b = 0; b < 8; ++b) {
            memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
            steel_drums_render(L, R, T, V, 120.0f);
            for (int k = 0; k < 128; ++k) { l += L[k] * L[k]; r += R[k] * R[k]; }
        }
        double bal = (r - l) / (r + l + 1e-20);
        if (bal > 0.9) ++rights; else if (bal < -0.9) ++lefts; else ++centred;
    }
    printf("     random pan over 40 hits: %d left, %d right, %d in between\n", lefts, rights, centred);
    check(lefts > 5 && rights > 5 && centred == 0, "random pan places every click hard left or right");
    /* Sequencer: step 0 with ratchet x4 gives four hits in the first step. */
    memset(drum_slots, 0, sizeof drum_slots);
    drum_slots[0].value[DP_SOURCE] = DS_IMPULSE; drum_slots[0].value[DP_LEVEL] = 99; drum_slots[0].value[DP_PROB] = 100;
    drum_slots[0].value[DP_RATCHET] = 3; drum_slots[0].value[DP_DECAY] = 0; drum_slots[0].steps = 1;
    drum_run = 1;
    int onsets = 0, quiet = 1000; float env = 0;
    for (int b = 0; b < 44; ++b) {           /* 44 blocks = 127.7 ms > one 16th at 120 BPM (125 ms) */
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
        steel_drums_render(L, R, T, V, 120.0f);
        for (int i = 0; i < 128; ++i) {       /* envelope onsets, at least 10 ms apart */
            env = fmaxf(fabsf(L[i]), env * 0.995f);
            if (env > 0.05f && quiet > 441) ++onsets;
            quiet = env > 0.02f ? 0 : quiet + 1;
        }
    }
    printf("     ratchet x4 onsets in one step: %d\n", onsets);
    check(onsets == 4, "ratchet x4 plays four hits inside one 16th");
    /* RATE 1/32: a single active step plays twice per bar (pattern loops twice). */
    drum_run = 0; render(1, 0); /* stop and restart from the bar start */
    drum_slots[0].value[DP_RATCHET] = 0; drum_slots[0].value[DP_RATE] = 1; drum_slots[0].steps = 1;
    drum_run = 1;
    onsets = 0; quiet = 1000; env = 0;
    for (int b = 0; b < 680; ++b) {          /* 680 blocks = 1.97 s: just under one bar at 120 BPM */
        memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
        steel_drums_render(L, R, T, V, 120.0f);
        for (int i = 0; i < 128; ++i) {
            env = fmaxf(fabsf(L[i]), env * 0.995f);
            if (env > 0.05f && quiet > 441) ++onsets;
            quiet = env > 0.02f ? 0 : quiet + 1;
        }
    }
    printf("     RATE 1/32, one active step: %d hits per bar\n", onsets);
    check(onsets == 2, "RATE 1/32 runs the pattern twice as fast");
    drum_run = 0; render(1, 0);
    /* COLOR shapes the GLITCH sounds: a bright SNAP has more high energy
     * than a dark one; THUMP keeps its bass (no glitch high-pass). */
    {
        double bright[2];
        for (int k = 0; k < 2; ++k) {
            render(200, 0);
            uint8_t *v = drum_slots[14].value;
            v[DP_SOURCE] = DS_SNAP; v[DP_LEVEL] = 99; v[DP_DECAY] = 60; v[DP_PITCH] = 24; v[DP_PAN] = 50;
            v[DP_RANDOM] = 0; v[DP_COLOR] = k ? 95 : 5;
            audition(14);
            double e = 0, d = 0, prev = 0;
            for (int b2 = 0; b2 < 40; ++b2) {
                memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
                steel_drums_render(L, R, T, V, 120.0f);
                for (int i = 0; i < 128; ++i) { e += L[i] * L[i]; d += (L[i] - prev) * (L[i] - prev); prev = L[i]; }
            }
            bright[k] = d / (e + 1e-20);
        }
        printf("     SNAP brightness COLOR 5: %.3f, COLOR 95: %.3f\n", bright[0], bright[1]);
        check(bright[1] > 2.0 * bright[0], "COLOR turns SNAP from dark to bright");
        render(200, 0);
        uint8_t *v = drum_slots[14].value;
        v[DP_SOURCE] = DS_THUMP; v[DP_COLOR] = 0; v[DP_DECAY] = 60;
        audition(14);
        double lo = 0, all = 0, y = 0;
        for (int b2 = 0; b2 < 60; ++b2) {
            memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V);
            steel_drums_render(L, R, T, V, 120.0f);
            for (int i = 0; i < 128; ++i) { y += (L[i] - y) * 0.017; lo += y * y; all += L[i] * L[i]; }
        }
        printf("     THUMP energy below ~120 Hz: %.0f%%\n", 100 * lo / (all + 1e-20));
        check(all > 1e-3 && lo > 0.5 * all, "THUMP is a low micro kick");
    }
    /* Microsounds after the keins references. */
    {
        static float buf[44100];
        int n;
        render(800, 0); /* let earlier test sounds die away */
        memset(drum_slots, 0, sizeof drum_slots);
        drum_level_gain = 1.0f;
        for (int s2 = 15; s2 <= 19; ++s2) {
            uint8_t *v = drum_slots[s2].value;
            v[DP_LEVEL] = 99; v[DP_PITCH] = 24; v[DP_PAN] = 50; v[DP_PROB] = 100;
        }
        #define CAPTURE(slot, blocks) do { audition(slot); n = 0; \
            for (int b = 0; b < (blocks); ++b) { memset(L, 0, sizeof L); memset(R, 0, sizeof R); memset(T, 0, sizeof T); memset(V, 0, sizeof V); \
                steel_drums_render(L, R, T, V, 120.0f); for (int i = 0; i < 128 && n < 44100; ++i) buf[n++] = L[i]; } } while (0)
        /* IMPULSE: at most 1.5 ms of signal (plus the high-pass tail). */
        drum_slots[15].value[DP_SOURCE] = DS_IMPULSE; drum_slots[15].value[DP_DECAY] = 99;
        CAPTURE(15, 20);
        int loud = 0; for (int i = 0; i < n; ++i) loud += fabsf(buf[i]) > 0.1f;
        printf("     IMPULSE: %d samples above 0.1 (%.2f ms)\n", loud, loud * 1000.0 / 44100);
        check(loud > 0 && loud <= 70, "IMPULSE is a single click of at most 1.5 ms");
        /* TRAIN: 3..20 impulses, 2..12 ms apart. */
        drum_slots[16].value[DP_SOURCE] = DS_TRAIN; drum_slots[16].value[DP_DECAY] = 99;
        int trains_ok = 1;
        for (int t = 0; t < 10; ++t) {
            CAPTURE(16, 120);
            float first = 0; for (int i = 0; i < n; ++i) first = fmaxf(first, fabsf(buf[i]));
            float thr = first * 0.08f;
            int onsets = 0, last = -10000, gap_min = 100000, gap_max = 0; float prev = 0;
            for (int i = 0; i < n; ++i) {
                float a2 = fabsf(buf[i]);
                if (a2 > thr && prev <= thr && i - last > 40) { if (last >= 0) { int g = i - last; gap_min = g < gap_min ? g : gap_min; gap_max = g > gap_max ? g : gap_max; } last = i; ++onsets; }
                prev = a2;
            }
            if (onsets < 3 || onsets > 20 || gap_min < 0.0018 * 44100 || gap_max > 0.0125 * 44100) {
                printf("     TRAIN %d: %d impulses, gaps %.1f..%.1f ms\n", t, onsets, gap_min * 1000.0 / 44100, gap_max * 1000.0 / 44100);
                trains_ok = 0;
            }
        }
        check(trains_ok, "TRAIN: 3-20 impulses, 2-12 ms apart");
        /* BLIP: a pure sine on one of the eight scale notes. */
        float blips[8] = {91, 93, 95, 96, 103, 105, 107, 108};
        steel_drums_set_blips(blips);
        drum_slots[17].value[DP_SOURCE] = DS_BLIP; drum_slots[17].value[DP_DECAY] = 99; drum_slots[17].value[DP_RANDOM] = 0;
        CAPTURE(17, 40);
        double best = 0, best_hz = 0, total = 0;
        for (int i = 0; i < n; ++i) total += buf[i] * buf[i];
        for (int k = 0; k < 8; ++k) {
            double hz = 440.0 * pow(2.0, (blips[k] - 69) / 12.0), w = 2 * M_PI * hz / 44100, c = 2 * cos(w), s1 = 0, s3 = 0;
            for (int i = 0; i < n; ++i) { double t2 = buf[i] + c * s1 - s3; s3 = s1; s1 = t2; }
            double e = (s1 * s1 + s3 * s3 - c * s1 * s3) / (n / 2.0);
            if (e > best) { best = e; best_hz = hz; }
        }
        printf("     BLIP at %.0f Hz holds %.0f%% of its energy\n", best_hz, 100 * best / total);
        check(best > 0.6 * total, "BLIP is a pure sine on a scale note");
        /* CUT: makes no sound, dips the synth layer, then lets go. */
        drum_slots[18].value[DP_SOURCE] = DS_CUT; drum_slots[18].value[DP_DECAY] = 0;
        CAPTURE(18, 2);
        float during = steel_drums_cut();
        double cut_sound = 0; for (int i = 0; i < n; ++i) cut_sound += fabsf(buf[i]);
        CAPTURE(19, 80); /* slot 19 is silent (source 0 = kick? no: level only) */
        printf("     CUT depth during %.2f, after 230 ms %.3f\n", during, steel_drums_cut());
        check(during > 0.5f && steel_drums_cut() < 0.05f && cut_sound < 1e-6, "CUT dips the synth layer briefly and makes no sound");
    }
    return fails != 0;
}
