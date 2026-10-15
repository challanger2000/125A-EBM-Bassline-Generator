# Trigger Notes Guide (development branch, 2026-10-15)

Source baseline `207176e0bfc941addbff1fad7f9316447c44eed7`. The previous successful C2/Global Gate/Chord version remains the fallback; no changes to `main`.

The read-only `TRIGGER NOTES` strip lives at x=210..542, y=190..214: clear of the BAR buttons at x>=550 and STEP labels at y>=222. The existing ROOT and SCALE selectors automatically refresh it through the established controller redraw. Notes reflect the **actual** `edit::inScale` pitch-class set used by the existing chord-trigger engine. It does not filter MIDI input; off-scale notes still resolve to the nearest in-scale degree as before. The display lists pitch classes, so trigger notes may be played in any valid MIDI octave.

Examples:

- KEY C2, MAJOR: `C D E F G A B`
- KEY C2, NATURAL MINOR: `C D Eb F G Ab Bb`
- KEY C2, MINOR PENTATONIC: `C Eb F G Bb`
- KEY C2, CHROMATIC: all 12 pitch classes

For common flat-key scales the strip prefers flat pitch-class labels; for sharp-key scales it prefers sharps. Enharmonic MIDI notes are equivalent.

No changes to MIDI scheduling, note choice, global gate, keys, scale IDs, parameter IDs, serialisation, labels of existing controls, GUI dimensions or logo resources. GUI policy tests assert example displays, exhaustive 10-scale x 37-root pitch-class consistency, and static non-overlap geometry. Local 18/18 CTest PASS; Windows VST3, validator and DAW interaction remain separately verified gates.
