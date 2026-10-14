# 125A EBM Bassline Generator

**Current status: experimental musical-engine v0.2.0, expanded grammar checkpoint. Not a VST3. Not release-ready.**

The engine develops original monophonic MIDI phrases using musical *roles* instead of the independent onset/pitch lottery used by v0.1. Each profile uses a different compositional grammar; every bar carries stable motif event IDs and explicit transformation records. A separate event-lineage audit now reconstructs each later bar from its original motif and named edits, rejecting undocumented pitch, velocity, gate, role or rhythm modifications. The three hand-authored v0.2 reference riffs are **specification fixtures**, not notes copied into the generator. No artist MIDI patterns are bundled.

## Three experimental style grammars with nine explicit devices

- `MINIMAL_BODY`: one-bar pedal, active rests, upper answer, either an intentional rest, a tonic pickup, or (in CROSS_PULSE) a chromatic return to the next bar. Eight exact repeats are allowed at VARIATION=0.
- `CLASSIC_EBM`: two-bar motor/response, deliberate pickups, optional fifth-degree pedal move, structured omission and anticipation.
- `DARK_ELECTRO`: two-bar contour question/answer, a targeted chromatic approach, optional phrase-end breathing, stable resolutions and scale-constrained responses whose upper contour rises above the next target. This last behavior was verified in the 2026-10-14 tonal audit.

Each profile now exposes three **different musical source devices**, with named structural invariants and rejection tests: `PEDAL_GAP` / `DELAYED_BACKBEAT` / `CROSS_PULSE`; `MOTOR_LOCK` / `BREAK_AND_SHOUT` / `OCTAVE_INTERLOCK`; `CLIMB_RELEASE` / `LATE_SUSPENSION` / `BROKEN_ANSWER`. The devices are engine-internal research concepts, not yet user-facing options or authentic recreations of a particular band. The source-motif rhythm and every bar's device label are checked for consistency.

Configuration: root MIDI 24–60, 4 or 8 bars in 4/4, deterministic seed, variation 0–100. The variation value currently maps to five **documented discrete musical stages** (0, 1–24, 25–49, 50–74, 75–100) with 0 preserving exact repetitions. **Bass/Kick accent interlock (2026-10-14):** at strong variation (75–100) the closing bar of CLASSIC_EBM or DARK_ELECTRO exchanges velocity emphasis between its on-kick bass anchor/resolution and a designated offbeat answering note, without changing onset, pitch, duration, note count or the sum of the two MIDI velocity numbers. MINIMAL_BODY and all lower variation bands retain their former MIDI output. This is an explicit, independently replayable REACCENT operation, not a per-note humanization lottery. It is an experimental groove decision rather than a proven audible improvement.

**Phrase-arc update (2026-10-14):** for positive variation, the last bar now supplies an intentional answer. On the higher levels the midpoint and the preparation for the ending receive named transforms; first source motifs are unchanged. This is a deterministic song-form hypothesis, not a validated EBM songwriting rule. The independent `composeNovel` helper selects from at most 16 candidates against the last eight matching motifs to reduce short-term repetition; it reports a structural metric, not artistic quality. Neither mechanism is realtime safe or wired to a DAW. MIDI export uses standard format 0, 480 PPQ, fixed file tempo 120 BPM; future VST3 host-following tempo/sync is a separate gate.

## Build/test (local, no paid Actions)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure  # seven local suites
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
- Dark Electro tonal-direction audit: [`docs/TONAL-CONTOUR-AUDIT-2026-10-14.md`](docs/TONAL-CONTOUR-AUDIT-2026-10-14.md)
- Bass/Kick accent-interlock experiment: [`docs/KICK-ANSWER-ACCENT-AUDIT-2026-10-14.md`](docs/KICK-ANSWER-ACCENT-AUDIT-2026-10-14.md)
- Motif-lineage regression: [`docs/MOTIF-LINEAGE-AUDIT-2026-10-13.md`](docs/MOTIF-LINEAGE-AUDIT-2026-10-13.md)
- Multi-bar phrase-form audit: [`docs/PHRASE-FORM-AUDIT-2026-10-14.md`](docs/PHRASE-FORM-AUDIT-2026-10-14.md)
- Test evidence: [`docs/QA-STATUS.md`](docs/QA-STATUS.md) and [quality audit](docs/QUALITY-AUDIT-2026-10-13.md)

Guidance: `challanger2000/125A-Engineering/START-HERE.md` and relevant standards. `main` is unchanged. v0.1 remains recoverable from Git history, but is **not** the accepted composition model.

