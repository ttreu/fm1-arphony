# GLIDER – FM-1 Custom Firmware

Fork of M-VAVE FM1 Open V14. The FM-1 becomes a latching drone instrument
with steel-guitar style glides, warm analog-style voices, effects and a
microsound drum track. The FM1 sequencer, DX7 and project pages are gone.

## Playing

**Tones** (default mode)

| Control | Function |
|---|---|
| White keys | press = tone on, press again = tone off (16 scale tones) |
| Black keys | glide the white key on its left (Fis also E, Cis also B) by 1-4 scale steps: one click = up, double click = down, the next click = back home. A single click starts after 350 ms (the double-click window) |
| SEL held + white key | key selector: the key of that note (C, D, E, F, G, A, B) |
| SEL held + PRESETS | key in semitones (also sharps) |
| PLAY | all tones off |
| SEL held + PLAY | tap tempo |
| OCT- / OCT+ | the keys reach an octave lower / higher (-2..+2); tones already sounding stay in their octave. Each octave keeps its own latched keys and glides; 16 tones in total, the oldest gives way |
| PRESETS | preset (22) |
| HOME | play page: glide time, glide steps, tone, overtones |
| EDIT (repeat) | SOUND 1-5: wave, detune, osc 2, sub / tone, tone env, drift, width / overtones, overtone brightness, undertones, float / float rate, detune LFO, detune, drift / interval, interval mode, osc 2, detune |
| ENV | attack, decay, sustain, release |
| LFO | vibrato rate, depth, delay, level |
| SEL | key, scale, tuning (equal / just), glide curve. Scales: major, minor, dorian, mixolydian, lydian, phrygian, hijaz, penta major/minor, harmonic/melodic minor, whole tone, hirajoshi, in-sen, double harmonic; chords as scales (the white keys stack only chord tones): sus2, sus4, dim, dim7, aug, maj7, min7, maj9, min9, add9 |
| FX (repeat) | 1 filter, 2 tape echo, 3 delay, 4 shimmer reverb, 5 tempo (BPM, sync, **CPU load**), 6 colour |

**Intervals** (SOUND 5/5): oscillator 2 plays an interval above or below
oscillator 1 (2nd to 2 octaves up, 2nd to octave down), so every key sounds a
dyad. IN KEY: the interval follows the scale (thirds are major or minor as the
key says) and glides along with the black keys. PURE: an exact just interval
on every key (parallel fifths and so on). Presets 19-22: FIFTH DRONE,
THIRD STEEL, SIXTH SWELL, TENTH GLASS.

**Drums** (tap SEQ to switch; the tones keep sounding)

| Control | Function |
|---|---|
| Any key | play and select its slot (A = classic hi-hat, others microsounds) |
| SEQ held + white keys | the selected slot's 16 steps |
| PLAY | start / stop the drums |
| ARP | a completely new random beat |
| EDIT (repeat) | 1 sound (source, pitch, decay, color), 2 rhythm (probability, ratchet, micro timing, scatter), 3 space (pan mode, pan, tape send, shimmer send), 4 slot (level, random, rate), 5 kit (drum level, hiss, BPM) |
| SELECT | select slot |
| HOME / ENV / LFO / FX | back to the tones |

All drum sounds are synthesized; GLIDER uses no samples (the sample bank in
the FM-1 is left alone). Sources: IMPULSE, TRAIN, BLIP (in the preset's key),
TICK, CUT (dips the synth layer), and eight sounds from GLITCH, shaped by
COLOR:

| Source | Sound | COLOR |
|---|---|---|
| CRUNCH | sample-and-hold digital noise (PITCH = hold rate) | smooth steps → harsh, jittery |
| CHIRP | a fast sine sweep | down ← → up |
| SNAP | a dry, gated band of noise with a click | dark → bright |
| DUST | a crackle of random clicks | sparse → dense |
| BITS | LFSR noise | tonal → noisy |
| THUMP | a micro kick (PITCH 18-200 Hz) | pure → click and drive |
| POP | a speaker pop: a jump that slides back | soft → sharp edge |
| HIGH | a very high, short sine beep (4-18 kHz) | pure → a beating second tone |

**MIDI in** (USB, BLE, DIN): channels 1-9 and 11-16 play the keyboard
(F3-G5 at OCT 0, other octaves in their own octave up to +-2; held notes; black keys glide while held), channel 10 plays the drum
slots from C1, CC 123 = all tones off, MIDI clock sets the tempo.

## Building on macOS

Toolchains next to this folder (see VA history in `../archive` for setup):
`../toolchains/jl/C$/JL/pi32` (JieLi 2.5.3 via innoextract),
`../toolchains/wine/Wine Stable.app`, `../toolchains/wasi/wasi-sdk-27.0-arm64-macos`.

```
python3 tools/build_web_mac.py      # browser simulator  -> build/preview/index.html
python3 tools/build_mac.py          # firmware + update page
tests/run_native.sh                 # DSP tests
node tests/steel_sim_test.mjs       # simulator: keys, glides, effects, tempo, MIDI
node tests/steel_drums_sim_test.mjs # simulator: drum track
node tests/steel_rhythm_check.mjs   # drum rhythm measurements
```

Outputs:
- `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc` – firmware package
- `build/official-web/FM1-GLIDER.html` – workbench page with this firmware
  and the original V15 recovery image (Chrome/Edge)

## Flashing

1. Back up projects, voices and samples you want to keep (they are not used
   by GLIDER but stay in Flash).
2. Open the workbench page in Chrome or Edge, *Device update*, connect the
   FM-1 by USB, start the update. Keep power and USB connected.
3. Back to stock: same page, *Recovery & force update*, original V15.

**From the command line** (once the FM-1 runs GLIDER or another FM1 Open
firmware; the first install from stock V15 needs the workbench page):

```
python3 -m pip install python-rtmidi   # once
python3 tools/flash_fm1.py info        # read-only: identity and installed build
python3 tools/flash_fm1.py check       # check build/dx7/generated/FM1-GLIDER.fwsc
python3 tools/flash_fm1.py flash       # flash (type FLASH to confirm)
```

It speaks the workbench's SysEx update protocol over USB-MIDI, checks the
package the same way and verifies the installed build after the restart.
Close the workbench page while it runs. If anything goes wrong, use the
workbench page, *Recovery & force update*.

## Status

Runs on a real FM-1 (tested on one unit, October 2026); also checked in the
simulator and with native DSP tests.

**CPU guard:** heavy presets (DARK CLUSTER, HOVER, UNDERTOW, ...) cost about
twice as much per tone as light ones. When the audio load reaches 92 % or
audio blocks are missed, release tails fade out quickly first; if that is
not enough, the oldest tone fades out within about 30 ms and the tone limit
drops by one ("CPU LIMIT: n TONES", never below 4). At the limit a new tone
replaces the oldest one. After 4 s below 75 % the limit rises again, up to
16. The CPU figure is on FX 5/6 (amber above 85 %). The browser simulator
has no CPU budget, so the guard only acts on the device.
