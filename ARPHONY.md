# ARPHONY – FM-1 Custom Firmware

A riff on top, the harmony below. The FM1 sequencer, DX7 and project pages are
gone; the panel plays one instrument with two layers:

- **ARP** – single notes: a riff over the current chord's tones.
- **HARM** – the bass plus up to two chord tones (NOTES 1–3).

## Playing

| Control | Function |
|---|---|
| PLAY | start / stop |
| SAVE | store everything as a user preset: the user preset playing, else the first free slot |
| SAVE held + white key | store it in user preset 1–16 |
| SEL held + SEQ | new RANDOM chords (CHORDS switches to RANDOM) |
| SEQ | a new RANDOM riff (RIFF switches to RANDOM; press again for the next) |
| Keys F3–E4 (low half) | a chord on that note, with its own bass |
| Keys F4–G5 (high half) | the same chord over the key's tonic (pedal point, e.g. G/D) |
| ARP | AUTO → HAND: stay on the chord that is playing; HAND → AUTO: back to the progression |
| SEL held + white key | the key (tonic) |
| SEL held + PRESETS | the key in semitones |
| SEL held + PLAY | tap tempo |
| OCT- / OCT+ | riff octave (C3–C7); with SEL held: bass octave |
| PRESETS | preset: the 15 factory presets, then the stored user presets (U1 …) |
| HOME | the main pages 1–4 (HARMONY, RIFF, GROOVE, FEEL), one after the other; the arpeggio starts on HOME 1 |
| SELECT | page |
| K1–K4 | the page's four settings: the first detent shows the value in its box, the next ones change it |

A key chord changes on the next beat; a stopped arpeggio starts. A key's chord
is the chord of that degree in the key's MODE, coloured by COLOR (triad, add9,
7th, 9th, sus2, 6th). Notes outside the mode give borrowed major chords (bIII,
bVI, bVII …). The LEDs show the current chord's tones.

## Display

One colour field that follows the chord (its root around the circle of
fifths, light for major, deep for minor, pale while stopped). On top four
boxes in two rows (K1 K2, K3 K4), a shade darker: each names its function; turn the
knob and the box shows the value for 1.5 s. Below the chord name, the
progression as dots, the riff as bars with the playhead, and preset, key and
mode.

## Pages

| Button | Page | K1 | K2 | K3 | K4 |
|---|---|---|---|---|---|
| HOME | 1 HARMONY | KEY | MODE | CHORDS | LENGTH |
| HOME again | 2 RIFF | RIFF | RATE | LEAD | HARMONY |
| HOME again | 3 GROOVE | BPM | BASS RHY | ARP OCT | TWIN |
| HOME again | 4 FEEL | GATE | SWING | ACCENT | MUTATE |
| SEL | CHORDS | COLOR | NOTES | BASS OCT | MIDI OUT |
| GLO | MIX 1/2 | ARP LVL | BASS LVL | CHORD LVL | HARM FX |
| GLO again | MIX 2/2 | TWIN LVL | LEVEL | WIDTH | DRIFT |
| EDIT | ARP SOUND 1/2 | SHAPE | DETUNE | OSC 2 | TONE |
| EDIT | ARP SOUND 2/2 | FM BELL | FM RATIO | TONE ENV | ARP LVL |
| EDIT | HARM SOUND 1/2 | SHAPE | DETUNE | OSC 2 | SUB |
| EDIT | HARM SOUND 2/2 | TONE | TONE ENV | DRIFT | WIDTH |
| ENV | ARP / HARM ENVELOPE | ATTACK | DECAY | SUSTAIN | RELEASE |
| LFO | MOTION | VIB RATE | VIBRATO | DRIFT | WIDTH |
| FX | 1 FILTER, 2 TAPE, 3 DELAY, 4 SHIMMER, 5 TEMPO (BPM, syncs, **CPU**), 6 COLOR | | | | |

**LEAD** (HOME 2, K3): a ready sound for the riff – SINE LONG (the default
in every preset), SINE, SINE SOFT, HARP, AIRY, or CLASSIC (the preset's own
riff sound of the first versions). Choosing one sets the ARP SOUND and ARP
ENVELOPE pages, which can then be fine-tuned; the bass sound is not touched.

**HARMONY** (how the riff meets the chords):

- **LEAD** – the riff is built on the tonic chord; each note moves to the
  nearest tone of the current chord. Shared tones stay, the others move by a
  step. The same figure takes a new meaning over each bass note.
