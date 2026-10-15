# v0.2 RC control and requirement traceability

Scope: `fix/p1-codex-qa-2026-10-15`; all controls are hit targets in the custom
`EbmPanel`, not independent UIDESC parameter widgets. All 16 cells per row and
all supported bar counts are covered by parameterized core policy tests.
Actual Studio One 8.1.2 interaction remains **NOT VERIFIED for every row**.
The native Windows probe adds selected actual HWND mouse/wheel/EDIT tests;
its current execution status is recorded in the Final-QA report.

| Control / target | Input → Controller | Parameter/Processor → result | State / automated checks | Native acceptance scope |
|---|---|---|---|---|
| STYLE | popup / Ctrl reset → `choice` / `click` | Session generation → locked overlay → full document → prepared MIDI | session, grammar, classic-drive, repair LOCK tests | probe tests Ctrl reset with locked note; all popup choices NOT VERIFIED |
| BARS 1/2/4/8 | popup → `choice(kind=1)` | exact editable length → prepared/exported MIDI | edit tests, 12 decoded exports | popup and all bar-count GUI combinations NOT VERIFIED |
| ROOT | popup / Ctrl reset → atomic `edit::transpose` | rejects out-of-range transaction; full document root + notes | repair roundtrip/rejection tests | full root popup/reset gestures NOT VERIFIED |
| SCALE | popup → `conformToScale` | unlocked notes quantized; shared snapshot scale | ten-scale tests, harmony tests | all popup choices NOT VERIFIED |
| TRIGGER NOTES | read-only drawing from ROOT/SCALE | no edit or parameter | 10 scales × 37 roots membership tests | typography and real screenshots NOT VERIFIED |
| VARIATION | drag, wheel ±1, integer text, Ctrl=30 | intensity session edit; visible notes unchanged | session/GUI policy tests; native EDIT probe | all repeat/drag variants require owner acceptance |
| SONG START | popup / Ctrl=2 | same prepared snapshot as notes; raw PPQ adjusted on audio consumer | default=2, EBM1=1, tuple stress includes 9999 | real DAW positions/seek NOT VERIFIED |
| TRANSPORT / MIDI NOTE | toggle / Ctrl=Transport | parameter 100 and explicit mode message; timed stepped queue | runtime ownership, seek/stop, actual VST3 mode offsets | full owner interaction NOT VERIFIED |
| GENERATE | click → Session + fresh editable overlay | full document; locks preserved; prepared plan | grammar, classic drive and frozen 3000-phrase corpus | repeated native DAW clicks NOT VERIFIED |
| VARIATE | click → `variateVisible` | one visible unlocked edit; 0%/all-locked honest no-op | edit/variation/session tests | musical suitability and repeated DAW clicks NOT VERIFIED |
| NOTE ON / LOCK | click toggle; Shift lock; Ctrl defaults | protected per-step edit → document/MIDI | parameterized hit/edit tests; native HWND toggle + lock/reset probe | exhaustive modifiers/cells in DAW NOT VERIFIED |
| PITCH | double-click scale-filtered popup; Ctrl=root | per-step pitch, LOCK protected | scales, editable state and MIDI tests | popup and all pitches in DAW NOT VERIFIED |
| VELOCITY | drag, wheel ±1, integer 1–127; Ctrl=100 | step velocity → actual emitted MIDI | edit tests; native wheel and malformed EDIT probe | exhaustive native range/drag NOT VERIFIED |
| LENGTH | drag, wheel ±1, integer effective length; Ctrl=1 | common effective/max policy; legacy stored length unchanged on load | edit + repair limits; native typed entry probe | all drag boundaries and DAW display NOT VERIFIED |
| GLOBAL GATE | drag, wheel ±1, integer 0–100; Ctrl=100 | parameter 102, 100 integer steps; one gesture; timed gate + export | actual ProcessData offset; 0/50/100 exports; override precedence | native probe checks gesture and typed value; full DAW automation NOT VERIFIED |
| COPY BAR / PASTE BAR | click → clipboard transaction | 16-step copy; locked destination rejected atomically | editable copy/lock tests | exhaustive DAW clipboard workflow NOT VERIFIED |
| CLEAR | click → `edit::clear` | clears unlocked notes only | edit/LOCK tests | native DAW click NOT VERIFIED |
| EXPORT MIDI | click → native save selector | current pattern → SMF, 480 PPQ, fixed 120 BPM | 12 generated files independently parsed for exact length/count; C++ checks gate ticks | save dialog/path errors/reimport NOT VERIFIED |
| Playhead | read-only parameter 101 → controller redraw | actual consumed snapshot/trigger phase | registered/read-only, native setter probe | host forwarding/display cadence NOT VERIFIED |
| BAR previous/next + tabs 1–8 | local click → `bar_` | display only; no music change | GUI geometry and controller UI-state tests | all tabs/zoom interactions NOT VERIFIED |
| ZOOM 100/150 | local toggle → VSTGUI zoom | display only; stored controller state | native probe: 20 cycles at each zoom, matching HWND client size | Studio One placement/screenshots/physical mouse NOT VERIFIED |

Only Trigger and Gate are host-automatable musical parameters. Other musical
controls persist through the full component document and notify host Dirty;
BAR/ZOOM persist in controller-only state. Generic automation of every visible
control is not advertised. Opening the editor does not publish a musical edit.
