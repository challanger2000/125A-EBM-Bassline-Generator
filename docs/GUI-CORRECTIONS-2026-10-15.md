# EBM Bassline Generator – GUI/variation corrections, 2026-10-15

Branch: `research/v0.2.0-musical-foundation`; source baseline `4be824b29bb5359475342e7d8952d12a886083c8`. This is an experimental development change, not a release or Studio One approval.

## Reproduced from user's Studio One screenshot

- Direct BAR buttons drawn at y=209..241 covered the step-number labels at y=222..242. Now they occupy y=190..214, separate from the step numbers. GUI hit rectangles and static tests use the same constants.
- Footer buttons received no pressed-state paint in the previous CView; `onMouseDown` did not capture release. Now there is visible pressed fill during the mouse-down interval and a persistent small acknowledgment marker + highlighted last-action border when the host coalesces WM_PAINT. `onMouseUp/onMouseCancel` clear pressed state. All six action buttons use one shared command classification.
- Native drag of active, unlocked VELOCITY or LENGTH cells did not exist; now vertical drag controls them continuously, preserving direct text entry and Ctrl+click reset. Velocity clamps 1..127, gate clamps against the next NoteOn or phrase end. Locked notes are not changed.
- VARIATE previously modified only the original four/eight-bar `v2::Session` composition, so source patterns truncated to 1 or 2 bars could remain identical. New `edit::variateVisible()` applies one of four bounded, deterministic, musically meaningful edits to an unlocked sounding step in the *selected bar* (accent, in-scale pitch, gate, or timing). Empty/all-locked patterns and intensity 0% report explicit reasons. The MIDI output is the actual edited grid, not hidden bars. Active playback uses existing fixed-plan exchange and continues held trigger.
- Variation intensity adjustment no longer overwrites unlocked user-edited MIDI notes. It modifies session intensity for future VARIATE/GENERATE without replacing the editable pattern.
- Studio One names MIDI 60 C3, so display uses `MIDI/12 - 2` instead of `MIDI/12 - 1`; MIDI pitches are **not** transposed behind the user's back. Old saved notes retain their pitch; their displayed octave name changes. Config root MIDI range remains 24..60, covering C0..C3 in Studio One naming.
- Existing three scale IDs remain intact for old EBM2 project recall. Seven new scales are appended: Harmonic Minor, Melodic Minor (ascending), Dorian, Phrygian, Mixolydian, Lydian, Minor Pentatonic. All freshly generated, unlocked notes are quantized to the actual selected scale. Locked steps intentionally stay untouched. On loading an old project, existing note pitches are preserved until a user action changes them.
- Correction to the original checkpoint: older EBM1 sessions retain SONG START BAR 1 in both Controller and Processor. New editable sessions default to BAR 2. Explicit EBM2/EBM3 song-start values are preserved.

## Local pre-CI proof and open gates

SDK-free C++17 Release CTest: 16 suites, including added variation lengths 1/2/4/8, scale membership, project-state migration, geometry, dragging bounds, and the live-MIDI scheduling tests. Any native Windows VST3/Validator, independent editor mouse/drag interaction and real Studio One human acceptance MUST be independently confirmed; do not infer them from CTest. No user approval or `main` update.
