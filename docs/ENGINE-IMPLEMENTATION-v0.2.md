# v0.2 music-engine implementation contract

## Important distinction
This is a newly built role-based **experimental composition system**, not music approved by a human listener. The three historical EBM interviews and rhythm studies in the research register motivate several approaches but do not scientifically validate the specific choices. Current style grammars and interval selection are **EMPIRICALLY TUNED / PROVISIONAL**. No genre-corpus statistics are claimed, and musical release gate is OPEN.

## Implementation (src/Composer.cpp)

- **Rhythmic grammar before notes:** each style constructs linked anchors, drive cells, pickups, answers and explicitly located silences relative to 4/4 kick references (0,4,8,12). No per-step Bernoulli/random density fill.
- **Pitch relationships:** stable pedal events, chosen meaningful answers and directed chromatic tension with a one-semitone explicit target across bars (including 8-to-1 loop seam). Events retain stable motif IDs, roles and target links.
- **Phrasing:** MINIMAL composes a 1-bar cell; CLASSIC and DARK independently compose two-bar connected cells. Four/eight-bar arrangements reuse cells; no new random bar is filled from scratch.
- **Operations:** OMIT removes only a named DRIVE; ANTICIPATE moves a chosen anchor one sixteenth earlier **and vacates the old step**; ANSWER changes pitch of a designated response while retaining rhythm; REPEAT is an exact reuse. All are logged with target bar and source event ID.
- **Variation:** 0 repeats untouched cells; >0 provides a guaranteed edit in a later bar (not the source motif); at >=60/100 and 8 bars additional answer revision is possible. Fixed interval and velocity constants need listening calibration.
- **MIDI safety:** full phrase is validated before SMF encoding. Bounds, monophony, positive gates, note-off before next note-on, event order and cross-bar resolutions checked. Original root-independent motif regenerated after transposition; no cumulative MIDI pitch clamp.
- **Realtime:** currently generation takes place in a standalone command-line program, not a host callback. **No realtime-safety or VST3 claim** until integrated and measured. The composing code allocates std::vectors and throws validation errors, therefore MUST NOT be run in VST3 `process()`.

## Known limitations / open follow-up

1. Three music grammars still operate in a constrained event vocabulary. Perceived authenticity and quality are **unverified**. The current bass instrument in audition WAVs is deliberately simple and is not a sonic target.
2. Random seeds can sometimes produce the same riff because a finite musical grammar has finite combinations. Initial raw diagnostic group of 90 seeds produced at least 85 different pieces; the future GENERATE button needs a bounded last-result novelty gate (without endless reroll loops), and tests for near-duplicates not just byte equality.
3. Musical skeleton remains fixed to sixteenth-note 4/4 / four-kick reference, for controlled evaluation. Additional drum styles or timebases must be musically justified, not added as feature inflation.
4. Gate and velocity numbers are provisional. No musical ratings, no audio comparison of actual EBM recordings, and no editorial verification of artist-specific transcription.
5. No VST3 host MIDI buses, Studio One, GUI, project recall, automation, zoom, DEMO licensing, macOS build or Windows executable has been implemented/tested for v0.2.
6. Development branch is `research/v0.2.0-musical-foundation`, not `main`; GitHub Actions remain unused until a musically promising baseline exists.
