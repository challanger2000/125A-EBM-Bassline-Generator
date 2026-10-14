# 125A EBM: core-groove novelty audit (2026-10-14)

**Branch:** `research/v0.2.0-musical-foundation`, based on `5ed8da0f36f10486480589b57f3fe4db114f9e61`.
**Scope:** C++17 GENERATE candidate selection only. **NOT a proof of musical quality, not a Windows/VST3 build, not a musical release approval.**

## WENN -> DANN: observed selection defect

The prior implementation added a proper onset-distance test, but it treated **every event**, including decorative `Pickup` and chromatic `Tension`, as part of the rhythmic novelty contract. Thus moving only a decorative pre-downbeat could earn two distinct-onset points while the playable hook/drive foundation remained identical to one of the eight previous motifs. This is a *misleading novelty label*, not merely a philosophical preference.

**Before measurement:** compiled actual pre-change C++ sources from the exact GitHub branch snapshot. For each style, requests `1..3000`, seed=`request * 1009`, 8 bars, variation=50, recent window=8 motifs in a 48-item history. An independent check mapped only `Anchor`, `Drive`, `Answer`, `Resolution` events in the one (Minimal) or two (Classic/Dark) original source bars, ignoring `Pickup`/`Tension` decorations. It counted chosen candidates where the backbone onset bitmap **exactly matched** one of the preceding eight, even though full-rhythm and structural novelty were declared sufficient.

| Style | Old accepted backbone repeats | New accepted backbone repeats | Requests |
|---|---:|---:|---:|
| MINIMAL_BODY | 207 | **0** | 3,000 |
| CLASSIC_EBM | 122 | **0** | 3,000 |
| DARK_ELECTRO | 254 | **0** | 3,000 |
| **Total** | **583** | **0** | **9,000** |

**Important:** This is one finite fixture, not a general quality or uniqueness guarantee. `Pickup` and `Tension` are not inherently unmusical or ignorable to the listener; omitting them from the *backbone novelty* screen only prevents decorations from masquerading as a new hook. It does **not** claim that the remaining 9,000 examples are all good or truly original EBM.

## New three-part candidate contract

1. `motifDistance` >= 4 (legacy contour-plus-onsets screen).
2. `motifRhythmDistance` >= 2 (all source onsets, independent of pitch).
3. New `motifCoreRhythmDistance` >= 2 (**Anchor/Drive/Answer/Resolution** onsets, independent of decorative pickups/tensions and independent of pitch).

Every screen compares the **minimum** separation from each of the eight recent style/length matching motifs. The pre-existing maximum **16 candidate seeds** remains unchanged. A pass means all three constraints passed on the finite history window. A failure returns a deterministic fallback explicitly marked `meetsTarget=false`; fallback candidate order is: strongest backbone separation, then all-onset separation, then legacy contour separation. With empty history the first candidate passes *vacuously*, and its reported distances of zero are not evidence of uniqueness.

`NoveltyResult` / `GenerateInfo` expose `minimumCoreRhythmicDistance` and `meetsCoreRhythmicTarget`. No SessionState schema, composition, MIDI rendering, or musical source rules changed. `Session::generate()` relays the independent result; it does not report `meetsTarget` as an artistic quality score. The selector uses containers, dynamic allocations and exceptions and is **not realtime-safe**; never call it on the audio thread.

Threshold 2 and the definition of core roles are **EMPIRICALLY TUNED, auditable design decisions**, not statistical inferences from EBM recordings. Other musical signatures, accent distribution, syncopation and listener preference remain independent future acceptance questions.

## Tests run and evidence limits

- C++17 GCC Release (`-Wall -Wextra -Wpedantic -Werror`): CTest **11/11 PASS**, including independent new `EBM_Core_Groove_Novelty` with **9,000** sequential GENERATE cases, fixed seeds and recent history. The test computes a **separate** backbone fingerprint from event roles, checks all three distances against the history, and confirms no source-bone duplicate is marked acceptable. It also exercises state restore determinism and inconsistent genre comparisons. `previousMislabels > 0` adversarial fixture proves that pickup-only novelty was a realistic case in the generator, not a vacuous check.
- GCC AddressSanitizer + UndefinedBehaviorSanitizer Debug with leak detection: existing **10/10 PASS**; new targeted suite **1/1 PASS with 750 requests** (`EBM_CORE_SAN_SMOKE=1`). A single initial full 9,000-case Debug/sanitizer sweep reached the test runner time limit; **do not report a full 9,000-case ASan pass**. The complete 9,000-case release test did finish.
- Raw source files `src/Composer.cpp` and `src/MidiWriter.cpp` are **unchanged**. A fixed `compose()` and `composeAlternative()` input therefore follows the identical note-generation and MIDI encoding path. The candidate **selection** and thus the seed shown by GENERATE can intentionally change; claiming that successive GENERATE MIDI sequences are byte-identical would be false.
- No real musical listening or prospective user preference score; no Windows build, VST3 processor/GUI, DAW, validator or realtime benchmark. No GitHub Actions required.

## Remaining question

The triple gate fixes a **specific false-positive reason**. A rhythmically different backbone need not be an effective bass hook. Do not keep adding abstract thresholds as a stand-in for musical performance. The next musical acceptance needs a small, purposefully chosen A/B corpus in fixed kick/synth context, not a mass review of arbitrary MIDI files.
