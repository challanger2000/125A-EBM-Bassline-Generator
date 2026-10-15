# Codex P1 repair tracking — 2026-10-15

Base binary: `b1d28792eb0da6471698488995b24a8adff5179b`
Working branch: `fix/p1-codex-qa-2026-10-15`
Release: **HOLD**; the base binary is untouched.

## P1-01 malformed input (code changed, NOT TESTED)
In `src/vst3/Plugin.cpp`, invalid single MIDI offsets/pitches now route through the already established bounded error-cleanup path instead of returning before it. The existing oversized-event path remains unchanged.

Needed: real Processor IEventList malformed-event → output NoteOff retry regression, plus host compatibility test for `kResultFalse`.

## P1-02 rejected trigger NoteOff (code changed, NOT TESTED)
In `include/ebm/LiveMidi.hpp`, input trigger ownership is cleared before attempting the bass NoteOff output, so a rejected output cannot cause the trigger to resume at the next process callback. Added a regression to `tests/live_midi_tests.cpp` covering one rejected NoteOff, retry, and no later NoteOn.

Needed: compile, run test, then full VST3 host stress.

## P1-03 coherent musical state publication (OPEN, no code change yet)
`Model::restore` publishes a new `live::Pattern` before publishing `root_`, `scale_`, `songStart_`, `gate_`, and `mode_`. `Processor` reads Song Start before the audio process acquires a new pattern, so merely bundling the three metadata values into `PatternExchange` without changing the PPQ read path will not close the race.

A safe change needs a **single real-time-visible prepared snapshot** containing the pattern and document-owned musical metadata (Root, Scale, Song Start, default Gate and Mode), together with a documented method to apply genuine host Gate/Mode automation without reverting a newly published state. The Processor must use Song Start from the SAME acquired snapshot that supplies the pattern. Then add a concurrent producer/audio-consumer regression checking full tuples instead of only pattern cells. This is a larger architectural change; no unverified partial fix has been made.

No new Actions runs have been requested or started for these commits. Do not label the repair branch PASS or FINAL before regressions, native host check and owner approval.
