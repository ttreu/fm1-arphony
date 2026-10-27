// Listening demos: (1) the four hovering presets, first one note, then a
// chord; (2) HOVER with the microsound drum track running.
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
const here = (p) => fileURLToPath(new URL(p, import.meta.url));
mkdirSync(here('../build/steel-test/'), { recursive: true });
const wasm = readFileSync(here('../build/web/simulator.wasm'));
async function render(script, file) {
  const { instance } = await WebAssembly.instantiate(wasm, {});
  const x = instance.exports; x._initialize?.();
  const rec = [], run = (s) => { for (let i = 0; i < Math.round(s * 44100 / 128); i++) rec.push(new Float32Array(x.memory.buffer, x.sim_render(), 256).slice()); };
  const tap = (k, i) => { x.sim_control(k, i, 1); run(0.03); x.sim_control(k, i, 0); run(0.03); };
  const turn = (e, d) => { x.sim_control(2, e, d); run(0.02); x.sim_control(2, e, 0); run(0.02); };
  x.sim_boot(1); run(0.5); rec.length = 0;
  script({ run, tap, turn });
  const pcm = new Int16Array(rec.length * 256);
  rec.forEach((b, i) => b.forEach((v, j) => { pcm[i * 256 + j] = Math.max(-32768, Math.min(32767, Math.round(v * 32767))); }));
  const h = Buffer.alloc(44);
  h.write('RIFF', 0); h.writeUInt32LE(36 + pcm.byteLength, 4); h.write('WAVEfmt ', 8); h.writeUInt32LE(16, 16); h.writeUInt16LE(1, 20);
  h.writeUInt16LE(2, 22); h.writeUInt32LE(44100, 24); h.writeUInt32LE(176400, 28); h.writeUInt16LE(4, 32); h.writeUInt16LE(16, 34);
  h.write('data', 36); h.writeUInt32LE(pcm.byteLength, 40);
  writeFileSync(here('../build/steel-test/' + file), Buffer.concat([h, Buffer.from(pcm.buffer)]));
  console.log('demo:', here('../build/steel-test/' + file));
}
const chord = [0, 7, 12, 16, 19, 23, 26];
await render(({ run, tap, turn }) => {
  turn(6, 14);                                // FLOAT GLASS (14), then UNDERTOW, HOVER, HALO
  for (let p = 0; p < 4; p++) {
    if (p) turn(6, 1);
    tap(1, 12); run(8);                       // one note (an octave above the root)
    for (const k of chord) if (k !== 12) tap(1, k);
    run(8);
    tap(0, 12); run(3);                       // PLAY: tones off
  }
}, 'float_presets_demo.wav');
await render(({ run, tap, turn }) => {
  turn(6, 16);                                // HOVER
  for (const k of [0, 7, 12, 16, 19]) tap(1, k);
  run(4);
  tap(0, 11); tap(0, 12);                     // SEQ: drum mode, PLAY: drums run
  run(24);
  tap(0, 12); tap(0, 8); tap(0, 12); run(6); // drums stop, HOME, tones off
}, 'keins_style_demo.wav');
