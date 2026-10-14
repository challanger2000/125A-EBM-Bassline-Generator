# Instrument-envelope sensitivity audition — 2026-10-14

**Gate:** Controlled listening stimuli and signal-level measurements VERIFIED locally; artistic suitability and EBM authenticity NOT VERIFIED. This changes **no C++ composition algorithm, no MIDI, no seed selector and no VST3**.

## Question / controlled contrast

WENN a composer intentionally leaves MIDI pauses, DANN an external synthesizer may fill those rests with a long release. A MIDI-only generator has no authority over that patch release setting. Before ranking bass patterns by sparse-note metrics or introducing a misguided musical rejection rule, measure the audible-context dependence.

- Source: exact development-branch `50c25b2bd5e06d52977abf3d49912307ebe511bf`, `ebm_session_demo`, three fixed previously specified seeds **20261014 / 20261033 / 20261052**, 8 bars, variation level **85**, style-dependent roots 36 / 36 / 33.
- Three original generated MIDI patterns, **not artist transcriptions**; note counts **46 / 67 / 47**. No pattern or velocity modified between treatments.
- Independent standard-library MIDI checker: **3/3 PASS**, format 0, 480 PPQ, 120 BPM, 4/4, all note-offs paired, correct eight-bar endpoint.
- Controlled one-factor bass-rendering A/B: release **18 ms (TIGHT)** vs **220 ms (LONG)**. Oscillator, harmonic content, filter, attack 4 ms, short decay, sustain, velocity response, note articulation, pitch and deterministic four-on-floor kick/hats remain unchanged. Release constants are **empirically chosen test contrasts**, not hardware-derived or preferred EBM settings.
- Bass stem levels matched within each pair over the exact 16-second phrase to better than 0.001 dB signal RMS; drum stem is identical. Sample alignment and waveform frame count are equal: **44.1 kHz, mono, PCM16, 17.0 s**, including one second tail. No peak normalization or limiter.
- Metric: RMS of bass during **MIDI-defined rest frames**, divided by RMS during MIDI-defined held-note frames; compare this ratio in dB. This is a relative spill metric, **not** perceived preference, beat masking, nor a musical quality score.

## Fixed measurements

| Source profile | Notes | MIDI-defined rest fraction | Rest / active, TIGHT (dB) | Rest / active, LONG (dB) | Increase of relative rest energy |
| --- | ---: | ---: | ---: | ---: | ---: |
| MINIMAL_BODY | 46 | 64.06% | -15.88 | -6.07 | **+9.81 dB** |
| CLASSIC_EBM | 67 | 38.28% | -13.82 | -3.95 | **+9.87 dB** |
| DARK_ELECTRO | 47 | 39.06% | -13.68 | -4.24 | **+9.44 dB** |

The longer release makes the MIDI-defined silences less silent with this particular synthesized sound. This is a useful observed **patch-dependence**, not evidence that either TIGHT or LONG is an intrinsically better bass sound. Audibility under kick/hats and the musical effect need listening.

## Independent and adversarial technical checks

1. Linux C++17 Release clean rebuild from the verified source snapshot, original CTest **11/11 PASS**. Existing C++ executable generated the MIDI, not a new Python motif generator.
2. Independent Python standard-library MIDI validator **3/3 PASS**.
3. `python tools/instrument_context_audition.py --self-test`: attack/on-time waveform is byte-identical prior to the note-off for both release settings; post-note-off envelope differs; invalid release and out-of-phrase MIDI events rejected.
4. A separate corrupted copy of the first MIDI file with **120 BPM changed to 100 BPM** is rejected by the strict MIDI audition reader, not silently rendered.
5. Six WAVs rendered twice with independent output directories: **6/6 byte-identical**. Output RMS match within pair (<0.001 dB), no peak >= -0.54 dBFS / 0.94 linear; observed peaks between -4.43 and -6.61 dBFS.
6. No third-party copyrighted audio, proprietary synthesis assets or sample recording involved. No GitHub Actions used. Audible preferences, DAW playback, Windows/native VST3, consumer synth patches and different tempos are NOT VERIFIED.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/ebm_session_demo previews/session
python tests/verify_midi.py previews/session/*.mid
python tools/instrument_context_audition.py --self-test
python tools/instrument_context_audition.py previews/session previews/patch_comparison
```

Requirements for optional research-renderer (already used by older audition tools): Python 3, numpy, scipy and mido. This renderer is **offline only**; it is not part of the VST3 core or a realtime callback.

## WENN -> DANN for the eventual product

- WENN ein Bassriff bewusst Pausen einsetzt, DANN muss deren tatsächliche Hörbarkeit mit mehr als einer Synth-Hüllkurve geprüft werden. Do not rank the same MIDI pattern as universally groovy solely from zero MIDI events in a rest.
- WENN a long synth release smears those rests, DANN the user should adjust the instrument envelope/patch first. The MIDI composer must not secretly alter the rhythm or add an unproven global gate correction.
- WENN pitch, gate, key, articulation or tempo later becomes user-facing VST3 functionality, DANN real host/synth tests are required. Current research only separates two deliberately synthetic release contexts, at 120 BPM.
- Small listening acceptance: just **three** style-matched pairs, not dozens of arbitrary outputs. The producer can assess which phrases remain useful at realistic patch settings. Musical preference is still OPEN.
