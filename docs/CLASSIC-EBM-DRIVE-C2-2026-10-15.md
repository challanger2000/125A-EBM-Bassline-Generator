# 125A EBM Bassline Generator — Classic EBM Drive and C2 default

Date: 2026-10-15 · Experimental development only; **not owner-approved Final**.
Source baseline: `research/v0.2.0-musical-foundation` at `d3ca7f29bb93b43d44974db3ccdafa3628ade458`.

## Musical problem (user Studio One feedback)

The original Classic EBM is the best-performing style, but repeated GENERATE
calls do not reliably yield driving, forceful EBM basslines. The user requested
motoric eighths, selected sixteenth doubles, syncopated accents and dominant
root-note motion, without destroying the older Classic character. KEY/ROOT must
start on **C2 (MIDI note 48 under the displayed Studio One convention)**.

## Scope/compatibility decision

Keep the original `v2::Composer`/session seeds and algorithms untouched, because
old project states store musical seeds and **recompute source phrases on load**.
Revising `classic()` retroactively would silently change previously saved audio.
The revised GENERATE and newly selected Classic style apply
`edit::generateClassicDrive` to the new *editable* pattern before existing
LOCK-merge and scale quantization. The existing VST3 EBM2 state format already
stores all 128 steps, so save/recall and MIDI export reproduce the chosen groove
exactly. Old EBM1/EBM2 project documents are never automatically re-grooved.

No GUI widgets or VST3 parameter IDs were added. Other styles are unchanged.

## New generated patterns

- Eight interrelated A/B motoric rhythmic devices, with four bounded weak-step
  treatments per source grammar; the style continues to use seed-selected
  compositions and deterministic generation.
- Musical pattern length remains exactly 1/2/4/8 bars; no invisible bars are
  added and no automatic re-alignment/quantization occurs during playback.
- Each bar contains 8–12 attacks on a 16-step grid, with offbeat syncopation
  and short one- or two-sixteenth gates.
- At least 75% tonic attacks; the other melodic answers are bounded to the
  fifth or octave and conformed to the selected 125A scale.
- Kick-aligned beats emphasize stronger velocities; quieter syncopations create
  momentum. No new MIDI outputs, custom audio effects, or unbounded RT work.
- Previously locked steps remain untouched; GENERATE updates unlocked cells.
- The note-output model remains monophonic and gates end on/before the next
  NoteOn; MIDI-Note trigger continues without forced phase reset.

## Root note and old states

- A **new** Config starts at `rootMidi=48` (Studio One C2), and GUI
  Ctrl+left-click on ROOT resets to the actual C2/MIDI48 default.
- Old session/project states with explicit root MIDI36 retain those values.
- `HostBridge::decode` accepts the historic canonical `storedCurrent` MIDI36 for
  no-current projects as well as the new canonical MIDI48. The file format,
  parameter IDs, and version remain EBM2/0.2.
- The display's existing Studio One octave mapping is not changed again.

## Measured acceptance / open gates

- Local CMake Release CTest: 17/17 suites, including a new 96-seed by
  1/2/4/8-bar musical fixture.
- Musical fixture checks 8–12 attacks/bar, an offbeat, >=75% root notes,
  all notes in the selected scale, exact bar count, deterministic output,
  valid real MIDI byte header and scheduler, LOCK, state/recall and old C1.
- The existing 16 tests continue checking core grammar, musical form,
  MIDI runtime, VST3 state and GUI command policy.
- Native Windows MSVC VST3 build, official Steinberg validator, independent
  module-host/editor test, user-host Studio One click/audition and perceptual
  approval must be recorded separately. **Do not call this Final based solely
  on local CTest.**
