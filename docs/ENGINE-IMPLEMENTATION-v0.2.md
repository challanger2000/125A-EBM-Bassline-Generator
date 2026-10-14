# v0.2 music-engine implementation contract

## Important distinction
This is a newly built role-based **experimental composition system**, not music approved by a human listener. The three historical EBM interviews and rhythm studies in the research register motivate several approaches but do not scientifically validate the specific choices. Current style grammars and interval selection are **EMPIRICALLY TUNED / PROVISIONAL**. No genre-corpus statistics are claimed, and musical release gate is OPEN.

## Implementation (src/Composer.cpp)

- **Rhythmic grammar before notes:** each style constructs linked anchors, drive cells, pickups, answers and explicitly located silences relative to 4/4 kick references (0,4,8,12). No per-step Bernoulli/random density fill.
- **Expanded phrase devices:** three named *source procedures per style* (`Device` tag in every motif bar), each with an invariant rhythmic identity. Validation prohibits faked device labels, mismatched source placement or a mid-phrase device switch. These structures are experimental original composition choices, not transcriptions. See [`GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md`](GRAMMAR-VOCABULARY-AUDIT-2026-10-13.md).
- **Pitch relationships:** stable pedal events, chosen meaningful answers and directed chromatic tension with a one-semitone explicit target across bars (including 8-to-1 loop seam). Events retain stable motif IDs, roles and target links.
- **Phrasing:** MINIMAL composes a 1-bar cell; CLASSIC and DARK independently compose two-bar connected cells. Four/eight-bar arrangements reuse cells; no new random bar is filled from scratch.
- **Operations:** OMIT removes only a named DRIVE; ANTICIPATE moves a chosen anchor one sixteenth earlier **and vacates the old step**; ANSWER changes pitch of a designated response while retaining rhythm; REPEAT is an exact reuse. All are logged with target bar and source event ID.
- **Variation:** five tested musical stages (0, 1–24, 25–49, 50–74, 75–100) add controlled transformations in later arrangement bars while preserving the first source motif(s). Discrete, not a continuous-percent law. Fixed interval and velocity choices still require listening calibration.
- **Recent-result novelty:** standalone `Novelty.cpp` selects a seed through up to 16 attempts using the last eight same-style/length source motifs and a documented structural distance score; returns a failure flag when the threshold cannot be met. This score measures structural separation, not musical excellence.
- **MIDI safety:** full phrase is validated before SMF encoding. Bounds, monophony, positive gates, note-off before next note-on, event order and cross-bar resolutions checked. Original root-independent motif regenerated after transposition; no cumulative MIDI pitch clamp.
- **Realtime:** currently generation takes place in a standalone command-line program, not a host callback. **No realtime-safety or VST3 claim** until integrated and measured. The composing code allocates std::vectors and throws validation errors, therefore MUST NOT be run in VST3 `process()`.

## Known limitations / open follow-up

1. Three style grammars now contain nine named structural devices, but the event vocabulary remains constrained. Perceived authenticity and quality are **unverified**. The current bass instrument in audition WAVs is deliberately simple and is not a sonic target.
2. Raw seeds still sometimes produce identical or near-identical riffs because the musical grammars have finite combinations. A bounded source-motif novelty helper is now implemented and technically tested but NOT connected to a GUI or musical listening. See `docs/QUALITY-AUDIT-2026-10-13.md` for the exact 300-seed data and accepted/fallback semantics.
3. Musical skeleton remains fixed to sixteenth-note 4/4 / four-kick reference, for controlled evaluation. Additional drum styles or timebases must be musically justified, not added as feature inflation.
4. Gate and velocity numbers are provisional. No musical ratings, no audio comparison of actual EBM recordings, and no editorial verification of artist-specific transcription.
5. No VST3 host MIDI buses, Studio One, GUI, project recall, automation, zoom, DEMO licensing, macOS build or Windows executable has been implemented/tested for v0.2.
6. Development branch is `research/v0.2.0-musical-foundation`, not `main`; GitHub Actions remain unused until a musically promising baseline exists.

## Kick/answer accent interlock experiment (2026-10-14)

The final response at `variation>=75` in CLASSIC_EBM and DARK_ELECTRO now performs a **paired REACCENT** after the previously established phrase-form edits. It transfers MIDI velocity numbers between a defined quarter-note-kick anchor/resolution and a non-kick answering note. The first source cell, all MIDI onsets, note gates and pitches remain unchanged; the pair's integer velocity sum is preserved, but that is **not** perceived-loudness conservation. The transfer has a distinct operation type and original/updated velocity values in its provenance log; the independent lineage replay checks the exact roles, kick positions, event IDs, last-bar placement, variation threshold and both before/after values. A forged operation or undocumented velocity is rejected. See `docs/KICK-ANSWER-ACCENT-AUDIT-2026-10-14.md` for controlled baseline and A/B details.

Minimal Body retains its static hypnotic accents; this is intentional. The source MIDI and the entire phrase for variation below 75 are byte-equivalent to the preceding GitHub commit. Synthetic WAV audition exists but human groove preference remains **OPEN**.
