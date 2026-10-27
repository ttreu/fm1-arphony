// Headless test of GLITCH DRUMS in the browser simulator.
// Usage: node tests/steel_drums_sim_test.mjs
// Checks the synthesized sounds (no samples), step editing, panning and
// random beats, and writes a demo.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
const out = here('../build/steel-test/');
mkdirSync(out, { recursive: true });
const { instance } = await WebAssembly.instantiate(readFileSync(here('../build/web/simulator.wasm')), {});
const x = instance.exports;
x._initialize?.();
const BTN = 0, KEY = 1, ENC = 2, B = { SEQ: 11, PLAY: 12, EDIT: 6, HOME: 8 };
const recording = [];
const block = () => new Float32Array(x.memory.buffer, x.sim_render(), 256).slice();
const run = (s) => { const a = []; for (let i = 0; i < Math.round(s * 44100 / 128); i++) { const b = block(); recording.push(b); a.push(b); } return a; };
const tap = (k, i) => { x.sim_control(k, i, 1); run(0.03); x.sim_control(k, i, 0); run(0.03); };
const turn = (e, d) => { x.sim_control(ENC, e, d); run(0.02); x.sim_control(ENC, e, 0); run(0.02); };
const stats = (blocks) => { let l = 0, r = 0, lr = 0, n = 0, peak = 0;
  for (const b of blocks) for (let j = 0; j < 256; j += 2) { l += b[j] * b[j]; r += b[j + 1] * b[j + 1]; lr += b[j] * b[j + 1]; n++; peak = Math.max(peak, Math.abs(b[j]), Math.abs(b[j + 1])); }
  return { rms: Math.sqrt((l + r) / (2 * n)), corr: lr / Math.sqrt(l * r + 1e-20), peak }; };
const band = (blocks, lo, hi) => { // crude: energy of first difference vs signal (brightness proxy)
  let e = 0, d = 0, prev = 0; for (const b of blocks) for (let j = 0; j < 256; j += 2) { const v = b[j] + b[j + 1]; e += v * v; d += (v - prev) ** 2; prev = v; }
  return d / (e + 1e-20); };
let fails = 0;
const check = (ok, msg) => { console.log((ok ? 'PASS ' : 'FAIL ') + msg); if (!ok) fails++; };
const knobs = () => { const m = new Uint8Array(x.memory.buffer, x.sim_knobs(), 128), t = (o) => new TextDecoder().decode(m.slice(o, o + 16)).split('\0')[0];
  return [0, 1, 2, 3].map((c) => `${t(c * 32)}=${t(c * 32 + 16)}`).join('  '); };
const dumpLcd = (name) => writeFileSync(`${out}/lcd_${name}.rgb565`, new Uint8Array(x.memory.buffer, x.sim_pixels(), 240 * 240 * 2).slice());

x.sim_boot(1); run(1.0);
// ---- drum mode, auditions
tap(BTN, B.SEQ); run(0.2);
dumpLcd('drums');
// Measure from the key press: short sounds are over before the key is released.
const strike = (k, s) => { x.sim_control(KEY, k, 1); const a = run(s); x.sim_control(KEY, k, 0); run(0.05); return a; };
const hat = strike(4, 0.15), keyF = strike(0, 0.15), keyG = strike(2, 0.15);
check(stats(hat).rms > 0.002, `A plays the hi-hat (rms ${stats(hat).rms.toFixed(3)})`);
check(stats(keyF).rms > 0.001 && stats(keyG).rms > 0.0005, `F and G play glitch sounds (rms ${stats(keyF).rms.toFixed(4)} / ${stats(keyG).rms.toFixed(4)})`);
const lowShare1 = (blocks) => { let lo = 0, all = 0, y = 0;
  for (const b of blocks) for (let j = 0; j < 256; j += 2) { const v = b[j] + b[j + 1]; y += (v - y) * 0.017; lo += y * y; all += v * v; }
  return lo / (all + 1e-20); };