## Offline GENERATE / Variation-level session (2026-10-14)

`ebm::v2::Session` is a standalone **control-thread-only** coordinator in
`include/ebm/Session.hpp`, not VST3, not realtime-safe, and not an audio callback.
It connects the existing motif composer and bounded novelty selector to a
reproducible generation workflow:

- `generate()` selects a source motif against the remembered recent results,
  retains the **actually selected seed** and returns its candidate count and
  `meetsNoveltyTarget` status. No guarantee of musical quality or uniqueness.
- `variate(level)` changes **the arrangement of the current source motif** without
  reselecting the seed. The range 0..100 still comprises five discrete levels;
  asking for the same effective level returns `false` instead of falsely
  claiming to create a new variation. `nextVariant()` separately cycles through
  four deliberate answer/omission/anticipation treatments and the canonical
  phrase without replacing the selected motif seed. At 0%, `nextVariant()`
  correctly does nothing; it never secretly changes intensity.
- `setRootMidi()` transposes the same composed pattern. Style/length changes
  intentionally require a fresh GENERATE; `clearCurrent()` retains history.
- `snapshot()` / `restore()` roundtrip the selected seed and up to 48 previous
  generation contexts, the selected alternative cursor, and atomic validation
  before restoring live state. Value-state schema v2 accepts valid v1 snapshots
  as canonical alternative 0; no binary VST3 state has been defined.
  The structure is a **local value snapshot**, not a finalized binary VST3
  preset/project schema.

Produce three inspectable eight-bar standard MIDI files (120 BPM, 4/4):

```sh
./build/ebm_session_demo previews/session
python tests/verify_midi.py previews/session/*.mid
```

Details, regression evidence, limitations and open gates:
[`docs/GENERATE-VARIATE-SESSION-AUDIT-2026-10-14.md`](docs/GENERATE-VARIATE-SESSION-AUDIT-2026-10-14.md).

## Repeated VARIATE button (2026-10-14, offline prototype)

- `Session::nextVariant()` cycles the **same chosen seed** through five inspectably
  different outputs at a nonzero intensity: canonical arrangement plus four
  explicitly logged musical changes (final answer, midphrase answer, omitted
  drive/alternative answer, and anticipation/response where needed).
- The first one/two source bars, style, key, phrase length and original motif
  remain locked. `validate()` independently reconstructs variations from
  typed edit records, rejecting unrelated changes or unsafe timings.
- At **0%**, VARIATE is neutral and reports no new result; repeated clicks never
  silently promote the intensity. Changing the strength resets the alternative
  cursor to canonical so the user's selection is predictable.
- Snapshot schema v2 stores a bounded alternative cursor (0..4). `restore()`
  handles v1 snapshots with canonical alternative and rejects malformed v2
  snapshots atomically. This is an **in-memory value contract**, NOT a VST3
  preset or project state implementation. No audio-thread allocations are allowed
  when this layer is eventually integrated into a plugin.

To export three tightly scoped original/VARIATE MIDI pairs:

```sh
./build/ebm_alternative_demo previews/alternatives
python tests/verify_midi.py previews/alternatives/*.mid
```

See [`docs/REPEATED-VARIATE-AUDIT-2026-10-14.md`](docs/REPEATED-VARIATE-AUDIT-2026-10-14.md).

## Independent rhythmic novelty screen (2026-10-14)

`composeNovel()` now requires **two independent, disclosed source-motif checks** against the last eight matching results:

- Existing structural onset/pitch score >= 4 (unchanged diagnostic definition).
- New rhythmic source-onset symmetric difference >= 2. Pure pitch or root changes with identical note-on positions are **not** called rhythmically new. Comparisons are based on the original one/two-bar motif, not later arrangements or user variations.

The selector remains deterministic, explores at most 16 seeds and returns `meetsTarget=false` when neither a sufficiently rhythmic nor structural candidate can be found. `NoveltyResult` and `GenerateInfo` also report `minimumRhythmicDistance` and `meetsRhythmicTarget`. If no candidate passes, the fallback maximizes rhythm difference first, then structural difference; **it is never silently labelled a success**.

This is not a score of catchiness, groove, or authentic EBM. No API is called from the audio thread. See [`docs/RHYTHMIC-NOVELTY-AUDIT-2026-10-14.md`](docs/RHYTHMIC-NOVELTY-AUDIT-2026-10-14.md).
