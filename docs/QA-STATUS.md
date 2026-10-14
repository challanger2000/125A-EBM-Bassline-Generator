# 125A EBM musical engine v0.2 grammar and phrase-ending correction — 2026-10-13

**Status (2026-10-14): local multi-bar phrase-form technical checks PASS; musical quality still NOT VERIFIED; NOT FINAL.**

Previous: [dark tonal-direction audit](TONAL-CONTOUR-AUDIT-2026-10-14.md) (new fifth test group). Before that: [motif-lineage proof](MOTIF-LINEAGE-AUDIT-2026-10-13.md). Previous A/B listening stimuli remain available, but no listening judgement is claimed.

See [phrase-ending audit](PHRASE-ENDINGS-AUDIT-2026-10-13.md) for the latest controlled cadence/rest evidence and counterexamples.

See [grammar vocabulary audit](GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md) for controlled before/after figures, measured limits and counterexamples; the [prior quality audit](QUALITY-AUDIT-2026-10-13.md) remains part of the history.

| Gate | Latest result |
|---|---|
| C++17 GCC Linux Release build (warnings as errors) | PASS locally |
| Release CTest with unconditional REQUIRE | PASS **4/4**; original 3,000 fixed-seed compositions and 2,400 adjacent intensity comparisons, plus new 900-seed source-device/identity tests adversarial fakes, new cadence/rest/identity checks (including 3,600 composed cases) |
| ASan/UBSan Debug CTest | PASS 4/4 |
| Novelty-session audit, fixed 100 seeds/style | 300/300 accepted at declared **structural** distance 4; not an artistic success measure |
| Independent MIDI parse (`mido`) | PASS 3/3 newly exported example files: valid NoteOn/Off, 8 bars, 120 BPM |
| Phrase endings (chromatic approach vs structural silence) | PASS locally; device-specific rejection tests and 300-seed/style fixed comparisons in phrase-ending audit |
| Every repeated/transformed bar reconstructible from source + explicit edits | PASS locally; 3,600 valid compositions, 12 adversarial mutations rejected |
| 18,000-MIDI baseline preservation (unchanged generation) | PASS, prior/new total MIDI bytes and FNV64 digest match (see lineage audit) |
| 9 source devices and independent rhythmic identity checks | PASS locally; controlled fixed-seed corpus details in new grammar audit |
| Human listening and style authenticity | **NOT VERIFIED** |
| Native Windows build and DAW/VST3 | **NOT IMPLEMENTED / NOT VERIFIED** |
| Steinberg Validator / 125A Tester | NOT APPLICABLE to current standalone engine |
| GitHub Actions | NOT RUN; preserving limited CI budget |
| New directed Dark Electro tonal contract (local test fixture) | **5/5** CTest Release and **5/5** ASan/UBSan; 4,000 targeted generated cases, 3 adversarial mutations rejected |
| Unaffected style preservation, 2,500 configurations each | **Minimal and Classic byte-identical before/after** |
| Dark original vs revised, 1,000 seed diagnosis | Unmotivated upper pitch: **119 -> 0**; un-rising summit: **122 -> 0** |
| Hierarchical 4/8-bar phrase placement (0/15/30/55/85) | **6/6** CTest Release and **6/6** ASan/UBSan; 9,000 arrangements, 7,200 adjacent-level comparisons |
| Independent regression test applied to the pre-change Composer | **FAIL as intended**: incorrect phrase-arc placement detected |
| Before/after terminal variation (8 bars, 15%; 300 fixed seeds per style) | Prior **0/300**, updated **300/300** per style have distinct final-bar response |
| 0% baseline preservation | 1,800 identical MIDI renders (3 styles x 300 seeds x 4/8 bars): same 679,896 bytes and FNV-1a64 `a85f73001b25c2e1` |
| Musical listening and EBM usefulness of new form | **NOT VERIFIED**; structural placement is only a compositional design hypothesis |
| Release | NOT READY |

No third-party artist transcriptions or commercial MIDI data are included.
