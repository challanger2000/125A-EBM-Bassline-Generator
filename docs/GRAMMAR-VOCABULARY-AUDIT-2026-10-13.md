# EBM compositional vocabulary expansion — evidence report

**Research checkpoint:** 2026-10-13, C++17 prototype v0.2, source-grammar expansion. Not a VST3 and not an artist/style-approved product. This checkpoint follows the narrower v0.2 quality correction. No CI/GitHub Actions requested or used.

## Problem and failure discovered during implementation

The original v0.2 had effectively one rhythmic source construction per style, with small positional choices. Named grammars initially became the same actual note patterns after allowing generic phrase-position selections. A deliberately adversarial device-relabel test exposed this: two differently named minimal forms could have identical rhythmic signatures. A second targeted review exposed two indistinguishable Classic forms. We redesigned the **device-defining anchors and rests** to remain structural invariants despite permitted within-device flexibility; validation rejects false re-labelling.

## Musical devices: 3 procedures per style, not artist reconstructions

| Profile | Device | Explicit musical intent; source invariant |
|---|---|---|
| MINIMAL_BODY | PEDAL_GAP | Bass anchor on step 8 and upper response on steps 10–13; leave kick at step 4 without a bass onset |
| MINIMAL_BODY | DELAYED_BACKBEAT | Delay second low anchor to step 12, place melodic response late on steps 13–14 |
| MINIMAL_BODY | CROSS_PULSE | Drive/kick coincidence at step 4, another anchor at step 8, delayed upper response on step 12–13 |
| CLASSIC_EBM | MOTOR_LOCK | A-bar upper answer at step 10, B-bar low DRIVE at 9 and response at 11; gives air on the kick at 8 |
| CLASSIC_EBM | BREAK_AND_SHOUT | A-bar upper answer at 9, B-bar DRIVE at 8 and response at 10; emphasizes kick/bass coincidence |
| CLASSIC_EBM | OCTAVE_INTERLOCK | A-bar upper answer at kick step 8 rather than a low note, B-bar DRIVE 9 and response 11 |
| DARK_ELECTRO | CLIMB_RELEASE | A-bar chromatic target resolved at step 12; B-bar response on 10, low return 13 |
| DARK_ELECTRO | LATE_SUSPENSION | A-bar chromatic target held until 13; B-bar response on 11, return 14 |
| DARK_ELECTRO | BROKEN_ANSWER | A-bar earlier resolution at 11; B-bar response on 10 and return 13 |

These grids are **creative rule designs**, not empirically measured EBM artist transcriptions. Variation continues to preserve first source motif(s) and applies named operations. The chosen device is now attached to every bar and validated for style, source rhythm, and intra-phrase consistency; simply renaming a device without rewriting the source is an error.

## Controlled before/after comparison

- Baseline: local exact source from GitHub `research/v0.2.0-musical-foundation` commit `9c2cab6bc51d85d7569b93cf1903c9ac0501c6a4` in `ebm_v02_work`. New grammar compiled from the changes in this checkpoint.
- Configuration for **both builds**: 3 styles, 300 seeds each (`314159 + style*1019 + index`, index 0..299), 8 bars, variation 85, root MIDI 36, identical 120 BPM MIDI writer. Rhythm signatures are the source bar onsets (first bar for Minimal; two for Classic/Dark). Full-MIDI uniqueness is a byte-identical comparator, separate from the onset signature.
- Tests compare fixed samples rather than claiming genre-wide performance. MIDI diversity and onset diversity are not listening scores.

| Profile | Previous different onset signatures / 300 | New / 300 | Previous distinct MIDI files / 300 | New / 300 |
|---|---:|---:|---:|---:|
| MINIMAL_BODY | 60 | **73** | 230 | **234** |
| CLASSIC_EBM | 155 | **162** | **279** | 274 |
| DARK_ELECTRO | 32 | **91** | 248 | **289** |

**Important negative observation:** Classic MIDI-byte uniqueness **decreased** by five in this fixed sample despite stronger rhythmic variation. The new system is not uniformly better on every diversity metric. More notes or MIDI byte diversity is not necessarily better musically; do not loosen musical constraints solely to increase this count. Structural motive quality remains unresolved.

## Tests actually performed (local Linux)

1. GCC C++17 Release, warnings as errors: **compiled**.
2. CTest: **2/2 PASS** including original 3,000 compositions, 2,400 variation contrasts, original novelty/regression checks, and new independent grammar-identity suite. New suite exercises 300 fixed seeds per style, confirms all 9 devices reachable, verifies their musically distinct source event roles, and rejects mismatched/fake device declarations.
3. GCC ASan+UBSan Debug: **2/2 PASS**, leak detection enabled.
4. `ebm_preview` generated one 8-bar MIDI per style. Independent Python standard-library parser `tests/verify_midi.py`: **3/3 PASS**, monophonic matched NoteOn/NoteOff, 120 BPM, 4/4, correct eight-bar endpoints. Preview note counts: 47 Minimal / 67 Classic / 47 Dark.
5. Neutral WAV rendering generated bass+kicks+hats for those three *new* MIDI previews; 44.1 kHz mono, ~16.40 s including tail, peaks around 0.49–0.50 FS, no clipping. **Audible quality and subjective authenticity NOT evaluated**.
6. Windows native build / Studio One / VST3 validator / GUI / state recall: **NOT IMPLEMENTED / NOT VERIFIED**.

## Scope of claim and next gate

This change measurably expands the vocabulary of **rhythmic ideas** in the tests, notably Dark Electro, while retaining deliberate phrase-level relationships and real musical role invariants. It has *not* established good EBM compositions, expressive bass sound or artistic acceptance. Next meaningful gate is a **small, reproducible blind A/B listening comparison** between old/new phrasing using level-matched identical playback sound and kick, coupled with explicitly annotated user usefulness criteria. Do not substitute the nine named devices or 2/2 CTest results for that listening evidence. Do not launch GUI/VST3 production merely because numerical variety increased.
