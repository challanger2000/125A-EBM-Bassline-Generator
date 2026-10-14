# Dark Electro — directed tonal contour regression, 2026-10-14

**Status: local C++ contract verification only. Musical listening acceptance remains OPEN. Not a VST3, not a release.**

## Failure hypothesis -> independent counterexample

Prior v0.2 Dark Electro grammar independently selected a target interval from {3,5,7} semitones and a summit from {7,10,12}; this allowed a nominal upward journey in the first motif to have a **summit equal to its destination**. The later answer was computed with `max(target+2, candidate from {7,10,12})`, which produced **+9** whenever target=+7 and candidate=+7. The grammar intends the *natural-minor* subset {0,3,5,7,10,12} for non-chromatic notes; +9 is outside this declared palette and was **not** marked as a chromatic approach to a target.

These are specific violations of the declared melodic intention, **not proof** that +9 is intrinsically a bad musical note in all EBM compositions. Deliberate raised-sixth color could be a separate future grammar if coupled to an appropriate melodic function and documented as such.

## Baseline diagnosis and correction

- Reference baseline: GitHub branch `research/v0.2.0-musical-foundation`, commit `01a6f40c4d6d055c047f7149d1a1be3347655c1c`.
- Baseline 1,000 Dark Electro fixed seeds, bars=8, variation=0, root MIDI 36: **119** seeds produced a B upper-answer +9, **122** had source summit <= the B resolution target.
- New grammar selects the summit from {7,10,12}, replacing an insufficient height by +10; similarly chooses an upper response from that explicitly documented pitch set, rather than synthesizing +9 through arithmetic. The answer variation now toggles between deliberate +10 and +12 variants instead of reverting to an answer at/below a +7 destination.
- A separate validation path rejects a Dark Electro non-TENSION event outside the palette; a chromatic TENSION still requires an explicit destination and one-semitone target distance. B answers must exceed their B resolution target. The first source summit must also be above the B resolution target.
- All selections are **EMPIRICALLY TUNED**, not measured artist presets.

## Measured reproduction

| Diagnostic | Previous | Corrected |
|---|---:|---:|
| Unmotivated answer +9 across 1,000 Dark seeds | 119 | **0** |
| First summit not higher than B destination across 1,000 Dark seeds | 122 | **0** |

**Preservation:** exactly the same 250 seeds x 2 lengths (4/8) x 5 variation values = **2,500 configurations/style** for MINIMAL_BODY and CLASSIC_EBM. Compared full MIDI byte streams with a streaming FNV-1a-64 digest: old/new digests match **5635722531446343602** (Minimal) and **12431501829073466943** (Classic). Dark Electro intentionally changes. This tests numeric sequence equivalence, not musical acceptance.

**Build:** local Linux GCC C++17 Release with `-Wall -Wextra -Wpedantic -Werror` for core/new test: **5/5 CTest passed**. AddressSanitizer and UndefinedBehaviorSanitizer Debug: **5/5 CTest passed**. The older local engine fixture used for repeatability testing was updated to the previously documented five variation stages (without committing that local fixture); other existing tests were unchanged. The repository's exact HEAD test source was reviewed but was not independently run byte-for-byte via GitHub Actions. New dedicated `EBM_Directed_Tonal_Contour` suite checks **4,000** Dark compositions (400 seeds x 2 lengths x 5 variations), the full non-tension pitch palette, contour and answer destination, plus 3 adversarial mutations.

**MIDI cross-check:** newly generated 3 style-preview files pass independent `tests/verify_midi.py` NoteOn/Off, monophony, 8 bars, 120 BPM. A seed=5 8-bar old/new Dark comparison also passes for both MIDI exports, with **47 notes in each** and **16 pitch alterations**; note-on timing, note-off timing and velocities are unchanged. The accompanying guide WAVs share exactly one synth, drum pattern and level-normalized bass stems. No listening ratings have been collected.

## WENN -> DANN acceptance

1. IF a Dark melody is specified as **ascending to a summit**, THEN its summit must be strictly higher than the named answering target.
2. IF a B-bar note is a **scale-tone answer**, THEN use a named degree from its declared pitch palette; a chromatic deviation requires an explicit and resolved musical role, not accidental arithmetic.
3. IF a later answer is varied, THEN it retains a functional position above the same target and an audible pitch change.
4. IF a change concerns only Dark, THEN Minimal/Classic must remain identical at the tested settings.
5. IF automated verification succeeds, THEN musical acceptance remains **NOT VERIFIED** until a listener-based comparison establishes usefulness in a production context.

## Boundaries

This is a precise tonal-consistency fix, not a fully validated composer. Other EBM modes, non-Aeolian minor patterns, major-key riffs, chromatic approaches with context-specific voice leading, professional sound design, real DAW integration and independent musician judgments are future gates. No action workflow or Windows host build was run.
