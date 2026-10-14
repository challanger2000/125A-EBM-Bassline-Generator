# GENERATE and variation-level session — implementation audit (2026-10-14)

**Scope:** prototype control layer; **not a GUI/VST3** and not musical acceptance.
**Engineering status:** locally tested C++17 on Linux. The GitHub branch used as
baseline is `research/v0.2.0-musical-foundation`, commit
`1b70c8cc4795834f1b0a659b07826ef3785facb4`.

## WENN -> DANN: required user behavior and corresponding checks

| Condition | Expected behavior / test contract |
|---|---|
| User presses GENERATE | Select a fresh source motif against up to eight previous matching motif contexts; retain the *selected*, not merely requested, seed. |
| Novelty search exhausts 16 candidates | Return best observed candidate **with** `meetsNoveltyTarget=false`; do not claim a guaranteed different/usable riff. |
| User changes variation strength | Recompose the *same* source motif (same seed and initial source bars); make changes only through documented musical operations. |
| User chooses same intensity band again | Report `false` (no change); do not misreport a new musical variant. |
| User restores variation 0 | Reproduce the original MIDI events byte-for-byte for the selected seed and key. |
| User transposes | Preserve note onset, gates and velocities; move pitch by exact semitone delta. |
| Project state is saved and recalled | Rebuild the same selected MIDI and bounded generation history from saved value data. Next GENERATE must remain deterministic. |
| Snapshot is damaged or outdated | Reject before replacing any current data; keep the last valid musical state. |
| Style or bar length changes | Invalidate incompatible current riff (keep historical source motifs for future GENERATE). |

The source motif is a deterministic seeded composition. `Session` delegates all
musical construction and variation to the existing `compose()` and all bounded
candidate comparison to `composeNovel()`; it does not duplicate per-step random
composition logic. History saves selected `Config` values instead of entire
potentially inconsistent event graphs and is capped at **48** entries (six
possible style/length contexts times eight candidate history entries). The
novelty metric is structural, not a subjective groove or creativity score.

## Tests actually run locally

- GCC C++17 Release with compiler warnings as errors: `ctest` **8/8 PASS**.
- GCC C++17 ASan/UBSan Debug with leak detection: `ctest` **8/8 PASS**.
- New deterministic `session_tests.cpp`: **1,440** counted Generate iterations
  (3 styles × 2 lengths × 10 initial seed configurations × 24 clicks),
  **4,320** intentional variation transitions, **1,440** immediate state
  save/reload comparisons, and additional 40+5+context-switch Generate
  operations per initial configuration.
- Test checks selected seed, MIDI bytes, motif identity, variation-band no-op,
  transpose and return, 48-entry history cap, determinism after recall,
  all-or-nothing rejection of five malformed snapshots and invalid controls.
- Independently parsed **three** session-produced eight-bar Standard MIDI
  Files with repository `tests/verify_midi.py`; monophonic balanced MIDI
  events, 120 BPM, 4/4 and eight-bar ending **3/3 PASS**.
- Existing music, tonal-direction, phrase-form, motif-lineage, accent and
  engine suites all continue to pass.

## Clearly unresolved product problems

1. Repeated VARIATE at the **same** intensity does **not** yet find another
   musical treatment of the same motif. `variate(level)` is a level-setting
   action, not a multi-alternative phrase editor; the return value prevents a
   misleading success message. Before a user-facing VARIATE button exists,
   add a bounded alternative selection procedure that retains motif anchors,
   note-off safety, style grammar, and verifiable edit lineage.
2. State representation is an in-memory C++ value struct, versioned as `1`.
   No byte-level VST3 state format, migration code or project-host test exists.
3. `Session` composes vectors and may throw exceptions. All methods must be
   called **outside the realtime processing callback**; future plugin requires
   an immutable, precomputed MIDI arrangement transferred safely to the audio
   thread with bounded scheduling.
4. There are **no** Windows-native tests, Studio One host checks, Steinberg
   Validator results, independent listening preference ratings or validated
   EBM-artist resemblance results for this new layer. MIDI correctness and
   reproducibility must never be called a musical release PASS.

## Next acceptance gate

Implement bounded genuinely different **VARIATE alternatives** based on a
single locked motif, then test repeated clicks on three curated examples in a
neutral synth + beat context. Subsequent VST3 work requires host/state/realtime
compliance under 125A rules.
