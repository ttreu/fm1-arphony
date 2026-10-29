#!/usr/bin/env python3
"""Check that no riff ARPHONY plays copies a riff of the reference recordings.

    python3 tests/riff_similarity.py ../examples build/arp-test/riffs.json

The recordings are transcribed roughly (harmonic-sum pitch salience, onsets,
two candidate lines per recording: strongest new note, highest new note).
A few riffs read by hand from the recordings and a well-known opening
arpeggio are added, because automatic transcription
of a full mix misses notes.

For every riff (riffs.json from tests/arp_riff_export.mjs) the longest run of
identical consecutive intervals with any reference line is measured
(transposition-invariant), and the longest run of identical absolute pitch
classes. Runs of repeated notes alone do not count. A run of LIMIT intervals
(LIMIT + 1 notes) or more fails. Needs numpy and scipy; ffmpeg reads m4a.
"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import find_peaks, stft

LIMIT = 6          # identical intervals in a row that count as a copy (7 notes)
WARN = 5
DISTINCT = 3       # a window must hold this many different notes to count (pedal or pulse alone does not)
MANUAL = {
    # Read by hand from the piano rolls (see ANALYSE.md).
    'ex13 ostinato (by hand)': [57, 55, 62, 50, 57, 55, 60, 52, 57, 55, 62, 50, 64, 55, 60, 52],
    'ex13 pings (by hand)': [74, 76, 74, 74, 76, 74],
    'ex5 inner pedal (by hand)': [66, 69, 66, 71, 66, 69, 66, 71],
    'ex7 falling line (by hand)': [76, 74, 72, 67, 64, 62, 60, 64, 74, 72],
    'ex3 figure (by hand)': [59, 57, 64, 66, 61, 59, 57, 64],
    # A well-known opening arpeggio (root twice, then up the chord).
    'opening arpeggio': [62, 62, 69, 74, 78, 81, 86],
}


def load(path):
    with tempfile.TemporaryDirectory() as tmp:
        wav = Path(tmp) / 'x.wav'
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', str(path), '-ac', '1', '-ar', '22050', str(wav)], check=True)
        sr, x = wavfile.read(wav)
    x = x.astype(float)
    return sr, x / (np.abs(x).max() + 1e-9)


def transcribe(path):
    sr, x = load(path)
    n, hop = 8192, 256
    f, t, z = stft(x, sr, nperseg=n, noverlap=n - hop)
    mag = np.abs(z)
    midis = np.arange(36, 97)

    def band(m):
        fc = 440 * 2 ** ((m - 69) / 12)
        idx = (f >= fc * 2 ** (-0.4 / 12)) & (f < fc * 2 ** (0.4 / 12))
        return mag[idx].max(0) if idx.any() else np.zeros(mag.shape[1])

    raw = {}
    sal = np.zeros((len(midis), mag.shape[1]))
    for i, m in enumerate(midis):
        for h, w in ((1, 1.0), (2, 0.6), (3, 0.45), (4, 0.3), (5, 0.2)):
            k = int(round(m + 12 * np.log2(h)))
            if 440 * 2 ** ((k - 69) / 12) < sr / 2:
                raw.setdefault(k, band(k))
                sal[i] += w * raw[k]
    for i in range(len(midis) - 1, -1, -1):          # remove octave/twelfth echoes of lower notes
        for d, w in ((12, 0.5), (19, 0.35), (24, 0.3)):
            if i - d >= 0:
                sal[i] = np.maximum(0, sal[i] - w * sal[i - d])
    flux = np.maximum(0, np.diff(np.log1p(sal), axis=1)).sum(0)
    flux = np.maximum(np.r_[0, flux] - np.convolve(np.r_[0, flux], np.ones(32) / 32, 'same'), 0)
    peaks, _ = find_peaks(flux, height=np.percentile(flux, 75), distance=int(0.08 * sr / hop))
    strongest, highest = [], []
    reg = (midis >= 48) & (midis <= 92)
    for p in peaks:
        a, b = min(p + 6, sal.shape[1] - 1), max(p - 3, 0)
        new = np.maximum(0, sal[:, a] - sal[:, b]) * reg
        if new.max() <= 0:
            continue
        strongest.append(int(midis[np.argmax(new)]))
        loud = np.where(new >= 0.4 * new.max())[0]
        highest.append(int(midis[loud[-1]]))
    return {'strongest': strongest, 'highest': highest}


def longest_run(a, b, trivial):
    """Longest common contiguous run of a and b that is not only repeated
    notes (`trivial[i]`: a[i] just repeats the note before)."""
    best = 0
    prev = [0] * (len(b) + 1)
    rep_prev = [0] * (len(b) + 1)
    for i in range(1, len(a) + 1):
        cur = [0] * (len(b) + 1)
        rep = [0] * (len(b) + 1)
        for j in range(1, len(b) + 1):
            if a[i - 1] == b[j - 1]:
                cur[j] = prev[j - 1] + 1
                rep[j] = rep_prev[j - 1] + 1 if trivial[i - 1] else 0
                if cur[j] > best and rep[j] < cur[j]:
                    best = cur[j]
        prev, rep_prev = cur, rep
    return best


def windows(notes):
    """Interval tuples of every 7-note window with at least DISTINCT notes."""
    out = set()
    for i in range(len(notes) - LIMIT):
        w = notes[i:i + LIMIT + 1]
        if len(set(w)) >= DISTINCT:
            out.add(tuple(int(b - a) for a, b in zip(w, w[1:])))
    return out


def fingerprint(iv):
    """32-bit FNV-1a of an interval tuple, as arp.c computes it."""
    h = 0x811c9dc5
    for d in iv:
        h ^= (max(-48, min(48, d)) + 64) & 0xff
        h = (h * 0x01000193) & 0xffffffff
    return h


def references(examples):
    refs = dict(MANUAL)
    for path in sorted(Path(examples).iterdir()):
        if path.suffix.lower() in ('.m4a', '.wav', '.mp3', '.aif', '.aiff'):
            for kind, notes in transcribe(path).items():
                refs[f'{path.stem} {kind}'] = notes
    return refs


def main():
    examples, riffs_file = Path(sys.argv[1]), Path(sys.argv[2])
    refs = references(examples)
    ref_windows = {name: windows(notes) for name, notes in refs.items()}
    riffs = json.loads(riffs_file.read_text())
    worst_iv = (0, '', '')
    worst_pc = (0, '', '')
    fails = 0
    for r in riffs:
        notes = [n for n in r['notes'] if n >= 0]
        iv = list(np.diff(notes))
        pcs = [n % 12 for n in notes]
        for name, ref in refs.items():
            k = longest_run(iv, list(np.diff(ref)), [d == 0 for d in iv])
            c = longest_run(pcs, [n % 12 for n in ref], [i > 0 and pcs[i] == pcs[i - 1] for i in range(len(pcs))])
            if k > worst_iv[0]:
                worst_iv = (k, r['name'], name)
            if c - 1 > worst_pc[0]:
                worst_pc = (c - 1, r['name'], name)
            shared = windows(notes) & ref_windows[name]
            if shared:
                fails += 1
                w = sorted(shared)[0]
                print(f'FAIL {r["name"]} vs {name}: {LIMIT + 1} notes with the same intervals {list(w)}')
    print(f'{len(riffs)} riffs against {len(refs)} reference lines')
    print(f'longest shared interval run: {worst_iv[0]} ({worst_iv[1]} vs {worst_iv[2]})')
    print(f'longest shared pitch-class run: {worst_pc[0] + 1} notes ({worst_pc[1]} vs {worst_pc[2]})')
    if not fails:
        print(f'PASS: no riff shares {LIMIT + 1} notes ({DISTINCT}+ different) with the same intervals with a recording')
        print('(the longest runs above may be pedal or repeated-note figures, which do not count)')
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
