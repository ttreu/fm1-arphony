# Analyse: Riffs und Harmonik der Referenzaufnahmen

Grundlage für ARPHONY. Ausgewertet wurden die Referenzaufnahmen in
`../examples` (Tonhöhen-Salienz als Piano-Roll, Onset- und Tempoerkennung,
Bass- und Akkordschätzung) und allgemeine Harmonielehre modaler Popmusik.

## Was die Beispiele zeigen

| Beispiel | Bass / Harmonie | Riff oben | Tempo |
|---|---|---|---|
| 2 | G → C → D → A (wechselt alle 1–2 Takte) | E-Moll-pentatonische Figur, Töne E, D, C, G, F# bleiben über allen Bässen | ~94–96 BPM, 8tel |
| 3 | E-Orgelpunkt durchgehend, darüber A (A/E) ↔ G (Em) | B, A, E, F#, G um E herum | ~96 BPM |
| 5 | D-Bass, F#3 als gehaltener Innen-Orgelpunkt, A/B wechseln | A4 ↔ B4 über F#4 (D6-Farbe) | ~85 BPM |
| 6 | D – F# – A – B geschichtet (D6 / Bm7) | B4-Pings über A/F# | ~89 BPM, 8tel |
| 7 | C (Orgelpunkt) → D → C → A | absteigende Linie E5–D5–C5, darunter C/D/E/G (Cadd9) | ~96 BPM |
| 12 | Bb ↔ C im Wechsel | D, G, F, D# – G-dorisch/-moll | ~92 BPM |
| 13 | Intro: A-Dur-Klang (C#, E) bleibt liegen, der Bass fällt A → G → F → E. Danach F-Bass, lange gehalten | Intro: pulsierendes C#4. Hauptteil: dichtes 16tel-Ostinato nur aus C-D-E-G-A (C3–E4), darüber eine zweite Schicht mit D5/E5-Pings im punktierten Viertelraster | ~83 BPM, 16tel |

Wiederkehrende Muster:

1. **Ostinato über wanderndem Bass.** Die Riff-Töne bleiben weitgehend gleich,
   der Bass wechselt darunter. Weil die Akkorde viele Töne teilen, bekommt
   dieselbe Figur über jedem Bass eine neue Bedeutung (z. B. E–D–C–G über C
   = Cadd9, über D = D7sus/D9). → in ARPHONY: Modus **LEAD**.
2. **Orgelpunkte.** Ein Bass bleibt liegen, während die Akkorde darüber
   wechseln (E-Pedal in Bsp. 3, C in Bsp. 7), oder eine Mittelstimme bleibt
   liegen (F# in Bsp. 5). → Slash-Akkorde in den Progressionen, obere
   Tastenhälfte = Akkord über dem Grundton, Riff **PEDAL**.
3. **Farbige Akkorde statt Dreiklänge:** add9, 6, maj7, m7/m9, sus2 –
   D-F#-A-B, C-D-E-G, F#-G#-A-C#-E. Terzen fehlen oft oder liegen oben.
   → Akkordqualitäten und COLOR.
4. **Zwei-Akkord-Wechsel, die nicht auflösen:** C↔D, Bb↔C, i↔IV.
5. **Ruhiges Tempo (≈ 85–120 BPM) mit 8tel- oder 16tel-Bewegung**, teils
   triolisch (12/8-Shuffle).
6. **Pentatonik über der IV (Bsp. 13).** Das Riff benutzt nur die
   Pentatonik der Tonart und lässt den Basston (F) aus; über F klingt das als
   Fmaj9/6/9. Dazu kommt eine zweite, sparsame Riff-Schicht hoch darüber.
   → HARMONY **PENTA**, Riff **INTERLOCK**, **TWIN PINGS**.
7. **Fallender Bass unter liegendem Akkord (Bsp. 13).** Der Klang bleibt, der
   Bass steigt diatonisch ab (A–G–F–E) und färbt ihn um (A, A/G, Fmaj7#5,
   A/E). → Progression **LINE DOWN** mit HARMONY PEDAL.
8. **Offene Voicings:** Bass tief (G1–E2), gehaltene Töne eine Oktave und
   mehr darüber (z. B. D2 – F#3 – A3).

## Allgemeine Harmonielehre dazu

- **Dorisch** (Moll mit großer Sexte) lebt von der Dur-IV: i – IV, i9 – IV.
- **Äolisch:** bVI – bVII, oft ohne Auflösung zur i.
- **Lydisch / Mixolydisch:** IV – V, die nicht auf der I landet; die #4 bzw.
  b7 als Farbe.
- **Chromatische Mediante:** I – bIII (z. B. Aadd9 – Cmaj7).
- **Orgelpunkt:** ein liegender Bass unter wechselnden Akkorden (IV/I, bVII/I).
- **Steigender harmonischer Rhythmus:** gegen Ende eines Abschnitts zwei
  Akkorde pro Takt statt einem.

## Umsetzung in der Firmware

| Befund | ARPHONY |
|---|---|
| Ostinato bleibt, Bass wandert | HARMONY **LEAD**: Riff auf dem Tonika-Akkord, jeder Ton rückt zum nächsten Akkordton |
| Riff bleibt komplett stehen | HARMONY **PEDAL** |
| Orgelpunkt-Akkorde (G/D, IV/I, bVII/I) | Slash-Akkorde in MAJ9 PEDAL und MIX PEDAL; obere Tastenhälfte |
| add9 / maj7 / m9 / 6 | Akkordqualitäten, COLOR für gespielte Akkorde |
| Dorisch, Äolisch, Lydisch, Mixolydisch | MODE; Progressionen i9–IV, i–III–VII–IV, bVI–bVII, IV–V, I–bIII, I–IV/I–i–bVII/I |
| bis zu 3 Töne unten, offen | Bass + Quinte/Oktave + Farbton (9, 7, 6 oder Dezime) |
| schrittweiser Bass | Bassoktave wird jeweils zum vorherigen Basston gewählt |
| 8tel/16tel, Triolen | RATE 1/8, 1/8T, 1/16, 1/16T; Riff TRIPLET; SWING |
| Delay-lastige Synth-Pings | Riff PING, synchrones Ping-Pong-Delay (1/8 punktiert), Tape-Echo |
| Pentatonik über der IV, zweite Schicht oben (Bsp. 13) | HARMONY PENTA, Riff INTERLOCK, TWIN PINGS / OCTAVE, Preset PENTA IV |
| fallender Bass unter liegendem Akkord (Bsp. 13) | Progression LINE DOWN, Preset LINE DOWN |

Die Progressionen kopieren keine Aufnahme (weder Reihenfolge noch Rhythmus);
sie verwenden allgemeine modale Bewegungen.

## Abstand zu den Beispielen

Kein Riff der Firmware darf eine Tonfolge der Beispiele 1:1 wiederholen.

- Überarbeitet nach einer Prüfung: Riff RISE (hatte die Kontur eines
  bekannten „Grundton zweimal, dann aufwärts“-Intros), INTERLOCK und die TWIN-Pings
  (nach Beispiel 13 gebaut), Progression LINE DOWN (jetzt chromatisch
  1–7–b7–6 statt A–G–F–E). Presets mit Tonart und Tempo eines Vorbilds liegen
  jetzt in anderen Tonarten und Tempi.
- Sperrliste in der Firmware: alle 7-Ton-Figuren der Beispiele (automatisch
  transkribiert, zwei Linien pro Beispiel, plus von Hand gelesene Figuren und
  ein bekanntes Eröffnungs-Arpeggio), als Intervalle,
  transpositionsunabhängig und gespiegelt: 2740 Fingerabdrücke.
- Jeder Riff wird so geprüft, wie er klingt (über alle Akkorde, über die
  Akkordwechsel hinweg). RANDOM-Riffs mit Treffer werden verworfen, feste
  Riffs um eine Leiterstufe variiert.
- Tests: alle 652 800 Kombinationen aus festem Riff, Progression, Harmonie-
  Modus, Modus der Akkorde, Rate, Länge und Oktave – 0 Treffer; dazu Presets und 300 Zufalls-
  Riffs im Simulator gegen die Beispiele – 0 Treffer.
- Grenzen: Die automatische Transkription eines ganzen Mixes verpasst Töne;
  Figuren aus nur einem oder zwei Tönen (Puls, Orgelpunkt) zählen nicht;
  MUTATE verändert Töne live und wird nicht geprüft.
