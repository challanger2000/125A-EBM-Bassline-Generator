# MIDI NOTE phase continuity — development RC, NOT FINAL

Baseline: preserved RC source `4ef178d00c16f24c09e9b3f77214e76a635b77da`,
branch head before this repair `d79b782402145f46571a95f04c7aaac70f761029`.
[Previous Windows build / ZIP](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38787724599#artifacts)
remains intact. All work stays on `fix/p1-codex-qa-2026-10-15`; main and b1d2879
are unchanged. No plugin identity, state bytes, parameter IDs, composition,
Transport scheduler or GUI has been changed.

## Cause and defined musical behavior

Previously LiveMidi.hpp only preserved triggerPhase when matching Off and next
On occurred at the exact same sample. Otherwise it cleared ownership **and
phase**, and the next On restarted step one. An ordinary 1-sample gap or host
block boundary therefore changed rhythm. Retune-at-boundary also lacked the
scheduler's numerical endpoint tolerance, permitting duplicate boundary attacks
at 96 kHz / 256 samples; the new oracle detected and guards this case.

Gate ownership and the clock now have separate lifetime:

| Event | Sound / note ownership | Sequence phase |
|---|---|---|
| First NoteOn after reset/explicit stop | open, schedule step1 at that exact sample | start0 |
| New root while held, including same-sample Off/On | original gate-preserving retune | continue |
| Matching NoteOff without simultaneous replacement | immediate bass Off at the input offset, close gate | continue silently |
| New On after any gap, including same pitch | open gate, next attacks stay on original grid | continue, never restart automatically |
| Short or long gap | no output notes; no deferred release timeout | advance with callback samples and tempo |
| Explicit stop, mode change, project restore, processing lifecycle cleanup or invalid input/output failure | clear ownership, release current bass, retry rejected Off before any further On | reset0 |

A gap is not stretched or filled with a sounding bass. On reopening the gate
between grid attacks, no off-grid step1 or retroactive note is invented; the
next original grid attack sounds with the new root. Same-sample retuning of an
already sounding note remains as before. After a long rest without an explicit
stop, the phase still continues. A new phrase start requires an explicit reset;
there is no inferred short/long cutoff, millisecond window or note tail hold.

An explicit host stop is the **observed kPlaying true -> false edge** in a
supplied VST3 ProcessContext. It releases at callback offset0 and dominates
chased input in that callback. A continuously stopped host must still allow
live keyboard play, so kPlaying=false alone is not a repeated panic. Missing
context is not fabricated as a stop. If a host sends only NoteOff (e.g. another
Stop press while already stopped), sound stops immediately but the clock is
retained: the plugin cannot distinguish that event from a musical gap. On
processing/deactivation cleanup the existing pending-Off mechanism is retained;
actual Off completion still requires a host output callback accepting the event.

The 1e-8 step endpoint tolerance follows the existing scheduler's grid tolerance;
it prevents a floating-point duplicate at an exact endpoint. It is a numerical
comparison, not a musical gap/stop timeout.

## Studio One and primary interface evidence

The pinned Steinberg SDK 3.8.1 `ivstevents.h` defines Event.sampleOffset relative
to the current block, individual NoteOn/Off and optional noteId. It exposes no
"these two notes form a chord change" flag or promised shared offset.
`ivstprocesscontext.h` separately defines kPlaying. Sources:

- [Steinberg Event API](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1Event.html)
- [Steinberg ProcessContext API](https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html)
- Actual headers under the repository-pinned FetchContent SDK were inspected
  before implementation; production code uses those existing fields only.

**Studio One 8.1.2 event delivery is NOT MEASURED in this cloud session.** No
Studio One runtime or captured input trace is available. Attempts to consult
PreSonus's official Music Editor / Action Menu manual at s1manual.presonus.com
were blocked by this environment's network proxy (403). A specific host event
ordering or timing guarantee is therefore not asserted. The owner's observed
separated Off/On defect is the reported reproduction, not an invented capture.

The actual loaded VST3 probe supplies documented legal delivery cases: same
sample, early Off / late On, overlaps, Off at block end / On in next block,
long silence, stop with chased input, stopped live input, both sample formats.
Those are synthetic SDK-host checks, **not Studio One tests**. Owner acceptance
must record the real incoming sampleOffset/pitch/on-off stream for drawn C/E/A/G
notes with touching ends, deliberate gaps and overlaps, repeated same notes,
live keyboard changes and Stop/restart. Compare generated absolute onsets to
the original phrase grid. A listening check remains necessary for the selected
downstream instrument's envelope response to retuning.

## Regression and available results

[Retained local transcripts](qa/midi-phase-2026-10-15/):

- **19/19 Release CTest PASS**, including all previous Transport timing matrices,
  state/automation/gate/harmony/GUI policy regressions.
- **60 phase cases PASS**: 800/48000/96000 Hz × 1/31/127/256 samples × gaps
  0/1/17/300/5000 samples; C/E/A/G exact grid/pitches, immediate Off, balanced
  voices. Long silent clock wrap (1000 callbacks), same-key retrigger, explicit
  stop/rejected-Off retry, missing context and mode-reset cases also PASS.
- New regression run against old RC header **fails as expected** at the missing
  original-grid attack. Only an unused transportValid test-adapter field was
  added to that external baseline header so the new fixture compiles; no old
  behavior was altered. This is a deliberate negative control, not a candidate
  functional failure.
- ASan/UBSan/leak and TSan repair suites **PASS**, no findings. Existing callback
  allocation instrumentation remains **zero**. No allocation/lock/IO/unbounded
  work added to audio code: one clock flag and two transport-observation flags,
  bounded processing and constant-time silent advance.
- **Actual loaded Linux VST3 PASS**, kSample32 and kSample64: root transitions,
  separated/cross-block/long gaps and host stop/chase/live restart, plus all
  earlier malformed-input, Off backpressure, state, automation and lifecycle
  checks. The new probe restores its fixture and closes its last held voice
  before the older tests run.
- Steinberg Linux Validator **537 PASS / 0 FAIL**. Local diagnostic Linux module
  uses the same external entry-point object as prior QA, never shipped.
- Windows-target MinGW SDK syntax checks **PASS** for the actual shared plugin
  and native probe sources. This does not itself establish Windows execution.
- Release paced core diagnostic: 750 callbacks, 48 kHz/64; mean0.697 µs,
  p950.811 µs, p991.582 µs, max61.852 µs, timer-pair mean0.023 µs, 0 overruns
  at1333.333 µs deadline. Not a DAW or long-duration performance claim.

## Windows handoff

Implementation / tested source: **`a54512812206e248d4c028ba2f2ea53ceb3a34d7`**.

**Windows PASS:** [Actions run38790308058](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38790308058),
job116612845977, 3m48s. Exactly **one** bundled follow-up run, no intermediate
Actions builds and no reruns. All workflow steps succeeded: MSVC Release x64,
all19 suites (including the new phase oracle), expected failing negative control,
12 actual MIDI exports, actual staged module host/phase/automation/state tests
in both formats, 40 Win32 editor cycles with selected gestures at both zooms,
and Steinberg Validator. The Windows validator count is not independently
visible in public HTML; the Linux537 count is not substituted for it.

[Download new Windows x64 ZIP — Actions Artifacts](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38790308058#artifacts)
(GitHub login required): **`125A-EBM-v0.2-MIDI-PHASE-RC-Windows-x64`**,
artifact **11998958012**, displayed **1.63 MB**.
[Direct artifact link](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38790308058/artifacts/11998958012).
GitHub-reported archive SHA-256:
`a7a0bcaec3f46dcfc8f810347b30c0ddd0a7033c6e5bfd634e216c561c8753d0`.
The archive contains the complete branded VST3, QA logs/MIDI fixtures,
SOURCE-COMMIT.txt and SHA256SUMS.txt. The previous RC ZIP was not replaced.

[Observed Windows evidence](qa/midi-phase-2026-10-15/windows-actions.txt) records
public run/job conclusions and archive metadata. Authenticated raw logs, job
summary and ZIP payload were not independently downloaded in this cloud session;
the digest above identifies the archive, not the inner DLL. One nonfatal Actions
Node20 deprecation warning remains as previously documented; GitHub ran Node24.
The Plugin Tester was never run in Actions.

**NOT VERIFIED:** actual Studio One, owner-local Plugin Tester v0.2.10 FINAL,
listening/visual acceptance. No FINAL, main merge or publication.

## Recurring-error prevention

Do not infer sequencer restart from "no trigger currently held", a matching
NoteOff, a short/long duration or callback boundaries. Separate sound gate,
rhythm clock and explicit transport/lifecycle reset. Every root-change repair
must check delayed Off/On and block boundaries against an independent absolute
onset oracle, in addition to coincident Off/On. Do not loosen an obsolete test
or regenerate a reference merely to make this new musical contract pass.
