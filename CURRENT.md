# Current development candidate

125A EBM Bassline Generator v0.2, experimental Windows x64 MIDI-only VST3.
Active branch: `fix/p1-codex-qa-2026-10-15`.

## MIDI phase follow-up

The previous RC has a confirmed MIDI NOTE restart defect on separated Off/On.
[Current repair, semantics and QA](docs/MIDI-PHASE-RC-2026-10-15.md) supersede
that RC's phase/trigger claims. Its completed build remains preserved below.
Tested source `a54512812206e248d4c028ba2f2ea53ceb3a34d7`.
[Windows build PASS / new ZIP](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38790308058#artifacts):
`125A-EBM-v0.2-MIDI-PHASE-RC-Windows-x64`, artifact11998958012 (GitHub login).

## Preserved earlier candidate

- Source `4ef178d00c16f24c09e9b3f77214e76a635b77da`; implementation `b269552ac88f58b106f79554d5288f6da8630087`.
- [Earlier QA, all15 findings](docs/FINAL-QA-CODEX-RC-2026-10-15.md).
- [Earlier Windows build and ZIP](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38787724599#artifacts), artifact11998965328; retained as historical RC, not corrected for this new defect.
- Original owner-tested source `b1d28792eb0da6471698488995b24a8adff5179b` and `main` remain unchanged.

No new approved FINAL/Gumroad release. Each new binary requires owner-local
125A Plugin Tester v0.2.10 FINAL and Studio One 8.1.2 acceptance.
