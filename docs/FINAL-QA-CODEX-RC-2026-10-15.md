> Follow-up: the subsequent MIDI NOTE phase defect and corrected candidate are
> documented in [MIDI phase RC](MIDI-PHASE-RC-2026-10-15.md). This report records
> the preserved earlier build; it does not certify separated trigger Off/On.

# 125A EBM Bassline Generator v0.2 — technical RC QA

Status: **development release candidate, NOT FINAL**. Report date: 2026-10-15.
Branch: `fix/p1-codex-qa-2026-10-15`.
Implementation and regression commit: `b269552ac88f58b106f79554d5288f6da8630087`
(abbreviated **R** below). Incoming partial repair baseline: `0ea9683`.
Original owner-tested `b1d28792eb0da6471698488995b24a8adff5179b` and `main`
`11f10afa9817bc2da526fb776fa3a0555bec12e7` are preserved.

## Evidence and acceptance boundary

All 15 findings have an implemented repair and targeted regression coverage.
PASS below means the specified available automated regression passed; it does
not mean complete native GUI or actual DAW acceptance. The single bundled Windows Actions run **succeeded**, including native execution.
The artifact is a technically checked RC for owner acceptance, not an approved
FINAL release. Exact evidence boundaries are documented below.

Actual local results, retained in [raw evidence](qa/codex-rc-2026-10-15/README.md):

- Release build and **19/19 CTest suites PASS**, no skipped tests. Explicit
  negative control fails with exit 1 in Release, as required.
- AddressSanitizer/UBSan/leak and ThreadSanitizer repair tests **PASS**, no findings.
- Actual loaded Linux VST3 ProcessData, both 32/64-bit formats: malformed inputs,
  output rejection/retry, trigger ownership, sample-offset automation and
  lifecycle **PASS**. This uses an external local diagnostic Linux entry object;
  shipping target remains Windows x64.
- Steinberg SDK 3.8.1 validator on that Linux module: **537 PASS / 0 FAIL**.
- Twelve real MIDI files (1/2/4/8 bars × Gate 0/50/100) independently parsed:
  exact EOT/bar length, note counts and order **PASS**. Separate C++ decoding
  verifies gate duration ticks and zero-gate silence.
- Baseline vs repaired composer: **3,000 deterministic phrases identical**,
  FNV-1a-64 `68003fbe9b876b02`. Existing composer algorithms were not edited.
- No instrumented allocations in the core callback. Paced Linux 48 kHz/64 sample
  diagnostic: 750 callbacks, mean 0.346 µs, p95 0.661 µs, p99 0.912 µs,
  max 13.540 µs, timer-pair mean 0.024 µs, 0 overruns at 1333.333 µs deadline.
  This one-second diagnostic is not a Windows DAW performance/stress claim.
- MinGW Windows-target SDK syntax checks passed for Plugin.cpp, EbmPanel.cpp and
  the native host probe. This is not Windows execution or MSVC validation.

## All 15 independent findings

All repair/test references below belong to **R**, including validation of
retained incoming fixes. Detailed contracts: [architecture](REPAIR-ARCHITECTURE-2026-10-15.md).

