# EBM motif-lineage integrity audit — 2026-10-13

**Experimental v0.2 core only.** This audit verifies **structural correctness**, not audible musical quality. No VST3/Windows/DAW or musical PASS claim is implied.

## Confirmed failure in prior branch HEAD e79e1be8daf41274099ba0c79a3088c7aa242a73

A generated eight-bar DARK_ELECTRO composition (seed 7, variation 85) was deliberately mutated after generation: in bar index 2, event index 1 had its relative pitch raised by one semitone, without adding a corresponding ANSWER operation. The *previous* `validate()` accepted it. The altered file would have valid MIDI note-on/off data but would violate the core design principle that variants retain a traceable source motif. The same weakness existed for unlogged gate/velocity/role mutations.

**WENN** a later bar is marked as an A/B motif repeat, **DANN** all its events must match their source event ID/pitch/onset/gate/velocity/role exactly unless an explicit typed edit justifies the change. Otherwise reject. A normal MIDI validation cannot detect this.

## Correction

- Extend each typed `Edit` with before/after *pitch interval* for `ANSWER` edits; non-pitch operations must keep these fields zero. The *source* event's interval remains immutable.
- Independently replay every bar's event operations, in order, starting from the original A/B motif. Check the exact event IDs, source step, allowed musical role, vacated anticipation destination, deleted DRIVE event, and before/after pitch interval. Note-gate truncation is applied after each replay step as in the actual monophonic engine.
- Compare reconstructed bar to produced bar **including** velocity, note duration, role, device identity, event IDs, target links and transformed flag. Any undocumented difference is a hard validation error.
- Never use the production `omit()`, `anticipate()` or `answer()` mutation functions inside this independent check; otherwise the validator can repeat the same mistake.
- Reject forged/reordered/duplicated edits and attempts to modify the immutable source motif. The `Repeat` enum remains a conceptual label; creating a fake logged REPEAT is explicitly rejected.

## Reproducible locally executed checks

- Initial red test: `BUG: unlogged change ACCEPTED before=3 after=4` against the preceding HEAD's compiled C++ core.
- Revised GCC C++17 Release CTest: **4/4 PASS**; new suite verifies 3,600 compositions = three styles x 100 seeds x 2 lengths x 6 variation levels and exercises twelve deliberately corrupted history/event cases.
- Revised GCC ASan/UBSan Debug CTest with leak detection: **4/4 PASS**.
- Full before/after Standard MIDI baseline: 3 styles x 500 fixed seeds x 2 lengths x 6 variation levels = **18,000** MIDI files generated per build, 6,699,594 output bytes each, streamed over all bytes into FNV-1a 64-bit hash **`64a726d5a981ff78` for BOTH versions**. This is a reproducible technical output-equivalence check under exactly the stated sample, **not** a cryptographic proof over every possible seed. Compositional MIDI content was not intentionally changed by this patch.
- No GitHub Actions. No separate MIDI-format parser was run for this no-change gate because the prior renderer and writer are identical and the generated MIDI-byte corpus is unchanged; the earlier parser results still apply to those fixed previews.

## Consequence for musical development

The quality objective is still *compelling, useful EBM riffs*, not passing four test groups or optimizing note counts. The new contract specifically prevents unlogged melodic/velocity/gate drift from undermining motif identity during later engine revisions, and makes selective musical variation auditable. This patch does **not** establish that the prior phrase endings are musically preferable; the blind A/B pack's listener judgement remains **OPEN**. Do not label this an improved sound or call the product release-ready.
