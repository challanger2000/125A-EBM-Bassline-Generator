# Musical phrase endings — controlled audit (2026-10-13)

**Status: technical behavior verified locally; musical usefulness UNVERIFIED. NOT VST3, NOT FINAL.**

## WENN -> DANN: why this change was necessary

**Problem:** The previous nine-grammar implementation forced a -1-semitone leading note on every MINIMAL source bar, every CLASSIC B bar, and both DARK source bars — irrespective of whether the musical purpose was hypnotic stability, forceful rest, or genuine forward chromatic tension. This was an implementation-imposed flourish, not a demonstrated EBM rule. A 120 BPM, kick-based sequence can be effective *without* ending each bar with a leading tone.

**Design decision (EMPIRICALLY TUNED; no claim of genre measurement):** Phrase-ending strategy follows a selected compositional device, and remains reproducible for a fixed seed. Multiple results remain possible without random per-step note scattering.

- **MINIMAL PEDAL_GAP:** a selected silence after the upper answer OR an explicitly tonic/pedal return on step 14/15; never an automatic chromatic leading tone. **MINIMAL DELAYED_BACKBEAT:** leave the bar ending open after a late melodic answer. **MINIMAL CROSS_PULSE:** retains the chromatic next-bar approach because rhythmic anticipation is the device's intention.
- **CLASSIC BREAK_AND_SHOUT B:** after the low anchor on step 12, deliberately leave the final two 16ths open. **MOTOR_LOCK** and **OCTAVE_INTERLOCK** retain B's chromatic pickup.
- **DARK LATE_SUSPENSION B:** a late low return is followed by silence at the last sixteenth; its A bar still resolves to the B-bar opening as specified. **CLIMB_RELEASE** and **BROKEN_ANSWER** retain the B-to-A chromatic pickup. A's internal summit neighbor and A-to-B resolution are not removed.

This does **not** claim silence is universally superior to chromatic motion. Different deliberate roles are meant to sound structurally distinct when compared in an identical playback context.

## Fixed-seed before/after measurements

Compiled the previous commit `4d33af4a076cb7069231ce5a4bc9071c704e8e2c` and the changed source with the same compiler and the same fixed sample: seeds `314159 + style*1019 + i`, i=0..299; root C, 8 bars, VARIATION=0. Counts below refer to **unmodified source motif bars**, not all eight arranged bars; MINIMAL has 1 source bar per seed, CLASSIC/DARK have 2. This isolates the composition grammar from later variations.

| Style | Previous bars with a chromatic *next-bar* approach | Revised | Revised bars ending with at least one silent 16th |
|---|---:|---:|---:|
| MINIMAL_BODY | 300 / 300 | 98 / 300 | 219 / 300 |
| CLASSIC_EBM | 300 / 600 | 198 / 600 | 102 / 600 |
| DARK_ELECTRO | 600 / 600 | 502 / 600 | 98 / 600 |

In a **separate** 300-seed sample (same fixed seeds, 8 bars, VARIATION=85), source-onset signature / full MIDI-byte diversity changed as follows:

| Style | Previous onset signatures | Revised onset signatures | Previous unique MIDI | Revised unique MIDI |
|---|---:|---:|---:|---:|
| MINIMAL_BODY | 73 | 86 | 234 | 243 |
| CLASSIC_EBM | 162 | 162 | 274 | 274 |
| DARK_ELECTRO | 91 | 91 | 289 | 289 |

Differences are a direct consequence of changed motif endings. **Neither ratio nor file-byte uniqueness is a musical preference score**. Rhythmic variety can increase with negative space; the current sample provides no proof of how catchy the results are. The same metrics must not be optimized without reference to listening and intended musical roles.

## Tests executed — exact scope

- GCC C++17 Release, warnings as errors: **build PASS**; CTest **3/3 PASS**: original Engine contract, source-device adversarial contract, and new cadence/rest contract.
- GCC AddressSanitizer + UndefinedBehaviorSanitizer Debug: **3/3 PASS**, with leak detection enabled.
- The cadence suite covers 3 styles × 300 fixed seeds × 2 lengths × 2 intensities = **3,600 compositions**, plus source event-ID mutation and false-closing-tone counterexamples. The 3,600 cases supplement the engine's pre-existing 3,000-generation test, which still passes.
- Existing 300-seed diversity tests remain unrelaxed. MINIMAL measured 86/300 distinct rhythm signatures and 243/300 different complete MIDI exports under the specified seed sample.
- Three new eight-bar previews, fixed 120 BPM, passed independent Python standard-library parser: no stuck notes, monophonic NoteOn/NoteOff, correct timebase, valid 8-bar ending. Preview note counts: MINIMAL 47, CLASSIC 67, DARK 43.
- Existing first-source source-event IDs are now checked independently of whether the former closing tension note exists. That source-ID mutation previously escaped validation on one DARK device when its B-to-A tension was intentionally absent; this regression is now caught.
- Windows-native compilation, Studio One, VST3 buses, lifecycle/GUI/automation and subjective musical approval remain **NOT VERIFIED**. CI/GitHub Actions have **not** been run.

## Acceptance still open

A/B playbacks should use the same seed, patch, kick, level and tempo. The harmonic/rhythmic difference cannot be treated as objectively better without human listening. Do **not** ask the product owner to audition dozens of seeds. Preselect at most three short, musically distinct paired examples, and compare with deliberate weak counterexamples. A MIDI-only generator's quality additionally depends on its destination synthesizer/envelope and musical context.
