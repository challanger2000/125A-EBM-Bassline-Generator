# EBM Bassline Generator — static-first VSTGUI v0.2 prototype

**STATUS:** Initial native editor wiring, NOT release-ready. No visual/Studio One confirmation, no full 125A Plugin Tester. The source is developed on `research/v0.2.0-musical-foundation`, never `main`.

## Functional design, before visual embellishment

- Root editor logical extent **1100 x 590**, zoom factors **100 and 150%** (native **1650 x 885**). No automatic fit promise for arbitrary host decoration; 150% must be verified in Studio One for pointer alignment and actual window extent.
- Canonical master SVG is from the user's `125A-Branding` repository and loaded by original geometry in the logo view; NO redrawn text as substitute.
- Top: product name, fixed two-state zoom.
- Primary row: direct style popup (`Minimal Body`, `Classic EBM`, `Dark Electro`), direct bars popup (4/8), selectable MIDI root note C1–C4, absolute variation 0–100 slider (Ctrl+click resets 30), Transport/MIDI NOTE toggle (Ctrl+click defaults to Transport).
- Working area: selected BAR, previous/next; sixteen numbered grid positions; attacks, pitches and velocity are derived **from the actual composed MIDI phrase**; no editable-but-unwired decorative steps.
- Bottom: GENERATE, VARIATE, EXPORT LOOP MIDI and EXPORT ONE-SHOT MIDI. Native file-selector dialog for MIDI saves, with NoteOff checked by existing MIDI writer.
- No fabricated scale selector, BPM control, MIDI humanize, other hidden settings, or audio processing.

## Control-to-MIDI contracts

| UI action | Non-realtime session operation | Host document | Measurable MIDI effect |
|---|---|---|---|
| GENERATE | `Session::generate()` | Full semantic snapshot via `IMessage` | Seed/motif changes; bounded novelty selection |
| VARIATE | `Session::nextVariant()` | Full semantic snapshot | Source seed unchanged; alternate treatment |
| STYLE | `Session::setStyle()` + regenerate | Full snapshot | Re-compose with selected grammar |
| BARS | `Session::setBars()` + regenerate | Full snapshot | 4 or 8 bars (64/128 sixteenths) |
| ROOT | `Session::setRootMidi()` | Full snapshot | Transposes current notes, preserving timing |
| VARIATION | `Session::variate(level)` | Full snapshot | Exact 0–100 value; 0% neutral |
| TRIGGER | VST3 TriggerModeId + host snapshot | VST3 parameter + full snapshot | Transport vs matching external MIDI NoteOn/NoteOff |
| EXPORT LOOP | `render(...Loopable)` -> `toMidi()` | File dialog only | Preserves loop-seam pickup |
| EXPORT ONE-SHOT | `render(...OneShot)` -> `toMidi()` | File dialog only | Suppresses unanswerable final pickup |
| BAR prev/next | UI-only state | UI-only controller state | Display only; **no MIDI modification** |
| ZOOM | `VST3Editor::setZoomFactor` | UI-only controller state | Display only; **no MIDI modification** |

**State ownership:** Processor persists full music in `getState`/`setState`; Controller owns UI-only BAR/ZOOM; Controller receives and validates full music document on `setComponentState`. A control-thread message carries a versioned fixed-length binary document to `Processor::notify`, where all data is validated before `host::Model::restore` publishes an immutable MIDI plan. The audio callback itself never composes, allocates memory or locks a mutex. `sendMessage` is a transport ACK, not proof Studio One processed/played the pattern; host probe and DAW checks are mandatory.

## Explicit unfixed issues / gates

- No Windows SDK compile or Steinberg Validator claim until actual workflow result (the previous non-GUI build passed).
- No UI screenshot/visual review or real 100/150% clickable hitbox DAW proof yet.
- Most musical parameters intentionally **not host automatable yet**; the UI sends checked state snapshots instead. Do not mark general VST3 automation PASS.
- No callback against main thread IConnectionPoint guaranteed without running native host probe.
- Not FINAL and no sonic suitability assertion.

## Native host proof (next Windows job)

`tests/vst3_host_probe.cpp` dynamically loads the actual staged VST3 using Steinberg hosting utilities, validates one input and one output MIDI bus, sends the exact full-document `IMessage` format to the **actual Processor IConnectionPoint**, verifies processor state changes and rejects a corrupted state without mutation. It also checks saved/restored UI-only 150% zoom and runs five HWND editor attach/detach cycles. This verifies the message **receiver** and editor lifecycle, not the physical pointer click/drag inside Studio One. It is pending Windows CI until executed; no claim of host PASS before the result.
