# Composition contract v0.2 (DESIGN ONLY)

**Purpose:** produce a distinctive, independently authored EBM bass **motif** with coherent rhythmic drive, a memorable pitch gesture and meaningful controlled development. This specification is not a MIDI-generation implementation or proof of artistic success.

## Pre-code decision: discard the musical method of v0.1
- v0.1's `composeBar` samples onset positions individually to meet a note count, without a kick reference or structural rhythmic roles.
- v0.1's `intervalFor` samples per-note pitch independently from style-specific weighted sets; an occasional last-step tonic is not voice-leading.
- v0.1's `develop` only replaces one or two later pitches/velocities and does **not** change onset/rhythm; this cannot perform the documented note-removal or timing-displacement process.
- v0.1's style distinction mainly changes distributions and event count, not how a musical idea is built.
- Therefore existing v0.1 code is **archived as technical baseline only**, not an approved musical substrate. Keep its MIDI validity/seed tests as reusable infrastructure, not its composition logic.

## Model before programming
Every event carries:
- grid position / offset / gate / MIDI pitch / velocity;
- a **musical role**: `ANCHOR`, `DRIVE`, `PICKUP`, `ANSWER`, `TENSION`, `RESOLUTION`, or intentional `REST`;
- a parent motif ID and source event ID where it is derived/varied.
Each motif carries a rhythmic fingerprint, interval-contour fingerprint, register envelope, kick-reference pattern and an explicitly defined conclusion.

## Required generation stages
1. Select an idiom-specific **composition grammar** and phrase intent, not an event count.
2. Choose an existing abstract structural device (pulse/pickup, gap-and-answer, repeated pedal with upper fixed response, delayed entry, etc.) and a kick context; generate the **rhythmic skeleton** as linked onset-rest groups.
3. Assign pitch roles in context: stable anchors, identifiable contour, accent-target intervals, occasional directed chromatic passing tone only when a target note is scheduled. Avoid calling arbitrary out-of-key tones 'tension.'
4. Place note gates and accents relative to musical roles and the kick. Do not scatter arbitrary velocity accents independently.
5. Build larger forms by reusing and transforming motif *objects*; never compose subsequent bars completely independently unless that is a declared section contrast.
6. Candidate validation and rejection. If a candidate violates the selected formal goal, backtrack to its structural choice, not cosmetically patch a random interval.
7. Emit a valid monophonic MIDI representation, with host-preserving note-off handling in the future VST3. Timing and pitch transformations must leave held-note cleanup deterministic.

## Three provisional grammars (not artist presets)
**MINIMAL BODY:** 8/16/32-step loops, hard repetition permitted, a stable rhythm with tension-producing pickups and rests, narrow motif economy, one or two strong pitch gestures. 4/8 bars may consist of identical repeats; NEVER force an 'ending fill' on a minimal hypnotic cell.

**CLASSIC EBM:** 16/32-step rhythmic grammar, high motor drive with deliberately placed accents/interruptions and interval answers. Strong rhythm has to be understood with an explicit kick template; not automatically 10-12 hits per bar. Rhythm transformation can remove, advance or delay *designated* events while preserving the recognizable pulse.

**DARK ELECTRO (PROVISIONAL):** 16/32-step call/answer, more audible directed pitch contour, chosen dissonant neighbor/passing notes with a written target/resolution and deliberate varied gate lengths. Distinctness from classic EBM needs listening evidence; label may be dropped.

These are distinct *procedures* in the specification; assigning each a set of random weights does not implement them.

## Typed musical transformations and their preconditions
- `OMIT`: remove an interior drive note if the remaining pulse retains identity. Forbidden if it deletes the only unique motif anchor.
- `ANTICIPATE`: shift a designated note ahead by one subdivision **and vacate** the expected strong position, creating a measurable pickup/syncopation relation. Check adjacent gate/same-pitch retrigger legality.
- `DELAY`: move a note to a later empty slot without destroying the bar's return or violating the kick-interlock intent.
- `ANSWER`: retain the rhythmic signature while substituting an explicitly contour-compatible response, not an independent random pitch.
- `PEDAL_MOVE`: change the low anchor while keeping a chosen upper response stable if the target harmony permits it (not a blanket transpose).
- `CADENCE`: place an approach and target across the barline only when the style actually requires a resolved ending; do not auto-enforce tonic on each bar.
- `REACCENT`: shift dynamic emphasis between two designated roles while preserving timing/pitch identity.
- `REPEAT`: exact reprise is a valid deliberate transformation when the selected idiom benefits from hypnosis.

## Observable 'WENN -> DANN' contracts
1. IF a rhythmic cell is chosen, THEN its accented weak positions and following rests must be derivable from its skeleton, rather than emerge by chance.
2. IF a variation is requested, THEN preserve the named fingerprint and perform one identifiable transformation; otherwise return an explicit no-change, not a misleading 'new variation'.
3. IF a chromatic tension event occurs, THEN its role and target resolution must be present in the motif plan.
4. IF a bass note anticipates the next kick, THEN both kick timing and displaced bass-note timing must be known and inspectable.
5. IF a minimal profile deliberately repeats, THEN do not force filler or changes just to make eight exported bars appear 'complex.'
6. IF pitches transpose, THEN the motif's interval relationships, note-ons/offs, motif links, and loop seam remain intact (avoid destructive clamps).
7. IF the musical form is selected as 4/8 bars, THEN its arrangement must be defined before rendering MIDI: e.g. A A A A' or AB AB A B' according to idiom. Forms are examples, not mandatory universal rules.
8. IF a generated line is legal MIDI but cannot express a coherent motif fingerprint, THEN mark it MUSICALLY REJECTED independent of technical PASS.

## Parameter decisions deferred pending evidence
No universal note-density, humanize amount, swing percentage, fixed BPM, pitch register, mandatory tonic cadence, chord-progression generator, or 'style authenticity' is specified on the strength of one artist interview or one fan tab. A MIDI-only plugin cannot guarantee the listener's result regardless of synthesizer patch.

## Intended next milestone
**Before writing v0.2 engine code:** author and validate a few explicit original rhythm/pitch/gate/velocity target phrases, with typed operations showing expected before/after, and prove the results can be distinguished by their musical *structure*, not just by their onset count.
