// Headless test of GLIDER in the browser simulator.
// Usage: node tests/steel_sim_test.mjs [out-dir]
// Writes steel_demo.wav (listening sample) and lcd_*.rgb565 screen dumps.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const out = process.argv[2] || fileURLToPath(new URL('../build/steel-test/', import.meta.url));
mkdirSync(out, { recursive: true });
const wasm = readFileSync(new URL('../build/web/simulator.wasm', import.meta.url));
const { instance } = await WebAssembly.instantiate(wasm, {});
const x = instance.exports;
x._initialize?.();
const BTN = 0, KEY = 1, ENC = 2;
const B = { OCT_DOWN: 0, OCT_UP: 1, FX: 2, SCL: 3, ENV: 4, LFO: 5, EDIT: 6, GLO: 7, HOME: 8, PLAY: 12 };
const recording = [];
const block = () => new Float32Array(x.memory.buffer, x.sim_render(), 256).slice();
const run = (seconds) => { const n = Math.round(seconds * 44100 / 128), a = [];
  for (let i = 0; i < n; i++) { const b = block(); recording.push(b); for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]); }
  return a; };
const tap = (kind, i) => { x.sim_control(kind, i, 1); run(0.03); x.sim_control(kind, i, 0); run(0.03); };
const turn = (enc, d) => { x.sim_control(ENC, enc, d); run(0.02); x.sim_control(ENC, enc, 0); run(0.02); };
const rms = (a) => Math.sqrt(a.reduce((s, v) => s + v * v, 0) / a.length);
const hz = (a) => { let c = 0; for (let i = 1; i < a.length; i++) if ((a[i - 1] < 0) !== (a[i] < 0)) c++; return c / 2 / (a.length / 44100); };
const cents = (f, ref) => 1200 * Math.log2(f / ref);
let fails = 0;
const check = (ok, msg) => { console.log((ok ? 'PASS ' : 'FAIL ') + msg); if (!ok) fails++; };
const dumpLcd = (name) => writeFileSync(`${out}/lcd_${name}.rgb565`, new Uint8Array(x.memory.buffer, x.sim_pixels(), 240 * 240 * 2).slice());

x.sim_boot(1); run(1.5);
dumpLcd('boot');
check(rms(run(0.3)) < 1e-4, 'silent after boot');

// Pitch tests use the LAP STEEL preset (C3 major, just), dry.
turn(6, 8); run(0.3);
x.sim_control(BTN, B.FX, 1); run(0.05); x.sim_control(BTN, B.FX, 0); run(0.05);   // FX 1/4 FILTER
tap(BTN, B.FX); turn(5, -99);                                         // FX 2/4 TAPE: MIX off
tap(BTN, B.FX); turn(4, -99);                                         // FX 3/4 DELAY: MIX off
// Pure test tone: remove second oscillator, sub, drift and vibrato.
tap(BTN, B.EDIT); turn(3, -99); turn(4, -99); turn(5, -99);          // SOUND 1/2: DETUNE, OSC2, SUB
tap(BTN, B.EDIT); turn(4, -99);                                       // SOUND 2/2: DRIFT
tap(BTN, B.LFO); turn(3, -99);                                        // VIBRATO depth
tap(BTN, B.HOME);

const C3 = 130.81, just = (r) => C3 * r;
tap(KEY, 0); run(1.2); const t1 = run(1.0);
check(Math.abs(cents(hz(t1), C3)) < 5, `white key F latches C3 (${hz(t1).toFixed(2)} Hz)`);
tap(KEY, 1); run(2.0); const t2 = run(1.0);                           // black Fis: glide up one step
check(Math.abs(cents(hz(t2), just(9 / 8))) < 5, `black Fis glides up to just D (${hz(t2).toFixed(2)} Hz, want ${just(9 / 8).toFixed(2)})`);
tap(KEY, 1); run(2.0); const t3 = run(1.0);
check(Math.abs(cents(hz(t3), C3)) < 5, `black Fis again glides back to C3 (${hz(t3).toFixed(2)} Hz)`);
tap(KEY, 0); run(4.0); const t4 = run(0.5);
check(rms(t4) < rms(t1) * 0.01, 'white key again releases the tone');

