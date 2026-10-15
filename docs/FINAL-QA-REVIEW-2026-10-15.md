> Historical checkpoint. Current repair/QA authority: [Final-QA RC](FINAL-QA-CODEX-RC-2026-10-15.md).
> Contracts or unverified statuses below describe the named earlier baseline.

# 125A EBM Bassline Generator — independent Final-QA review (2026-10-15)

**Verdict: NOT RELEASE READY.** This is a read-only source/QA-evidence review of development baseline `c311bed6a4c0b1ed0ede44acacecb69a8f36492b` on `research/v0.2.0-musical-foundation`. The shipped experimental VST3 and `main` remain unchanged. No new Windows binary, new independent Windows test, or 125A Tester run is claimed.

## Evidence verified

- Existing Windows workflow **38743205343**, job **116449962974**, same commit: Windows MSVC VST3 successfully built, CTest **18/18 PASS**, Steinberg Validator **537 passed / 0 failed**, independent host probe PASS (MIDI/event buses, binary document message, corrupt state rejection, 150% controller zoom state, **five** attach/remove Windows editor cycles). Workflow package staging and upload PASS. [Windows workflow](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38743205343).
- Exact run log confirms these results (the validator reports no bypass parameter because this is a MIDI instrument).
- `tests/gate_harmony_tests.cpp`: C/Em/Am/G diatonic mapping, 0/50/100% Gate, MIDI export gate, EBM2 state migration and ~600 seconds **virtual/offline MIDI time**, not a physically paced 10-minute Studio One stress run.
- `tests/live_midi_tests.cpp`: 3 sample rates (44.1/48/96 kHz) × 5 block sizes (1/31/127/257/512), transport stop/seek, single NoteOff backpressure and MIDI trigger logic.
- `tests/host_bridge_tests.cpp`: fixed-length document roundtrip, invalid corruption, 15 chunk/rate playback variations, lock-free immutable pattern publication with 10,000 attempted control-thread updates. These are host-neutral/unit tests, not an actual long-running DAW stress test.
- User's Studio One audition (in this conversation) confirms useful Classic EBM groove, chord-triggered performance including keyboard use with stopped DAW transport, and visible/current GUI functionality. This is *targeted human acceptance*, not exhaustive control-by-control QA.

## Source-confirmed findings requiring repair or explicit product decision

### QA-01 — MIDI input overflow does not explicitly flush sounding notes (priority HIGH)

`src/vst3/Plugin.cpp` `Processor::process`, MIDI input handling: if `data.inputEvents->getEventCount() > live::Runtime::maxTriggers` (64), returns `kResultFalse` **before calling** the live scheduler or its `release()` path. Any previously active bass note is left in scheduler state and may remain held downstream while repeated oversize blocks prevent processing. This conflicts with the 125A guardrail for MIDI overflow cleanup. Add a bounded RT-safe error/flush path, with a host-backpressure regression; do not allocate or lock in `process()`. **Source defect confirmed; real DAW manifestation unverified.**

### QA-02 — EBM1 legacy UI and transport Song Start can disagree (priority MEDIUM)

`src/vst3/Plugin.cpp` `Controller::setComponentState` creates an editable fallback at Song Start **2** for legacy documents (`doc.editable.enabled == false`). In `src/HostBridge.cpp`, `Model::restore` stores Song Start **1** while the editable overlay is disabled. Thus the displayed start bar can differ from the actual runtime offset for an EBM1 document, until a GUI edit publishes a new EBM3 document. Regression must cover a *legacy* saved document end-to-end through both Controller and Processor, including host playback position. **Source mismatch confirmed; affected historical user projects unverified.**

### QA-03 — LENGTH label can overstate effective sounding duration (priority LOW/MEDIUM)

The GUI (`src/vst3/EbmPanel.cpp`) labels each cell via `edit::lengthText(step.length)`, while `edit::toPhrase` in `include/ebm/EditPattern.hpp` always truncates duration at the next NoteOn/selected pattern end. Example: note at step 0 with LENGTH 16 ("1 BAR"), followed by note at step 2, exports/plays the first note for 2 sixteenth-steps (1/8). Numeric editing allows this length, whereas drag clamps it at the next event. Consider showing the *effective gate* or limiting direct numeric length to the next note, while preserving saved data and clear 100% global gate semantics. **Source discrepancy confirmed; not a MIDI engine crash.**

## GUI/feature traceability — implementation vs independent UI validation

