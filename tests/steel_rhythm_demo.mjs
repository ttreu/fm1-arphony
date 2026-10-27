// Listening demo: factory drum kit, then three random beats (ARP), 8 s each,
// over a HOVER chord with OVERTONE turned up.
import { readFileSync, writeFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
const { instance } = await WebAssembly.instantiate(readFileSync(here('../build/web/simulator.wasm')), {});
const x = instance.exports; x._initialize?.();
const rec = [], run = (s) => { for (let i = 0; i < Math.round(s * 44100 / 128); i++) rec.push(new Float32Array(x.memory.buffer, x.sim_render(), 256).slice()); };
const tap = (k, i) => { x.sim_control(k, i, 1); run(0.03); x.sim_control(k, i, 0); run(0.03); };
const turn = (e, d) => { x.sim_control(2, e, d); run(0.02); x.sim_control(2, e, 0); run(0.02); };
x.sim_boot(1); run(0.5); rec.length = 0;
turn(6, 16); turn(5, 70);                         // HOVER, OVERTONE up (PLAY page K4)
for (const k of [0, 7, 12, 16, 19]) tap(1, k);
run(3);
tap(0, 11); tap(0, 12); run(8);                   // drums: factory kit
for (let i = 0; i < 3; i++) { tap(0, 10); run(8); }  // ARP: random beats
tap(0, 12); tap(0, 8); tap(0, 12); run(5);
const pcm = new Int16Array(rec.length * 256);
rec.forEach((b, i) => b.forEach((v, j) => { pcm[i * 256 + j] = Math.max(-32768, Math.min(32767, Math.round(v * 32767))); }));
const h = Buffer.alloc(44);
h.write('RIFF', 0); h.writeUInt32LE(36 + pcm.byteLength, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20);
h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(176400, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
h.write('data', 36); h.writeUInt32LE(pcm.byteLength, 40);
writeFileSync(here('../build/steel-test/rhythm_demo.wav'), Buffer.concat([h, Buffer.from(pcm.buffer)]));
console.log('demo:', here('../build/steel-test/rhythm_demo.wav'));
