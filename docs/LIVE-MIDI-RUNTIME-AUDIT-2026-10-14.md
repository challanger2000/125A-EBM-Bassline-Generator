# 125A EBM — host-neutral real-time MIDI scheduler (2026-10-14)

**STATUS:** Experimental C++17 MIDI runtime module. NOT a VST3, NOT tested on Windows, NOT a Studio One or musical-approval claim.

## Why this implementation

The existing `compose()`, `composeNovel()` and `Session` implementations use vectors and can throw. They must never be called from the DAW audio callback. Following the **architectural separation**, not guitar-specific behavior, in the completed 125A Rifferator (`src/core/MidiRuntime.hpp` and `docs/ARCHITECTURE.md`): prepare an immutable MIDI pattern off-thread; run a fixed-capacity note scheduler on the host audio thread. Rifferator's Heavier7Strings policy, guitar articulation and degree mapping do not belong in EBM.

`ebm::live::prepare()` converts a validated, monophonic four- or eight-bar `Phrase` into a 128-position fixed-size table of note-on and note-off boundaries, preserving MIDI pitch, gate and velocity. Invalid input never mutates an already-prepared snapshot. The allocation-free, non-throwing `Runtime::process()` reads that table. Its consumer-provided sink must likewise be bounded/non-blocking and `noexcept`.

## Runtime contract

1. `Transport` tracks **absolute project PPQ** (quarters) and `tempo / sampleRate`; event scheduling is independent of callback block size. DAW stop or seek emits a matching NoteOff when a note is sounding. Sample on which the grid boundary is crossed receives the event; NoteOff precedes NoteOn on the same step, including the 8-bar seam.
2. `MidiNote` begins the bass phrase on an external NoteOn and ends it on **matching** NoteOff. Spurious NoteOff does not kill the phrase. Trigger input events are sorted on a bounded stack array; when offsets coincide, NoteOff is handled first. Trigger ownership and phase survive a prepared-pattern revision edit.
3. A revision change releases only the currently sounding bass note. It does **not** change the selected GENERATE seed, restart a held external MIDI trigger, or otherwise rewrite the selected riff.
4. A rejected outgoing NoteOff is retained and retried before further output notes. No `std::vector`, allocation, locks, file I/O, formatting, logging or dynamic exceptions in the audio callback.
5. The caller must publish an immutable pattern snapshot and retain it during each process call. This module does not implement cross-thread snapshot exchange, VST3 `ProcessData` bus ownership, VST3 state serializer, controller, GUI or native MIDI output routing. That adapter and host QA remain separate tasks.

## Tests actually run

- New `EBM_Live_MIDI_Scheduler` release suite: **PASS**; 45 matrices (3 EBM styles × 44.1/48/96 kHz × block sizes 1/31/127/257/512), matching source MIDI note timing and note-off order across two loops and early third-cycle playback. Includes deliberate empty/bad source, upper-limit MIDI duration, host stop, seek, wrong external NoteOff, live-edit revision without trigger reset and rejected sink NoteOff retry.
- Whole local GCC C++17 Release CTest: **13/13 PASS**; 12 previous engine suites unaffected.
- New suite under GCC AddressSanitizer+UndefinedBehaviorSanitizer with leak detection: **PASS**. The first full parallel sanitizer suite reached the execution time limit after 10/13 completed suites with no observed failures; the remaining heavy suites were not counted as passed from that truncated run.
- `src/LiveMidi.cpp` compiles with `-fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic -Werror`.
- Not yet executed: Windows native, Steinberg Validator/host adapter, Studio One with actual MIDI instrument, sample-rate changes mid-held-note, full external 125A Plugin Tester and real-time thread allocation instrumentation. No claims of musical quality based on these tests.

## Implementation boundary

**Never** call `compose()` / `composeNovel()` / `Session::generate()` / `Session::nextVariant()` or `toMidi()` from `process()`. Controller performs the heavy work before publication. The VST3 implementation should use the repo's own EBM source events, not import Heavier7Strings articulations, note-stop semantics or GUI layouts. Before VST3 release: test event-bus sink failures, host loops/seeks, absolute PPQ, sync switching, trigger offsets, active-note cleanup, automation, state save/restore, editor lifecycle, and Studio One instrument routing. All remain open.