| User-facing control | Controller / output path found | Existing objective QA | Actual mouse-in-Studio-One gate |
|---|---|---|---|
| STYLE | popup -> `Session::setStyle`, `generate`, full document/Processor | engine/host tests | targeted owner use; per-style clicks NOT VERIFIED |
| BARS 1/2/4/8 | popup -> editable count -> `toPhrase` -> live scheduler/export | edit tests + current GUI hit targets | end-to-end per-count NOT VERIFIED |
| KEY/ROOT | popup -> transpose non-locked notes, song root in state | C2 default and diatonic regression | owner C2 accepted; reset+reopen NOT VERIFIED |
| SCALE and TRIGGER NOTES | popup -> `inScale` quantization and GUI text | exhaustive 10 modes × 37 roots guide test | owner screenshot, not all modes visually measured |
| VARIATION and VARIATE | intensity slider; deterministic unlocked visible-bar edits | engine/edit tests | owner clicked; rapid dragging/repeat/recall NOT VERIFIED |
| SONG START | popup -> persisted start -> ProcessContext PPQ offset | state codec tests; legacy mismatch QA-02 | bar 1/2 actual DAW seek NOT VERIFIED |
| TRANSPORT / MIDI NOTE | mode parameter 100 -> block scheduler | live tests + owner keyboard stopped DAW | targeted owner PASS; multi-overlap/overload NOT VERIFIED |
| ZOOM 100/150 | controller state/editor zoom | 150% state + five attach/remove | source/layout and owner screenshot; 20 cycles NOT VERIFIED |
| PREV/NEXT + direct BAR tabs | local controller BAR changes | hit geometry tests | 1/2/4/8 navigation exhaustive NOT VERIFIED |
| GENERATE | Session generate -> motoric Classic editable overlay -> Processor | 18-test suite + owner musical approval | repeated editing during playback targeted owner use, no exhaustive gesture script |
| CLEAR | `edit::clear` retains LOCK notes | unit tests | Ctrl/lock semantics full GUI NOT VERIFIED |
| COPY / PASTE BAR | per-step clipboard + locked destination rejection | edit::copyBar tests, source path | copy/paste among all bar counts NOT VERIFIED |
| NOTE ON / LOCK | toggle; Shift+click lock; Ctrl reset | edit/host unit tests | owner live input works, full gesture matrix NOT VERIFIED |
| PITCH | double-click scale-constrained note selection | scale/host tests | all scales and locked-state GUI NOT VERIFIED |
| VELOCITY | double-click numeric or vertical drag, Ctrl default 100 | hit/drag-value tests | real mouse drag/typed edge values NOT VERIFIED |
| LENGTH | same editable numeric/drag + `lengthText` | numeric length/codec tests; QA-03 | actual cutoff feedback NOT VERIFIED |
| GLOBAL GATE | VST3 parameter 102, atomic MIDI behavior & MIDI export | gate tests and owner function confirmation | fast host automation and saved 0/100% after reopen NOT VERIFIED |
| EXPORT MIDI | native file selector -> edit::toPhrase -> toMidi(gate) | MIDI byte/header/event tests | filesystem dialog, naming and 1/2/4/8 imported in Studio One NOT VERIFIED |

The 125A control-coverage gate requires a tested chain **mouse -> VSTGUI callback -> document/parameter -> Processor -> emitted MIDI -> GUI feedback -> state recall**; source presence and CTest do not prove a complete chain.

## Remaining Final gates (NOT VERIFIED)

1. **125A Plugin Tester** native Windows run on the exact staged binary; no evidence of a Tester invocation in Windows workflow 38743205343.
2. Studio One **all-control** click/drag/numeric-entry/lock/default-reset test matrix at 100% and 150% zoom; at least **20 editor open/close/reopen cycles** per guardrails (the current independent probe performs five).
3. Real Studio One project save / close / reopen; state migration from an actual EBM1/EBM2 project, exact note/velocity/gate/ROOT/SCALE recall and automation.
4. Paced realtime MIDI-stress, long-running real host CPU p95/p99/max/deadlines/dropouts at representative host sample rates/block sizes; virtual 10-minute test is **not** paced realtime. Previous shared bass+kick audio interruption is of unknown cause, not proof of this MIDI plugin failing.
5. Automated adversarial input overflow, rapid chord-switch same-sample NoteOff/On, dense automation, host output capacity/backpressure and invalid-event recovery tests, especially for QA-01.
6. MIDI export/import check in Studio One for each bar count and GLOBAL GATE extremes; no hidden DAW trigger harmony is expected in an exported plain pattern MIDI file.
7. Reconcile stale `docs/QA-STATUS.md` (historical standalone-engine figures) before publishing a customer-facing release claim.

## Release decision

**HOLD — no Final/Gumroad package, tag, or main update.** Fix QA-01 and QA-02, resolve QA-03 usability policy, extend test coverage, and run the missing gates before owner approval. No plugin/source implementation was modified by this audit. A documentation-only QA report commit does not imply that any Windows regression was re-run.
