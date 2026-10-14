# Repeated VARIATE, fixed musical identity — 2026-10-14

**Status: local technical checks passed; musical quality not independently heard or approved. Experimental C++17 standalone engine, not VST3.**

## User need and independent acceptance condition

The former `variate(level)` only chose among five discrete **intensity bands**. Repeated presses within the same band reproduced identical MIDI. A user-visible VARIATE button needs multiple different ideas without generating a *new* initial riff.

WENN the user presses VARIATE again, DANN preserve the original source bars, seed, style, root and length, but return a structurally distinct, audited phrase development. When the control is at 0%, return *no change* without silently increasing it. All event mutations must be explainable by the source motif plus an exact operation log.

## Implementation

The musical grammar exposes `composeAlternative(Config, alternative)` with indices 0..4. Zero is the already established arrangement. The other four alternatives add one typed edit: changed final answer, changed middle answer, deliberate named drive omission/alternative answer, or rhythmic anticipation/targeted answer if that note has already been moved. The alternative set is finite and deterministic, and every resulting composition runs the existing strict independent `validate()`/lineage verifier.

`Session::nextVariant()` selects the next distinct member of this bounded set, retaining the selected original GENERATE seed. Explicit `variate(intensity)` keeps its previous meaning: setting the intensity band and returning the canonical arrangement. Changing key regenerates the same alternative in the new register; it does not reset to a different melody. New in-memory SessionState `version=2` saves the alternative index; valid v1 (index=0) migrates without losing the chosen riff. Unsupported versions, out-of-range alternative index, or a nonzero alternative at neutral intensity are rejected atomically. Neither generation nor this state engine is allowed in a real-time audio callback.

## Discovered and fixed counterexamples

1. Four-bar CLASSIC EBM at intensity >=50 has already anticipated anchor ID 6 in the canonical phrase. A second anticipation is invalid. Alternative four therefore changes another named melodic answer, rather than moving the same note twice.
2. Four-bar DARK ELECTRO at intensity >=50 initially gave the same pitch change for alternative one and three. The alternative now edits the *other* answering event; duplicate MIDI outputs are specifically rejected by a new test.
3. Both failure classes were caught by new tests before the final build; tests were **not** relaxed to mask them.

## Exact reproducible tests

- Release build (GCC C++17 with warnings as errors): CTest **9/9 PASS**.
- Debug AddressSanitizer+UndefinedBehaviorSanitizer with `detect_leaks=1`: CTest **9/9 PASS**.
- 6,600 generated alternative compositions (3 profiles × 2 phrase lengths × 55 seeds × 4 non-neutral levels × 5 alternatives): all five MIDI outputs distinct per configuration, source-bar event identity preserved, no non-logged changes; full compositor validation executed.
- 5,280 `Session::nextVariant()` transitions, 6,600 save/restore/invalid-state checks, deterministic MIDI roundtrip, transposition by seven semitones and back, reset by explicit `variate(level)` and v1→v2 state compatibility tests.
- Baseline composition preserved on 3,000 configurations before vs after: independent native MIDI encoder FNV64 digest **2516969645277329636** on both binaries. Original `compose(config)` behavior is unchanged.
- Three original/varied pairs exported through the **actual Session** and parsed independently by `tests/verify_midi.py`: 6/6 MIDI PASS (8 bars, 120 BPM, 4/4, monophonic matched NoteOn/NoteOff).

## Still open

- Five distinct files are **not necessarily five musically good variants**. No subjective preference score, listener blind test, EBM-artist pattern corpus, DAW check or calibrated bass-synth audition has been performed for this new feature.
- No Windows/VST3, sample-block MIDI scheduler, GUI, bypass, automation, VST3 state bytes or Studio One test; `Session` uses allocations and validation exceptions by design and must be invoked off the audio thread.
- No automatic semantic guarantee that GENERATE produces a commercially usable groove. Novelty and role consistency are structural diagnostics, not musical-quality ratings.