- **PEDAL** – the riff stays exactly on the tonic chord; only the harmony
  below moves (maj7, sus and add9 colours appear by themselves).
- **FOLLOW** – the riff is transposed to each chord's root.
- **PENTA** – the riff stays on the key's pentatonic (major pentatonic in
  MAJOR, LYDIAN, MIXOLYD; minor pentatonic otherwise), whatever the bass does.
  Over IV it leaves out the bass note and sounds as maj9/6/9 (C D E G A over F).

**TWIN** – a second riff layer: OCTAVE doubles every riff note an octave up
(softer, panned the other way); PINGS adds sparse high notes two octaves up
the riff ladder on a dotted-quarter grid (in a pentatonic: D5 and E5 over a
riff from C3), like a bell line above a busy ostinato.

**No silent gaps:** a riff may leave steps out, but the note before then
holds right up to the next note (or the next chord). GATE sets how long the
other notes sound within their step.

**RIFF:** RISE, PEDAL, CASCADE, BROKEN, OCTAVES, TRIPLET (for 1/8T), PING
(few long notes, for long delays), WHEEL (six against the bar), UP, DOWN, UP DOWN,
PULSE, SWAY, LEAPS, SPARKLE, SIGH (a falling line with held notes),
INTERLOCK (a busy 16th ostinato, made for PENTA), RANDOM.

**RANDOM** (SEQ): a new riff from rules, not from a table. A motif of four
steps (three in triplet rates) is played, moved up or down the chord ladder,
played again and closed by a fresh figure; steps go mostly to neighbouring
chord tones, sometimes leap, a third of the riffs keep returning to one inner
pedal tone; a few left-out steps, where the note before holds. 16, 8 or (triplets) 12 steps.

**Never a copy.** The firmware holds fingerprints of every 7-note figure in
the reference recordings (as intervals, in any key and upside down;
`app/arp_forbidden.inc`). Each time the riff, the chords or their settings
change, the riff is checked as it will sound – over every chord, across the
chord changes, as the progression repeats. A RANDOM riff that contains such a
figure is thrown away and made again; a built-in riff is nudged (one step a
chord tone up or down) until none is left. Figures of fewer than three
different notes (a pulse, a pedal and one neighbour) do not count. MUTATE
changes notes while playing and is not checked. Riff steps
are positions on the chord's ladder, so the same riff fits any chord.

**BASS RHY:** HOLD, 8THS, 16THS, SYNC (3+3+2), OCTAVES, ROOT 5, QUARTERS,
DOTTED. The upper chord tones always ring through the chord; the bass marks
every chord change.

**MODE** colours the chords. Every progression is written in its own mode
(the * in the table below); choosing the progression sets MODE there, and it
sounds as written. Turn MODE and the chords keep their roots but take the new
mode's notes in the same colour: Em9 – Aadd9 (Dorian) becomes Emaj9 – Aadd9
in MAJOR, E9 – Aadd9 in MIXOLYD, Em7 – Am(add9) in PHRYGIAN. Chords that are
borrowed already in the progression's own mode (a root outside it, or the
other third, like the minor i in MIX PEDAL) stay as written. MODE also sets
the chords on the keys, TONIC and PENTA.

**CHORDS** (progressions, relative to KEY; LENGTH = ½, 1, 2 or 4 bars each):

| Name | Chords (own mode *) | Idea |
|---|---|---|
| I-9 IV | i9 – IV(add9) *DORIAN | Dorian vamp with the major IV |
| I III VII IV | i7 – IIImaj7 – VIIadd9 – IV6/9 *DORIAN | Dorian four |
| BVI BVII | bVImaj7 – bVIIadd9 *MINOR | Aeolian, never resolves |
| VI VII I | bVImaj7 – bVII6/9 – i(add9) – i7 *MINOR | |
| MAJ9 PEDAL | Imaj9 – IV/I *MAJOR | the bass stays on the tonic |
| II III IV | ii7 – iii7 – IVmaj7 – iii7 *MAJOR | rising steps |
| IV V LYDIAN | IVadd9 – Vsus2 – IVmaj7 – ii9 *MAJOR | IV – V that floats |
| I BIII | Iadd9 – bIIImaj7 *MAJOR | chromatic mediant |
| MIX PEDAL | I – IV/I – i – bVII/I *MIXOLYD | major/minor mixture over one bass note |
| I IV MINOR | i(add9) – iv7 – i(add9) – bVIIsus2 *MINOR | |
| III IV | IIIadd9 – IVadd9 – IIImaj7 – IV6 *DORIAN | Dorian, the bass a tone apart |
| I VII IV III | i9 – VIIadd9 – IV6 – IIImaj7 *DORIAN | Dorian wheel |
| LINE DOWN | Iadd9 – Imaj7/7 – I7/b7 – IV6/9/6 *MAJOR | the chord stays, the bass falls by semitones (1 – 7 – b7 – 6) |
| IV PENTA | IVmaj9 – IVmaj9 – V6/9 – vi7 *MAJOR | for HARMONY PENTA |
| TONIC | the key's own chord (MODE, COLOR) | for playing chords on the keys |

