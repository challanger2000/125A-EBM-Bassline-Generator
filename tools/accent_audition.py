#!/usr/bin/env python3
"""Controlled original-vs-reaccented MIDI audition; not an artistic-quality test.

Run after building old and new snapshots and exporting matching 8-bar MIDIs:
    python tools/accent_audition.py MIDI_DIRECTORY OUTPUT_DIRECTORY

The file pair must differ in exactly two last-bar velocity values (one on
quarter-note kick, one offbeat answer). Note times/pitches/durations are locked.
Bass stems are RMS-matched within each comparison pair; drums are unchanged.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import sys

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paired_audition import read_midi, render_bass, render_drums, rms, db, SR, PPQ

STYLES = ("CLASSIC_EBM", "DARK_ELECTRO")
# Mixed assignment, old is not systematically A.
OLD_FIRST = (False, True)


def check_pair(old: tuple, new: tuple) -> tuple:
    if len(old) != len(new):
        raise ValueError("Note count changed")
    changes = []
    for o, n in zip(old, new):
        if o[:3] != n[:3]:
            raise ValueError("Timing, pitch or gate changed")
        if o[3] != n[3]:
            changes.append((o, n))
    if len(changes) != 2:
        raise ValueError("Expected exactly two paired accent edits")
    strong, weak = sorted(changes, key=lambda pair: pair[0][3], reverse=True)
    step_s = strong[0][0] // (PPQ // 4)
    step_w = weak[0][0] // (PPQ // 4)
    if not (112 <= step_s < 128 and 112 <= step_w < 128):
        raise ValueError("Accent edits must occur in last bar")
    if step_s % 4 != 0 or step_w % 4 == 0:
        raise ValueError("The transfer must run from an on-kick accent to offbeat answer")
    if strong[1][3] != weak[0][3] or weak[1][3] != strong[0][3]:
        raise ValueError("Velocity exchange must conserve paired sum")
    return tuple(changes)


def run(midi_dir: Path, output_dir: Path):
    output_dir.mkdir(parents=True, exist_ok=True)
    drums = render_drums()
    report = []
    for index, name in enumerate(STYLES):
        old = read_midi(midi_dir / (name + "_old.mid"))
        new = read_midi(midi_dir / (name + "_new.mid"))
        edits = check_pair(old, new)
        a, b = render_bass(old), render_bass(new)
        a_rms, b_rms = rms(a), rms(b)
        if a_rms <= 0 or b_rms <= 0:
            raise ValueError("Silent stem")
        target = math.sqrt(a_rms * b_rms)
        a *= target / a_rms
        b *= target / b_rms
        if abs(db(rms(a)) - db(rms(b))) >= .001:
            raise ValueError("Bass stems not equally normalized")
        clips = []
        for tag, is_old in (("A", OLD_FIRST[index]), ("B", not OLD_FIRST[index])):
            signal = (a if is_old else b) + drums
            peak = np.max(np.abs(signal))
            if peak > 0.92:
                raise ValueError("Peak too high")
            dest = output_dir / (f"{index+1:02d}_{tag}.wav")
            wavfile.write(dest, SR, np.int16(np.rint(signal * 32767)))
            clips.append({"file": dest.name, "old": is_old,
                          "sha256": hashlib.sha256(dest.read_bytes()).hexdigest(),
                          "peak_dbfs": round(db(float(peak)), 3)})
        report.append({"style": name, "note_count": len(old),
                       "velocity_changes": [{"step": int(o[0]//(PPQ//4)),
                                             "old_velocity": o[3], "new_velocity": n[3]}
                                            for o,n in edits],
                       "bass_stem_rms_target_dbfs": round(db(target),3),
                       "clips": clips})
    (output_dir/"QA.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    (output_dir/"READ_FIRST.txt").write_text(
        "125A EBM: paired accent transfer A/B, 8 bars, 120 BPM, 44.1 kHz mono.\n"
        "Same neutral synth and beat; matched bass stem RMS within each pair.\n"
        "Exactly two velocities change near the final bar, not pitches or timing.\n"
        "Pair 01 CLASSIC: A=NEW, B=OLD. Pair 02 DARK: A=OLD, B=NEW.\n"
        "Synthetic guide only. Neither EBM authenticity nor musical preference verified.\n",
        encoding="utf-8")
    return report


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: accent_audition.py INPUT_MIDI_DIR OUTPUT_WAV_DIR")
    for item in run(Path(sys.argv[1]), Path(sys.argv[2])):
        print(item)
