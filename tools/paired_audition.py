#!/usr/bin/env python3
"""Create bounded, level-matched, blind A/B of a deliberately selected MIDI pair.

Research QA only; WAVs are neutral audition guides, NOT finished EBM productions.
Input directory expects STYLE_old.mid and STYLE_new.mid, from the same seed.
No external sample or copyrighted recording is used.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import numpy as np
import mido
from scipy.io import wavfile
from scipy.signal import lfilter

SR = 44100
BPM = 120
PPQ = 480
BARS = 8
BEATS = BARS * 4
BEAT_SEC = 60 / BPM
FRAMES = round((BEATS * BEAT_SEC + 0.4) * SR)
STYLES = ("MINIMAL_BODY", "CLASSIC_EBM", "DARK_ELECTRO")
# Fixed, documented blind presentation; labels are intentionally not generation order.
AB_IS_OLD = ((True, False), (False, True), (True, False))


def read_midi(path: Path):
    mf = mido.MidiFile(str(path))
    assert mf.type == 0 and mf.ticks_per_beat == PPQ and len(mf.tracks) == 1
    tick, current, notes = 0, None, []
    meter, tempo = None, None
    for msg in mf.tracks[0]:
        tick += msg.time
        if msg.type == "set_tempo":
            tempo = msg.tempo
        elif msg.type == "time_signature":
            meter = (msg.numerator, msg.denominator)
        elif msg.type == "note_on" and msg.velocity > 0:
            assert current is None, "Polyphony or missing NoteOff"
            current = (tick, msg.note, msg.velocity)
        elif msg.type == "note_off" or (msg.type == "note_on" and msg.velocity == 0):
            assert current is not None and msg.note == current[1] and tick > current[0]
            notes.append((current[0], tick, current[1], current[2]))
            current = None
    assert tick == BEATS * PPQ and current is None
    assert meter == (4, 4) and tempo == 500_000
    return tuple(notes)


def render_bass(notes):
    output = np.zeros(FRAMES, dtype=np.float64)
    for start, end, pitch, velocity in notes:
        onset = round(start / PPQ * BEAT_SEC * SR)
        duration = (end - start) / PPQ * BEAT_SEC
        count = max(2, round((duration + 0.05) * SR))
        t = np.arange(count, dtype=np.float64) / SR
        frequency = 440.0 * 2 ** ((pitch - 69) / 12.0)
        phase = frequency * t
        oscillator = (0.56 * (2.0 * np.mod(phase, 1.0) - 1.0)
                      + 0.18 * (2.0 * np.mod(phase * 1.006, 1.0) - 1.0)
                      + 0.26 * np.sin(2.0 * math.pi * phase))
        pole = np.exp(-2.0 * np.pi * min(1150.0, 10 * frequency + 240) / SR)
        filtered = lfilter([1 - pole], [1, -pole], oscillator)
        envelope = np.minimum(1, t / 0.007) * np.minimum(1, np.maximum(0, duration + 0.025 - t) / 0.035)
        envelope *= 0.63 + 0.37 * np.exp(-t / 0.22)
        chunk = 0.27 * (velocity / 127) * np.tanh(filtered * 2.0) * envelope
        stop = min(FRAMES, onset + count)
        output[onset:stop] += chunk[:stop-onset]
    return output


def render_drums():
    out = np.zeros(FRAMES, dtype=np.float64)
    for beat in range(BEATS):
        onset = round(beat * BEAT_SEC * SR)
        t = np.arange(round(0.23 * SR)) / SR
        freq = 62 * np.exp(-t / 0.04) + 45
        kick = 0.36 * np.sin(2 * np.pi * np.cumsum(freq) / SR) * np.exp(-t / 0.08)
        out[onset:onset+len(t)] += kick
    rng = np.random.default_rng(1125)
    for hit in range(BEATS * 2):
        onset = round((hit + 0.5) * BEAT_SEC / 2 * SR)
        t = np.arange(round(0.046 * SR)) / SR
        noise = rng.standard_normal(len(t))
        hp = np.concatenate(([0], np.diff(noise)))
        out[onset:onset+len(t)] += 0.014 * hp * np.exp(-t / 0.012)
    return out


def rms(x):
    return float(np.sqrt(np.mean(np.square(x[:round(BEATS * BEAT_SEC * SR)]))))


def db(x):
    return 20 * math.log10(max(x, 1e-20))


def run(input_dir: Path, output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    drum = render_drums()
    report = []
    for i, style in enumerate(STYLES):
        old_notes = read_midi(input_dir / f"{style}_old.mid")
        new_notes = read_midi(input_dir / f"{style}_new.mid")
        # This comparison is intentionally narrowly scoped to the optional end note:
        # every other note and its timing, gate, pitch, velocity must remain unchanged.
        assert set(new_notes).issubset(set(old_notes)), f"{style}: unrelated note change"
        deleted = tuple(x for x in old_notes if x not in set(new_notes))
        assert deleted, f"{style}: no actual revision"
        assert all(((start // 120) % 16) >= 14 for start, end, pitch, velocity in deleted), f"{style}: non-ending note deleted"
        old = render_bass(old_notes)
        new = render_bass(new_notes)
        r_old, r_new = rms(old), rms(new)
        assert r_old > 0 and r_new > 0
        target = math.sqrt(r_old * r_new)
        bass_old, bass_new = old * (target / r_old), new * (target / r_new)
        assert abs(db(rms(bass_old)) - db(rms(bass_new))) < 0.001
        blind = []
        for j, is_old in enumerate(AB_IS_OLD[i]):
            label = "AB"[j]
            audio = (bass_old if is_old else bass_new) + drum
            peak = float(np.max(np.abs(audio)))
            assert peak < 0.92, f"{style}: potential clipping {peak}"
            pcm = np.int16(np.rint(audio * 32767))
            dest = output_dir / f"{i+1:02d}_{label}.wav"
            wavfile.write(dest, SR, pcm)
            # This is fair *signal-level* matching, not psychoacoustic or perceived-loudness matching.
            blind.append({"label":label, "is_old":is_old,"wav":dest.name,
                          "sha256":hashlib.sha256(dest.read_bytes()).hexdigest(),
                          "peak_dbfs":round(db(peak),3)})
        report.append({"style":style,"old_notes":len(old_notes),"new_notes":len(new_notes),
            "removed_count":len(deleted),"removed_step_in_bar":[int((n[0]//120)%16) for n in deleted],
            "bass_rms_old_before_dbfs":round(db(r_old),3),"bass_rms_new_before_dbfs":round(db(r_new),3),
            "bass_rms_target_dbfs":round(db(target),3),"blinded":blind})
    (output_dir / "QA-data.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (output_dir / "READ_FIRST.txt").write_text(
      "125A EBM – controlled phrase-endings A/B (2026-10-13)\n"
      "Three pairs, 120 BPM, 4/4, eight bars; identical synthetic bass patch and kick/hat context.\n"
      "Bass RMS is equalized WITHIN each pair; drums are rendered identically. No master limiter.\n"
      "A/B order is deliberately mixed. Do not mistake lower note count for higher musical quality.\n"
      "SYNTHETIC GUIDE ONLY – not a VST3, finished bass sound, artist transcription, or listening PASS.\n"
      "Open ANSWER-KEY.txt after listening if desired. The generated QA-data.json has technical stats.\n",
      encoding="utf-8")
    (output_dir / "ANSWER-KEY.txt").write_text(
      "Pair 01 Minimal: A=OLD / B=NEW\nPair 02 Classic: A=NEW / B=OLD\nPair 03 Dark: A=OLD / B=NEW\n",
      encoding="utf-8")
    print(json.dumps(report, indent=2))
    return report

if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input_dir", type=Path)
    ap.add_argument("output_dir", type=Path)
    args = ap.parse_args()
    run(args.input_dir, args.output_dir)
