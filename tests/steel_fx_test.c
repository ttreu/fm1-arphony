/* Native unit test of the GLIDER effects (filter, tape echo, delay). */
#ifndef FM1_HOST_TEST
#define FM1_HOST_TEST
#endif
#include "../app/steel_fx.c"
#include <math.h>
#include <stdio.h>
#include <string.h>
struct steel_patch steel_patches[2];
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static float L[FM1_BLOCK_FRAMES], R[FM1_BLOCK_FRAMES];
static void reset(void)
{
    memset(delay_l, 0, sizeof delay_l); memset(delay_r, 0, sizeof delay_r); memset(tape_buf, 0, sizeof tape_buf);
    memset(&dl_line, 0, sizeof dl_line); memset(&dr_line, 0, sizeof dr_line); memset(&tape_line, 0, sizeof tape_line);
    memset(ladder, 0, sizeof ladder); tape_lp = tape_hp = 0; tape_delay = delay_delay = 0;
}
static struct steel_patch base(void)
{
    struct steel_patch p; memset(&p, 0, sizeof p);
    p.tape_tone = 0.5f; p.drive = 1.0f; p.tape_time = 0.3f * STEEL_RATE; p.delay_time = 0.5f * STEEL_RATE;
    return p;
}
int main(void)
{
    /* Delay timing: an impulse returns after the set time on the left. */
    reset(); struct steel_patch p = base(); p.delay_mix = 1.0f;
    tape_delay = p.tape_time; delay_delay = p.delay_time;
    int peak_at = -1; float peak = 0;
    for (int b = 0; b < 400; ++b) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        if (b == 0) for (int i = 0; i < 8; ++i) L[i] = R[i] = 0.5f;
        steel_fx(L, R, &p, NULL);
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i)
            if (b > 2 && fabsf(L[i]) > peak) { peak = fabsf(L[i]); peak_at = b * FM1_BLOCK_FRAMES + i; }
    }
    printf("     delay echo at %.1f ms (set 500 ms), level %.3f\n", peak_at * 1000.0 / STEEL_RATE, peak);
    check(fabs(peak_at * 1000.0 / STEEL_RATE - 500.0) < 3.0, "ping-pong delay time is accurate");

    /* Tape at 120% self-oscillates but stays bounded and finite. */
    reset(); p = base(); p.tape_feedback = 1.2f; p.tape_mix = 0.9f; p.tape_wow = 0.5f; p.tape_tone = 0.3f;
    tape_delay = p.tape_time;
    double late = 0; float maxabs = 0; int finite = 1;
    for (int b = 0; b < 344 * 20; ++b) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        if (b < 4) for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) L[i] = R[i] = (float)((i * 7919 % 200) - 100) / 200.0f;
        steel_fx(L, R, &p, NULL);
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) {
            if (!isfinite(L[i])) finite = 0;
            if (fabsf(L[i]) > maxabs) maxabs = fabsf(L[i]);
            if (b >= 344 * 19) late += L[i] * L[i];
        }
    }
    late = sqrt(late / (344.0 * FM1_BLOCK_FRAMES));
    printf("     tape 120%%: rms after 19 s %.3f, peak %.3f\n", late, maxabs);
    check(finite && maxabs < 2.5f, "self-oscillating tape stays bounded");
    check(late > 0.02, "tape keeps oscillating on its own");

    /* Tape at 50% dies away. */
    reset(); p = base(); p.tape_feedback = 0.5f; p.tape_mix = 0.9f; tape_delay = p.tape_time;
    late = 0;
    for (int b = 0; b < 344 * 6; ++b) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        if (b < 4) for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) L[i] = R[i] = 0.3f;
        steel_fx(L, R, &p, NULL);
        if (b >= 344 * 5) for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) late += L[i] * L[i];
    }
    check(sqrt(late / (344.0 * FM1_BLOCK_FRAMES)) < 1e-3, "tape at 50% decays to silence");

    /* Ladder: 4 kHz sine through a 200 Hz cutoff is strongly attenuated. */
    reset(); p = base(); p.filter_on = 1; p.cutoff = 20.0f + 0; p.drive = 1.0f; p.resonance = 0;
    p.cutoff = 69.0f + 12.0f * log2f(200.0f / 440.0f);
    double in = 0, out = 0; float ph = 0;
    for (int b = 0; b < 200; ++b) {
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) { ph += 4000.0f / STEEL_RATE; if (ph >= 1) ph -= 1; L[i] = R[i] = 0.3f * sinf(6.2831853f * ph); }
        if (b > 50) for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) in += L[i] * L[i];
        steel_fx(L, R, &p, NULL);
        if (b > 50) for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) out += L[i] * L[i];
    }
    double att = 10 * log10(out / in);
    printf("     ladder 200 Hz on 4 kHz: %.1f dB\n", att);
    check(att < -60, "24 dB ladder attenuates far above cutoff");
    /* Maximum resonance and drive: stable. */
    reset(); p.resonance = 3.9f; p.drive = 5.0f; p.cutoff = 70.0f; maxabs = 0; finite = 1;
    for (int b = 0; b < 2000; ++b) {
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) L[i] = R[i] = (b < 2) ? 0.8f : 0.0f;
        steel_fx(L, R, &p, NULL);
        for (int i = 0; i < FM1_BLOCK_FRAMES; ++i) { if (!isfinite(L[i])) finite = 0; if (fabsf(L[i]) > maxabs) maxabs = fabsf(L[i]); }
    }
    printf("     ladder max resonance peak %.3f\n", maxabs);
    check(finite && maxabs < 4.0f, "ladder stays stable at full resonance and drive");
    return fails != 0;
}
