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

## 2026-10-14 — Kick/answer accent interlock (source checkpoint)

- GCC C++17 Release CTest: **7/7 PASS locally**; AddressSanitizer+UndefinedBehaviorSanitizer CTest: **7/7 PASS locally**.
- New independent kick/answer suite: **2,400 paired reaccent cases PASS** (2 styles × 300 seeds × 2 phrase lengths × 2 high intensities), **8,400 unchanged cases**, **14 deliberately corrupted logs/notes/configurations rejected**.
- Paired source-to-output audit across **7,200 identical configurations**: **5,600 exact matches**; **1,600 high-intensity Classic/Dark phrases differ only in precisely two last-bar velocities**. All note onsets, pitches, gates, counts are unchanged. The source motifs remain untouched.
- Four independent MIDI examples (2 styles × old/new): checked against the standard MIDI parser, **4/4 PASS**. Four WAV previews 44.1 kHz 16-bit mono, same drum accompaniment and sample-aligned timing; **4/4 byte-identical on repeat rendering**. These are laboratory audition materials only.
- **NOT VERIFIED:** subjective improvement in groove, EBM authenticity, perceptual loudness matching, Windows/Studio One/VST3 and host lifecycle. GitHub Actions **not run**. No musical/production release PASS.

## 2026-10-14 — Offline GENERATE / Variation-level session

- GCC C++17 Release: 8/8 CTest PASS locally, including the new
  `EBM_Generate_Variate_Session` test (1,440 counted consecutive Generate
  iterations, 4,320 intended variation reversals, and 1,440 verified
  value-state save/reloads across all three styles, 4/8 bars, fixed seeds).
- ASan+UBSan Debug: 8/8 CTest PASS locally with leak detection enabled.
- New regressions: selected-seed retention; source-motif identity after
  variation; 0%-to-85%-to-0% exact MIDI restoration; seven-semitone transpose
  and return; recent-history bound of 48; 5-next-GENERATE project-state replay;
  invalid state/setting rejection without modifying the last valid riff;
  style/length context reset; no-change reporting on repeated intensity.
- `ebm_session_demo` generated three 8-bar MIDI examples via the session,
  independently parsed with `tests/verify_midi.py`: 3/3 PASS.
- No claim of continuous variation, new alternative on repeated VARIATE click,
  subjective musical benefit, Windows-native testing, VST3 host/state API,
  Studio One, validator or release. No GitHub Actions run.

## 2026-10-14 — Repeated VARIATE with preserved source motif

- GCC C++17 Linux Release: **9/9 CTest PASS**; Debug ASan/UBSan with leak detection: **9/9 PASS**.
- Added `EBM_Repeat_Variate_Alternatives`: **6,600** tested composition alternatives across 3 profiles × 2 lengths × 55 seeds × 4 nonzero intensity bands × 5 treatments. All five MIDI outputs are distinct per case; source bars, seed and musical lineage remain fixed.
- New Session tests include **5,280 repeated VARIATE transitions**, **6,600 save/restore and invalid-state checks**, exact transposition and return, state-v1-to-v2 migration, strict 0% neutral behavior and explicit `variate(level)` reset to the canonical arrangement.
- Unmodified `compose()` baseline: **3,000 existing MIDI outputs matched byte-for-byte** before and after the change. FNV-1a 64-bit on concatenated MIDI bytes: `2516969645277329636` in both builds.
- Independent `tests/verify_midi.py` tested **six MIDI files**, three original/first-alternative pairs: 6/6 PASS, no stuck notes. No sound-quality review implied.
- Defects found by adversarial testing and corrected: already-anticipated Classic anchor at four bars/high intensity; two identical Dark Electro four-bar alternatives at high intensity. Both are now distinct role-based treatments.
- **UNVERIFIED:** Windows-native build, VST3, Studio One, playback usefulness, credible EBM style adherence and real listener preference. No GitHub Actions run during local tests. NOT FINAL.

## 2026-10-14 — Independent rhythmic novelty gate (current checkpoint)

- GCC C++17 Release CTest **10/10 PASS locally**, including new adversarial source-rhythm checker. Debug ASan + UBSan CTest with `detect_leaks=1`, parallel 4: **10/10 PASS locally**. A first sequential sanitizer run hit a shell/tool timeout; the later complete run passed without test errors.
- Original seed selection claimed acceptable novelty on 3,000/3,000 requests **per style** even when the source rhythm duplicated one of the prior eight: 0 MINIMAL, **76 CLASSIC**, **184 DARK** mislabeled rhythmic-repeat cases in this fixed sample.
- Revised two-gate selection on **the same** 3,000 requests/style: **0 repeated source rhythms** across the recent-eight window. **9,000/9,000** met both declared *structural* thresholds in this sample; no general or artistic-success guarantee.
- Composer and arrangement C++ files unchanged: only the novelty selector, Session exposed diagnostic, tests and documentation were modified. VARIATE/source motif functionality is still verified by existing regression suites.
- `minimumRhythmicDistance` counts presence/absence differences of grid onset positions (source motif only), is independent of root pitch and phrase variation. Threshold 2 is a provisional engineering acceptance gate, NOT a psychoacoustic cutoff. A fallback after 16 tries must set `meetsTarget=false` and surface the diagnostic score rather than pretend success.
- **Still NOT VERIFIED:** musical catchiness, EBM musical preference, production usefulness, Windows native, VST3/Studio One, realtime performance/host-state bytes. No GitHub Actions used.