check(lowShare1(keyF) < 0.1 && lowShare1(keyG) < 0.1, `no kick or snare on F/G (bass share ${(100 * lowShare1(keyF)).toFixed(1)}% / ${(100 * lowShare1(keyG)).toFixed(1)}%)`);
// SOURCE on G cannot reach kick, snare or hi-hat.
tap(KEY, 2); for (let i = 0; i < 6; i++) turn(2, -5);
check(knobs().startsWith('SOURCE=IMPULSE'), `SOURCE on G stops at IMPULSE (${knobs().split('  ')[0]})`);
tap(KEY, 4); turn(2, 5);
check(knobs().startsWith('SOURCE=HIHAT'), `A stays the classic hi-hat (${knobs().split('  ')[0]})`);
// The eight GLITCH sounds sit on keys 9..16 of the default kit; SOURCE ends at HIGH.
const glitchNames = ['CRUNCH', 'CHIRP', 'SNAP', 'DUST', 'BITS', 'THUMP', 'POP', 'HIGH'];
let glitchOk = true;
for (let k = 9; k <= 16; k++) {
  const a = strike(k, 0.25);
  tap(KEY, k); run(0.05);
  const name = knobs().split('  ')[0];
  if (name !== `SOURCE=${glitchNames[k - 9]}` || !(stats(a).rms > 0.0003)) { glitchOk = false; console.log(`     key ${k}: ${name}, rms ${stats(a).rms.toFixed(4)}`); }
}
check(glitchOk, 'keys 9-16 play CRUNCH, CHIRP, SNAP, DUST, BITS, THUMP, POP, HIGH');
for (let i = 0; i < 4; i++) turn(2, 10);
check(knobs().startsWith('SOURCE=HIGH'), `SOURCE ends at HIGH, no sample sources (${knobs().split('  ')[0]})`);
check(knobs().split('  ')[3].startsWith('COLOR='), `DRUM 1/5 has COLOR on knob 4 (${knobs()})`);
// ---- step editing: hold SEQ + white keys on the selected slot (F)
tap(KEY, 0); run(0.5);
const before = x.sim_drum_steps(0);
x.sim_control(BTN, B.SEQ, 1); run(0.05); tap(KEY, 6); tap(KEY, 7); x.sim_control(BTN, B.SEQ, 0); run(0.1);
const after = x.sim_drum_steps(0);
check(after === (before ^ 0b11000), `SEQ + white keys 4 and 5 toggle steps of F (0x${before.toString(16)} -> 0x${after.toString(16)})`);
check(knobs().startsWith('SOURCE=IMPULSE'), `still in drum mode after editing steps (${knobs()})`);
x.sim_control(BTN, B.SEQ, 1); run(0.05); tap(KEY, 6); tap(KEY, 7); x.sim_control(BTN, B.SEQ, 0); run(0.1); // undo the test steps
// ---- running pattern: sound, width from random panning
tap(BTN, B.PLAY);
const groove = run(8.0);
const g = stats(groove);
// Microsound layer like keins: 4..20 tiny clicks per second, quiet.
const clicksPerSecond = (() => { // envelope onsets of the high band, at least 4 ms apart (as in the keins analysis)
  let n = 0, env = 0, last = -1e9, idx = 0, hp = 0, armed = true;
  for (const bl of groove) for (let j = 0; j < 256; j += 2) { const v = bl[j] + bl[j + 1]; const d = v - hp; hp = v;
    env = Math.max(Math.abs(d), env * 0.99);
    if (armed && env > 0.01 && idx - last > 176) { n++; last = idx; armed = false; }
    if (env < 0.004) armed = true; idx++; }
  return n / (groove.length * 128 / 44100); })();