## User presets

SAVE stores every setting, with the RANDOM riff and RANDOM chords, in one of
16 user presets; the PRESETS knob reaches them after the factory presets (the
display shows e.g. "U3 SIX TO SEVEN": slot 3, grown from SIX TO SEVEN).
Tempo under MIDI clock and the MIDI OUT setting stay as they are when one is
loaded.

**Flash:** user presets live in the last 64 KB of the FM-1's sample area
(ARPHONY plays no samples; GLITCH uses the same place). The first SAVE
overwrites the end of the sample bank that is on the device. Back up the
sample bank with the stock firmware or the workbench before, if you want to
keep it for the original firmware.

## RANDOM chords

SEL held + SEQ makes a new progression of two or four chords in the current
MODE, the way the built-in ones are made: mostly from the tonic, roots from
the mode without diminished chords, plus the borrowed bVII, bVI and bIII;
steps of a tone or a semitone are preferred (IV – V, bVI – bVII), no V – I at
the end; colours add9, 7, 9, 6, sus2, now and then a chord over the tonic.
Turning MODE recolours them like the built-in progressions; the riff over them
passes the same copy check.

## Presets

DORIAN SWAY, MODAL FOUR, SIX TO SEVEN, MAJ9 SHUFFLE, RISING STEPS, LYDIAN
FLOAT, MEDIANT, PEDAL MIX, MINOR DRIVE, THIRD FOURTH, DORIAN WHEEL, LINE DOWN,
PENTA IV, GLASS KEYS, HAND DORIAN. Edits are not saved; a preset change loads the preset (the tempo
stays while MIDI clock runs).

## MIDI out

MIDI OUT (SEL page, K4) sends the notes to the FM-1's MIDI outputs (USB, BLE,
DIN as set in the FM1 MIDI preferences): the riff and TWIN on channel 1, the
bass on channel 2, the chord tones on channel 3, with note-offs when a note's
gate ends and on STOP. A device setting: it stays when a preset is loaded.

## MIDI in

USB, BLE, DIN, all channels: a note-on chooses a chord like the keys (below
F4: own bass, F4 and up: over the pedal). CC 123/120 stops. MIDI clock sets
the tempo; Start restarts from the first chord, Continue runs, Stop stops.

## Sound engine

Fourteen voices: eight for the riff and TWIN (round robin, so tails overlap),
two for the bass and four for the upper chord tones (the old chord rings out under the
new one). Each voice: two oscillators (sine → triangle → saw → square), FM
bell (ARP), sub (HARM), drift, a 12 dB low-pass with its own envelope, ADSR.
The riff runs through GLIDER's tape echo, ladder filter and ping-pong delay;
the harmony stays dry outside the filter and delay and reaches the tape echo
and the shimmer reverb by HARM FX.

The sequencer runs in the audio interrupt at 96 ticks per quarter (timing to
one 2.9 ms block); the control task does the music theory and publishes
complete pitch tables, so the audio code calls no Flash code.

**CPU guard:** when the audio load reaches 90 % or blocks are missed, the riff
uses fewer overlapping voices (down to 2); after 4 s below 70 % they come
back. The load is shown on FX 5/6.

## Building, testing, flashing (macOS)

```
python3 tools/build_web_mac.py      # browser simulator  -> build/preview/index.html
node tests/arp_sim_test.mjs         # simulator: transport, chords, LEAD, HAND, pedal, presets (+ demo WAVs)
tests/run_native.sh                 # effect DSP tests; every riff/chord combination against the references
python3 tools/make_forbidden.py ../examples                 # rebuild the fingerprints from the recordings
node tests/arp_riff_export.mjs && python3 tests/riff_similarity.py ../examples build/arp-test/riffs.json
                                    # presets and 300 RANDOM riffs, as played, against the recordings
python3 tools/build_mac.py          # firmware + update page
python3 tools/flash_fm1.py flash    # only with FM1 Open firmware already on the device
```
