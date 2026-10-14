# 125A EBM: MIDI loop vs one-shot ending — 2026-10-14

**Gate:** original C++17 musical engine; local Linux QA completed. No VST3, Windows, Studio One, host transport, or subjective musical approval.

## WENN -> DANN: exact user-facing distinction

Some existing EBM grammar devices place a Tension note at step 14 or 15 of the last bar, deliberately resolved by the Anchor at step 0 of the *following repeated loop*. This is valid for a repeated riff but produces an unresolved final note in a finite MIDI export.

- WENN the user loops the riff, DANN retain that final chromatic pickup and every other note. Loopable remains the DEFAULT mode and the existing render(composition) API is byte-compatible.
- WENN the user exports a finite stand-alone phrase, DANN omit only last-bar Tension events with targetBarDelta==1 (their musical destination is outside the file). Keep all other notes, their positions/pitches/gates/velocities, and the full 4/8 bar file duration.
- WENN tension resolves within the written phrase, DANN preserve it in both modes.
- WENN the mode is invalid or musical source state is damaged, DANN reject it before exporting, not silently render.
- WENN variation is 0%, DANN never silently select a new alternate-treatment slot; the source generator explicitly forbids that.

This output-context option is not an automatic sound-improvement claim and does NOT alter synth releases, gates or incoming MIDI events.

## Old baseline defect, measured

Fixed seeds 10000 through 10299, variation bands 0/15/55/85 and 4 vs 8 bars. Counts of compositions with last-bar next-loop pickup, out of 1,200 per style and length:

| Style | 4 bars | 8 bars |
| --- | ---: | ---: |
| MINIMAL_BODY | 444 | 444 |
| CLASSIC_EBM | 836 | 836 |
| DARK_ELECTRO | 836 | 836 |

This is NOT a flaw in Loopable mode, only a mismatch with finite OneShot export.

## Regression and negative cases

The independent test suite runs 3 profiles x 2 lengths x 300 seeds x (one neutral + three intensity bands with five alternate treatments) = **28,800** cases. OneShot omits **16,928** explicitly directed cross-loop pickups; **11,872** cases remain completely unchanged. Loopable outputs match the old default in all cases. The test constructs expected OneShot events independently from the source-event graph and checks all retained note fields. It also serializes both MIDI output modes, rejects an unknown mode and rejects deliberately corrupted note duration.

Two original test-assumption errors were found and fixed: legal Minimal CROSS_PULSE pickup can be at step **14 or 15** (not only 15); and 0% is explicitly neutral, so other VARIATE alternatives are invalid. Production safeguards were not relaxed to make either test pass.

At fixed original preview seeds, default MIDI from the previous executable and new Loopable CLI is **3/3 byte identical**. New ebm_boundary_demo exports three matched Loopable/OneShot pairs using seed 10000, variation 85, eight bars, 120 BPM/4/4. Independent Python MIDI validation: **6/6 PASS** for note-on/off pairing, no overlap, phrase length and tempo.

Fresh GCC C++17 Release, warnings as errors: **12/12 CTest PASS**. AddressSanitizer+UndefinedBehaviorSanitizer with leak checking: **each of the 12 suites PASS across split runs**. A combined invocation timed out at 45 seconds after 10/12 had passed; the two remaining heavy suites passed separately (Session 27.01s, Core Novelty 54.44s). Therefore a single uninterrupted sanitizer suite PASS is *not* claimed.

## Remaining limitations

The output choice is exposed through the standalone ebm_preview command line (loop or one-shot) and C++ render overload, not through a VST3 GUI. OneShot intentionally does not preserve the final approach for seamless looping; Loopable does. No automatic changes to instrument envelope, MIDI gate duration, performance velocity, key or musical style. Musical taste and actual synth/host playback remain open. Both compose and composeAlternative remain NOT realtime-safe (allocations and exceptions); future VST3 audio callback must not execute them. GitHub Actions were not run.
