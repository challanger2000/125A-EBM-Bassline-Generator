# 125A EBM Bassline Generator

**Current status: experimental musical-engine v0.2.0, expanded grammar checkpoint. Not a VST3. Not release-ready.**

The engine develops original monophonic MIDI phrases using musical *roles* instead of the independent onset/pitch lottery used by v0.1. Each profile uses a different compositional grammar; every bar carries stable motif event IDs and explicit transformation records. A separate event-lineage audit now reconstructs each later bar from its original motif and named edits, rejecting undocumented pitch, velocity, gate, role or rhythm modifications. The three hand-authored v0.2 reference riffs are **specification fixtures**, not notes copied into the generator. No artist MIDI patterns are bundled.

## Three experimental style grammars with nine explicit devices

- `MINIMAL_BODY`: one-bar pedal, active rests, upper answer, either an intentional rest, a tonic pickup, or (in CROSS_PULSE) a chromatic return to the next bar. Eight exact repeats are allowed at VARIATION=0.
- `CLASSIC_EBM`: two-bar motor/response, deliberate pickups, optional fifth-degree pedal move, structured omission and anticipation.
- `DARK_ELECTRO`: two-bar contour question/answer, a targeted chromatic approach, optional phrase-end breathing, stable resolutions, rhythm-preserving answer variation.

Each profile now exposes three **different musical source devices**, with named structural invariants and rejection tests: `PEDAL_GAP` / `DELAYED_BACKBEAT` / `CROSS_PULSE`; `MOTOR_LOCK` / `BREAK_AND_SHOUT` / `OCTAVE_INTERLOCK`; `CLIMB_RELEASE` / `LATE_SUSPENSION` / `BROKEN_ANSWER`. The devices are engine-internal research concepts, not yet user-facing options or authentic recreations of a particular band. The source-motif rhythm and every bar's device label are checked for consistency.

Configuration: root MIDI 24–60, 4 or 8 bars in 4/4, deterministic seed, variation 0–100. The variation value currently maps to five **documented discrete musical stages** (0, 1–24, 25–49, 50–74, 75–100) with 0 preserving exact repetitions. The independent `composeNovel` helper selects from at most 16 candidates against the last eight matching motifs to reduce short-term repetition; it reports a structural metric, not artistic quality. Neither mechanism is realtime safe or wired to a DAW. MIDI export uses standard format 0, 480 PPQ, fixed file tempo 120 BPM; future VST3 host-following tempo/sync is a separate gate.

## Build/test (local, no paid Actions)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure  # four local suites
```

For three optional diagnostic examples (not a 90-file listening assignment):

```sh
./build/ebm_preview previews
python tests/verify_midi.py previews/*.mid
```

Windows multi-config generators generally place the CLI at `build/Release/ebm_preview.exe`.

- Musical requirements: [`docs/MUSICAL-CONTRACT-v0.2.md`](docs/MUSICAL-CONTRACT-v0.2.md)
- Three original hand-authored target motifs: [`docs/COMPOSED-REFERENCE-RIFFS-v0.2.md`](docs/COMPOSED-REFERENCE-RIFFS-v0.2.md)
- Source evidence: [`docs/RESEARCH-EBM-MUSICAL-EVIDENCE.md`](docs/RESEARCH-EBM-MUSICAL-EVIDENCE.md)
- Implemented architecture and caveats: [`docs/ENGINE-IMPLEMENTATION-v0.2.md`](docs/ENGINE-IMPLEMENTATION-v0.2.md)
- Musical phrase-ending audit: [`docs/PHRASE-ENDINGS-AUDIT-2026-10-13.md`](docs/PHRASE-ENDINGS-AUDIT-2026-10-13.md)
- New compositional breadth audit: [`docs/GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md`](docs/GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md)
- Motif-lineage regression: [`docs/MOTIF-LINEAGE-AUDIT-2026-10-13.md`](docs/MOTIF-LINEAGE-AUDIT-2026-10-13.md)
- Test evidence: [`docs/QA-STATUS.md`](docs/QA-STATUS.md) and [quality audit](docs/QUALITY-AUDIT-2026-10-13.md)

Guidance: `challanger2000/125A-Engineering/START-HERE.md` and relevant standards. `main` is unchanged. v0.1 remains recoverable from Git history, but is **not** the accepted composition model.
