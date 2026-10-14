#!/usr/bin/env python3
"""Reproducible patch-release sensitivity audition for three EXISTING source MIDI riffs.

This is a controlled comparison of TWO instrument envelopes, not a musical
quality scorer, EBM emulation, or a proposed change to the MIDI composer.
Only the release time changes within each pair. Requires numpy, scipy, mido
(the existing 125A research-audition Python dependencies).

Usage:
    python tools/instrument_context_audition.py MIDI_FOLDER WAV_FOLDER
    python tools/instrument_context_audition.py --self-test
"""
from __future__ import annotations
import hashlib
import json
import math
import sys
import tempfile
from pathlib import Path

import numpy as np
from scipy.io import wavfile
from scipy.signal import lfilter

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paired_audition import read_midi, render_drums, SR, FRAMES, PPQ, BEAT_SEC, BEATS

STYLES = ('MINIMAL_BODY', 'CLASSIC_EBM', 'DARK_ELECTRO')
# Engineer-chosen *test contrast*, not experimentally derived genre settings.
RELEASE_S = {'TIGHT': 0.018, 'LONG': 0.220}
# Use exactly one extra second of zero-padding for release tails; same length in both variants.
AUDIO_FRAMES = round((BEATS * BEAT_SEC + 1.0) * SR)
ACTIVE_FRAMES = round(BEATS * BEAT_SEC * SR)


def _db(x: float) -> float:
    return 20.0 * math.log10(max(float(x), 1e-20))


def _rms(x: np.ndarray) -> float:
    return float(np.sqrt(np.mean(np.square(x))))


def render_bass(notes: tuple, release_s: float) -> tuple[np.ndarray, np.ndarray]:
    """Same oscillator/filter/attack/decay/sustain; release is ONLY treatment."""
    if not (0 < release_s <= 1.0):
        raise ValueError('Invalid release')
    output = np.zeros(AUDIO_FRAMES, dtype=np.float64)
    gate = np.zeros(ACTIVE_FRAMES, dtype=bool)
    for tick_start, tick_end, pitch, velocity in notes:
        onset = round(tick_start / PPQ * BEAT_SEC * SR)
        length = (tick_end - tick_start) / PPQ * BEAT_SEC
        release_length = 5.0 * release_s
        count = max(1, round((length + release_length) * SR))
        t = np.arange(count, dtype=np.float64) / SR
        freq = 440.0 * (2.0 ** ((pitch - 69) / 12.0))
        phase = 2.0 * math.pi * freq * t
        oscillator = np.sin(phase) + 0.27 * np.sin(2 * phase) + 0.10 * np.sin(3 * phase)
        pole = math.exp(-2.0 * math.pi * 820.0 / SR)
        filtered = lfilter([1 - pole], [1, -pole], oscillator)
        # Analytic, continuous gate envelope at t=length avoids a fake click.
        attack = np.minimum(1.0, t / 0.004)
        held = (0.67 + 0.33 * np.exp(-t / 0.060)) * attack
        held_at_off = (0.67 + 0.33 * math.exp(-length / 0.060)) * min(1.0, length / 0.004)
        release = held_at_off * np.exp(-np.maximum(0.0, t-length) / release_s)
        envelope = np.where(t < length, held, release)
        chunk = 0.24 * (velocity / 127.0) * np.tanh(1.7 * filtered) * envelope
        end = min(AUDIO_FRAMES, onset+count)
        output[onset:end] += chunk[:end-onset]
        if tick_start >= 0 and tick_end <= BEATS * PPQ:
            off = round(tick_end / PPQ * BEAT_SEC * SR)
            gate[onset:off] = True
        else:
            raise ValueError('Note exceeds phrase bounds')
    return output, gate


def render_drums_extended() -> np.ndarray:
    # Reuse EXISTING neutral kick+hat design; zero-pad the larger tail equally.
    src = render_drums()
    out = np.zeros(AUDIO_FRAMES, dtype=np.float64)
    out[:len(src)] = src
    return out


