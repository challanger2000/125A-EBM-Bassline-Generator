# Editable EBM MIDI pattern — development checkpoint (2026-10-15)

Branch: `research/v0.2.0-musical-foundation`; **experimental v0.2**, not FULL, no release authorization.

## User-facing contract

- **1 / 2 / 4 / 8 BARS** selects exact pattern and Standard MIDI File duration. The established grammar composes a 4- or 8-bar parent, with a selectable shorter playback/export excerpt; no fake claim that the grammar itself composes independent 1- or 2-bar multi-part phrase forms.
- **KEY / ROOT** sets the tonic and pitch register; **SCALE** offers Chromatic, Major or Natural Minor. For generated riffs, the original composition remains in its original tonal language unless the user explicitly selects a scale. Selecting a diatonic scale quantizes unlocked notes deterministically, and the pitch chooser filters to that scale. This is a first implementation, not a musicology approval.
- **NOTE ON** clickable to toggle. SHIFT+click locks/unlocks a step; Ctrl+click restores the default (Off, tonic, velocity 100, gate 1). **PITCH** double-click opens a scale-filtered note popup. **VELOCITY** and **LENGTH** double-click open a native VSTGUI text-edit box for numeric input (1..127 and 1..128 sixteenth notes). Locked cells reject normal edits.
- **COPY BAR / PASTE BAR** duplicate all 16 steps. Pasting into any locked target step is rejected atomically; no partial paste. **CLEAR** removes all unlocked notes, leaving locks unchanged. GENERATE/VARIATE preserve locked steps.
- **EXPORT MIDI** is the only export button. It writes the selected number of bars, includes manual pitch/velocity/duration edits and inserts NoteOffs before the end of the selection. The exported MIDI is at 480 PPQ with a fixed 120 BPM tempo event; Studio One may adopt song tempo when importing.
- **SONG START** defaults to DAW bar 2 for new editable sessions; the native transport PPQ is shifted by 4 quarter notes for each offset bar. Bar 1 of the pattern therefore aligns with DAW bar 2. Older EBM1 documents without editable state retain the original absolute transport alignment until edited, to prevent surprise project changes.
- **MIDI NOTE trigger editing** publishes a bounded prepared MIDI table through the triple buffer, not by replacing the trigger. The live runtime retains the trigger, musical phase and currently sounding note until the NoteOff step cached when its NoteOn was emitted. Same-length edits cannot truncate that note. A pattern-length change must release the active note once because its old modulo end-step may become unreachable. Native Studio One behaviour remains unverified.
- **HELP line** changes with hovered control; Ctrl/Shift/Double-click instructions are visible in the plugin.
- UI-only BAR and zoom remain Controller-side; audio-producing user steps are persisted in the versioned processor document. Former `EBM1` documents are decoded without changing the older audio state.

## Architecture & limits

The `v2::Session` musical grammar remains 4/8 bars to avoid changing the established composer. The new `edit::Pattern` is a fixed 128-step monophonic MIDI overlay, independently saveable and editable even after CLEAR. Audio callback never composes, allocates, blocks on a mutex, or opens a file. `host::Document` is now `EBM2` with checksum; strict `EBM1` decode migration is regression-tested. Serialized state of manual edits does not live in Controller UI-only bytes.

### Test evidence and unknowns

- GCC C++17 Release: **16/16 CTest PASS** with the unchanged 15 test groups and an added edit-pattern/legacy-state/midi-trigger-live-update group.
- The current source is **not yet native-MSVC validated at the time of writing**; GitHub Actions Windows VST3, SDK Validator and actual independent host lifecycle checks are next gates. No GUI native compile/Studio One operation is inferred from Linux core tests.
- **Host-driven playhead feedback is implemented as a read-only VST3 output parameter** (ID 101, value 16=inactive). It derives from host PPQ or the MIDI-trigger phase and is emitted only when its step changes. Host-to-Controller UI display feedback is dependent on the host and therefore **NOT VERIFIED in Studio One**. There is no independently animated/faked playhead timer.
- VST3 host automation is implemented only for the Trigger Mode parameter; musical edits pass a full versioned document through the host message path. Do not claim automation completeness. GUI numeric focus/keyboard/Esc behavior needs native-host tests.
- GUI layout in actual Studio One at 100/150% and click routing, 20 repeated editor lifecycles, 125A Plugin Tester and user's musical acceptance remain **NOT VERIFIED**.
