# 125A EBM Bassline Generator — Final-QA gate review
Date: 2026-10-15
Candidate: `b1d28792eb0da6471698488995b24a8adff5179b`
Branch: `research/v0.2.0-musical-foundation`
Status: **RELEASE HOLD — independent Codex review and owner DAW acceptance still required.**

## Reviewed evidence
- Windows CI run [38759093060](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38759093060): **SUCCESS** for MSVC build, 18 CTest tests, native Windows host state probe, 20 Win32 attach/remove editor lifecycles, Steinberg validator, and experimental ZIP staging.
- Previous baseline CI [38743205343](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38743205343) provided validator tally **537 PASS / 0 FAIL**; this tally must not be copied to the current commit without consulting its logs.
- Owner reports **all PASS** on the downloaded candidate with locally installed **125A Plugin Tester v0.2.10 FINAL**. This is owner-reported evidence; the raw TXT/JSON tester reports were not attached to this review.
- Production Tester was intentionally removed from cloud CI at `b1d2879` to avoid duplicate Actions spending. Timed-out earlier cloud tester experiments do not prove a plugin defect.
- Source review covered `src/vst3/Plugin.cpp`, `src/vst3/EbmPanel.cpp`, `src/HostBridge.cpp`, `include/ebm/EditPattern.hpp`, and key regression tests.

## Three original findings
- **QA-01 input overflow:** `Processor::process` now marks cleanup pending on >64 input events and calls the existing shutdown/release path before returning failure. CTest includes an oversized-trigger and NoteOff-backpressure regression in `tests/live_midi_tests.cpp`. **Source fix + scheduler regression PASS via Windows CI.** This does **not** exercise the complete actual IEventList -> Processor -> IEventList overflow path in a real VST3 host. Separate integration regression: **NOT VERIFIED**.
- **QA-02 EBM1 Song Start:** Controller fallback now uses DAW bar 1, matching `Model::restore` legacy behavior. Host-neutral EBM1 restore/start assertion is in `tests/edit_pattern_tests.cpp` and CI PASSED. Independent real Controller+Processor playback on an archived EBM1 project: **NOT VERIFIED**.
- **QA-03 LENGTH:** Display now calculates the effective length capped by the next NoteOn or bar-count end without mutating stored length. Existing `edit::toPhrase` contract regression passes. Real rendered GUI label and round-trip numeric entry: **NOT VERIFIED**.

## Function-by-function source audit
| Function | Implementation traced | Evidence | Remaining |
|---|---|---|---|
| STYLE, GENERATE, VARIATE | Controller -> Session -> editable pattern -> document message -> HostBridge -> runtime | Engine/edit/host CI; owner musical audition | Repeat-click and live DAW gestures |
| BARS 1/2/4/8; BAR navigation | Selector, state, `toPhrase`, rendered bar count | Edit/GUI policy tests | Every selection through real mouse |
| ROOT, SCALE, TRIGGER NOTES | Menu -> MIDI transpose/scale -> displayed allowed classes | Harmony + scale tests | GUI readability, locked-note cases |
| MIDI NOTE / TRANSPORT | Trigger parameter, MIDI events, PPQ/song-start | Live tests, owner prior playing | Dense chord changes with real host |
| NOTE, PITCH, VELOCITY, LENGTH, LOCK | Click, drag, numeric editor -> pattern transaction | Edit/host/GUI policy tests | User interaction at boundary values and locked steps |
| CLEAR, COPY, PASTE | Editable steps, clipboard and lock checks | Edit tests | Real GUI behavior across 1/2/4/8 bars |
| GLOBAL GATE 0–100% | Parameter, atomic gate, runtime + MIDI export | Gate/harmony tests | Fast automation save/restore in Studio One |
| EXPORT MIDI | Native file dialog -> `edit::toPhrase` -> `toMidi` -> filesystem | MIDI writer/test contracts | File dialog and reimport of 1/2/4/8 bars |
| ZOOM 100/150; editor reopen | Controller state, VSTGUI editor, host probe | 20 Win32 lifecycles + zoom state | Actual mouse and close/reopen in Studio One |

## Additional review observations (not pre-judged as defects)
1. `Controller::valueChanged` accepts LENGTH entries 1–128 even beyond the next event, whereas dragging clamps at next event. The display shows effective length but numeric editor shows saved length. **UX inconsistency: assess during owner DAW acceptance; retain backward-compatible serialization.**
2. `Processor::process` still returns immediately on an *invalid individual event offset or pitch* before entering cleanup. A malformed host event could leave a sounding voice pending. **Candidate robustness issue; targeted adversarial full-VST3 regression and Codex source review requested.**
3. The over-limit input path returns `kResultFalse` after successful cleanup. Confirm host compatibility and whether the intended contract should return success after safely discarding malformed input; do not change without test evidence.

## Final release blockers
- Raw current **125A Tester v0.2.10** output archived with exact bundle/hash (owner-reported PASS alone is not a reproducible report).
- Native Studio One Pro 8.1.2 interaction: every click/drag/numeric/popup, 100%/150% zoom, save/reopen, real project EBM1/EBM2 migration.
- Realtime paced playback and timing/dropout/CPU measurements with kick+bass, output-backpressure adversaries.
- Export 1/2/4/8 bars and reimport at Gate 0/50/100%.
- Independent **Codex read-only** source/QA-evidence inspection; no automatic code changes.
- Reconcile historical `docs/QA-STATUS.md` (engine-era data) before FINAL publication.

## Decision
**CURRENT CI PASS; OWNER-REPORTED LOCAL TESTER PASS; FINAL-QA NOT YET FULLY PASSED.** No Final label, protected-main change, tag or commercial package permitted on this evidence alone. Request Codex to challenge all findings and evidence, propose corrections only. Owner must explicitly approve implementation changes.
