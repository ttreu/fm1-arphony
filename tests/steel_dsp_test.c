/* Native unit test of the GLIDER voice DSP. */
#ifndef FM1_HOST_TEST
#define FM1_HOST_TEST
#endif
#include "../app/steel_dsp.c"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static double block_rms(void)
{
    int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out);
    double s = 0; for (int i = 0; i < FM1_BLOCK_WORDS; ++i) s += (double)out[i] * out[i];
    return sqrt(s / FM1_BLOCK_WORDS) / 8388608.0;
}
static double run(int blocks) { double r = 0; for (int i = 0; i < blocks; ++i) r = block_rms(); return r; }
int main(void)
{
    struct steel_patch p = {0};
    p.shape = 0; p.detune = 1; p.tone = 120; p.spread = 0;
    p.attack = 1.0f / (0.002f * STEEL_RATE); p.decay = 0.001f; p.sustain = 1.0f;
    float release_s = 2.0f;
    p.release = 1.0f - steel_exp2(-9.965784f / (release_s * STEEL_RATE));
    p.vib_rate = 5; p.glide_step = 1.0f / (0.5f * STEEL_RATE); p.level = 0.5f;
    steel_patches[0] = p; steel_patch_index = 0;
    steel_commands[0].target = steel_commands[0].target2 = 57; steel_commands[0].gate = 1; steel_commands[0].note_serial = 1;
    double on = run(200);
    steel_commands[0].gate = 0;
    double after1 = run((int)(1.0 * STEEL_RATE / 128));
    double db = 20 * log10(after1 / on);
    printf("     release 2 s: after 1 s %.1f dB (expect about -30)\n", db);
    check(db < -24 && db > -36, "release follows its time constant");
    /* Overtones: harmonics 2..5 appear as pure partials above the played tone. */
    {
        struct steel_patch q = {0};
        q.shape = 0; q.tone = 140; q.sustain = 1.0f; q.decay = 0.001f; q.release = 0.001f;
        q.attack = 1.0f / (0.002f * STEEL_RATE); q.level = 0.5f; q.glide_step = 1;
        static float wave[44100];
        double with[6], without[6];
        for (int pass = 0; pass < 2; ++pass) {
            for (int k = 0; k < 4; ++k) q.overtone[k] = pass ? 0.4f : 0.0f;
            steel_patches[0] = q; steel_patch_index = 0;
            memset(voices, 0, sizeof voices);
            steel_commands[1].target = steel_commands[1].target2 = 57; steel_commands[1].gate = 1; steel_commands[1].note_serial++;
            for (int b = 0; b < 344 + 20; ++b) {
                int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out);
                if (b >= 20) for (int i = 0; i < 128 && (b - 20) * 128 + i < 44100; ++i) wave[(b - 20) * 128 + i] = out[2 * i] / 8388608.0f;
            }
            steel_commands[1].gate = 0; for (int b = 0; b < 800; ++b) { int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out); }
            for (int h = 1; h <= 5; ++h) {
                double w = 2 * M_PI * 220.0 * h / 44100.0, c = 2 * cos(w), s1 = 0, s2 = 0;
                for (int i = 0; i < 44100; ++i) { double t = wave[i] + c * s1 - s2; s2 = s1; s1 = t; }
                (pass ? with : without)[h] = s1 * s1 + s2 * s2 - c * s1 * s2;
            }
        }
        printf("     harmonic 2..5 energy vs fundamental: off %.1e/%.1e/%.1e/%.1e  on %.2f/%.2f/%.2f/%.2f\n",
               without[2] / without[1], without[3] / without[1], without[4] / without[1], without[5] / without[1],
               with[2] / with[1], with[3] / with[1], with[4] / with[1], with[5] / with[1]);
        int ok = 1;
        for (int h = 2; h <= 5; ++h) ok &= with[h] / with[1] > 0.05 && with[h] / with[1] > 100 * without[h] / without[1];
        check(ok, "OVERTONE adds pure harmonics 2..5 to a sine tone");
    }
    /* Detune LFO: two oscillators at 0 cents only beat once the LFO sweeps. */
    {
        struct steel_patch q = {0};
        q.shape = 0; q.osc2 = 1.0f; q.tone = 140; q.sustain = 1.0f; q.decay = 0.001f; q.release = 0.001f;
        q.attack = 1.0f / (0.002f * STEEL_RATE); q.level = 0.5f; q.glide_step = 1;
        double depth[2];
        for (int pass = 0; pass < 2; ++pass) {
            q.detune = 0.0f; q.detune_lfo = pass ? 25.0f : 0.0f;
            steel_patches[0] = q; steel_patch_index = 0;
            memset(voices, 0, sizeof voices);
            steel_commands[2].target = steel_commands[2].target2 = 57; steel_commands[2].gate = 1; steel_commands[2].note_serial++;
            double sum = 0, sq = 0, acc = 0; int n = 0;
            for (int b = 0; b < 344 * 4; ++b) {   /* RMS over 20-block (58 ms) windows */
                int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out);
                for (int i = 0; i < 128; ++i) acc += (double)out[2 * i] * out[2 * i];
                if ((b + 1) % 20 == 0) {
                    double e = sqrt(acc / (20 * 128)) / 8388608.0; acc = 0;
                    if (b > 100) { sum += e; sq += e * e; ++n; }
                }
            }
            double mean = sum / n; depth[pass] = sqrt(sq / n - mean * mean) / mean;
            steel_commands[2].gate = 0; for (int b = 0; b < 800; ++b) { int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out); }
        }
        printf("     level wobble without/with detune LFO: %.3f / %.3f\n", depth[0], depth[1]);
        check(depth[0] < 0.02 && depth[1] > 0.1, "DET LFO makes two unison oscillators beat slowly");
    }
    /* Undertones and FLOAT. */
    {
        struct steel_patch q = {0};
        q.shape = 0; q.tone = 140; q.sustain = 1.0f; q.decay = 0.001f; q.release = 0.001f;
        q.attack = 1.0f / (0.002f * STEEL_RATE); q.level = 0.5f; q.glide_step = 1;
        q.undertone[0] = 0.5f; q.undertone[1] = 0.3f;
        steel_patches[0] = q; steel_patch_index = 0;
        memset(voices, 0, sizeof voices);
        steel_commands[3].target = steel_commands[3].target2 = 69; steel_commands[3].gate = 1; steel_commands[3].note_serial++;
        static float wave[44100];
        for (int b = 0; b < 364; ++b) {
            int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out);
            if (b >= 20) for (int i = 0; i < 128 && (b - 20) * 128 + i < 44100; ++i) wave[(b - 20) * 128 + i] = out[2 * i] / 8388608.0f;
        }
        double e[3]; const double hz[3] = {440.0, 220.0, 440.0 / 3};
        for (int k = 0; k < 3; ++k) {
            double w = 2 * M_PI * hz[k] / 44100, c = 2 * cos(w), s1 = 0, s3 = 0;
            for (int i = 0; i < 44100; ++i) { double t2 = wave[i] + c * s1 - s3; s3 = s1; s1 = t2; }
            e[k] = s1 * s1 + s3 * s3 - c * s1 * s3;
        }
        printf("     undertones vs tone: 1/2 %.2f, 1/3 %.2f\n", e[1] / e[0], e[2] / e[0]);
        check(e[1] / e[0] > 0.1 && e[2] / e[0] > 0.03, "UNDERTONE adds the octave and twelfth below");
        steel_commands[3].gate = 0; for (int b = 0; b < 800; ++b) { int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out); }
        /* FLOAT: the 2nd harmonic's level wanders over time; without it, it stays put. */
        double spread[2];
        for (int pass = 0; pass < 2; ++pass) {
            q.undertone[0] = q.undertone[1] = 0; q.overtone[0] = 0.4f; q.overtone[1] = q.overtone[2] = q.overtone[3] = 0;
            q.float_depth = pass ? 1.0f : 0.0f; q.float_rate = 0.5f;
            steel_patches[0] = q; steel_patch_index = 0;
            memset(voices, 0, sizeof voices);
            steel_commands[3].gate = 1; steel_commands[3].note_serial++;
            double lo = 1e30, hi = 0;
            for (int win = 0; win < 24; ++win) {          /* 24 windows of 0.25 s = 6 s */
                static float seg[11008];
                for (int b = 0; b < 86; ++b) {
                    int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out);
                    for (int i = 0; i < 128; ++i) seg[b * 128 + i] = out[2 * i] / 8388608.0f;
                }
                if (win < 2) continue;
                double w = 2 * M_PI * 880.0 / 44100, c = 2 * cos(w), s1 = 0, s3 = 0;
                for (int i = 0; i < 11008; ++i) { double t2 = seg[i] + c * s1 - s3; s3 = s1; s1 = t2; }
                double en = s1 * s1 + s3 * s3 - c * s1 * s3;
                lo = en < lo ? en : lo; hi = en > hi ? en : hi;
            }
            spread[pass] = 10 * log10(hi / lo);
            steel_commands[3].gate = 0; for (int b = 0; b < 800; ++b) { int32_t out[FM1_BLOCK_WORDS]; memset(out, 0, sizeof out); steel_render(out); }
        }
        printf("     2nd harmonic level range over 6 s: FLOAT off %.1f dB, on %.1f dB\n", spread[0], spread[1]);
        check(spread[0] < 1.0 && spread[1] > 3.0, "FLOAT makes the partials hover");
    }
    return fails != 0;
}
