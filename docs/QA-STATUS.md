# 125A EBM musical engine v0.2.0 QA, 2026-10-13

**Status: technical local tests PASS, musical listening gate OPEN; NOT FINAL.**

| Gate | Recorded result |
|---|---|
| GCC/Linux C++17 -Wall -Wextra -Wpedantic -Werror Release | PASS, 3 library sources + 2 executables |
| CTest musical-contract suite | PASS 1/1; 2,400 compositions = 3 styles × 100 seeds × 4 intensities × 4/8 bars |
| Sanitized Debug CTest (-fsanitize=address,undefined) | PASS 1/1, no recorded sanitizer errors |
| Independent MIDI verification | PASS 3 files using third-party mido parser: 8 bars/120 BPM, matched NoteOn/Off, 1-voice max |
| Preview event counts | MINIMAL 47, CLASSIC 67, DARK 48; one independently rendered MIDI preview per style |
| Distinctness diagnostic | At least 85 unique MIDI files in 90 seed test items; stochastic duplicates are still possible |
| Adversarial contract tests | Invalid roots/bars/styles/variation, deliberate overlap, missing chromatic target, missing kick, bad motif start, invalid note-off overlap rejected |
| Root identity | PASS C2→G2→C2, rhythm/gate/velocity preserved |
| Musical quality/style authenticity | **NOT VERIFIED**: no human listening/rating accepted |
| Windows native build, Studio One / VST3 | **NOT TESTED / NOT IMPLEMENTED** |
| Steinberg Validator / 125A Tester | **NOT APPLICABLE YET** |
| GitHub Actions | **NOT RUN**; save budget |

## Interpretation

The deterministic contract test does **not** establish that generated basslines are objectively compelling. It establishes a repeatable role/phrase/gate structure, meaningful transformations, and MIDI correctness. The nearest meaningful milestone is controlled sonic A/B against the v0.1 baseline, with curated examples rather than 90 user-listening tasks. No release/GUI claim is permitted on these test results.
