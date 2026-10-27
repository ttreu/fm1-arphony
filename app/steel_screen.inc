/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GLIDER panel. Included by screen.c to reuse its LCD painter.
 *
 * Keys:    white = latch a tone, black = latch a glide of its white key(s)
 *          hold SEL + black key = flip that key's glide direction
 * Buttons: HOME play page, EDIT sound pages, ENV envelope, LFO vibrato,
 *          SEL key/scale/glide, FX effect pages (filter, tape, delay,
 *          reverb, tempo, color), PLAY all tones off, SEL+PLAY tap tempo,
 *          OCT-/OCT+ octave
 * Knobs:   K1-K4 edit the page, PRESETS preset, SELECT page
 *
 * Drum mode (tap SEQ): every key plays/selects its drum slot (A = classic
 * hi-hat, all others glitch sounds); hold SEQ + white keys = 16 steps;
 * PLAY starts/stops the drums; ARP = a new random beat; EDIT drum pages;
 * SELECT slot; HOME or any
 * page button returns to the tones (the drums keep running). */
#include "steel.h"
#include "steel_drums.h"
#ifndef FM1_HOST_TEST
unsigned fm1_audio_load_percent(void); /* app/board/audio.c */
#endif
static unsigned steel_cpu_load(void)
{
#ifdef FM1_HOST_TEST
    return 0; /* the browser has no interrupt budget to show */
#else
    return fm1_audio_load_percent();
#endif
}

enum { SPAGE_PLAY, SPAGE_SOUND, SPAGE_SOUND2, SPAGE_SOUND3, SPAGE_SOUND4, SPAGE_SOUND5, SPAGE_ENV, SPAGE_VIB, SPAGE_SCALE,
       SPAGE_FILTER, SPAGE_TAPE, SPAGE_DELAY, SPAGE_REVERB, SPAGE_TEMPO, SPAGE_COLOR, SPAGES };
static const struct { char title[14]; uint8_t p[4]; } steel_pages[SPAGES] = {
    {"PLAY", {SP_GLIDE_TIME, SP_GLIDE_STEPS, SP_TONE, SP_OVERTONES}},
    {"SOUND 1/5", {SP_SHAPE, SP_DETUNE, SP_OSC2, SP_SUB}},
    {"SOUND 2/5", {SP_TONE, SP_TONE_ENV, SP_DRIFT, SP_SPREAD}},
    {"SOUND 3/5", {SP_OVERTONES, SP_OVERTONE_TILT, SP_UNDERTONES, SP_FLOAT}},
    {"SOUND 4/5", {SP_FLOAT_RATE, SP_DETUNE_LFO, SP_DETUNE, SP_DRIFT}},
    {"SOUND 5/5", {SP_INTERVAL, SP_IV_MODE, SP_OSC2, SP_DETUNE}},
    {"ENVELOPE", {SP_ATTACK, SP_DECAY, SP_SUSTAIN, SP_RELEASE}},
    {"VIBRATO", {SP_VIB_RATE, SP_VIB_DEPTH, SP_VIB_DELAY, SP_LEVEL}},
    {"KEY / GLIDE", {SP_ROOT, SP_SCALE, SP_TUNING, SP_GLIDE_CURVE}},
    {"FX 1/6 FILTER", {SP_FILTER, SP_RESO, SP_LFO_RATE, SP_LFO_DEPTH}},
    {"FX 2/6 TAPE", {SP_TAPE_TIME, SP_TAPE_FEEDBACK, SP_TAPE_WOW, SP_TAPE_MIX}},
    {"FX 3/6 DELAY", {SP_DELAY_TIME, SP_DELAY_FEEDBACK, SP_DELAY_MIX, SP_SPREAD}},
    {"FX 4/6 SHIMMER", {SP_REV_SIZE, SP_REV_DAMP, SP_SHIMMER, SP_REV_MIX}},
    {"FX 5/6 TEMPO", {SP_BPM, SP_TAPE_SYNC, SP_DELAY_SYNC, SP_LFO_SYNC}},
    {"FX 6/6 COLOR", {SP_DRIVE, SP_TAPE_TONE, SP_TONE, SP_LEVEL}},
};
static unsigned steel_page, steel_ready, steel_full;
static unsigned drum_mode, drum_slot = DRUM_KEY_HAT, drum_page, seq_used;
enum { DPAGES = 5 };
static const struct { char title[16]; uint8_t p[4]; } drum_pages[DPAGES] = {
    {"DRUM 1/5 SOUND", {DP_SOURCE, DP_PITCH, DP_DECAY, DP_COLOR}},
    {"DRUM 2/5 RHYTHM", {DP_PROB, DP_RATCHET, DP_MICRO, DP_SCATTER}},
    {"DRUM 3/5 SPACE", {DP_PAN_MODE, DP_PAN, DP_TAPE, DP_SHIMMER}},
    {"DRUM 4/5 SLOT", {DP_LEVEL, DP_RANDOM, DP_RATE, 253}},
    {"DRUM 5/5 KIT", {255, 252, 254, 253}}, /* 255 drum level, 252 hiss bed, 254 BPM, 253 unused */
};
static uint32_t steel_keys_before, steel_buttons_before, steel_frame_ms, steel_notice_until;
static const char *steel_notice;
static char steel_key_text[24];
/* Notice with the new key, e.g. "KEY D MINOR". */
static void steel_key_notice(uint32_t now)
{
    char value[12];
    steel_param_text(SP_ROOT, steel_get(SP_ROOT), value);
    size_t n = strlen(value);
    while (n && value[n - 1] >= '0' && value[n - 1] <= '9') value[--n] = 0; /* note name without octave */
    strcpy(steel_key_text, "KEY "); strcat(steel_key_text, value); strcat(steel_key_text, " ");
    strcat(steel_key_text, steel_scale_name(steel_get(SP_SCALE)));
    steel_notice = steel_key_text; steel_notice_until = now + 1500U;
}
static uint32_t steel_sig[4];

