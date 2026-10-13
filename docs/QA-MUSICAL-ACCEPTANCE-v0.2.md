# Musical acceptance gate v0.2: separate from build/MIDI QA

**Status: design only / NOT VERIFIED.** No Windows, Steinberg, host or musical PASS claim is authorized by this document.

## Composition tests (independent of implementation)
1. **Motif identity:** annotate an original one-bar or two-bar motif before generation (onset groups, contour, strong events, intended rests). Repeat/variation must preserve exactly the explicitly locked group. Intentional contrast must have a declared new role.
2. **Formal negative tests:** disallow an incoherent random note stream falsely tagged as a motif. A valid MIDI file with independent pitch draws is not sufficient.
3. **Musical transform evidence:** `OMIT` changes actual onset count at a named location, `ANTICIPATE` moves a note and vacates its prior position, `ANSWER` changes the contour while retaining the specified rhythm, `REPEAT` is byte-equivalent in the corresponding bar when requested.
4. **Kick relationship:** derive co-onset, anticipations, delayed responses and deliberate silences from both bass and kick event grids. There is no globally correct collision percentage. Check the intended relation rather than a generic count.
5. **Tension -> release:** annotate a non-scale approach tone's destination and verify the destination actually occurs in the expected register/timing.
6. **User variants:** when the user selects VARIATE, provide a genuinely different phrase, unless an explicit repeat option was selected. Identity invariants must remain intact.
7. **Style invariants:** differences must be detectable by grammar/phrase structure; a profile that differs only in random weights or note count fails specification conformance.
8. **Host/MIDI correctness:** carry forward all strict v0.1 technical tests: seed identity, monophony, event order, 4/8 bars, valid 0..127 pitches, correct NoteOffs, transpose/return, state roundtrip once VST3 exists. Regressions cannot be fixed by removing checks.

## Independent technical measures (diagnostics, never substitutes for listening)
- normalized onset bitmap and rhythmic autocorrelation: does a motif recur?
- role-specific onset and accent positions against the kick context;
- stable versus changed intervals under transformation;
- diatonic/chromatic target tracking and contour closure;
- distances between start, answer and repeated phrase; distribution alone does not prove musical identity;
- candidate duplicates and near-duplicates over many seeds; generation uniqueness is not itself musical quality.

## Human musical selection without dumping QA onto the user
- **Internal development:** automatically validate all technical cases; manually preselect and document at most THREE deliberately different, original musical proof examples, not 90 arbitrary seed outputs. Review one failure from each grammar internally and adjust specific musical rules.
- **Comparable playback:** fixed beat and a neutral mono bass synth, then one representative tight-filter/envelope synth. Level/velocity perception and gate effects must be noted. Supply a paired version without beat for pitch/rhythm inspection.
- **Listening questions:** Is the cell memorable after one or two plays? Does it make musical sense with the kick? Is there a coherent identity through the variation? Do three profiles yield qualitatively different ideas, not just faster/slower note chatter? Would a songwriter plausibly keep the riff?
- **Honest evidence policy:** independent listening is presently unavailable; any audition score is OPEN. Do not falsely claim 'musical PASS' because metadata, onset counts, a green CTest or an assistant's description looks good.
- **Release gate:** user preference may guide final sonic approval, but the product owner must not be forced to review dozens of tracks or repair our technical design. No VST3 GUI/packaging work before the musical proof is credible.

## Counterexamples the next model must reject
- Random onsets/pitches satisfying only a numeric density quota.
- An 'EBM style' selected solely by a different table of probabilities.
- A variation that only alters one arbitrary velocity or pitch, without a role-dependent reason.
- An anticipation with an un-vacated strong beat (often just extra density).
- A chromatic note labelled tension but not followed by a specified target.
- An unconditional end-of-bar tonic that spoils a deliberately unresolved/hypnotic phrase.
- Variation mode producing exactly the same result without explicitly telling the user no change occurred.
- Strong kick/bass clashing declared a fault solely because onset times coincide: simultaneous hits can be intentional.

## Current phase status
- Source-based theory and original musician workflow descriptions: **DOCUMENTED**.
- Genre-specific statistical patterns: **NOT MEASURED**.
- 3 original, independent gold-standard phrase fixtures: **TO AUTHOR / REVIEW**.
- New musical engine / MIDI proof samples: **NOT IMPLEMENTED**.
- Real listener comparison / musical quality: **NOT VERIFIED**.
- CI/Steinberg/DAW: **NOT RUN for v0.2**.