// White key G (index 2 = third scale tone, E in C major just 5/4).
tap(KEY, 4); run(1.2); const e = run(1.0);
check(Math.abs(cents(hz(e), just(5 / 4))) < 5, `third white key plays just E (${hz(e).toFixed(2)} Hz, want ${just(5 / 4).toFixed(2)})`);
tap(KEY, 4); run(3);
// Glide steps: +2 steps from C should land on E.
turn(3, 1);                                                           // PLAY page K2 = STEPS -> +2
tap(KEY, 0); tap(KEY, 1); run(2.5); const s2 = run(1.0);
check(Math.abs(cents(hz(s2), just(5 / 4))) < 5, `glide of +2 steps lands on E (${hz(s2).toFixed(2)} Hz)`);
tap(KEY, 1); tap(KEY, 0); run(3); turn(3, -1);
// Double click on a black key glides down (C -> H below); the next single click glides back.
tap(BTN, B.HOME);
tap(KEY, 0); tap(KEY, 1); tap(KEY, 1); run(2.5); const dn = run(1.0);
check(Math.abs(cents(hz(dn), C3 * 15 / 16)) < 5, `double click glides down to H (${hz(dn).toFixed(2)} Hz, want ${(C3 * 15 / 16).toFixed(2)})`);
tap(KEY, 1); run(2.5); const home = run(1.0);
check(Math.abs(cents(hz(home), C3)) < 5, `next click glides back home to C3 (${hz(home).toFixed(2)} Hz)`);
tap(KEY, 1); const waiting = run(0.25);                              // still inside the 350 ms double-click window
check(Math.abs(cents(hz(waiting), C3)) < 40, `a single click waits for a possible second click (${hz(waiting).toFixed(1)} Hz)`);
run(2.5); const upAgain = run(1.0);
check(Math.abs(cents(hz(upAgain), just(9 / 8))) < 5, `then the single click glides up (${hz(upAgain).toFixed(2)} Hz)`);
tap(BTN, B.PLAY); run(4);
check(rms(run(0.3)) < 1e-3, 'PLAY switches all tones off');
// OVERTONE (PLAY page K4) must be clearly audible, even with the FX filter fairly closed.
{
  const harmonics = async (amount) => {
    const { instance } = await WebAssembly.instantiate(wasm, {}); const y = instance.exports; y._initialize?.();
    const go = (sec) => { const a = []; for (let i = 0; i < Math.round(sec * 44100 / 128); i++) { const b = new Float32Array(y.memory.buffer, y.sim_render(), 256); for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]); } return a; };
    y.sim_boot(1); go(0.5);
    if (amount) { y.sim_control(2, 5, amount); go(0.05); y.sim_control(2, 5, 0); go(0.05); }
    y.sim_control(1, 12, 1); go(0.05); y.sim_control(1, 12, 0); go(3); const a = go(2);
    return [1, 3, 4, 5].map((h) => { const w = 2 * Math.PI * 261.63 * h / 44100, c = 2 * Math.cos(w); let s1 = 0, s2 = 0;
      for (const v of a) { const t = v + c * s1 - s2; s2 = s1; s1 = t; } return 10 * Math.log10(s1 * s1 + s2 * s2 - c * s1 * s2 + 1e-20); });
  };
  const off = await harmonics(0), on = await harmonics(99);
  const gain = [1, 2, 3].map((k) => (on[k] - on[0]) - (off[k] - off[0]));
  check(gain.every((g) => g > 15), `OVERTONE lifts harmonics 3/4/5 by ${gain.map((g) => g.toFixed(0)).join('/')} dB on the start preset`);
}
// INTERVAL: oscillator 2 sounds an interval (FIFTH DRONE pure 5th; THIRD STEEL thirds in key, gliding along).
{
  const { instance } = await WebAssembly.instantiate(wasm, {}); const y = instance.exports; y._initialize?.();
  const go = (sec) => { const a = []; for (let i = 0; i < Math.round(sec * 44100 / 128); i++) { const b = new Float32Array(y.memory.buffer, y.sim_render(), 256); for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]); } return a; };
  const press = (key) => { y.sim_control(1, key, 1); go(0.05); y.sim_control(1, key, 0); go(0.05); };
  const bin = (a, f) => { const w = 2 * Math.PI * f / 44100, c = 2 * Math.cos(w); let s1 = 0, s2 = 0;
    for (const v of a) { const t = v + c * s1 - s2; s2 = s1; s1 = t; } return 10 * Math.log10(s1 * s1 + s2 * s2 - c * s1 * s2 + 1e-20); };
  // Peak within +-25 cents: oscillator 2 is detuned and swept by the detune LFO.
  const db = (a, f) => { let m = -1e9; for (let c = -25; c <= 25; c += 2.5) m = Math.max(m, bin(a, f * 2 ** (c / 1200))); return m; };
  y.sim_boot(1); go(0.5);
  y.sim_control(2, 6, 18); go(0.05); y.sim_control(2, 6, 0); go(0.3);         // preset 19 FIFTH DRONE (C)
  press(12); go(3); let a = go(1.5);
  const c4 = 261.63, fifth = db(a, c4 * 1.5) - db(a, c4), off = db(a, c4 * 1.4) - db(a, c4);
  check(fifth > -12 && fifth - off > 15, `FIFTH DRONE: C4 key also sounds the pure fifth G4 (${fifth.toFixed(1)} dB, off-pitch ${off.toFixed(1)} dB)`);
  press(12); go(4);
  y.sim_control(2, 6, 1); go(0.05); y.sim_control(2, 6, 0); go(0.3);          // preset 20 THIRD STEEL (E major)
  const e4 = 329.63;
  press(12); go(3); a = go(1.5);
  const major = db(a, e4 * 5 / 4) - db(a, e4), minor = db(a, e4 * 6 / 5) - db(a, e4);
  check(major > -12 && major - minor > 15, `THIRD STEEL: E4 key adds the major third G#4 in key (${major.toFixed(1)} vs ${minor.toFixed(1)} dB)`);
  press(12); go(4);
  press(14); go(3); a = go(1.5);                                               // F#4: the third in E major is minor (A)
  const fs = e4 * 9 / 8, m3 = db(a, e4 * 4 / 3) - db(a, fs), M3 = db(a, fs * 5 / 4) - db(a, fs);
  check(m3 > -12 && m3 - M3 > 15, `THIRD STEEL: F#4 key adds the minor third A4 (${m3.toFixed(1)} vs ${M3.toFixed(1)} dB)`);
  press(14); go(4);
  press(12); go(1); press(13); go(4); a = go(1.5);                             // black key: E4 glides to F#4, the third follows to A4
  const glided = db(a, e4 * 4 / 3) - db(a, fs), stale = db(a, e4 * 5 / 4) - db(a, fs);
  check(glided > -12 && glided - stale > 15, `THIRD STEEL: the third glides along in key (${glided.toFixed(1)} vs ${stale.toFixed(1)} dB)`);
  press(13); press(12); go(4);
  // Key selector: hold SEL + white key D -> the keyboard is in D major (E4 key now plays D4).
  y.sim_control(0, 3, 1); go(0.05); press(9); y.sim_control(0, 3, 0); go(0.1);
  press(12); go(3); a = go(1.5);
  const d4 = 293.66, inD = db(a, d4) - db(a, e4);
  check(inD > 15, `SEL + white key D selects the key D (D4 ${inD.toFixed(1)} dB above E4)`);
  press(12); go(3);
}
// Chord scales: SUS2 stacks C, D, G, C', D', ... on the white keys.
{
  const { instance } = await WebAssembly.instantiate(wasm, {}); const y = instance.exports; y._initialize?.();
  const go = (sec) => { const a = []; for (let i = 0; i < Math.round(sec * 44100 / 128); i++) { const b = new Float32Array(y.memory.buffer, y.sim_render(), 256); for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]); } return a; };
  const press = (kind, key) => { y.sim_control(kind, key, 1); go(0.05); y.sim_control(kind, key, 0); go(0.05); };
  const bin = (a, f) => { const w = 2 * Math.PI * f / 44100, c = 2 * Math.cos(w); let s1 = 0, s2 = 0;
    for (const v of a) { const t = v + c * s1 - s2; s2 = s1; s1 = t; } return 10 * Math.log10(s1 * s1 + s2 * s2 - c * s1 * s2 + 1e-20); };
  const db = (a, f) => { let m = -1e9; for (let c = -25; c <= 25; c += 2.5) m = Math.max(m, bin(a, f * 2 ** (c / 1200))); return m; };
  y.sim_boot(1); go(0.5);
  y.sim_control(2, 6, 8); go(0.05); y.sim_control(2, 6, 0); go(0.3);          // LAP STEEL, C3 just
  press(0, B.SCL); y.sim_control(2, 3, 15); go(0.05); y.sim_control(2, 3, 0); go(0.2); // SCALE -> SUS2
  press(1, 4); go(2); let a = go(1);                                             // third white key: G3
  check(db(a, 196.0) > db(a, 164.81) + 15, 'SUS2: third white key plays G3, not E3');
  press(1, 4); go(3);
  press(1, 6); go(2); a = go(1);                                                 // fourth white key: C4
  check(db(a, 261.63) > db(a, 174.61) + 15, 'SUS2: fourth white key plays the octave C4');
}
// OCT -/+ only shift the keyboard: a sounding tone keeps its octave; the same key one octave down adds a new tone.
{
  const { instance } = await WebAssembly.instantiate(wasm, {}); const y = instance.exports; y._initialize?.();
  const go = (sec) => { const a = []; for (let i = 0; i < Math.round(sec * 44100 / 128); i++) { const b = new Float32Array(y.memory.buffer, y.sim_render(), 256); for (let j = 0; j < 256; j += 2) a.push(b[j] + b[j + 1]); } return a; };
  const press = (kind, key) => { y.sim_control(kind, key, 1); go(0.05); y.sim_control(kind, key, 0); go(0.05); };
  const bin = (a, f) => { const w = 2 * Math.PI * f / 44100, c = 2 * Math.cos(w); let s1 = 0, s2 = 0;
    for (const v of a) { const t = v + c * s1 - s2; s2 = s1; s1 = t; } return 10 * Math.log10(s1 * s1 + s2 * s2 - c * s1 * s2 + 1e-20); };
  const db = (a, f) => { let m = -1e9; for (let c = -25; c <= 25; c += 2.5) m = Math.max(m, bin(a, f * 2 ** (c / 1200))); return m; };
  y.sim_boot(1); go(0.5);
  y.sim_control(2, 6, 8); go(0.05); y.sim_control(2, 6, 0); go(0.3);          // LAP STEEL, C major
  press(1, 12); go(2); const before = go(1);                                   // C4
  press(0, B.OCT_DOWN); go(2); const after = go(1);
  check(Math.abs(db(after, 261.63) - db(before, 261.63)) < 3 && db(after, 130.81) < db(after, 261.63) - 20,
    `OCT - leaves the sounding C4 where it is (${(db(after, 261.63) - db(before, 261.63)).toFixed(1)} dB change)`);
  press(1, 12); go(2); const both = go(1);                                     // same key, now C3 as a second tone
  check(db(both, 130.81) > db(both, 261.63) - 12 && db(both, 261.63) > db(before, 261.63) - 6,
    `after OCT - the same key adds C3 while C4 keeps sounding (${db(both, 130.81).toFixed(1)} / ${db(both, 261.63).toFixed(1)} dB)`);
  press(1, 12); go(3); const upper = go(1);                                    // releases only C3
  check(db(upper, 130.81) < db(upper, 261.63) - 15, 'pressing it again releases only C3');
}
// PLAY page K4 is OVERTONE; BPM lives on FX 5/6 TEMPO (K1). SEL + PLAY taps it.
const knobs = () => { const m = new Uint8Array(x.memory.buffer, x.sim_knobs(), 128), t = (o) => new TextDecoder().decode(m.slice(o, o + 16)).split('\0')[0];
  return [0, 1, 2, 3].map((c) => [t(c * 32), t(c * 32 + 16)]); };
