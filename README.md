# 125A EBM Bassline Generator

**Current status: musical-engine research prototype v0.2.0. Not a VST3. Not release-ready.**

The engine develops original monophonic MIDI phrases using musical *roles* instead of the independent onset/pitch lottery used by v0.1. Each profile uses a different compositional grammar; every bar carries stable motif event IDs and explicit transformation records. The three hand-authored v0.2 reference riffs are **specification fixtures**, not notes copied into the generator. No artist MIDI patterns are bundled.

## Three experimental grammars

- `MINIMAL_BODY`: one-bar pedal, active rests, upper answer, chromatic return to the next bar. Eight exact repeats are allowed at VARIATION=0.
- `CLASSIC_EBM`: two-bar motor/response, deliberate pickups, optional fifth-degree pedal move, structured omission and anticipation.
- `DARK_ELECTRO`: two-bar contour question/answer, two targeted chromatic approaches, stable resolutions, rhythm-preserving answer variation.

Configuration: root MIDI 24–60, 4 or 8 bars in 4/4, deterministic seed, variation 0–100. 0 means literal musical repetitions; positive values trigger typed transformations. At 60+ intensity another answer may change in eight-bar phrases. Intensity is *not* a probability of isolated per-step mutation. MIDI export uses standard format 0, 480 PPQ, fixed file tempo 120 BPM; future VST3 host-following tempo/sync is a separate gate.

## Build/test (local, no paid Actions)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
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
- Test evidence: [`docs/QA-STATUS.md`](docs/QA-STATUS.md)

Guidance: `challanger2000/125A-Engineering/START-HERE.md` and relevant standards. `main` is unchanged. v0.1 remains recoverable from Git history, but is **not** the accepted composition model.
