// Headless test of ARPHONY in the browser simulator.
// Usage: node tests/arp_sim_test.mjs [out-dir]
// Writes one demo WAV per preset (demo_NN_NAME.wav) and LCD dumps (lcd_*.rgb565).
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const out = process.argv[2] || fileURLToPath(new URL('../build/arp-test/', import.meta.url));
mkdirSync(out, { recursive: true });
const wasm = readFileSync(new URL('../build/web/simulator.wasm', import.meta.url));
const BTN = 0, KEY = 1, ENC = 2;
const B = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SCL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, ARP: 10, PLAY: 12 };
let fails = 0;
const check = (ok, msg) => { console.log((ok ? 'PASS ' : 'FAIL ') + msg); if (!ok) fails++; };

async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const x = instance.exports;
  x._initialize?.();
  const s = { x, recording: [], watch: null };
  s.block = () => new Float32Array(x.memory.buffer, x.sim_render(), 256).slice();
  s.run = (seconds, keep = true) => {
    const n = Math.round(seconds * 44100 / 128), a = [];
    for (let i = 0; i < n; i++) {
      const b = s.block();
      if (keep) s.recording.push(b);
      for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]);
      if (s.watch) s.watch();
    }
    return a;
  };
  s.tap = (kind, i) => { x.sim_control(kind, i, 1); s.run(0.03); x.sim_control(kind, i, 0); s.run(0.03); };
  // K1-K4 (encoders 2-5): the first detent only shows the value, so a turn
  // wakes the knob first; afterwards the box falls back to its name.
  s.turn = (enc, d) => {
    if (enc >= 2 && enc <= 5) { x.sim_control(ENC, enc, Math.sign(d)); s.run(0.02); x.sim_control(ENC, enc, 0); s.run(0.02); }
    x.sim_control(ENC, enc, d); s.run(0.02); x.sim_control(ENC, enc, 0); s.run(0.02);
  };
  s.settle = () => s.run(1.6);
  s.knobs = () => {
    const p = x.sim_knobs(), m = new Uint8Array(x.memory.buffer, p, 4 * 2 * 16), r = [];
    for (let c = 0; c < 4; c++) {
      const str = (o) => { let t = ''; for (let k = 0; k < 16 && m[o + k]; k++) t += String.fromCharCode(m[o + k]); return t; };
      r.push([str(c * 32), str(c * 32 + 16)]);
    }
    return r;
  };
  x.sim_boot(1); s.run(1.5, false);
  return s;
}
const rms = (a) => Math.sqrt(a.reduce((t, v) => t + v * v, 0) / a.length);
const wav = (name, blocks) => {
  const frames = blocks.length * 128, data = new Int16Array(frames * 2);
  let peak = 1e-9;
  for (const b of blocks) for (const v of b) peak = Math.max(peak, Math.abs(v));
  const g = Math.min(1, 0.9 / peak);
  blocks.forEach((b, i) => { for (let j = 0; j < 256; j++) data[i * 256 + j] = Math.max(-32767, Math.min(32767, b[j] * g * 32767)); });
  const h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + data.byteLength, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16);
  h.writeUInt16LE(1, 20); h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(44100 * 4, 28);
  h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34); h.write('data', 36); h.writeUInt32LE(data.byteLength, 40);
  writeFileSync(`${out}/${name}.wav`, Buffer.concat([h, Buffer.from(data.buffer)]));
};
const dumpLcd = (s, name) => writeFileSync(`${out}/lcd_${name}.rgb565`,
  new Uint8Array(s.x.memory.buffer, s.x.sim_pixels(), 240 * 240 * 2).slice());
const pc = (n) => ((n % 12) + 12) % 12;

