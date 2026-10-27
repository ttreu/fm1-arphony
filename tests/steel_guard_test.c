/* Native test: the CPU guard sheds tones under load and recovers.
 * Build: see tests/run_native.sh */
#include "../app/steel_internal.h"
#include <stdio.h>
static int fails;
static void check(int ok, const char *what) { printf("%s %s\n", ok ? "PASS" : "FAIL", what); fails += !ok; }
static int32_t out[2 * FM1_BLOCK_FRAMES];
static void render(unsigned blocks) { while (blocks--) steel_render(out); }
static unsigned count(uint16_t bits) { unsigned n = 0; for (; bits; bits &= bits - 1U) ++n; return n; }
static void set(unsigned p, unsigned v) { steel_edit(p, (int)v - (int)steel_get(p)); }
int main(void)
{
    steel_init();
    set(SP_ATTACK, 0); set(SP_RELEASE, 99); /* long tails */
    for (unsigned w = 0; w < 12; ++w) steel_white(w);
    render(50);
    check(count(steel_white_state()) == 12 && steel_tone_limit() == 16, "12 tones latched, limit 16");
    uint32_t t = 10000;
    check(!steel_guard(60, 0, t) && steel_tone_limit() == 16, "normal load changes nothing");
    check(steel_guard(95, 0, t += 1000) && steel_tone_limit() == 11, "high load: limit drops to 11");
    check(count(steel_white_state()) == 11 && !(steel_white_state() & 1U), "the oldest tone (first key) gave way");
    check(!steel_guard(95, 0, t += 400) && steel_tone_limit() == 11, "waits for the load reading to fall");
    check(steel_guard(95, 0, t += 700) && steel_tone_limit() == 10, "still high a second later: limit 10");
    check(steel_guard(10, 1, t += 100) && steel_tone_limit() == 9, "missed audio blocks act at once: limit 9");
    render(60); /* ~170 ms: quick fades are done */
    check((steel_ringing() & 7U) == 0, "dropped tones fade out quickly");
    steel_white(13);
    check(count(steel_white_state()) == 9 && !(steel_white_state() & 8U) && (steel_white_state() & (1U << 13)),
          "at the limit a new tone replaces the oldest one");
    render(60);
    uint16_t before = steel_ringing();
    steel_white(13); /* normal release: a long tail */
    render(20);
    uint16_t tail = steel_ringing() & (uint16_t)~before;
    check(count(tail) == 1, "a released tone rings on");
    check(!steel_guard(95, 0, t += 2000) && steel_tone_limit() == 9 && count(steel_white_state()) == 8,
          "under load the release tail is faded first, no tone dropped");
    render(60);
    check(!(steel_ringing() & tail), "the tail is gone");
    unsigned rises = 0;
    for (unsigned i = 0; i < 60; ++i) rises += steel_guard(50, 0, t += 500);
    check(steel_tone_limit() == 16 && rises == 7, "after calm periods the limit returns to 16");
    steel_all_off();
    for (unsigned w = 0; w < 4; ++w) steel_white(w);
    steel_guard(99, 1, t += 2000);
    check(count(steel_white_state()) == 4 && steel_tone_limit() == 16, "never fewer than 4 tones");
    return fails ? 1 : 0;
}