| # / priority | Actual cause and consequence | Repair / commit | Targeted regression and actual result | Remaining risk / acceptance |
|---|---|---|---|---|
| 1 P1 | Invalid single MIDI event returned before releasing a sounding note. Invalid offsets/pitches/velocity or failed reads could leave ownership live. | R: unified fail-closed ownership cleanup, retained pending pitch, deferred Off retry; bounded input validation. | PASS: repair runtime cases; actual Linux VST3 8 malformed cases per format, rejected first Off then exactly one retry and no revival. Windows native probe passed the same cases. | Host must continue callbacks after existing kResultFalse convention; real Studio One error/backpressure NOT VERIFIED. |
| 2 P1 | Failed matching trigger NoteOff left triggerHeld true, so riff resumed after retry. Same-time unrelated Off could also break chord replacement. | R: clear ownership before release attempt; preserve pending output; scan same-offset replacement group. | PASS: rejected Off/no later On; 64-root handoff and balanced release; same-offset unrelated Off preserves onset at sample 400; actual Linux and Windows processor both formats. | Extreme real device/DAW load NOT VERIFIED. |
| 3 P1 | Pattern snapshot and independently read Root/Scale/SongStart could belong to different publications; partial Gate/Mode fixes risked reverting automation or a late state-reset flag stopping a new trigger. | R: immutable full tuple acquired once per callback; sequenced Gate/Mode commands with bounded CAS; distinct GUI edit and project-load publication, reset intent consumed with revision. | PASS: concurrent tuple stress and 2,000 correlated pattern/root publications, no torn pitch; rejected producer transactions counted separately; TSan; control precedence and state-reset retry. | Full exchange may reject a producer edit rather than publish partial data. Real concurrent host/UI automation stress NOT VERIFIED. |
| 4 P2 | Only final parameter point was applied to entire block. | R: validated fixed 256-point plan, bounded sort and sample-offset slices; one document snapshot for all slices. Gate metadata declares existing integer 0–100 semantics (100 steps). | PASS: ordering/invalid bounds; actual VST3 Gate Off at255 and Mode Off at64 in both formats on Linux and Windows. | ID/domain/storage unchanged; legacy host envelopes and Studio One interpolation need owner check. |
| 5 P2 | Musical edits notified processor but not host Dirty state. Incoming IPtr helper also failed compilation. | R: pinned SDK EditController::setDirty(true); retain full edit message and report rejected publication honestly. | PASS: shared-source build and actual Windows HWND note edit with handler Dirty assertion. | Actual Studio One save prompts NOT VERIFIED; native test alone is not DAW acceptance. |
| 6 P2 | Processor emitted playhead ID101 that Controller never registered. | Incoming 47be45b registration retained; R adds read-only metadata/setter native probe. | PASS: SDK build and executed Windows native registration/read-only/setter assertions. | Real host parameter forwarding/redraw cadence NOT VERIFIED. |
| 7 P2 | Ctrl STYLE reset recreated overlay and lost locked cells. | R: shared preserveLocks transaction for reset, style selection and generation, including locked OFF notes. | PASS: exact locked ON/OFF preservation, editable suite; executed Windows native Shift LOCK + Ctrl STYLE test. | Exhaustive popup/DAW gestures NOT VERIFIED. |
| 8 P2 | ROOT transposition clipped pitches, irreversibly losing information. | R: shared atomic transpose validates entire ±36 pitch transaction before mutation; reject out-of-range, including popup route. | PASS: boundary rejection leaves full document unchanged; reversible transposition roundtrip. | Some extreme root choices now intentionally reject; actual menu feedback/DAW use NOT VERIFIED. |
| 9 P2 | New Controller began SongStart2, Processor default1. | R: shared new editable default2; explicit legacy-disabled EBM1 fallback remains1. | PASS: Model new default2; legacy state suites and restore controls tests. | Existing serialized start values retained; DAW project positioning NOT VERIFIED. |
| 10 P2 | LENGTH displayed overlap-limited effective length but numeric/drag used raw length. | R: common effective/max helpers for drawing, numeric, drag and wheel; no rewrite on legacy load. | PASS: overlap/end-of-bar bounds and effective/raw regression; executed Windows native typed LENGTH1. | Exhaustive physical drag/zoom boundary acceptance NOT VERIFIED. |
| 11 P2 | Gate drag emitted a gesture per movement; shutdown could leave a gesture open. | Incoming e055cad/e6d9900/0ea9683 lifecycle fix retained; R integrates value helper and checks one begin/end across three native moves. | PASS: actual Windows drag, three movements, exactly one begin/end gesture, at both zooms. | Studio One automation recording and cancellation outside native probe NOT VERIFIED. |
| 12 P2 | Gate/Variation lacked wheel and numeric paths; native edit geometry/focus required real handling. | R: wheel dispatch ±1 and native CTextEdit for Gate/Variation; complete bounds and focus/commit handling. | PASS: integer policy and actual Windows wheel, native EDIT Gate50/Variation45, at both zooms. | Native subset passed; exhaustive real DAW gestures NOT VERIFIED. |
| 13 P2 | Workflow path filter and targets missed transitive inputs, potentially shipping untested changes. | R: include/src/tests/resource/CMake/workflow coverage; repair branch trigger; all19 tests, actual exports and staged-binary host/validator gates; archived evidence/checksums. | PASS: coverage check (53 inputs), workflow structure/targets and one successful bundled Windows execution. | Future source roots must be added to the guard; one Actions runtime deprecation warning is documented below. |
| 14 P3 | stoi accepted prefixes such as 12x, violating integer validation. | R: full-consumption from_chars parser with range checks; invalid entry leaves state unchanged. | PASS: suffix, decimal, whitespace, overflow and range negatives; Release failing negative-control confirmed; executed Windows native invalid EDIT regression at both zooms. | Native keyboard/layout/focus variants NOT VERIFIED. |
| 15 P3 | Current and historical QA/default/release statements contradicted each other. | R and this report: CURRENT marker, historical banners, corrected legacy/new defaults, explicit current authority and scope. | PASS: current documentation cross-review; traceability for every requested control, separate platform and acceptance evidence. | Older reports remain historical evidence, never authority for this new binary. |

