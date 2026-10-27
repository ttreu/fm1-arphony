// Measures how rhythmic the drum track is: bar-to-bar repetition of the
// click energy per 16th, and how much of it sits right on the 16th grid.
// Usage: node tests/steel_rhythm_check.mjs  (exit 1 if a beat fails)
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
const wasm = readFileSync(here('../build/web/simulator.wasm'));
async function measure(randomRolls) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const x = instance.exports; x._initialize?.();
  const run = (s) => { const a = []; for (let i = 0; i < Math.round(s * 44100 / 128); i++) a.push(new Float32Array(x.memory.buffer, x.sim_render(), 256).slice()); return a; };
  const tap = (k, i) => { x.sim_control(k, i, 1); run(0.03); x.sim_control(k, i, 0); run(0.03); };
  x.sim_boot(1); run(0.5);
  tap(0, 11);                                         // SEQ: drum mode
  for (let i = 0; i < randomRolls; i++) tap(0, 10);   // ARP: random beats
  x.sim_control(0, 12, 1);                            // PLAY down: the bar starts with this block
  const blocks = run(4 * 240 / 90 + 0.05); x.sim_control(0, 12, 0);
  // high-band (first difference) energy per sample
  const e = []; let prev = 0;
  for (const b of blocks) for (let j = 0; j < 256; j += 2) { const v = b[j] + b[j + 1]; e.push((v - prev) ** 2); prev = v; }
  const sixteenth = 60 / 90 / 4 * 44100, bars = 4, bins = [];
  let onGrid = 0, all = 0;
  for (let bar = 0; bar < bars; bar++) {
    const row = [];
    for (let s = 0; s < 16; s++) {
      const a = Math.round((bar * 16 + s) * sixteenth), b = Math.round((bar * 16 + s + 1) * sixteenth);
      let sum = 0, head = 0;
      for (let i = a; i < b && i < e.length; i++) { sum += e[i]; if (i - a < 0.015 * 44100) head += e[i]; }
      row.push(sum); onGrid += head; all += sum;
    }
    bins.push(row);
  }
  const corr = (p, q) => { const mp = p.reduce((s, v) => s + v, 0) / 16, mq = q.reduce((s, v) => s + v, 0) / 16;
    let n = 0, dp = 0, dq = 0; for (let i = 0; i < 16; i++) { n += (p[i] - mp) * (q[i] - mq); dp += (p[i] - mp) ** 2; dq += (q[i] - mq) ** 2; }
    return n / Math.sqrt(dp * dq + 1e-30); };
  let rep = 0; for (let b = 1; b < bars; b++) rep += corr(bins[b - 1], bins[b]);
  return { repeat: rep / (bars - 1), grid: onGrid / all };
}
let fails = 0;
for (const rolls of [0, 1, 2, 3, 4, 5]) {
  const m = await measure(rolls);
  const ok = m.repeat > 0.6 && m.grid > 0.6;
  if (!ok) fails++;
  console.log(`${ok ? 'PASS' : 'FAIL'} ${rolls ? `random beat ${rolls}` : 'factory kit   '}: bars repeat ${m.repeat.toFixed(2)}, energy within 15 ms of a 16th ${(100 * m.grid).toFixed(0)}%`);
}
process.exit(fails ? 1 : 0);
