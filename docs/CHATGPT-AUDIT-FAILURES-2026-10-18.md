# Audit failure record — 2026-10-18

**Owner of these audit omissions: ChatGPT (assistant).** This is an accountability record, not a PASS certificate. An independent Claude review of commit `3a78441` found seven findings after earlier audits had been described too confidently. The original six SDK-independent tests passed in Claude's environment, but that did not establish complete correctness.

## Findings missed by prior ChatGPT reviews

| ID | Area | Missed behavior | Required acceptance evidence |
| --- | --- | --- | --- |
| F1 | VST3 event input | Non-note events counted against 64-note trigger budget, causing false block rejection | Mixed event stream with >64 non-note events but <=64 valid notes; overflow/NoteOff retry tests |
| F2 | Live MIDI | Simultaneous chord changes yielded transient zero-length notes and intermediate retunes | Same-offset Off/On/On; at most one final retune, no transient NoteOn |
| F3 | Live MIDI | Held lower note lost when higher note released | Last-note-priority across callbacks; releasing top note restores still-held note |
| F4 | PatternExchange | Pending `resetOnAdopt` lost if a new publish superseded the unread pending slot | Restore followed by GUI edit before audio acquisition must retain reset, plus concurrency review |
| F5 | GUI/engine | Ctrl+click BARS changed editable pattern but not engine bar count | Set 4 bars then Ctrl+BARS to 8; GENERATE fills bars 5–8 for all styles |
| F6 | Windows CI | `tools/**` omitted from push path filter and coverage guard | Change to tool source triggers workflow; coverage guard includes tools |
| F7 | Host tempo | MIDI-note mode used implicit 120 BPM when host tempo flag invalid | Missing tempo, invalid numerical tempo, later valid tempo and host stop tests; documented fallback |

## ChatGPT's process errors

1. **Premature PASS language:** previous audits were framed more conclusively than their actual test coverage justified.
2. **Missing adversarial test matrices:** overlooked simultaneous MIDI offsets, held-note overlap, unrelated MIDI event floods, queued state replacement, and divergent GUI versus engine state.
3. **Insufficient runtime evidence:** Linux SDK-free tests do not validate VST3 GUI, Windows host behavior, Studio One, or live event bus failure handling.
4. **Coverage-policy gap:** verifying a workflow's existing included inputs without checking ALL compiled inputs (`tools/**`) allowed a blind spot.
5. **Lack of explicit unverified classification:** not distinguishing code inspection, deterministic regression test, CI build, Steinberg Validator, and actual Studio One/125A Tester acceptance.

## Corrective control (not yet evidence of success)

- Fixes F1–F7 are proposed on `fix/claude-f1-f7-2026-10-18`, PR #2, with additional live MIDI regression cases.
- The Windows workflow must complete on the exact final commit SHA. Record build, CTest, exported MIDI, native host probe, editor lifecycle, and Steinberg Validator separately.
- F4 requires a targeted queued-publish versus audio-acquire test; event input F1 and GUI F5 require specific regression evidence beyond reviewing the patch.
- On failure, analyze logs and repair; do not reinterpret a failed or missing result as PASS.
- Do not claim release readiness before owner-local Studio One and 125A Plugin Tester acceptance. Keep the repair PR unmerged until automated evidence is satisfactory.

**Status at creation:** Code changes committed, final Windows QA and owner-local acceptance not verified. Historical audit PASS statements do not override these open verification requirements.