## State, controls and realtime review

[Control traceability](CONTROL-TRACEABILITY-CODEX-RC-2026-10-15.md) covers STYLE,
BARS, ROOT, SCALE, TRIGGER NOTES, SONG START, modes, GENERATE, VARIATE, VARIATION,
NOTE ON, LOCK, PITCH, VELOCITY, LENGTH, GATE, COPY/PASTE BAR, CLEAR, MIDI EXPORT,
Playhead and zoom. Existing core suites exercise state roundtrips/corruption,
EBM1/2/3, scales, locks, gate/harmony, generation/variation, runtime discontinuity,
copy/paste/clear and boundaries. No serialized format, plugin class identity or
composer algorithm was changed. Gate stepCount metadata is an explicit repair.

The audio path uses fixed stack buffers, immutable published state and lock-free
64-bit commands (compile-time assertion). CAS has an eight-attempt limit; full
publication queues fail explicitly. No mutex, file IO or allocation was added
to process. Sanitizer and allocation instrumentation support this review but
cannot prove absence of every possible platform/host race. GUI/controller document
work remains off the audio thread. Host misuse, unsupported reentrancy and
stopped callbacks cannot be repaired by generating MIDI after processing ends.

## Windows candidate and owner handoff

Bundled Windows build **PASS**: [Actions run 38787724599](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38787724599),
job 116603954547, 3m10s, source `4ef178d00c16f24c09e9b3f77214e76a635b77da`
(the implementation R plus this report's initial documentation commit).
**One run, no reruns.** All workflow steps succeeded:

- MSVC Release x64 plugin and all19 C++ regression suites, including the
  expected-failing Release negative control.
- Twelve independently decoded MIDI exports.
- Actual staged branded Windows bundle: native ProcessData in both formats,
  malformed-input/Off retry/automation/lifecycle/state checks.
- **40 actual Win32 editor attach/remove cycles**, 20 each at 100/150% zoom,
  exact view/HWND sizes; selected native mouse/wheel/EDIT tests at both zooms:
  NOTE toggles, Dirty, LOCK + Ctrl STYLE, one Gate drag gesture, velocity wheel,
  typed Gate/Variation/LENGTH, malformed numeric rejection.
- Steinberg Validator **PASS on the staged Windows bundle**. Exact Windows
  validator test count is not independently visible in the public status page;
  do not substitute the Linux 537 count for an observed Windows count.
- Archive/checksums publication PASS.

[Download Windows x64 RC ZIP from Actions → Artifacts](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38787724599#artifacts):
`125A-EBM-v0.2-CODEX-RC-Windows-x64`, artifact ID **11998965328**, displayed
size **1.63 MB**. GitHub login is required to download Actions artifacts.
[Artifact link](https://github.com/challanger2000/125A-EBM-Bassline-Generator/actions/runs/38787724599/artifacts/11998965328).
GitHub-reported archive SHA-256:
`c17f82a6a98ecf76b8cd1c695ca1d144dc0d6ef204586b1a794f7c6ae0d393fe`.

The ZIP contains the complete branded VST3 bundle, QA logs, 12 MIDI fixtures,
SOURCE-COMMIT.txt and SHA256SUMS.txt. The archive digest is not the inner plugin
binary hash. The cloud session verified public run/job step conclusions and
artifact metadata; authenticated raw logs, job summary and ZIP payload were not
independently downloaded. The raw Windows logs and per-file hashes are included
by the successful workflow. [Observed Windows evidence](qa/codex-rc-2026-10-15/windows-actions.txt).

One nonfatal infrastructure warning: checkout@v4 and upload-artifact@v4 declare
Node20; GitHub forced Node24. Future maintenance should upgrade these actions to
current supported majors after compatibility review; no extra paid run was
started for this warning. The successful run was not repeated.

**NOT VERIFIED:** actual Studio One 8.1.2, owner-local
`125A_Plugin_Tester_v0.2.10_Windows_x64_FINAL`, listening/musical acceptance,
visual screenshots and exhaustive native control gestures. The Tester is never
invoked by Actions. Owner should load the new artifact, confirm scanner/validator,
load older projects, save/reopen edits, record Gate/Mode automation, test live
chord transitions/transport stop-seek, export/reimport all bar lengths and inspect
both zooms. A previous b1d2879 owner PASS does not certify this candidate.

No FINAL tag, main merge, release or Gumroad publication is authorized or made.
