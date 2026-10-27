// Renders the four keins-style presets (an open 9th chord, 10 s each) to
// build/steel-test/synth_presets_demo.wav for listening.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
mkdirSync(here('../build/steel-test/'), { recursive: true });
const { instance } = await WebAssembly.instantiate(readFileSync(here('../build/web/simulator.wasm')), {});
const x = instance.exports; x._initialize?.();
const rec = [], run = (s) => { for (let i = 0; i < Math.round(s * 44100 / 128); i++) rec.push(new Float32Array(x.memory.buffer, x.sim_render(), 256).slice()); };
const tap = (k, i) => { x.sim_control(k, i, 1); run(0.03); x.sim_control(k, i, 0); run(0.03); };
const turn = (e, d) => { x.sim_control(2, e, d); run(0.02); x.sim_control(2, e, 0); run(0.02); };
x.sim_boot(1); run(0.5); rec.length = 0;
const chord = [0, 7, 12, 16, 19, 23, 26];   // white keys: degrees 1, 5, 8, 10, 12, 14, 16
for (let preset = 10; preset < 14; preset++) {
  turn(6, preset - (preset === 10 ? 0 : preset - 1));
  for (const k of chord) tap(1, k);
  run(9);
  tap(0, 12); run(3);                        // PLAY: all tones off, release
}
const pcm = new Int16Array(rec.length * 256);
rec.forEach((b, i) => b.forEach((v, j) => { pcm[i * 256 + j] = Math.max(-32768, Math.min(32767, Math.round(v * 32767))); }));
const h = Buffer.alloc(44);
h.write('RIFF', 0); h.writeUInt32LE(36 + pcm.byteLength, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20);
h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(176400, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
h.write('data', 36); h.writeUInt32LE(pcm.byteLength, 40);
const out = here('../build/steel-test/synth_presets_demo.wav');
writeFileSync(out, Buffer.concat([h, Buffer.from(pcm.buffer)]));
console.log('demo:', out);