tap(BTN, B.HOME); run(0.1);
check(knobs()[3][0] === 'OVERTONE', `PLAY page K4 is OVERTONE (${knobs()[3].join(' ')})`);
for (let i = 0; i < 5; i++) tap(BTN, B.FX);                             // FX 5/6 TEMPO
check(knobs()[0][0] === 'BPM' && knobs()[0][1] === '90', `TEMPO page shows BPM 90 (${knobs()[0].join(' ')})`);
x.sim_control(BTN, B.SCL, 1); run(0.05);
for (let i = 0; i < 4; i++) { x.sim_control(BTN, B.PLAY, 1); run(0.02); x.sim_control(BTN, B.PLAY, 0); run(0.48); }
x.sim_control(BTN, B.SCL, 0); run(0.1);
check(Math.abs(Number(knobs()[0][1]) - 120) <= 1, `SEL+PLAY taps every 500 ms set ${knobs()[0][1]} BPM`);
// MIDI clock in: 24 ticks per beat.
const midiIn = (bytes) => { new Uint8Array(x.memory.buffer, x.sim_file(), bytes.length).set(bytes); return x.sim_midi(bytes.length); };
for (let i = 0; i < 24 * 4; i++) { midiIn([0xf8]); run(0.025); }
run(0.1);
const tick = Math.round(0.025 * 44100 / 128) * 128 / 44100;            // the simulator advances in 128-sample blocks
const expected = Math.round(60 / (24 * tick));
check(Number(knobs()[0][1]) === expected, `MIDI clock sets ${knobs()[0][1]} BPM (ticks every ${(tick * 1000).toFixed(1)} ms = ${expected} BPM)`);
dumpLcd('tempo');
// MIDI notes: F3 (53) holds the first white key while the note is down.
{
  x.sim_control(BTN, B.PLAY, 1); run(0.05); x.sim_control(BTN, B.PLAY, 0); run(4);   // all tones off first
  const quiet = rms(run(0.3));
  midiIn([0x90, 53, 100]); run(1.5); const held = run(1.0);
  midiIn([0x80, 53, 0]); run(5); const after = run(0.5);
  check(rms(held) > 0.01 && rms(after) < rms(held) * 0.02, `MIDI note on/off plays and releases a tone (rms ${rms(held).toFixed(3)} -> ${rms(after).toFixed(5)})`);
  midiIn([0x90, 77, 100]); run(1.5); const high = run(1.0); midiIn([0x80, 77, 0]); run(4);   // F5 = white key 14
  const pitchRatio = hz(high) / hz(held);
  check(Math.abs(pitchRatio - 4) < 0.15 || Math.abs(pitchRatio - 2) < 0.08, `MIDI F5 sounds higher than F3 (ratio ${pitchRatio.toFixed(2)})`);
  midiIn([0x99, 36, 110]); const drum = run(0.3);
  check(rms(drum) > quiet + 1e-4, `MIDI channel 10 plays the drum slot (rms ${rms(drum).toFixed(4)})`);
}
run(2.5);
dumpLcd('after_tests');