// ---- transport and the first preset (E Dorian, i9 - IV, two bars each, 104 BPM)
{
  const s = await boot(), x = s.x;
  dumpLcd(s, 'boot');
  check(rms(s.run(0.3)) < 1e-4, 'silent after boot');
  check(x.sim_arp_running() === 0, 'stopped after boot');
  s.tap(BTN, B.PLAY);
  const first = s.run(0.6);
  check(x.sim_arp_running() === 1, 'PLAY starts');
  check(rms(first) > 1e-3, `sound after PLAY (rms ${rms(first).toFixed(4)})`);
  check(pc(x.sim_arp_last_note(1)) === 4, `bass on E for i9 (note ${x.sim_arp_last_note(1)})`);
  // LEAD: every riff note belongs to the chord that is playing.
  const chordPcs = [[4, 6, 7, 11, 2], [9, 11, 1, 4]]; // Em9, Aadd9
  let wrong = 0, seen = 0, last = -1, chords = new Set(), basses = new Set();
  s.watch = () => {
    const n = x.sim_arp_last_note(0), c = x.sim_arp_chord();
    chords.add(c); basses.add(pc(x.sim_arp_last_note(1)));
    if (n !== last && n >= 0) { seen++; if (!chordPcs[c].includes(pc(n))) wrong++; last = n; }
  };
  s.run(9);
  s.watch = null;
  check(chords.has(0) && chords.has(1), 'the progression moves on (i9 -> IV)');
  check(basses.has(4) && basses.has(9), `bass follows: E and A (${[...basses]})`);
  check(seen > 20 && wrong <= Math.ceil(seen * 0.1), `LEAD riff notes stay in the chord (${wrong} of ${seen} outside, MUTATE off)`);
  dumpLcd(s, 'auto');
  // HAND: low G key = the chord on G (bIII in E Dorian).
  s.tap(KEY, 2);                                   // G3
  s.run(1.0);
  check(x.sim_arp_running() === 1, 'a key keeps it running');
  check(pc(x.sim_arp_last_note(1)) === 7, `low key G: bass on G (${x.sim_arp_last_note(1)})`);
  s.tap(KEY, 18);                                  // B4 (key 18): over the pedal
  s.run(1.0);
  check(pc(x.sim_arp_last_note(1)) === 4, `high key B: bass stays on the tonic E (${x.sim_arp_last_note(1)})`);
  dumpLcd(s, 'hand');
  s.tap(BTN, B.ARP); s.run(2.0);
  check(x.sim_arp_running() === 1, 'ARP: back to the progression, still running');
  s.tap(BTN, B.PLAY); s.run(3.0);
  check(x.sim_arp_running() === 0, 'PLAY stops');
  check(rms(s.run(0.5)) < 0.02, 'quiet after stop (tails fade)');
  // Pages: HOME 1-4 hold the main settings; HOME steps through them.
  const pages = [];
  for (let i = 0; i < 4; i++) { s.tap(BTN, B.HOME); pages.push(s.knobs().map((k) => k[0]).join(',')); }
  const want = ['KEY,MODE,CHORDS,LENGTH', 'RIFF,RATE,LEAD,HARMONY', 'BPM,BASS RHY,ARP OCT,TWIN', 'GATE,SWING,ACCENT,MUTATE'];
  // The first HOME press goes from HOME 1 (boot page) to HOME 2.
  check(pages.join('|') === [want[1], want[2], want[3], want[0]].join('|'), `HOME pages: ${pages.join(' | ')}`);
  // SEL held + white key = the key: key 7 (C4) -> C, shown on HOME 1 K1.
  x.sim_control(BTN, B.SCL, 1); s.run(0.05); s.tap(KEY, 7); x.sim_control(BTN, B.SCL, 0); s.run(0.05);
  check(s.knobs()[0][1] === 'C', `SEL + white key C sets the key (${s.knobs()[0][1]})`);
  s.tap(BTN, B.SCL);
  check(s.knobs()[0][0] === 'COLOR', `SEL opens CHORDS (K1 = ${s.knobs()[0][0]})`);
}