static void steel_set_notice(const char *s, uint32_t now) { steel_notice = s; steel_notice_until = now + 1500U; }

/* Keyboard picture: 16 white keys 15 px wide, black keys on top. */
static void steel_keyboard(uint16_t whites, uint16_t blacks, uint16_t glided)
{
    static const uint8_t black_after[STEEL_BLACK] = {0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14};
    for (unsigned w = 0; w < STEEL_WHITE; ++w) {
        uint16_t c = (whites >> w) & 1U ? ((glided >> w) & 1U ? AMBER : GREEN) : PANEL;
        paint_fill(w * 15U, 70, 14, 80, c);
    }
    for (unsigned b = 0; b < STEEL_BLACK; ++b) {
        unsigned x = black_after[b] * 15U + 10U;
        paint_fill(x - 1U, 69, 11, 47, BACK);
        paint_fill(x, 70, 9, 45, (blacks >> b) & 1U ? AMBER : DIM);
    }
}
static void drum_knob_text(unsigned col, char label[12], char value[12])
{
    unsigned param = drum_pages[drum_page].p[col];
    if (param == 255) { strcpy(label, "DRUM LVL"); steel_param_text(SP_LEVEL, steel_drums_level(), value); return; }
    if (param == 254) { strcpy(label, "BPM"); steel_param_text(SP_BPM, steel_get(SP_BPM), value); return; }
    if (param == 253) { strcpy(label, "-"); strcpy(value, ""); return; }
    if (param == 252) { strcpy(label, "HISS"); steel_param_text(SP_LEVEL, steel_drums_hiss(), value); return; }
    strcpy(label, steel_drums_param_name(param));
    steel_drums_param_text(drum_slot, param, value);
}
static void steel_draw_drums(uint32_t activity)
{
    char line[32], value[12];
    unsigned running = steel_drums_running(), now_step = steel_drums_step();
    uint16_t steps = steel_drums_steps(drum_slot);
    text(6, 6, "DRUMS", 2, ACCENT, BACK);
    text(76, 10, running ? "RUN" : "STOP", 1, running ? GREEN : DIM, BACK);
    steel_param_text(SP_BPM, steel_get(SP_BPM), value);
    text(168, 6, value, 2, steel_bpm_external() ? GREEN : AMBER, BACK);
    text(168, 22, steel_bpm_external() ? "EXT BPM" : "BPM", 1, DIM, BACK);
    steel_drums_slot_name(drum_slot, value);
    static const char notes[12][3] = {"F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E"};
    strcpy(line, notes[drum_slot % 12U]);
    if (drum_slot >= 12) strcat(line, drum_slot >= 24 ? "''" : "'");
    strcat(line, "  "); strcat(line, value);
    text(6, 30, line, 2, TEXT, BACK);
    /* 16 steps of the selected slot; the playhead is outlined when running. */
    for (unsigned s = 0; s < DRUM_STEPS; ++s) {
        unsigned x = 2U + s * 15U;
        uint16_t c = (steps >> s) & 1U ? (s % 4U ? GREEN : ACCENT) : PANEL;
        if (running && s == now_step) paint_fill(x - 1U, 61, 15, 30, AMBER);
        paint_fill(x, 62, 13, 28, c);
    }
    /* Mini keyboard: slots that just played flash; the selected one is outlined. */
    for (unsigned key = 0; key < DRUM_SLOTS; ++key) {
        unsigned x = 3U + key * 8U, black = steel_key_role(key) >= 16;
        uint16_t c = (activity >> key) & 1U ? AMBER : black ? DIM : PANEL;
        if (key == drum_slot) paint_fill(x - 1U, black ? 101 : 111, 9, black ? 14 : 34, TEXT);
        paint_fill(x, black ? 102 : 112, 7, black ? 12 : 32, c);
    }
}
static uint32_t drum_activity_shown;
static void steel_draw(void)
{
    const struct steel_preset *p = steel_current();
    uint16_t whites = steel_white_state(), blacks = steel_black_state(), glided = steel_white_glided();
    if (drum_mode) {
        whites = steel_drums_steps(drum_slot);
        blacks = (uint16_t)(drum_slot | (steel_drums_step() << 5) | (steel_drums_running() << 9));
        glided = (uint16_t)drum_activity_shown;
    }
    if (!paint_rendering) {
        if (steel_full) {
            paint_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, BACK);
            steel_full = 0; memset(steel_sig, 0, sizeof steel_sig); memset(cache, 0xff, sizeof *cache);
        }
        uint32_t s0 = signature(steel_preset_index() | (steel_bpm() << 4) | (steel_bpm_external() << 12) |
                                (drum_mode << 13) | (drum_slot << 14) | (steel_drums_running() << 19) |
                                (steel_drums_get(drum_slot, DP_SOURCE) << 20) | ((uint32_t)(steel_octave() + 2) << 28),
                                p->value, SP_ROOT + 4U);
        uint32_t s1 = (whites | ((uint32_t)blacks << 16)) ^ (drum_mode ? drum_activity_shown * 2654435761U : 0U);
        uint32_t s2 = signature(steel_page | (drum_mode << 5) | (drum_page << 6) | (steel_cpu_load() << 9), steel_notice ? steel_notice : "", steel_notice ? (unsigned)strlen(steel_notice) : 0);
        if (s0 != steel_sig[0]) { steel_sig[0] = s0; paint_fill(0, 0, FM1_SCREEN_WIDTH, 60, BACK); }
        if (s1 != steel_sig[1]) { steel_sig[1] = s1; paint_fill(0, 68, FM1_SCREEN_WIDTH, 84, BACK); }
        if (s2 != steel_sig[2]) { steel_sig[2] = s2; paint_fill(0, 156, FM1_SCREEN_WIDTH, 36, BACK); }
    } else {
        paint_fill(0, paint_top, FM1_SCREEN_WIDTH, PAINT_ROWS, BACK); /* compose each band from scratch */
    }
    if (drum_mode) {
        steel_draw_drums(drum_activity_shown);
        text(6, 160, steel_notice ? steel_notice : drum_pages[drum_page].title, steel_notice ? 1 : 2,
             steel_notice ? AMBER : TEXT, BACK);
        if (!steel_notice) text(6, 180, "SEQ+WHITE = STEPS  ARP = RANDOM", 1, DIM, BACK);
        for (unsigned col = 0; col < 4; ++col) {
            char label[12], value[12];
            drum_knob_text(col, label, value);
            knob(col, label, value, colors[col * 2U]);
        }
        return;
    }
    /* Header: preset, key, scale and glide. */
    char line[32], value[12];
    text(6, 6, p->name, 2, ACCENT, BACK);
    steel_param_text(SP_ROOT, p->value[SP_ROOT], value);
    strcpy(line, value); strcat(line, " "); strcat(line, steel_scale_name(p->value[SP_SCALE]));
    strcat(line, p->value[SP_TUNING] ? " JUST" : " EQUAL");
    text(6, 28, line, 1, TEXT, BACK);
    steel_param_text(SP_GLIDE_STEPS, p->value[SP_GLIDE_STEPS], value);
    strcpy(line, "GLIDE "); strcat(line, value); strcat(line, " STEP");
    if (steel_octave()) {
        char o[3] = {steel_octave() < 0 ? '-' : '+', (char)('0' + (steel_octave() < 0 ? -steel_octave() : steel_octave())), 0};
        strcat(line, "   OCT "); strcat(line, o);
    }
    text(6, 42, line, 1, DIM, BACK);
    /* Tempo, top right: EXT when following MIDI clock. */
    steel_param_text(SP_BPM, p->value[SP_BPM], value);
    text(168, 28, value, 2, steel_bpm_external() ? GREEN : AMBER, BACK);
    text(168, 44, steel_bpm_external() ? "EXT BPM" : "BPM", 1, DIM, BACK);
    steel_keyboard(whites, blacks, glided);
    /* Page title or a short notice. */
    text(6, 160, steel_notice ? steel_notice : steel_pages[steel_page].title, steel_notice ? 1 : 2,
         steel_notice ? AMBER : TEXT, BACK);
    if (!steel_notice && steel_page == SPAGE_TEMPO) {
        /* CPU load of the audio interrupt, for checking the hardware. */
        char load[32] = "CPU ", pct[8];
        unsigned l = steel_cpu_load(), n = 0;
        do { pct[n++] = (char)('0' + l % 10U); l /= 10U; } while (l && n < 6);
        while (n) { unsigned len = (unsigned)strlen(load); load[len] = pct[--n]; load[len + 1] = 0; }
        strcat(load, "%  SEL+PLAY = TAP");
        text(6, 180, load, 1, steel_cpu_load() > 85U ? AMBER : DIM, BACK);
    } else if (!steel_notice) text(6, 180, steel_page == SPAGE_SCALE ? "HOLD SEL + WHITE KEY = KEY" : "BLACK: 1 CLICK UP  2 CLICKS DOWN", 1, DIM, BACK);
    for (unsigned col = 0; col < 4; ++col) {
        unsigned param = steel_pages[steel_page].p[col];
        steel_param_text(param, steel_get(param), value);
        knob(col, steel_param_info(param)->name, value, colors[col * 2U]);
    }
}
static void steel_leds(uint16_t whites, uint16_t blacks, uint32_t extra)
{
    static uint32_t shown = ~0U;
    uint32_t bits = extra;
    for (unsigned key = 0; key < 27; ++key) {
        unsigned role = steel_key_role(key);
        if (role < 16 ? (whites >> role) & 1U : role < 27 && ((blacks >> (role - 16U)) & 1U)) bits |= 1U << key;
    }
    if (bits == shown) return;
    for (unsigned key = 0; key < 27; ++key)
        if (((bits ^ shown) >> key) & 1U) fm1_led_set(key + 14U, (int)((bits >> key) & 1U));
    shown = bits;
}
static void steel_control(const struct fm1_controls *c)
{
    uint32_t now = fm1_millis();
    if (!steel_ready) {
        steel_ready = steel_full = 1;
        /* The FM1 tracks are not used: switch them off so the SDK releases
         * their sound engines (heap and audio time). A device preference. */
        for (unsigned t = 0; t < GROOVE_TRACKS; ++t)
            if (groove_track_role(t) != GROOVE_TRACK_OFF) (void)groove_track_role_set(t, GROOVE_TRACK_OFF);
        for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
        steel_keys_before = c->keys; /* keys held at boot do nothing */
        steel_set_notice("GLIDER", now);
    }
    unsigned changed = groove_master_update(c->knob) != 0;
    uint32_t key_down = c->keys & ~steel_keys_before, pressed = c->buttons & ~steel_buttons_before;
    uint32_t released = steel_buttons_before & ~c->buttons;
    steel_keys_before = c->keys;
    steel_buttons_before = c->buttons;
    unsigned seq_held = (c->buttons & BUTTON(SEQ)) != 0;
    if (pressed & BUTTON(SEQ)) seq_used = 0;
    for (unsigned key = 0; key < 27; ++key) {
        if (!((key_down >> key) & 1U)) continue;
        unsigned role = steel_key_role(key);
        if (drum_mode && seq_held && role < 16) { steel_drums_toggle_step(drum_slot, role); seq_used = 1; }
        else if (drum_mode) { drum_slot = steel_drums_slot_of_key(key); steel_drums_hit(drum_slot); }
        else if (role < 16 && (c->buttons & BUTTON(SCL))) {
            steel_set_key((5U + key) % 12U); /* key selector: physical keys start at F */
            steel_key_notice(now);
        } else if (role < 16) steel_white(role);
        else if (role < 27) steel_black(role - 16U, now);
        changed = 1;
    }
    /* Tap SEQ: switch tones <-> drums. Holding it to edit steps does not switch. */
    if ((released & BUTTON(SEQ)) && !seq_used) { drum_mode ^= 1U; steel_full = 1; }
    static const struct { uint8_t button, page; } jumps[] = {
        {HOME, SPAGE_PLAY}, {ENV, SPAGE_ENV}, {LFO, SPAGE_VIB}, {SCL, SPAGE_SCALE}};
    for (unsigned i = 0; i < sizeof jumps / sizeof jumps[0]; ++i)
        if ((pressed & BUTTON(jumps[i].button)) && !(jumps[i].button == SCL && (steel_page == SPAGE_TEMPO || drum_mode))) {
            if (drum_mode) { drum_mode = 0; steel_full = 1; } /* SEL keeps drum mode: SEL+PLAY taps there */
            steel_page = jumps[i].page; /* SEL stays on TEMPO, where SEL+PLAY taps */
        }
    if (pressed & BUTTON(EDIT)) {
        if (drum_mode) drum_page = (drum_page + 1U) % DPAGES;
        else steel_page = steel_page >= SPAGE_SOUND && steel_page < SPAGE_SOUND5 ? steel_page + 1U : SPAGE_SOUND;
    }
    if ((pressed & BUTTON(PLAY)) && (c->buttons & BUTTON(SCL))) {
        steel_tap(now);
        steel_set_notice(steel_bpm_external() ? "TEMPO FOLLOWS MIDI CLOCK" : "TAP TEMPO", now);
    } else if ((pressed & BUTTON(PLAY)) && drum_mode) {
        steel_drums_transport(-1);
        steel_set_notice(steel_drums_running() ? "DRUMS RUNNING" : "DRUMS STOPPED", now);
    } else if (pressed & BUTTON(PLAY)) { steel_all_off(); steel_set_notice("ALL TONES OFF", now); }
    /* ARP in drum mode: a completely new random beat. */
    static uint32_t beats;
    if ((pressed & BUTTON(ARP)) && drum_mode) {
        steel_drums_randomize(now * 2654435761U + ++beats, steel_bpm());
        steel_set_notice("NEW RANDOM BEAT", now);
    }
    if (pressed & (BUTTON(OCT_DOWN) | BUTTON(OCT_UP))) {
        int octave = steel_transpose((pressed & BUTTON(OCT_UP)) ? 1 : -1);
        strcpy(steel_key_text, "KEYBOARD OCTAVE ");
        strcat(steel_key_text, octave < 0 ? "-" : octave > 0 ? "+" : "");
        char digit[2] = {(char)('0' + (octave < 0 ? -octave : octave)), 0};
        strcat(steel_key_text, digit);
        steel_notice = steel_key_text; steel_notice_until = now + 1500U;
        changed = 1;
    }
    if (pressed & BUTTON(FX)) {
        if (drum_mode) { drum_mode = 0; steel_full = 1; steel_page = SPAGE_COLOR; }
        steel_page = steel_page >= SPAGE_FILTER && steel_page < SPAGE_COLOR ? steel_page + 1U : SPAGE_FILTER;
    }
    steel_poll(now);
    static uint32_t health_at, health_overruns;
    if (now - health_at >= 500U) {
        struct fm1_audio_stats dma;
        fm1_audio_stats_read(&dma);
        unsigned overrun = dma.overruns != health_overruns && health_at;
        if (steel_guard(steel_cpu_load(), overrun, now)) {
            static char limit[] = "CPU LIMIT: 16 TONES";
            unsigned n = steel_tone_limit();
            limit[11] = n >= 10U ? (char)('0' + n / 10U) : ' ';
            limit[12] = (char)('0' + n % 10U);
            if (n < STEEL_VOICES) steel_set_notice(limit, now);
        } else if (overrun) steel_set_notice("AUDIO OVERLOAD - FEWER TONES", now);
        health_overruns = dma.overruns;
        health_at = now;
        if (steel_page == SPAGE_TEMPO && !drum_mode) changed = 1;
    }
    if (c->encoders[0]) {
        if (drum_mode) {
            int slot = (int)drum_slot + c->encoders[0];
            drum_slot = (unsigned)(slot < 0 ? 0 : slot >= DRUM_SLOTS ? DRUM_SLOTS - 1 : slot);
        } else steel_page = (unsigned)((int)steel_page + c->encoders[0] % SPAGES + SPAGES) % SPAGES;
    }
    if (c->encoders[6] && (c->buttons & BUTTON(SCL)) && !drum_mode) {
        steel_edit(SP_ROOT, c->encoders[6]); /* SEL + PRESETS: key in semitones */
        steel_key_notice(now);
    } else if (c->encoders[6]) {
        unsigned next = (unsigned)((int)steel_preset_index() + c->encoders[6] % STEEL_PRESETS + STEEL_PRESETS) % STEEL_PRESETS;
        steel_load(next);
    }
    for (unsigned col = 0; col < 4; ++col) {
        int delta = c->encoders[2 + col];
        if (!delta) continue;
        if (!drum_mode) { steel_edit(steel_pages[steel_page].p[col], delta); continue; }
        unsigned param = drum_pages[drum_page].p[col];
        if (param == 255) steel_drums_level_edit(delta);
        else if (param == 254) steel_edit(SP_BPM, delta);
        else if (param == 252) steel_drums_hiss_edit(delta);
        else if (param < DP_COUNT) steel_drums_edit(drum_slot, param, delta);
    }
    changed |= pressed != 0 || released != 0 || encoder_active(c->encoders);
    if (steel_notice && (int32_t)(now - steel_notice_until) >= 0) { steel_notice = NULL; changed = 1; }
    /* Drum activity and the playhead redraw at most every 40 ms. */
    static uint32_t drum_redraw;
    if (drum_mode && now - drum_redraw >= 40U) {
        drum_redraw = now;
        drum_activity_shown = steel_drums_activity();
        changed = 1;
    }
    if (drum_mode) {
        /* LEDs: the selected slot's steps on the white keys, plus its own key. */
        uint32_t extra = 1U << drum_slot;
        steel_leds(steel_drums_steps(drum_slot), 0, extra);
    } else steel_leds(steel_white_state(), steel_black_state(), 0);
    if (saved_cache.shown_page == ~0U) steel_full = 1; /* LCD transfer error: repaint all */
    if (changed || steel_full || now - steel_frame_ms >= 100U) {
        steel_frame_ms = now;
        saved_cache.shown_page = 0;
        paint_priority = 0;
        cache = &saved_cache;
        steel_draw();
    }
    paint_flush();
}
