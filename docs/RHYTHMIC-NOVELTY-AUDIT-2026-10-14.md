# GENERATE: independent rhythmic novelty check — 2026-10-14

**Status:** C++17 standalone structural QA only. NOT a validated musical-quality or EBM-authenticity ranking system; NOT a VST3 or shipping plugin.

## Concrete problem / counterexample

Before this change `motifDistance()` combined changed MIDI pitch intervals (1 point) and differing note-on locations (2 points). `composeNovel()` approved the first candidate scoring >=4. Consequently **identical source rhythmic figures with different pitch answers** could be declared novel, although the bass groove had the same onset structure. The defect was reproducible in actual generated sequences; it was not an inferred or invented problem.

Fixed-seed test protocol (both actual C++ builds, not synthetic extrapolation): for each of three styles, 3,000 consecutive GENERATE-like `composeNovel` requests, seed = request_index * 1009, phrase 8 bars, VARIATION 50, retaining the most recent prior results. Count cases where the chosen source one/two-bar onset bitmap was **identical** to any of the eight most recent matching style/length motifs, despite `meetsTarget=true`.

| Profile | Previous misleading accepted rhythm repeats / 3,000 | Two-gate implementation / 3,000 | Two-gate structural accepts / 3,000 |
| --- | ---: | ---: | ---: |
| MINIMAL_BODY | 0 | **0** | **3,000** |
| CLASSIC_EBM | 76 | **0** | **3,000** |
| DARK_ELECTRO | 184 | **0** | **3,000** |

The old test's structural metric remains visible and unchanged, but it is no longer used alone to advertise a *new rhythmic idea*. The finite 9,000-request sample is not an estimate of real-world taste or a universal uniqueness guarantee. The same three devices/fixed rhythmic grammars remain creatively limited.

## Engineering contract: WENN -> DANN

- WENN GENERATE compares a candidate against a recent same-style/length source motif, DANN measure **two independent features**:
  - `motifDistance` (legacy combined onset/pitch score >=4);
  - `motifRhythmDistance` (symmetric difference in occupied sixteenth-note positions, >=2).
- WENN a source has only new pitches on the identical grid, DANN rhythmic distance =0 even when legacy `motifDistance` exceeds 4. It cannot be declared rhythmically new.
- WENN variation, transposition or re-accent only alters later phrase development, DANN source rhythmic distance remains zero; variation must not be conflated with GENERATE novelty.
- WENN one of at most 16 selected candidate seeds passes **both** tests against each of up to eight matching recent sources, DANN return it with truthful `minimumDistance`, `minimumRhythmicDistance`, `meetsRhythmicTarget=true`, `meetsTarget=true`.
- WENN no candidate passes, DANN return the best available fallback prioritizing source rhythm difference (tie-break legacy score), clearly marked `meetsTarget=false`. **Do not silently restart unbounded generation.**
- WENN there is no relevant previous motif, DANN the first generated candidate passes vacuously with no direct comparison; zero reported distance is not evidence of audible originality.

## Verification / boundaries

- Fresh GCC C++17 Release: **10/10 CTest PASS**, with `-Wall -Wextra -Wpedantic -Werror`.
- Debug AddressSanitizer + UndefinedBehaviorSanitizer, leak detection, `ctest -j4`: **10/10 PASS**. Initial sequential sanitizer attempt timed out at the tool boundary; complete parallel retry passed.
- New independent test `tests/rhythmic_novelty_tests.cpp` checks same rhythm with different pitches (previous false positive), eight-source window, transposition/variation neutrality, bounded candidate trials, truthful fallback/gate flags, and Session state round-trip. Existing nine suites remain passing without loosening their musical/host invariants.
- Candidate source-reconstruction still validated by existing `validate()` / event-lineage tests. `src/Composer.cpp` / `src/MidiWriter.cpp` left unchanged, meaning original single-seed motifs and `VARIATE` output are unchanged. Only the **candidate chosen by GENERATE** can change.
- The threshold 2 is an *empirically chosen testable minimum* of differing source onset positions; not a mathematical proof of groove or a publishable psychomusicological quality metric.
- No blind listening, human preference survey, genre MIDI corpus, Windows native compilation, DAW/VST3 or full realtime callback tests. Session and selection use `std::vector` and exceptions by design and **must remain off the audio thread**.

## Remaining acceptance gap

At least a small representative blind A/B listening session on identical synth/beat/level, emphasizing **motif memorability, useful silence, bass/kick interplay and intent of variation** is still needed. The program cannot legitimately declare a generator phrase “good EBM” from onset-distance metrics alone. No requirement for the user to review 90 files.
