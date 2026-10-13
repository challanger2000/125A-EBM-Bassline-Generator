# EBM phrase-endings: controlled audio A/B experiment (2026-10-13)

**Gate:** technical audio-comparison controls PASSED locally. **Musical preference, rhythmic groove and perceived EBM authenticity: NOT VERIFIED.** Research prototype only; no VST3/DAW/Windows claims.

## Question, independent of the engine implementation

**Does omitting an automatic chromatic note near a barline produce a more usable musical phrase for the selected device?**

The changed engine makes some earlier compulsory pre-downbeat pickups optional or intentionally silent. Neither *more* nor *fewer* notes is inherently better; the question requires listening under controlled conditions. The previous automated rhythm-diversity counts cannot answer it.

## Exact source and treatments

- Previous version: `4d33af4a076cb7069231ce5a4bc9071c704e8e2c` (nine devices, compulsory end notes).
- Current version: `4afdf06818a3be596c24663c0e9de4a07a575b6c` (device-specific endings).
- Instrumentation: compile both snapshots as independent C++17 builds, export one eight-bar MIDI from each with *identical* style, seed, root MIDI=36, VARIATION=0, 4/4 120 BPM. No random candidate selection, no changes to the synth patch or drum pattern.
- Riff 01: MINIMAL_BODY, seed **2**, device PEDAL_GAP, old 48 vs new 40 MIDI notes, one 16th-note pickup removed on step 14 of **every** bar.
- Riff 02: CLASSIC_EBM, seed **1**, device BREAK_AND_SHOUT, old 68 vs new 64 notes, step-15 pickup removed on the **four** B bars.
- Riff 03: DARK_ELECTRO, seed **1**, device LATE_SUSPENSION, old 48 vs new 44 notes, step-15 pickup removed on the **four** B bars.

**Controlled difference:** the new notes are a subset of the old notes. Every retained note preserves its original *start, duration, pitch, and velocity*. No other note changes are accepted by `tools/paired_audition.py`. This is a concrete one-factor comparison of end-of-bar events, not a test of the entire musical engine.

## Neutral auditory context

- 44.1 kHz mono, 16-bit PCM; **16.40 seconds per clip** (eight bars at 120 BPM plus 0.4s tail).
- Same synthetic mono bass: two sawtooth oscillators with fixed 0.6% detune, supporting sine, single-pole low-pass, 7 ms attack, short release; *not* a commercially representative bass patch.
- Same fixed 4-on-floor synthetic kick on each beat and identical deterministic offbeat synthetic hats. No samples of artists.
- Bass stem RMS is equalized *within each pair* to the geometric mean of both original stem RMS values; measured numerical difference after normalization <0.001 dB, while drums are bit-identical and unscaled. No limiter or peak normalization; peaks stay below -6 dBFS in these examples.
- **Caveat:** bass stem RMS matching does not guarantee perceptual loudness matching; content, number of attacks and temporal masking can still affect impression. Different styles are *not* matched to one another because the comparison is only within a style pair.
- Presentation is blinded with fixed mixed A/B assignment (one pair has old in B). The answer key is kept in a separate text file in the optional ZIP. The generator code itself is not claimed to have performed subjective listening.

## Reproducibility and negative controls

1. C++ Release builds and CTest were rerun on both snapshots. Old: 2/2 CTest PASS. Current: 3/3 CTest PASS. No GitHub Actions run.
2. Six MIDI files validated with the independent `tests/verify_midi.py`: 6/6 PASS, note on/off balanced, exact 8-bar end and 120 BPM tempo.
3. All retained notes in old and new matched exactly, all removed note-ons located only at bar-local 14 (Minimal) or 15 (Classic/Dark); counts 8, 4 and 4, respectively.
4. Rendering twice yielded **6/6 byte-identical WAV outputs**. Every clip is exactly 723,240 mono samples, no clipping; the bit-identical drum context is constructed once per invocation.
5. Adversarial test: an extra alteration of the first non-ending note's pitch, with both NoteOn and NoteOff still valid, was **rejected** as an unrelated note change. The negative sample is not distributed.
6. The automated checks prove fair comparison conditions, not which version sounds better. Musical acceptance remains **OPEN** until a level-aware auditory comparison is completed.

## Bounded listening protocol, not a 90-file review

At most **three pairs** (six clips), each 16.4s. One comparison per style is a *screening experiment*, not a statistically powered musical preference study. The listener can mark `old/new/no clear preference` for rhythmic propulsion, memorable phrase identity, useful negative space, and compatibility with the kick. Preserve raw results even if they contradict the expected direction. No broad engine architecture should be revised on the strength of one clip alone.

### Next engineering gate

If the A/B comparison favors the new version for a given device, freeze that device and investigate a second independent seed. If the preference is weak, mixed or favors the old ending, retain the option; investigate the musical context (synth envelope, kick rhythm) rather than enforcing one type of cadence globally. Do not turn a CTest PASS or a note-count reduction into a claim of artistic success.
