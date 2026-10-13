# Music-engine quality audit — 2026-10-13

**Scope:** v0.2 prototype only, before VST3, including the most recent correction to variation scaling and a bounded motif-novelty selector. This document does NOT certify artistic quality or artist-style authenticity. Evidence: local GCC Release CTest, ASan/UBSan Debug CTest, independent Standard MIDI parser, exhaustive source inspection, and fixed-seed diversity diagnostics.

## Confirmed defects in the original v0.2 musical implementation

1. **Nearly dead variation range.** For the same seed and style, 1–59% produced identical output; 60–100% produced another identical output. All 300 tested seeds per style reproduced this collapse. The 0% version remained different.
2. **A valid MIDI file is not a novel musical motif.** Original raw v0.2 composition (300 seeds, 8 bars, one chosen level) produced only 242 distinct MINIMAL_BODY phrases, 284 CLASSIC_EBM, and 250 DARK_ELECTRO. Rhythmic signatures differed less often: 60, 149, and 32, respectively. These are fixed-seed **diagnostics**, not statistically established EBM population measurements.
3. **No recent-motif detection.** Existing `generate()` always used a single seed and could repeat very similar motifs. Byte-wise distinctness tests hid near-duplicates.

## Implemented corrective contract (still an experimental musical hypothesis)

### Variation intensity has five documented output classes

| Setting | Musical intention | Count of typed edits on 4- and 8-bar phrases |
|---|---|---:|
| 0 | exact motif repetition | 0 |
| 1–24 | one pitch-role answer movement in a later bar | 1 |
| 25–49 | plus an omission/answer response creating space/contrast | 2 |
| 50–74 | plus an anticipatory shift or deliberately omitted DRIVE | 3 |
| 75–100 | plus a second controlled contrast, without changing source cell | 4 |

The control is **discrete/stepped**, not continuously variable despite a stored 0–100 number. Future GUI must either show these five discrete levels explicitly or implement an evidence-led continuous musical intensity model; it must not silently present this as smooth resolution. We do not add arbitrary per-note probability or unrelated fills. The first motif bar(s) are immutable at every level. An 8-bar MINIMAL loop at 0% remains exact repetition.

### A bounded `composeNovel()` selection

- Compares against the **last eight matching style/length compositions**, even when the user's recent history interleaves other styles. Pitches are analyzed as intervals relative to root, not absolute MIDI note numbers.
- Structural distance ignores velocity and later-bar variation; it counts changed motif onsets (weight 2) and interval changes at aligned onsets (weight 1). This is a deliberate testable heuristic, NOT a human groove or creativity score.
- Tries at most **16 reproducible candidate seeds**, selects the first meeting a declared distance of 4 from all recent relevant motifs, or returns the best candidate with `meetsTarget=false` when the target is not achieved. No unbounded reroll and no false guarantee.
- Algorithm allocates and runs only in standalone/offline/control context. **Do not run in an audio callback.** It must not be assumed equivalent to an intuitive song-quality judgment.
- The existing single-seed `generate()` API is unchanged. A future GENERATE button must explicitly call the novelty selector, retain the selected seed/state, and surface its quality flag if needed; there is currently no GUI or host integration.

## Reproducible checks

- Linux GCC C++17 Release with compiler warnings as errors: **PASS**.
- `ctest --output-on-failure`: **PASS, 1/1**. Composition loop: **3 styles × 100 fixed seeds × 2 lengths × 5 intensities = 3,000 compositions**. Separate **2,400 adjacent-level contrasts** confirmed the five represented outputs differ. All tests use `REQUIRE` which executes in Release even with NDEBUG, unlike `assert`.
- Sanitized build with `-fsanitize=address,undefined`: **PASS, 1/1** on same suite.
- Novelty simulated GENERATE sessions: **100 per style** (300 combined) with recent-history cap 8 and at most 16 attempts; **300/300 met structural distance >=4** in this specific fixed-seed scenario, with candidate counts MINIMAL 117, CLASSIC 100, DARK 106 (323 total candidates). This proves nothing about unheard output, all possible seeds, or the sufficiency of distance 4.
- Independent Python `mido` parser: **3/3 MIDI previews PASS** with matched NoteOns/NoteOffs, 4/4, eight bars at 120 BPM. Preview event counts 47 MINIMAL, 67 CLASSIC, 47 DARK (these are generated *examples* not measurements of the EBM genre).
- Negative fixtures: invalid MIDI events, overlap, missing chromatic target, invalid kick reference, invalid motif start, invalid composition settings; **rejected**.
- **UNVERIFIED:** Windows-native compilation, Studio One, VST3 buses, host automation/lifecycle, objective musical superiority, artist resemblance, blind listener usefulness, sound-design adequacy. CI / GitHub Actions **not run**.

## Remaining product risks / next step

**The sound and phrase authenticity gate remains OPEN.** Fixed source-motif grammar may be musically too recognizable across GENERATE calls even when distance 4 passes. Before VST3/GUI, use a small blinded, level-matched listening comparison of *only three curated independent examples* against the earlier v0.1 approach and against intentional negative controls. A quantitative onset score is not an objective musical preference. If the generated motifs still feel interchangeable, revise the *compositional grammars* (especially limited DARK_ELECTRO rhythm structures), not the MIDI exporter or a GUI cosmetic. Do not ask the product owner to test 90 examples.