// Listening demo with the start preset (SYNTHISCAPES): drone chord, glides,
// then the tape echo is pushed into self-oscillation and the filter closes.
x.sim_boot(1); run(0.5); recording.length = 0;
for (const k of [0, 7, 11]) tap(KEY, k);                              // F, C, E keys = C, G, H? (scale degrees 1,5,7)
run(4);
tap(KEY, 14); run(3);                                                 // add a high tone (G key)
tap(KEY, 13); run(3);                                                 // Fis (upper) glides E+F keys
tap(KEY, 15); run(3);                                                 // G# glides the high tone
tap(KEY, 15); run(2); tap(KEY, 13); run(3);
for (let i = 0; i < 4; i++) tap(BTN, B.FX);                           // FX 4/6 SHIMMER
turn(4, 50); turn(5, 30); run(6);                                      // shimmer and reverb mix up
for (let i = 0; i < 4; i++) tap(BTN, B.FX);                           // FX 4/6 -> 5 -> 6 -> 1 -> 2/6 TAPE
dumpLcd('demo');
turn(3, 85); run(3);                                                  // TAPE FB up to 120%: self-oscillation
tap(BTN, B.PLAY); run(5);                                             // keys off, tape keeps singing
for (let i = 0; i < 5; i++) tap(BTN, B.FX);                           // TAPE -> ... -> FX 1/6 FILTER
for (let i = 0; i < 12; i++) { turn(2, -3); run(0.25); }              // close the filter slowly
dumpLcd('demo_fx');
turn(3, 0); tap(BTN, B.FX); turn(3, -85); run(6);                     // tape feedback back down: fades
const silence = run(1.0);
check(rms(silence) < 0.02, `echo tail fades after feedback is lowered (rms ${rms(silence).toFixed(4)})`);
const pcm = new Int16Array(recording.length * 256);
recording.forEach((b, i) => b.forEach((v, j) => { pcm[i * 256 + j] = Math.max(-32768, Math.min(32767, Math.round(v * 32767))); }));
const header = Buffer.alloc(44);
header.write('RIFF', 0); header.writeUInt32LE(36 + pcm.byteLength, 4); header.write('WAVEfmt ', 8);
header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(2, 22); header.writeUInt32LE(44100, 24);
header.writeUInt32LE(44100 * 4, 28); header.writeUInt16LE(4, 32); header.writeUInt16LE(16, 34); header.write('data', 36);
header.writeUInt32LE(pcm.byteLength, 40);
writeFileSync(`${out}/steel_demo.wav`, Buffer.concat([header, Buffer.from(pcm.buffer)]));
console.log(`demo: ${out}steel_demo.wav`);
process.exit(fails ? 1 : 0);
