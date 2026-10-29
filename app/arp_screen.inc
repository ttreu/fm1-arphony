/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* ARPHONY panel. Included by arp_ui.inc to reuse the LCD painter.
 *
 * Keys:    a key = a chord on that note (in the key's mode and COLOR).
 *          F3..E4: the chord over its own bass; F4..G5: over the key's
 *          tonic (pedal point). SEL held + white key = the key.
 * Buttons: SAVE store a user preset (held + white key: slot 1-16), PLAY start/stop, SEQ a new RANDOM riff (with SEL: new chords), ARP hold the chord (HAND) / back to the
 *          progression (AUTO), HOME riff pages, SEL chord pages, GLO mix,
 *          EDIT sound pages, ENV envelopes, LFO motion, FX effect pages,
 *          OCT-/OCT+ riff octave (with SEL: bass octave), SEL+PLAY tap tempo
 * Knobs:   K1-K4 edit the page, PRESETS preset (with SEL: key), SELECT page */
#include "arp.h"
#ifndef FM1_HOST_TEST
unsigned fm1_audio_load_percent(void); /* app/board/audio.c */
#endif
static unsigned arp_cpu_load(void)
{
#ifdef FM1_HOST_TEST
    return 0; /* the browser has no interrupt budget to show */
#else
    return fm1_audio_load_percent();
#endif
}

/* HOME 1-4: the main settings, to try things out fast. */
enum { PG_HOME1, PG_HOME2, PG_HOME3, PG_HOME4, PG_CHORDS, PG_MIX, PG_MIX2, PG_ARP1, PG_ARP2, PG_HARM1, PG_HARM2, PG_ARP_ENV, PG_HARM_ENV,
       PG_MOTION, PG_FILTER, PG_TAPE, PG_DELAY, PG_REVERB, PG_TEMPO, PG_COLOR, PAGES };
static const struct { char title[15]; uint8_t p[4]; } arp_pages[PAGES] = {
    {"1 HARMONY", {AP_KEY, AP_MODE, AP_PROG, AP_CHORD_LEN}},
    {"2 RIFF", {AP_PATTERN, AP_RATE, AP_LEAD, AP_HARMONY}},
    {"3 GROOVE", {AP_BPM, AP_BASS_RHY, AP_ARP_OCT, AP_TWIN}},
    {"4 FEEL", {AP_GATE, AP_SWING, AP_ACCENT, AP_MUTATE}},
    {"CHORDS", {AP_COLOR, AP_NOTES, AP_BASS_OCT, AP_MIDI_OUT}},
    {"MIX 1/2", {AP_A_LEVEL, AP_BASS_LVL, AP_CHORD_LVL, AP_HARM_FX}},
    {"MIX 2/2", {AP_TWIN_LVL, AP_LEVEL, AP_SPREAD, AP_DRIFT}},
    {"ARP SOUND 1/2", {AP_A_SHAPE, AP_A_DETUNE, AP_A_OSC2, AP_A_TONE}},
    {"ARP SOUND 2/2", {AP_A_FM, AP_A_RATIO, AP_A_TONE_ENV, AP_A_LEVEL}},
    {"HARM SOUND 1/2", {AP_H_SHAPE, AP_H_DETUNE, AP_H_OSC2, AP_H_SUB}},
    {"HARM SOUND 2/2", {AP_H_TONE, AP_H_TONE_ENV, AP_DRIFT, AP_SPREAD}},
    {"ARP ENVELOPE", {AP_A_ATTACK, AP_A_DECAY, AP_A_SUSTAIN, AP_A_RELEASE}},
    {"HARM ENVELOPE", {AP_H_ATTACK, AP_H_DECAY, AP_H_SUSTAIN, AP_H_RELEASE}},
    {"MOTION", {AP_VIB_RATE, AP_VIB_DEPTH, AP_DRIFT, AP_SPREAD}},
    {"FX 1/6 FILTER", {AP_FILTER, AP_RESO, AP_LFO_RATE, AP_LFO_DEPTH}},
    {"FX 2/6 TAPE", {AP_TAPE_TIME, AP_TAPE_FEEDBACK, AP_TAPE_WOW, AP_TAPE_MIX}},
    {"FX 3/6 DELAY", {AP_DELAY_TIME, AP_DELAY_FEEDBACK, AP_DELAY_MIX, AP_SPREAD}},
    {"FX 4/6 SHIMMER", {AP_REV_SIZE, AP_REV_DAMP, AP_SHIMMER, AP_REV_MIX}},
    {"FX 5/6 TEMPO", {AP_BPM, AP_TAPE_SYNC, AP_DELAY_SYNC, AP_LFO_SYNC}},
    {"FX 6/6 COLOR", {AP_DRIVE, AP_TAPE_TONE, AP_HARM_FX, AP_LEVEL}},
};
static unsigned arp_page, arp_ready, arp_full, sel_used, save_used;
static uint32_t arp_keys_before, arp_buttons_before, arp_frame_ms, arp_notice_until;
static const char *arp_notice;
static char arp_notice_text[28];
static uint32_t arp_sig[3];

/* Physical key 0..26 (F3..G5): white keys 0..15, black keys 255. */
static unsigned arp_key_role(unsigned key)
{
    static const uint8_t white[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
    for (unsigned i = 0; i < 16; ++i) if (white[i] == key) return i;
    return 255;
}
static void arp_set_notice(const char *s, uint32_t now) { arp_notice = s; arp_notice_until = now + 1500U; }
static void arp_key_notice(uint32_t now)
{
    char value[16];
    strcpy(arp_notice_text, "KEY ");
    arp_param_text(AP_KEY, arp_get(AP_KEY), value); strcat(arp_notice_text, value); strcat(arp_notice_text, " ");
    arp_param_text(AP_MODE, arp_get(AP_MODE), value); strcat(arp_notice_text, value);
    arp_set_notice(arp_notice_text, now);
}
static void arp_value_notice(unsigned p, uint32_t now)
{
    char value[16];
    strcpy(arp_notice_text, arp_param_info(p)->name); strcat(arp_notice_text, " ");
    arp_param_text(p, arp_get(p), value); strcat(arp_notice_text, value);
    arp_set_notice(arp_notice_text, now);
}
/* Look: one full colour field (the "stage") whose colour follows the chord
 * that sounds - its root around the circle of fifths, light for major, deep
 * for minor, pale while stopped - with the chord name large in the middle,
 * the progression as dots and the riff as thin bars; below a dark footer
 * with the page and the four knobs. */
static const uint16_t stage_major[12] = {0xed31, 0x8d3d, 0xc6f0, 0xd47d, 0x8f13, 0xec74, 0x8ebd, 0xee91, 0xa47d, 0x96b0, 0xec7a, 0x8f7a};
static const uint16_t stage_minor[12] = {0x8a66, 0x3271, 0x7466, 0x71b1, 0x3469, 0x89a9, 0x33b1, 0x8ba6, 0x49b1, 0x4c66, 0x89ae, 0x346e};
static const uint16_t stage_idle[12] = {0xcd53, 0x9d59, 0xbe53, 0xbcf9, 0x9e55, 0xccf5, 0x9e19, 0xce13, 0xacf9, 0xae53, 0xccf7, 0x9e57};
/* Upper half: page and tempo, the four knobs. Lower half: the chord, the
 * progression dots, the riff bars, preset and key. */
enum { KNOBS_TOP = 22, CHORD_TOP = 132, RIFF_TOP = 184, LINE_TOP = 226 };
static uint32_t knob_until[4];                      /* a turned knob shows its value until then */
static unsigned arp_stage_shade(void)
{
    if (!arp_running()) return 64U | arp_get(AP_KEY) % 12U;
    return arp_chord_shade(arp_chord_position());
}
static uint16_t arp_stage(unsigned shade)
{
    unsigned pc = shade & 15U;
    if (pc > 11U) pc = 0;
    return shade & 64U ? stage_idle[pc] : shade & 16U ? stage_minor[pc] : stage_major[pc];
}
static uint16_t arp_ink(unsigned shade) { return (shade & 16U) && !(shade & 64U) ? WHITE : INK; }
static void arp_draw_header(unsigned shade)
{
    uint16_t bg = arp_stage(shade), ink = arp_ink(shade);
    char title[40], value[16];
    strcpy(title, arp_notice ? arp_notice : arp_pages[arp_page].title);
    if (!arp_notice && arp_page == PG_TEMPO) {
        char pct[8]; unsigned l = arp_cpu_load(), n = 0;
        do { pct[n++] = (char)('0' + l % 10U); l /= 10U; } while (l && n < 6);
        strcat(title, "  CPU ");
        while (n) { unsigned len = (unsigned)strlen(title); title[len] = pct[--n]; title[len + 1] = 0; }
        strcat(title, "%");
    }
    title[28] = 0;
    text(8, 7, title, 1, ink, bg);
    if (!arp_notice || strlen(arp_notice) < 26U) {
        arp_param_text(AP_BPM, arp_get(AP_BPM), value);
        strcat(value, arp_bpm_external() ? " EXT" : " BPM");
        text(232U - 6U * (unsigned)strlen(value), 7, value, 1, ink, bg);
    }

}
static void arp_draw_chord(unsigned shade)
{
    uint16_t bg = arp_stage(shade), ink = arp_ink(shade);
    char value[16];
    arp_chord_name(0, value);
    if (!arp_is_hand()) arp_chord_name(arp_chord_position(), value);
    unsigned n = (unsigned)strlen(value), scale = n <= 9U ? 4U : 3U, w = 6U * scale * n;
    text(w < 240U ? (240U - w) / 2U : 0U, CHORD_TOP + 4U, value, scale, ink, bg);
    unsigned count = arp_is_hand() ? 1U : arp_chord_count(), now = arp_chord_position();
    if (count > 8U) count = 8U;
    unsigned x0 = 120U - count * 6U;
    for (unsigned i = 0; i < count; ++i) {
        unsigned big = i == now && arp_running();
        paint_fill(x0 + i * 12U + (big ? 1U : 3U), CHORD_TOP + (big ? 38U : 40U), big ? 8U : 4U, big ? 8U : 4U, ink);
    }
}
static void arp_draw_riff(unsigned shade)
{
    uint16_t bg = arp_stage(shade), ink = arp_ink(shade), head = ink == WHITE ? INK : WHITE;
    unsigned len = arp_pattern_length(), step = arp_step_position(), running = arp_running();
    if (len > 16U) len = 16U;
    unsigned x0 = 120U - len * 6U;
    for (unsigned i = 0; i < len; ++i) {
        unsigned x = x0 + i * 12U;
        int v = arp_pattern_step(i);
        uint16_t c = running && i == step ? head : ink;
        paint_fill(x, RIFF_TOP, 8, 38, bg);
        if (v >= 0) {
            unsigned h = 4U + (unsigned)v * 3U;
            if (h > 36U) h = 36U;
            paint_fill(x, RIFF_TOP + 38U - h, 8, h, c);
        } else paint_fill(x, RIFF_TOP + 36U, 8, 2, c); /* left out: the note before holds */
    }
}
static void arp_draw_line(unsigned shade)
{
    const struct arp_preset *p = arp_current();
    uint16_t bg = arp_stage(shade), ink = arp_ink(shade);
    char line[32], value[16];
    text(8, LINE_TOP + 4U, p->name, 1, ink, bg);
    arp_param_text(AP_KEY, p->value[AP_KEY], value); strcpy(line, value); strcat(line, " ");
    arp_param_text(AP_MODE, p->value[AP_MODE], value); strcat(line, value);
    if (arp_is_hand()) strcat(line, " HAND");
    text(232U - 6U * (unsigned)strlen(line), LINE_TOP + 4U, line, 1, ink, bg);
}
static uint32_t arp_sig4;
static void arp_draw(void)
{
    char value[16];
    unsigned shade = arp_stage_shade();
    uint16_t bg = arp_stage(shade), ink = arp_ink(shade);
    if (!paint_rendering) {
        if (arp_full) {
            paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, bg);
            arp_full = 0; memset(arp_sig, 0, sizeof arp_sig); arp_sig4 = 0; memset(cache, 0xff, sizeof *cache);
        }
        const struct arp_preset *p = arp_current();
        char names[16];
        uint32_t head = signature(arp_page | (arp_cpu_load() << 6) | (arp_bpm() << 14) | (arp_bpm_external() << 22) | (shade << 24),
                                  arp_notice ? arp_notice : "", arp_notice ? (unsigned)strlen(arp_notice) : 0);
        uint32_t chord = shade | (arp_chord_position() << 8) | (arp_chord_count() << 12) | (arp_is_hand() << 16) | (arp_running() << 17);
        for (unsigned i = 0; i < arp_chord_count(); ++i) { arp_chord_name(i, names); chord = signature(chord, names, (unsigned)strlen(names)); }
        uint32_t riff = shade | (arp_step_position() << 8) | (arp_running() << 13) | (arp_pattern_length() << 14);
        for (unsigned i = 0; i < arp_pattern_length(); ++i) riff = riff * 33U + (uint32_t)(arp_pattern_step(i) + 3);
        uint32_t line = signature(shade | (arp_is_hand() << 8), p->name, (unsigned)strlen(p->name));
        line = signature(line, p->value, AP_MODE + 1U);
        if (head != arp_sig[0]) { arp_sig[0] = head; paint_fill(0, 0, FM1_SCREEN_WIDTH, KNOBS_TOP, bg); }
        if (chord != arp_sig[1]) { arp_sig[1] = chord; paint_fill(0, CHORD_TOP, FM1_SCREEN_WIDTH, RIFF_TOP - CHORD_TOP, bg); }
        if (riff != arp_sig[2]) { arp_sig[2] = riff; paint_fill(0, RIFF_TOP, FM1_SCREEN_WIDTH, LINE_TOP - RIFF_TOP, bg); }
        if (line != arp_sig4) { arp_sig4 = line; paint_fill(0, LINE_TOP, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT - LINE_TOP, bg); }
    } else {
        paint_fill(0, paint_top, FM1_SCREEN_WIDTH, PAINT_ROWS, bg); /* compose each band from scratch */
    }
    arp_draw_header(shade);
    for (unsigned col = 0; col < 4; ++col) {
        unsigned param = arp_pages[arp_page].p[col];
        arp_param_text(param, arp_get(param), value);
        knob(col, arp_param_info(param)->name, value, bg, ink, (int32_t)(fm1_millis() - knob_until[col]) < 0);
    }
    arp_draw_chord(shade);
    arp_draw_riff(shade);
    arp_draw_line(shade);
}
/* LEDs: the keys of the current chord's tones. */
static void arp_leds(void)
{
    static uint32_t shown = ~0U;
    uint16_t pcs = arp_running() || arp_is_hand() ? arp_chord_pcs(arp_chord_position()) : 0;
    uint32_t bits = 0;
    for (unsigned key = 0; key < 27; ++key)
        if (pcs & (1U << ((5U + key) % 12U))) bits |= 1U << key;
    if (bits == shown) return;
    for (unsigned key = 0; key < 27; ++key)
        if (((bits ^ shown) >> key) & 1U) fm1_led_set(key + 14U, (int)((bits >> key) & 1U));
    shown = bits;
}
/* PRESETS knob: the factory presets, then the stored user presets. */
static void arp_preset_step(int delta)
{
    unsigned factory = arp_preset_count(), total = factory + ARP_USER_SLOTS;
    int slot = arp_user_slot();
    unsigned at = slot >= 0 ? factory + (unsigned)slot : arp_preset_index();
    for (unsigned guard = 0; guard < total && delta; ++guard) {
        at = (at + (delta > 0 ? 1U : total - 1U)) % total;
        if (at < factory || arp_user_stored(at - factory)) {
            delta += delta > 0 ? -1 : 1;
            if (!delta) break;
        }
    }
    if (at < factory) arp_load(at); else (void)arp_user_load(at - factory);
}
static void arp_save_notice(int result, unsigned slot, uint32_t now)
{
    if (result) { arp_set_notice("SAVE FAILED", now); return; }
    strcpy(arp_notice_text, "SAVED AS USER ");
    char n[3] = {(char)(slot >= 9U ? '1' : '1' + slot), (char)(slot >= 9U ? '0' + slot - 9U : 0), 0};
    strcat(arp_notice_text, n);
    arp_set_notice(arp_notice_text, now);
}
static void arp_page_cycle(unsigned first, unsigned last)
{
    arp_page = arp_page >= first && arp_page < last ? arp_page + 1U : first;
}
static void arp_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    if (!arp_ready) {
        arp_ready = arp_full = 1;
        /* The FM1 tracks are not used: switch them off so the SDK releases
         * their sound engines (heap and audio time). A device preference. */
        for (unsigned t = 0; t < GROOVE_TRACKS; ++t)
            if (groove_track_role(t) != GROOVE_TRACK_OFF) (void)groove_track_role_set(t, GROOVE_TRACK_OFF);
        for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
        arp_keys_before = c->keys; /* keys held at boot do nothing */
        arp_set_notice("ARPHONY", now);
    }
    unsigned changed = groove_master_update(c->knob) != 0;
    uint32_t key_down = c->keys & ~arp_keys_before, pressed = c->buttons & ~arp_buttons_before;
    uint32_t released = arp_buttons_before & ~c->buttons;
    arp_keys_before = c->keys;
    arp_buttons_before = c->buttons;
    unsigned sel = (c->buttons & BUTTON(SCL)) != 0, save = (c->buttons & BUTTON(SAVE)) != 0;
    if (pressed & BUTTON(SCL)) sel_used = 0;
    if (pressed & BUTTON(SAVE)) save_used = 0;
    for (unsigned key = 0; key < 27; ++key) {
        if (!((key_down >> key) & 1U)) continue;
        unsigned pc = (5U + key) % 12U; /* the keys start at F */
        if (save) {                                   /* SAVE held + white key: user preset 1-16 */
            unsigned slot = arp_key_role(key);
            if (slot < 16) { arp_save_notice(arp_user_save(slot), slot, now); save_used = 1; }
        } else if (sel) {
            if (arp_key_role(key) < 16) { arp_set_key(pc); arp_key_notice(now); sel_used = 1; }
        } else {
            arp_hand_chord(pc, key >= 12U);
        }
        changed = 1;
    }
    /* SEL: on release (unless it was held for a combination), the chord pages. */
    if ((released & BUTTON(SCL)) && !sel_used) arp_page = PG_CHORDS;
    if ((released & BUTTON(SAVE)) && !save_used) {  /* SAVE tap: this user preset, or the first free one */
        int slot = arp_user_slot();
        for (unsigned s = 0; slot < 0 && s < ARP_USER_SLOTS; ++s) if (!arp_user_stored(s)) slot = (int)s;
        if (slot < 0) arp_set_notice("ALL FULL: HOLD SAVE + WHITE KEY", now);
        else arp_save_notice(arp_user_save((unsigned)slot), (unsigned)slot, now);
    }
    if (pressed & BUTTON(HOME)) arp_page_cycle(PG_HOME1, PG_HOME4);
    if (pressed & BUTTON(GLO)) arp_page = arp_page == PG_MIX ? PG_MIX2 : PG_MIX;
    if (pressed & BUTTON(EDIT)) arp_page_cycle(PG_ARP1, PG_HARM2);
    if (pressed & BUTTON(ENV)) arp_page = arp_page == PG_ARP_ENV ? PG_HARM_ENV : PG_ARP_ENV;
    if (pressed & BUTTON(LFO)) arp_page = PG_MOTION;
    if (pressed & BUTTON(FX)) arp_page_cycle(PG_FILTER, PG_COLOR);
    if ((pressed & BUTTON(PLAY)) && sel) {
        arp_tap(now);
        sel_used = 1;
        arp_set_notice(arp_bpm_external() ? "TEMPO FOLLOWS MIDI CLOCK" : "TAP TEMPO", now);
    } else if (pressed & BUTTON(PLAY)) {
        unsigned was = arp_running();
        arp_transport(-1);
        arp_set_notice(was ? "STOP" : "PLAY", now);
    }
    if ((pressed & BUTTON(SEQ)) && sel) {          /* SEL + SEQ: new chords */
        arp_random_chords(now);
        sel_used = 1;
        arp_page = PG_HOME1; /* CHORDS: shows RANDOM */
        arp_set_notice("NEW RANDOM CHORDS", now);
    } else if (pressed & BUTTON(SEQ)) {
        arp_random_riff(now);
        arp_page = PG_HOME2; /* RIFF: shows RANDOM */
        arp_set_notice("NEW RANDOM RIFF", now);
    }
    if (pressed & BUTTON(ARP)) {
        if (arp_is_hand()) { arp_auto(); arp_set_notice("AUTO: THE PROGRESSION", now); }
        else { arp_hold(); arp_set_notice("HAND: PLAY CHORDS ON THE KEYS", now); }
    }
    if (pressed & (BUTTON(OCT_DOWN) | BUTTON(OCT_UP))) {
        unsigned p = sel ? AP_BASS_OCT : AP_ARP_OCT;
        arp_edit(p, (pressed & BUTTON(OCT_UP)) ? 1 : -1);
        if (sel) sel_used = 1;
        arp_value_notice(p, now);
    }
    arp_poll(now);
    static uint32_t health_at, health_overruns;
    if (now - health_at >= 500U) {
        struct fm1_audio_stats dma;
        fm1_audio_stats_read(&dma);
        unsigned overrun = dma.overruns != health_overruns && health_at;
        if (arp_guard(arp_cpu_load(), overrun, now) && arp_voice_limit() < 6U) arp_set_notice("CPU: SHORTER ARP TAILS", now);
        else if (overrun) arp_set_notice("AUDIO OVERLOAD", now);
        health_overruns = dma.overruns;
        health_at = now;
        if (arp_page == PG_TEMPO) changed = 1;
    }
    if (c->encoders[0]) arp_page = (unsigned)((int)arp_page + c->encoders[0] % PAGES + PAGES) % PAGES;
    if (c->encoders[6] && sel) {
        arp_edit(AP_KEY, c->encoders[6]); /* SEL + PRESETS: key */
        sel_used = 1;
        arp_key_notice(now);
    } else if (c->encoders[6]) arp_preset_step(c->encoders[6]);
    /* The first detent of a knob only shows its value; the next ones (while
     * the value shows) change it. */
    for (unsigned col = 0; col < 4; ++col) {
        int delta = c->encoders[2 + col];
        if (!delta) continue;
        if (!knob_until[col]) delta += delta > 0 ? -1 : 1;
        if (delta) arp_edit(arp_pages[arp_page].p[col], delta);
        knob_until[col] = now + 1500U;
    }
    changed |= pressed != 0 || released != 0 || encoder_active(c->encoders);
    if (arp_notice && (int32_t)(now - arp_notice_until) >= 0) { arp_notice = NULL; changed = 1; }
    static unsigned page_before;                           /* another page: its names, not old values */
    if (arp_page != page_before) { for (unsigned col = 0; col < 4; ++col) knob_until[col] = 0; page_before = arp_page; changed = 1; }
    for (unsigned col = 0; col < 4; ++col)                 /* a value box turns back into its name */
        if (knob_until[col] && (int32_t)(now - knob_until[col]) >= 0) { knob_until[col] = 0; changed = 1; }
    arp_leds();
    if (saved_cache.shown_page == ~0U) arp_full = 1; /* LCD transfer error: repaint all */
    unsigned period = arp_running() ? 40U : 100U; /* the playhead moves */
    if (changed || arp_full || now - arp_frame_ms >= period) {
        arp_frame_ms = now;
        saved_cache.shown_page = 0;
        paint_priority = 0;
        cache = &saved_cache;
        arp_draw();
    }
    paint_flush();
}