check(clicksPerSecond >= 4 && clicksPerSecond <= 20 && g.peak < 0.6, `microsound layer: ${clicksPerSecond.toFixed(1)} clicks/s, peak ${g.peak.toFixed(2)} (keins: 5-15/s, quiet)`);
const bal = groove.map((b) => { let l = 0, r = 0; for (let j = 0; j < 256; j += 2) { l += b[j] * b[j]; r += b[j + 1] * b[j + 1]; } return l + r > 1e-6 ? (r - l) / (r + l) : null; }).filter((v) => v !== null);
const hard = bal.filter((v) => Math.abs(v) > 0.6).length / bal.length;
check(Math.min(...bal) < -0.25 && Math.max(...bal) > 0.25 && hard < 0.2, `moderate panning like keins (balance ${Math.min(...bal).toFixed(2)}..${Math.max(...bal).toFixed(2)}, ${(100 * hard).toFixed(0)}% of blocks clearly one-sided)`);
dumpLcd('drums_running');
// No kick or snare in the default pattern: hardly any energy below 120 Hz.
const lowShare = (blocks) => { let lo = 0, all = 0, y = 0;
  for (const b of blocks) for (let j = 0; j < 256; j += 2) { const v = b[j] + b[j + 1]; y += (v - y) * 0.017; lo += y * y; all += v * v; }
  return lo / (all + 1e-20); };
const ls = lowShare(groove);
check(ls < 0.05, `default pattern has no bass weight (energy below ~120 Hz: ${(100 * ls).toFixed(1)}%)`);
// ---- ARP in drum mode: a completely new random beat, five times.
const allSteps = () => Array.from({ length: 27 }, (_, k) => x.sim_drum_steps(k)).join(',');
const countClicks = (blocks) => { let n = 0, env = 0, last = -1e9, idx = 0, hp = 0, armed = true;
  for (const bl of blocks) for (let j = 0; j < 256; j += 2) { const v = bl[j] + bl[j + 1]; const d = v - hp; hp = v;
    env = Math.max(Math.abs(d), env * 0.99);
    if (armed && env > 0.01 && idx - last > 176) { n++; last = idx; armed = false; }
    if (env < 0.004) armed = true; idx++; }
  return n / (blocks.length * 128 / 44100); };
let prevSteps = allSteps(), beatsOk = true, rates = [];
for (let i = 0; i < 5; i++) {
  tap(BTN, 10); run(0.3);                                                // ARP
  const now = allSteps();
  const beat = run(8.0), st = stats(beat), cps = countClicks(beat);
  rates.push(cps.toFixed(1));
  if (now === prevSteps || cps < 3 || cps > 20 || st.peak > 0.6) beatsOk = false;
  prevSteps = now;
}
check(beatsOk, `ARP rolls five different beats, each in the keins range (clicks/s: ${rates.join(', ')})`);
tap(KEY, 4); run(0.1);
check(knobs().startsWith('SOURCE=HIHAT'), `A is still the hi-hat after random beats (${knobs().split('  ')[0]})`);
// ---- back to the tones: drums keep running underneath a drone chord
tap(BTN, B.HOME);
for (const k of [0, 7, 11]) tap(KEY, k);
const both = run(8.0);
check(stats(both).rms > g.rms, 'drone chord plays over the running drums');
tap(BTN, B.SEQ); tap(BTN, B.PLAY); tap(BTN, B.HOME); tap(BTN, B.PLAY); run(5);
check(stats(run(0.5)).rms < 0.02, 'drums stopped and tones off: it fades out');

const pcm = new Int16Array(recording.length * 256);
recording.forEach((b, i) => b.forEach((v, j) => { pcm[i * 256 + j] = Math.max(-32768, Math.min(32767, Math.round(v * 32767))); }));
const h = Buffer.alloc(44);
h.write('RIFF', 0); h.writeUInt32LE(36 + pcm.byteLength, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20);
h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(176400, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
h.write('data', 36); h.writeUInt32LE(pcm.byteLength, 40);
writeFileSync(`${out}/drums_demo.wav`, Buffer.concat([h, Buffer.from(pcm.buffer)]));
console.log(`demo: ${out}drums_demo.wav`);
process.exit(fails ? 1 : 0);
