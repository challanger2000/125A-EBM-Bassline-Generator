# Three original EBM target compositions — musical proof v0.2

**Scope:** explicitly authored, structured musical target fixtures — **not generator output**, not original-artist transcriptions, and not proof of listening quality. Files are used to derive and test a future role-based MIDI composition engine. Prior probabilistic v0.1 remains a technical comparison, not an approved musical model.

All use 4/4, **120 BPM**, eight bars, steps 0–15 per bar (sixteenth-grid), and kick at steps 0/4/8/12. The authoritative pitches, durations, velocities, note roles, answer/approach targets and transformations are stored in `fixtures/v0.2/`.

## MINIMAL BODY — Pedal / Empty Space (C2)

- **One-bar source A:** step/pitch: `00 C2, 02 C2, 05 C2, 08 C2, 10 G2, 14 B1`.
- **Musical intention:** deliberate kick/bass coincidence at 0 and 8; silence at kick 4; fixed fifth answer at 10. B1 at 14 explicitly approaches next bar's C2 at 0.
- **Eight-bar form:** `A A A A A A A A`. Repetition is deliberate, not an error; variation is not mandatory.

## CLASSIC EBM — Drive / Displacement (C2)

- **Bar A:** onsets `0 2 3 4 7 10 12 14 15`; G2 fifth answers at 3/10, Eb2 at 14. Step 7 is a pickup, leaving kick 8 without a simultaneous bass onset.
- **Bar B:** onsets `0 3 4 6 7 9 11 12 15`; selected upper answers and end-bar B1 lead to next A's C2.
- **Eight-bar form:** `A B A B_OMIT A B A_ANTICIPATE B`.
- **OMIT:** in bar 4 remove only the interior DRIVE at step 9. **ANTICIPATE:** in bar 7 move A's C2 step-12 event to **step 11**, explicitly vacating step 12. Moving and deleting are required, rather than merely changing a velocity.

## DARK ELECTRO — Directed Question / Answer (A1)

- **Bar A contour:** `A1 C2 E2 F2 E2 D#2`. The F2 resolves down to E2; the final D#2 approaches E2 at the next bar's first step.
- **Bar B contour:** `E2 G2 E2 C2 B1 A1`.
- **B_ANSWER:** keep **all rhythmic onset positions**, changing two answer tones (G2→F2, C2→D2). The stable E2 entrance and A1 ending are untouched.
- **Eight-bar form:** `A B A B_ANSWER A B A B_ANSWER`.

## WENN → DANN checks

1. **WENN** an event is marked TENSION, **DANN** its required RESOLUTION/ANCHOR must exist at the written destination, including across the loop boundary.
2. **WENN** ANTECIPATE is chosen, **DANN** the old onset is vacated, the new slot was empty, the role is updated and no overlap is introduced.
3. **WENN** OMIT is chosen, **DANN** it removes a designated nonessential DRIVE event while leaving the motif recognizable.
4. **WENN** ANSWER is chosen, **DANN** the rhythm is held but the named melodic response really changes.
5. **WENN** a minimal one-bar cell was intentionally designed for repetition, **DANN** it may repeat exactly over eight bars without a forced fill.
6. **WENN** a test catches a formatting-correct MIDI file with no coherent motif function, **DANN** that file fails musical-contract acceptance.

## Evidence / outstanding gates

The three authored event fixtures have been checked locally for non-overlap, bounds, motif operations, target resolutions and loop length. Six deliberately broken examples were correctly rejected. Exported **format-0 MIDI** was also re-parsed by the independent `mido` MIDI library: MINIMAL BODY 48 notes, CLASSIC EBM 71, DARK ELECTRO 48; exactly 8 bars per file, note-on/off matched. A simple synthetic bass/kick/hat WAV for each target was created as a playback guide; it is **not the eventual synthesizer sound**. These audio previews are not committed to this Git research branch.

**NOT VERIFIED:** musical attractiveness, EBM genre authenticity as perceived by listeners, independent musician review, VST3/Studio One, Windows build. No GitHub Actions were run. Original artist recordings or transcriptions are neither copied nor bundled.

Next: build a **new role-and-motif-based composition grammar** able to invent independent rhythmic/pitch structures that satisfy these behavioral contracts. Copying these three fixtures or randomly perturbing them is *not* accepted as completing that requirement.