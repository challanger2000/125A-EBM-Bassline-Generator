> Historical checkpoint. Current repair/QA authority: [Final-QA RC](FINAL-QA-CODEX-RC-2026-10-15.md).
> Contracts or unverified statuses below describe the named earlier baseline.

# EBM v0.2 – Global Gate, musical LENGTH display, diatonic MIDI chord roots

Baseline: `5abb13335cffeaca253291bc23a47d85a7f8793a` on `research/v0.2.0-musical-foundation`. This is an experimental update; `main` and the previous Windows package must remain unchanged until owner acceptance.

## User contract

- LENGTH retains its original integer sixteenth-step edit semantics but the grid displays musical values (`1/16`, `1/8`, `3/16`, `1/4`, `1 BAR`, etc.). Double-click still accepts an integer number of sixteenth steps; mouse drag is retained.
- GLOBAL GATE is a separate, visible, draggable 0–100% control. Ctrl+left click resets to **100%**. At 100%, the original per-note LENGTH is preserved. At 0%, MIDI NoteOns are suppressed and sounding bass notes are released. Individual LENGTH values are never rewritten.
- Gate affects real-time NOTE OFF timing sub-step accurately and MIDI-file exported lengths at 480 PPQ; export bar count and NoteOn times do not change. Gate value is VST3 automatable and project-stored.
- In **MIDI NOTE** mode, DAW notes are chord roots inside the fixed KEY/ROOT + SCALE. For C Major: C triggers C chord tones, E triggers Em, A triggers Am, G triggers G. A new trigger while a previous trigger is held does not reset the phrase counter; an old NoteOff following the new NoteOn must not silence it. Direct chord changes may re-articulate an already sounding note at the same MIDI sample, keeping its remaining gate.
- For **CHROMATIC** scale the behavior is conventional semitone transposition, as the 12-tone scale has no implied diatonic chord quality. Off-scale root notes are resolved to nearest in-scale note, ties lower. Both source pitch and target are clamped to 0..127.
- In TRANSPORT mode, original rhythm and fixed KEY/ROOT remain unchanged. MIDI export is the static current GRID in the song KEY/ROOT; it cannot encode the future chord triggers written separately in the DAW.

## State and legacy compatibility

- Document `EBM3` retains all previous `EBM2` bytes (the old CRC is replaced by one Gate byte + new CRC). `EBM1` and `EBM2` decode with default Gate 100%, without transposing previously saved pitches. VST3 parameter `GateId=102` is new; existing TriggerMode=100 and PlayheadStep=101 unchanged.
- The Gate gesture uses a separate bounded control message, not a full pattern snapshot per mouse move. Process callback reads atomic Gate/key/scale; no audio-thread allocation, file access, mutex or producer-side composition.

## Evidence and open gates

- New CTest covers scale-degree mapping, 0/50/100% Gate behavior, 1/2/4/8 naming, MIDI export differences, triggering chord changes, new/legacy state, note-off ownership and GUI hit regions.
- Pre-CI: run all existing CTest suites under Linux. Windows VST3 native compile, Steinberg validator, independent editor lifecycle, actual mouse operation, DAW chord and automation interaction, multi-minute run, and customer musical acceptance are **NOT VERIFIED** until those checks have separately run.
