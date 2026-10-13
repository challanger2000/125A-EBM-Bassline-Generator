# Musical contract – exploratory v0.1.0

## Scope / evidence

This is a **rule-based hypothesis**, not a transcription of any artist, not trained on copyrighted tracks, and not proven to produce musically outstanding riffs. Broad historic EBM traits motivate the architecture, but no claim is made that preset weights are measured from a representative corpus. Values below are **EMPIRICALLY TUNED / PROVISIONAL** until blind listening and reference studies.

## Candidate styles

- **MINIMAL_BODY**: short one-bar pedal-centred motifs, 7–9 onsets, limited pitches, restrained variation; deliberate repetition is valid.
- **CLASSIC_EBM**: two-bar energetic motifs, 10–12 onsets/bar, four-on-floor anchors and 16th-note pushes, octave/fifth movement, short gate.
- **DARK_ELECTRO**: two-bar contour-oriented motifs, 6–8 onsets/bar, longer note lengths, minor/chromatic tension and explicit resolution.

These are our **provisional musical design labels**, not formal genre definitions or reconstructions of specific bands.

## Phrase behavior

- 4 or 8 bars, 16 steps per bar, 4/4. Generator is deterministic from (style, bars, rootMidi, seed, variation).
- Bar 1 creates a unique motif. Two-bar profiles add a distinct response bar.
- Repetitions retain rhythmic onset positions and most notes; developments affect later sections only; the last note of a modified ending lands on the tonic.
- Variation 0 yields exact motif repetitions. Intensity >0 changes at most two pitches/velocities in a development bar (plus optional cadence), but can choose no changes for a specific seed.
- Generated MIDI is a **monophonic** single-channel phrase (note-off before note-on on shared ticks).
- Key changes transpose the exact phrase, not recompose it.

## Evaluation before VST3

Automated checks: fixed-seed reproducibility, legal MIDI bytes, no overlapping notes, 4/8-bar boundaries, distinct outputs, transposition identity and negative parameter tests. Musical acceptance gate is **OPEN**: listen to all 90 8-bar MIDI examples using identical synth/drum material and rated blinded for groove, stylistic relevance, variation, non-triviality and usefulness; save failed examples as frozen regression references, do not only retune weights until tests go green.

## Known limitations

Not a VST3, no DAW sync, no user-editable grid, no MIDI-host routing, no GUI, no hand-reviewed audio references, no musician listening scores. Explicit 120 BPM in stand-alone MIDI files only; when imported, DAW may adapt tempo. Testing on Studio One Windows remains OPEN. Style distinctness here is by design constraints, not verified in blind perception tests.
