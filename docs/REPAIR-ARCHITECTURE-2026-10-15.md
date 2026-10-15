# Realtime repair contracts

The incoming repair branch `0ea968364f1efd12365139e27967873c423c279a` contained
partial repairs. Two actual compilation defects were found before any Actions
run: an impossible negative comparison on an unsigned scale in the Werror core,
and `auto*` deduction from the SDK's `IPtr<IComponentHandler2>`. Both are fixed.

## One published musical document

The existing three-slot exchange now owns Pattern, Root, Scale, Song Start,
document Gate/Mode defaults, control tickets and project-restore cleanup intent.
The RT consumer acquires a slot once per host callback. Every automation slice
uses that same immutable slot; raw project PPQ is shifted only with its own
Song Start. The previous valid slot is retained until adoption of the next one.
A temporarily full exchange may reject a producer transaction; it must never
publish a partial tuple or claim the GUI edit succeeded in that case.

Gate and Mode are independent subsequent control commands. Each lock-free
64-bit word packs a sequence and an integer value; CAS attempts are bounded at
8, with an explicit failure result. A real state restore captures the current
tickets and publishes its own defaults. Commands after that capture override
those defaults. GUI pattern-only updates preserve the prior baseline/tickets,
so an unrelated musical edit cannot revert an already applied host automation
value. GUI mode switching has its own small command message, like Gate.
State-load cleanup is consumed once when that same revision is adopted; a late
producer-side flag cannot accidentally cancel a newly started trigger. GUI
pattern-only publications explicitly keep ownership/phase. None of this internal
protocol is serialized: EBM1/2/3 bytes and IDs are stable.

Host Gate/Mode queues retain all points, sort a fixed stack plan (256 points),
and split the callback at their offsets. Trigger offsets are rebased per slice;
output offsets are translated back exactly. Gate has always stored integer
0–100%; its host metadata now declares the corresponding 100 discrete steps
rather than claiming a continuous parameter. ID 102, normalized endpoints,
round-to-nearest-percent decoding and stored values are unchanged. This is a
deliberate metadata repair; old host envelopes should be checked by the owner.
Mode retains its existing one-step metadata and ID 100.

## Failure completion and trigger ownership

Malformed offsets/pitches/velocities, event read failure, input overflow or bad
automation cannot schedule a new note. Cleanup clears trigger ownership before
attempting output. A rejected NoteOff retains its active pitch and pending
cleanup for a later callback; accepted NoteOffs are not duplicated. A callback
failure closes MIDI input ownership safely rather than replaying a lost trigger
NoteOff. Healthy same-length pattern edits keep the original ringing gate,
trigger and phase. Simultaneous unrelated NoteOffs do not break chord handoffs.

The existing `kResultFalse` error convention is retained. A host must continue
callbacks for deferred output completion; real Studio One backpressure/error
handling remains an owner QA case, not an independently established PASS.

## Editable state and GUI

New Processor and Controller defaults agree on editable Song Start 2; explicit
legacy EBM1 fallback remains 1. ROOT transposition rejects an out-of-range
transaction atomically instead of clipping and losing pitch information.
STYLE selection/reset and GENERATE share exact LOCK overlay preservation.
LENGTH drawing, drag baseline, wheel and numeric entry share effective/max
helpers; decoding old stored lengths never rewrites them. Gate drag opens one
host gesture, closed on mouse-up, cancel or editor shutdown. Every numerical
control has wheel and integer entry, with a full-input parser that rejects
suffixes, decimals, overflow and out-of-range values.

Regression infrastructure uses unconditional CHECK/REQUIRE in Release. The
explicit negative-control option must exit 1; it is an expected failure, not a
passing functional test. MIDI export validation parses actual events, NoteOff
ordering, expected counts and exact End-of-Track, including empty Gate-0 files.

The new native Windows probes compile against the pinned SDK interfaces and
load the actual staged module. They cover both processing formats and selected
native HWND gestures; they do not replace Studio One or visual acceptance.
