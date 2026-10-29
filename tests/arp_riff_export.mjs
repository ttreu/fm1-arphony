// Records the riff note sequences ARPHONY plays (every preset, and many
// RANDOM riffs over several presets) for tests/riff_similarity.py.
// Usage: node tests/arp_riff_export.mjs [out.json] [random-count]
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const outFile = process.argv[2] || fileURLToPath(new URL('../build/arp-test/riffs.json', import.meta.url));
const randomCount = Number(process.argv[3] || 300);
mkdirSync(new URL('../build/arp-test/', import.meta.url), { recursive: true });
const wasm = readFileSync(new URL('../build/web/simulator.wasm', import.meta.url));
const BTN = 0, ENC = 2, PLAY = 12, SEQ = 11, PRESETS = 15;

async function boot() {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const x = instance.exports; x._initialize?.();
  const run = (s) => { for (let i = 0; i < Math.round(s * 44100 / 128); i++) x.sim_render(); };
  x.sim_boot(1); run(1.0);
  return { x, run };
}
const press = (s, kind, i) => { s.x.sim_control(kind, i, 1); s.run(0.03); s.x.sim_control(kind, i, 0); s.run(0.03); };
const turn = (s, enc, d) => { s.x.sim_control(ENC, enc, d); s.run(0.02); s.x.sim_control(ENC, enc, 0); s.run(0.02); };
// Every riff note while playing, in order (the counter catches repeated notes).
const record = (s, notes) => {
  const seq = []; let count = s.x.sim_arp_count();
  for (let b = 0; b < 120 * 344 && seq.length < notes; b++) {
    s.x.sim_render();
    const c = s.x.sim_arp_count();
    if (c !== count) { count = c; seq.push(s.x.sim_arp_last_note(0)); }
  }
  return seq;
};

const riffs = [];
{
  const s = await boot();
  for (let p = 0; p < PRESETS; p++) {
    if (p) turn(s, 6, 1);
    s.run(0.1);
    press(s, BTN, PLAY);
    riffs.push({ name: `preset ${p + 1}`, notes: record(s, 96) });
    press(s, BTN, PLAY); s.run(1.5);
  }
}
{
  const s = await boot();
  let preset = 0;
  for (let r = 0; r < randomCount; r++) {
    if (r % 20 === 0) { turn(s, 6, 1); preset = (preset + 1) % PRESETS; s.run(0.1); }  // vary key, chords, rate
    press(s, BTN, SEQ);
    press(s, BTN, PLAY);
    riffs.push({ name: `random ${r + 1} (preset ${preset + 1})`, notes: record(s, 32) });
    press(s, BTN, PLAY); s.run(0.6);
  }
}
writeFileSync(outFile, JSON.stringify(riffs));
console.log(`${riffs.length} riffs -> ${outFile}`);
