# 125A EBM musical engine v0.2 grammar expansion — 2026-10-13

**Status: local technical checks PASS; musical quality still NOT VERIFIED; NOT FINAL.**

See [grammar vocabulary audit](GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md) for controlled before/after figures, measured limits and counterexamples; the [prior quality audit](QUALITY-AUDIT-2026-10-13.md) remains part of the history.

| Gate | Latest result |
|---|---|
| C++17 GCC Linux Release build (warnings as errors) | PASS locally |
| Release CTest with unconditional REQUIRE | PASS **2/2**; original 3,000 fixed-seed compositions and 2,400 adjacent intensity comparisons, plus new 900-seed source-device/identity tests and adversarial fakes |
| ASan/UBSan Debug CTest | PASS 2/2 |
| Novelty-session audit, fixed 100 seeds/style | 300/300 accepted at declared **structural** distance 4; not an artistic success measure |
| Independent MIDI parse (`mido`) | PASS 3/3 newly exported example files: valid NoteOn/Off, 8 bars, 120 BPM |
| 9 source devices and independent rhythmic identity checks | PASS locally; controlled fixed-seed corpus details in new grammar audit |
| Human listening and style authenticity | **NOT VERIFIED** |
| Native Windows build and DAW/VST3 | **NOT IMPLEMENTED / NOT VERIFIED** |
| Steinberg Validator / 125A Tester | NOT APPLICABLE to current standalone engine |
| GitHub Actions | NOT RUN; preserving limited CI budget |
| Release | NOT READY |

No third-party artist transcriptions or commercial MIDI data are included.
