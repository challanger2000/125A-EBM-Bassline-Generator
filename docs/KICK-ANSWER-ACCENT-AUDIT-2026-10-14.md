# EBM Bass–Kick Accent Interlock Audit — 2026-10-14

**Version/status:** 125A EBM v0.2.0 standalone C++ research prototype, NOT VST3, NOT FINAL. Grooviness and listener preference are **UNVERIFIED**. Inputs and output MIDI evidence are ORIGINAL engine results, not transcriptions of DAF, Nitzer Ebb, or Front 242.

## Problem and expected behavior (set before changing code)

All first-source motifs have stable, deliberate accent contours. The previous engine changed pitches, timing and omitted hits in higher variation levels, but **the closing bar's on-kick bass accent always remained stronger than the offbeat answer**, for both CLASSIC_EBM (MIDI velocities 120 vs 108) and DARK_ELECTRO (115 vs 101). In 300 fixed seeds per style at variation 85, this occurred in **600/600 phrases**, and the final bar's two velocities were identical to those of its source B motif. This is an observable limitation, not proof those prior phrases sounded bad.

**Hypothesis (EMPIRICALLY TUNED):** For a *strong* variation, exchanging dynamic emphasis between an on-kick bass anchor/resolution and its deliberate offbeat answer can create a call-and-response accent contrast with the kick, without changing rhythm or the melodic line. This is a controlled creative option, not a claim about average EBM songs or an optimum velocity law.

### WENN → DANN musical contract

- IF `variation < 75` or style is `MINIMAL_BODY`, THEN MIDI remains bit-for-bit identical to the previous implementation.
- IF variation is 75–100 in `CLASSIC_EBM`, THEN the **final B bar** exchanges MIDI velocities between anchor ID `108` on kick step 12 and answering note ID `107` on weak step 10/11: **120 ↔ 108**. Onset, pitch, duration, other velocities and note count stay identical.
- IF variation is 75–100 in `DARK_ELECTRO`, THEN final B-bar **resolution** ID `101` at kick step 0 exchanges velocities with answering note ID `104` at weak step 10/11: **115 ↔ 101**. The resolution remains on the same pitch; its role and target are unchanged.
- IF a REACCENT is applied, THEN its source/pair IDs, roles, beat positions, prior/updated velocities, phrase-last-bar placement and strong-variation setting must be reconstructible from the untouched source motif. Missing, fake or out-of-range edits FAIL validation.
- IF the two velocities exchange, THEN their **arithmetic sum** is preserved. This prevents an unintended systematic velocity boost but does **not** establish equality of actual acoustic loudness, transient impact or psychoacoustic strength. A downstream synth might map velocity to filter cutoff as well as amplitude.
- IF no musical benefit is perceived in level-matched A/B, THEN retain/rethink the accent-transfer option rather than assuming the new rule is superior.

## Reproducible local measurements

**Baseline:** research branch commit `11ff818284b3cdcd7a76b20447b7f06dee244b83`, verified against local source Git blob SHA; branch was rechecked before the intended commit.

Build environment: Linux GCC 14.2, C++17, release warnings-as-errors. `ctest`: **7/7 PASS**. Separate Debug ASan + UBSan build: **7/7 PASS** with leak detection enabled.

Independent, fixed configuration audit: styles `MINIMAL_BODY`, `CLASSIC_EBM`, `DARK_ELECTRO`; 200 seeds each (`seed * 7187 + style * 101`), 4 and 8 bars, variation `0, 15, 30, 55, 85, 100`: **7,200 baseline-vs-revised composition pairs**.

| Comparison | Results |
|---|---:|
| Exact same rendered note events, all velocities | 5,600 / 7,200 |
| Two velocity changes only (same pitches, onsets, gates, counts) | 1,600 / 7,200 |
| Unintended changed pitches/onsets/durations | **0** |
| Strong-variation accent transfers in separate regression suite | 2,400 / 2,400 |
| Unchanged non-target controls in suite | 8,400 / 8,400 |
| Tampered event/operation configurations rejected | 14 / 14 |

The regression suite itself evaluates 3 styles × 300 seeds × 2 bar counts × 6 variations = **10,800** compositions. Changes are confined to the final response of Classic/Dark at 75–100; all other test cases retain their prior note events.

## Audio comparison, controlled but not musically rated

Four standard MIDI files from equal parameters (Classic and Dark, each old/new), **4/4 PASS** under the independent MIDI parser: eight bars, 4/4, 120 BPM, matched NoteOn/NoteOff. The optional `tools/accent_audition.py` demands exactly two last-bar velocity changes and rejects any unrelated note, pitch, gate or time modification. It renders a neutral saw/sine bass against **the same deterministic 4-on-floor kick and offbeat hats** used for earlier QA. Two A/B pairs are mixed in order; bass stem RMS is equalized within each pair; the drums are unchanged. Four WAV files reproduce byte-for-byte on a second rendering. These are 44.1 kHz, 16-bit mono guides (~16.4 seconds each), **not the intended EBM bass patch**. Bass-stem RMS matching is not true perceived-loudness matching.

- CLASSIC_EBM example: two changes on final-bar sixteenth steps 11 and 12 (full-phrase steps 123 and 124), formerly velocity 108/120, now 120/108. A = new, B = old.
- DARK_ELECTRO example: final-bar steps 0 and 10 (full-phrase steps 112 and 122), formerly 115/101, now 101/115. A = old, B = new.

## Open gates

- Musician/listener preference and actual EBM production usefulness: **NOT VERIFIED**. Only two targeted diagnostic pairs are supplied, not a bulk listening assignment.
- A real bass patch, velocity-to-filter response, different kick templates and actual host tempo playback may change what sounds musically preferable; do not generalize from this laboratory synth.
- Windows native build, Steinberg Validator, Studio One event routing, automation, and project state: **NOT IMPLEMENTED / NOT VERIFIED** for this standalone research engine.
- No GitHub Actions were run; the source checkpoint must remain on the research branch with `main` unchanged.