def run(midi_dir: Path, wav_dir: Path) -> list:
    wav_dir.mkdir(parents=True, exist_ok=True)
    drums = render_drums_extended()
    report = []
    for idx, style in enumerate(STYLES, start=1):
        midi_path = midi_dir / (style + '_SESSION.mid')
        notes = read_midi(midi_path)  # independent MIDI parser checks BPM, 8 bars, note-offs
        if not notes:
            raise ValueError('Empty MIDI: '+str(midi_path))
        stems = {name: render_bass(notes, rel) for name,rel in RELEASE_S.items()}
        assert np.array_equal(stems['TIGHT'][1],stems['LONG'][1])
        gate = stems['TIGHT'][1]
        if not np.any(gate) or not np.any(~gate):
            raise ValueError('No usable MIDI note/rest contrast')
        rms_values = {name:_rms(stem[:ACTIVE_FRAMES]) for name,(stem,_) in stems.items()}
        if min(rms_values.values())<=1e-12:
            raise ValueError('Silent bass')
        matched_rms = math.sqrt(rms_values['TIGHT']*rms_values['LONG'])
        files=[]
        rest_ratios={}
        for name in RELEASE_S:
            bass = stems[name][0] * (matched_rms/rms_values[name])
            playing = bass[:ACTIVE_FRAMES]
            active_rms = _rms(playing[gate]);rest_rms = _rms(playing[~gate])
            rest_ratios[name] = _db(rest_rms / active_rms)
            if abs(_db(_rms(playing))-_db(matched_rms)) > .001:
                raise AssertionError('Bass RMS mismatch')
            summed = bass + drums
            peak = float(np.max(np.abs(summed)))
            if not np.isfinite(peak) or peak>=0.94:
                raise ValueError('Peak invalid or potential clipping')
            out = wav_dir/f'{idx:02d}_{style}_{name}.wav'
            wavfile.write(out,SR,np.int16(np.rint(summed * 32767.0)))
            files.append({'variant':name,'file':out.name,
                          'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),
                          'peak_dbfs':round(_db(peak),3),
                          'bass_rms_dbfs':round(_db(_rms(playing)),4),
                          'rest_to_active_rms_db':round(rest_ratios[name],2)})
        report.append({'style':style,'midi':midi_path.name,
                       'midi_sha256':hashlib.sha256(midi_path.read_bytes()).hexdigest(),
                       'notes':len(notes),'midi_rest_fraction':round(float(np.mean(~gate)),4),
                       'treatments':files,
                       'extra_rest_bleed_db':round(rest_ratios['LONG']-rest_ratios['TIGHT'],2)})
    (wav_dir/'QA.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    (wav_dir/'READ_FIRST.txt').write_text(
        '125A EBM – instrument-envelope sensitivity AUDITION, not a genre quality test.\n'
        'Three source MIDI riffs chosen before listening. Each pair changes ONLY the synth release:\n'
        'TIGHT 18 ms vs LONG 220 ms. Oscillator/filter/attack/decay/sustain are identical.\n'
        'Each bass stem pair is RMS matched over the 16-second phrase. Drums are identical.\n'
        '120 BPM, 4/4, 8 bars, 44.1 kHz mono PCM WAV; one second common tail.\n'
        'The analysis measures how much sound spills into MIDI-defined rests.\n'
        'A large bleed difference does not show which patch is better; it shows\n'
        'instrument-dependence of audible negative space. No artist samples or AI sounds.\n'
        'Musical usability, preference and authenticity: NOT VERIFIED.\n',encoding='utf-8')
    return report


def self_test() -> None:
    # No external input: deliberately hostile parser, silent source and envelope bounds.
    try:
        render_bass(((0,120,36,100),), -0.01)
    except ValueError:
        pass
    else:
        raise AssertionError('Invalid release accepted')
    try:
        render_bass(((0, BEATS*PPQ+1,36,100),), 0.2)
    except ValueError:
        pass
    else:
        raise AssertionError('Out-of-range note accepted')
    notes=((0,120,36,112),(360,480,39,92))
    tight,mask1=render_bass(notes,RELEASE_S['TIGHT'])
    long,mask2=render_bass(notes,RELEASE_S['LONG'])
    assert np.array_equal(mask1,mask2)
    assert not np.array_equal(tight,long)
    assert np.array_equal(tight[:round(.125*SR)],long[:round(.125*SR)]), 'Release affected held note'
    assert _rms(long[round(.2*SR):round(.3*SR)])>_rms(tight[round(.2*SR):round(.3*SR)])*10
    print('PASS: envelope-only isolation; invalid release, invalid note, silence response')


if __name__=='__main__':
    if len(sys.argv)==2 and sys.argv[1]=='--self-test':
        self_test()
    elif len(sys.argv)==3:
        print(json.dumps(run(Path(sys.argv[1]),Path(sys.argv[2])),indent=2))
    else:
        raise SystemExit('Usage: instrument_context_audition.py MIDI_FOLDER WAV_FOLDER | --self-test')