// ---- every preset: a demo recording and basic sanity
{
  const s = await boot(), x = s.x;
  for (let i = 0; i < 15; i++) {
    if (i) s.turn(6, 1);
    s.run(0.2, false);
    s.recording = [];
    s.tap(BTN, B.PLAY);
    const a = s.run(12);
    s.tap(BTN, B.PLAY);
    s.run(2.5);
    let peak = 0; for (const v of a) peak = Math.max(peak, Math.abs(v));
    check(rms(a) > 3e-3 && peak < 2.0 && Number.isFinite(peak), `preset ${i + 1}: plays, no blow-up (rms ${rms(a).toFixed(3)}, peak ${peak.toFixed(2)})`);
    wav(`demo_${String(i + 1).padStart(2, '0')}`, s.recording);
    if (i === 2) dumpLcd(s, 'preset3');
  }
}
// ---- PENTA IV (preset 13): Eb pentatonic riff over Ab, TWIN pings two octaves up
{
  const s = await boot(), x = s.x;
  s.turn(6, 12); s.run(0.2);
  s.tap(BTN, B.PLAY); s.run(0.5);
  let wrong = 0, seen = 0, high = 0, last = -1, basses = new Set();
  s.watch = () => {
    const n = x.sim_arp_last_note(0), tw = x.sim_arp_last_note(2);
    basses.add(pc(x.sim_arp_last_note(1)));
    if (tw >= 74) high++;
    if (n !== last && n >= 0) { seen++; if (![3, 5, 7, 10, 0].includes(pc(n))) wrong++; last = n; }
  };
  s.run(12);
  s.watch = null;
  check(seen > 30 && wrong === 0, `PENTA: riff only on Eb F G Bb C (${wrong} of ${seen} outside)`);
  check(basses.has(8), `PENTA IV: bass on Ab (${[...basses]})`);
  check(high > 0, `TWIN PINGS: high notes from D5 up (${high})`);
}
// ---- No silent gaps: in SIX TO SEVEN (PING riff: many left-out steps, GATE 30 %)
// a left-out step holds the note before; only the short GATE gap before a
// played note may be open.
{
  const s = await boot(), x = s.x;
  s.turn(6, 2); s.run(0.2);
  s.tap(BTN, B.PLAY); s.run(0.5);
  let open = 0, blocks = 0;
  s.watch = () => { blocks++; if (!x.sim_arp_gated()) open++; };
  s.run(8);
  s.watch = null;
  // PING: 16 steps, 6 notes; each held note runs to the next. A gap would be
  // 10 of 16 steps silent; at most the GATE gaps of non-held notes remain.
  check(open / blocks < 0.05, `PING riff: no silent gaps (${(100 * open / blocks).toFixed(1)} % of the time without a riff note)`);
}
// ---- the first detent of a knob only shows the value; the next changes it
{
  const s = await boot(), x = s.x;
  const raw = (enc, d) => { x.sim_control(ENC, enc, d); s.run(0.02); x.sim_control(ENC, enc, 0); s.run(0.02); };
  const before = s.knobs()[0][1];                  // HOME 1 K1: KEY
  raw(2, 1);
  check(s.knobs()[0][1] === before, `first detent shows KEY ${before} and keeps it (${s.knobs()[0][1]})`);
  raw(2, 1);
  check(s.knobs()[0][1] !== before, `the next detent changes KEY (${before} -> ${s.knobs()[0][1]})`);
  const shown = s.knobs()[0][1];
  s.run(1.7); raw(2, 1);
  check(s.knobs()[0][1] === shown, `after the value faded, a detent only shows it again (${s.knobs()[0][1]})`);
}
// ---- MODE recolours a progression: DORIAN SWAY (E Dorian, Em9 - Aadd9) in MAJOR
// plays Emaj9 (G#, D#) instead of Em9 (G, D); turning back restores it.
{
  const s = await boot(), x = s.x;
  const riffPcs = (sec) => { const pcs = new Set(); let count = x.sim_arp_count();
    for (let b = 0; b < sec * 344; b++) { x.sim_render(); const c = x.sim_arp_count();
      if (c !== count) { count = c; if (x.sim_arp_chord() === 0) pcs.add(pc(x.sim_arp_last_note(0))); } }
    return pcs; };
  s.tap(BTN, B.PLAY); s.run(0.3);
  const dorian = riffPcs(3.5);
  s.turn(3, -1);                                   // HOME 1 K2: MODE DORIAN -> MAJOR
  check(s.knobs()[1][1] === 'MAJOR', `K2 sets MODE (${s.knobs()[1][1]})`);
  s.tap(BTN, B.PLAY); s.run(2.5); s.tap(BTN, B.PLAY); s.run(0.3);   // restart on chord 1
  const major = riffPcs(3.5);
  check(dorian.has(7) && !dorian.has(8), `DORIAN: Em9 riff has G (${[...dorian]})`);
  check(major.has(8) && !major.has(7), `MAJOR: recoloured to Emaj9, G# instead of G (${[...major]})`);
  s.turn(4, 1); s.settle(); s.turn(4, -1);         // CHORDS away and back: MODE returns to DORIAN
  check(s.knobs()[1][1] === 'DORIAN', `choosing the chords sets their own mode (${s.knobs()[1][1]})`);
}
// ---- RANDOM: SEQ makes a new riff every time, and it keeps playing
{
  const s = await boot(), x = s.x;
  s.tap(BTN, B.PLAY); s.run(1);
  const riffs = new Set();
  for (let r = 0; r < 12; r++) {
    s.tap(BTN, 11);                                // SEQ
    s.run(2.2);
    const seq = []; let count = x.sim_arp_count();
    for (let b = 0; b < 3 * 344 && seq.length < 24; b++) {
      x.sim_render(); const c = x.sim_arp_count();
      if (c !== count) { count = c; seq.push(x.sim_arp_last_note(0)); }
    }
    riffs.add(seq.join(','));
  }
  check(riffs.size >= 11, `SEQ: 12 random riffs, ${riffs.size} different`);
  check(s.knobs()[0][1] === 'RANDOM', `RIFF shows RANDOM (${s.knobs()[0][1]})`);
  check(x.sim_arp_running() === 1 && rms(s.run(0.5)) > 1e-3, 'still playing after new riffs');
}
// ---- SEL + SEQ: random chords; they follow MODE
{
  const s = await boot(), x = s.x;
  s.tap(BTN, B.PLAY); s.run(0.5);
  const seen = new Set();
  for (let r = 0; r < 8; r++) {
    x.sim_control(BTN, B.SCL, 1); s.run(0.05); s.tap(BTN, 11); x.sim_control(BTN, B.SCL, 0); s.run(0.05);
    seen.add(s.knobs().map((k) => k[1]).join(','));
  }
  check(s.knobs()[2][1] === 'RANDOM', `SEL + SEQ: CHORDS shows RANDOM (${s.knobs()[2][1]})`);
  check(x.sim_arp_running() === 1 && rms(s.run(2)) > 1e-3, 'random chords keep playing');
  const chords = new Set();
  s.watch = () => chords.add(x.sim_arp_chord());
  s.run(10); s.watch = null;
  check(chords.size >= 2, `random chords change (${[...chords]})`);
}
// ---- user presets: SAVE held + white key stores, PRESETS finds it again
{
  const s = await boot(), x = s.x;
  s.turn(6, 2); s.run(0.2);                       // SIX TO SEVEN
  s.tap(BTN, B.HOME);                              // HOME 2: RIFF, RATE, LEAD, HARMONY
  s.turn(4, 2);                                    // LEAD two sounds further
  const lead = s.knobs()[2][1];
  x.sim_control(BTN, 9, 1); s.run(0.05); s.tap(KEY, 4); x.sim_control(BTN, 9, 0); s.run(0.05);  // SAVE + 3rd white key
  s.turn(6, -1); s.run(0.2);                       // away to a factory preset
  s.turn(6, 1); s.run(0.2);                        // back: the stored one
  s.tap(BTN, B.HOME); s.tap(BTN, B.HOME); s.tap(BTN, B.HOME); s.tap(BTN, B.HOME);  // round to HOME 2
  check(s.knobs()[2][1] === lead, `user preset 3 keeps LEAD ${lead} (${s.knobs()[2][1]})`);
  // On a fresh boot (same Flash) the PRESETS knob reaches it after the 15 factory presets.
  const flash = new Uint8Array(x.memory.buffer, x.sim_flash(), 536576).slice();
  const t = await boot(), y = t.x;
  new Uint8Array(y.memory.buffer, y.sim_flash(), 536576).set(flash);
  t.turn(6, 15); t.run(0.2); t.tap(BTN, B.HOME);
  check(t.knobs()[2][1] === lead, `after a restart: PRESETS 16 = user preset (${t.knobs()[2][1]})`);
}
// ---- MIDI out: notes on channels 1 (riff), 2 (bass), 3 (chords); offs after stop
{
  const s = await boot(), x = s.x;
  x.sim_midi_ports(6, 0); x.sim_midi_capture(1);
  x.sim_control(BTN, B.SCL, 1); s.run(0.05); x.sim_control(BTN, B.SCL, 0); s.run(0.05);   // SEL page
  s.turn(5, 1);                                    // K4: MIDI OUT on
  check(s.knobs()[3][1] === 'CH 1 2 3', `MIDI OUT on (${s.knobs()[3][1]})`);
  const held = new Map(), ons = [0, 0, 0];
  const read = () => { for (let m; (m = x.sim_midi_output());) {
      const n = m & 255, b = new Uint8Array(x.memory.buffer, x.sim_file(), n);
      for (let i = 0; i + 2 < n + 1 && i < n; i += 3) {
        const st = b[i], ch = st & 15, key = `${ch}:${b[i + 1]}`;
        if ((st & 0xf0) === 0x90 && b[i + 2]) { if (ch < 3) ons[ch]++; held.set(key, (held.get(key) || 0) + 1); }
        else if ((st & 0xf0) === 0x80 || (st & 0xf0) === 0x90) held.set(key, Math.max(0, (held.get(key) || 0) - 1));
      } } };
  s.watch = read;
  s.tap(BTN, B.PLAY); s.run(6);
  s.tap(BTN, B.PLAY); s.run(1);
  s.watch = null; read();
  check(ons[0] > 10 && ons[1] > 2 && ons[2] > 0, `MIDI notes on channel 1/2/3: ${ons.join('/')}`);
  const hanging = [...held.values()].reduce((a, b) => a + b, 0);
  check(hanging === 0, `no hanging MIDI notes after stop (${hanging})`);
}
console.log(fails ? `${fails} FAILED` : 'ALL PASSED');
process.exit(fails ? 1 : 0);
