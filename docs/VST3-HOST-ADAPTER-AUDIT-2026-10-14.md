> Historical checkpoint. Current repair/QA authority: [Final-QA RC](FINAL-QA-CODEX-RC-2026-10-15.md).
> Contracts or unverified statuses below describe the named earlier baseline.

# 125A EBM Bassline Generator — first MIDI-only VST3 adapter, 2026-10-14

**Status:** VST3 **source implemented**, C++17 host-neutral adapter compiled and regressions passed on Linux. Native Windows VST3 module, SDK/Steinberg Validator and Studio One **PENDING** until the single batched Windows job completes. NOT FINAL. No graphical editor implemented.

## Problem -> correct architecture

The v0.2 composer uses dynamic `std::vector`, exceptions and bounded candidate search. It MUST NOT be called from `AudioEffect::process()`. The completed Rifferator V1 is the functional architectural reference for PPQ transport, MIDI trigger, NoteOff ordering, host integration, state and backend/UI separation. EBM uses original independent note generation, not H7 guitar technique mapping, and does not copy its user interface.

New modules:
- `include/ebm/HostBridge.hpp`, `src/HostBridge.cpp`: control-side `v2::Session`, checked fixed document codec, three-slot producer/consumer pattern exchange, existing no-heap `live::Runtime` playback.
- `src/vst3/Plugin.hpp/.cpp`: a minimal `AudioEffect` and `EditController`, two event buses (`Trigger In` 16 channels; `Bass MIDI Out` 1 channel), host PPQ/tempo transport, trigger-note mode, event output and saved component document. Exactly **one host-visible parameter** is implemented: Playback Trigger (Transport/MIDI Note).
- `CMakeLists.txt`: `EBM_ENABLE_VST3=OFF` by default, `ON` fetches Steinberg SDK **v3.8.1_build_84** and builds experimental VST3 without VSTGUI; the original standalone test targets remain available.
- `.github/workflows/windows-vst3.yml`: one targeted native MSVC x64 build/test/official Validator check on a single coherent branch push. A successful compile alone does not grant musical or DAW acceptance.

## Memory/threading protocol

- Non-realtime producers may generate/restore and call `live::prepare()`; they hold `documentMutex_` and a short writer-only mutex. The audio callback never touches these mutexes.
- A slot is exclusively `FREE`, `WRITING`, `READY` or `READING`, chosen with atomic CAS and acquire-release publication. Producer writes plain fixed-layout `live::Pattern` only in WRITING, then publishes READY. Audio atomically claims READY->READING and releases its previous READING slot only after the new one is active. An overwritten, not-yet-claimed pending READY slot is reclaimable; a READING slot is not.
- Each actual adopted revision changes `Block.revision`, which causes the existing runtime to release an old sounding voice without dropping a held trigger or destroying musical phase.
- The fixed-size little-endian host document stores the *complete* session options, current seed, alternate, history and playback mode, protected by FNV-1a against accidental corruption. It is not a security/MAC scheme. Decode checks exact length, magic, version, canonical padding, checksum and `Session::restore()` musical consistency **before** mutating the destination. Incompatible/future formats are rejected, not silently migrated.
- The only processing-path work is bounded event-buffer reads, atomics, PPQ arithmetic, and `Steinberg::Vst::Event` output. `compose`, `composeNovel`, vectors, file I/O and GUI calls are NOT in process(). The event sink must remain bounded/nonblocking (Steinberg `IEventList::addEvent` is host-owned).
- Mode changes, stop/restart and host seeks use the existing runtime note-off discipline; current version is MIDI-only with no audio buses.

## Reproducible Linux evidence

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 8
ctest --test-dir build --output-on-failure -j 8
```

**14/14 local CTest PASS** in GCC C++17 Release with `-Wall -Wextra -Wpedantic -Werror` (13 preserved suites + `EBM_VST3_Host_Bridge_Contract`). The new test covers document encode/decode and byte-identical roundtrip, deliberate corrupted/truncated/wrong-magic state, atomic preservation on malformed restoration, 15 source-rate/block-size pairs at 44.1/48/96kHz and block sizes 1/31/127/512/4096, balanced MIDI NoteOn/NoteOff, mode trigger, and newest-pattern publication. All source compositional/MIDI tests are unchanged.

ASan/UBSan Debug initial full 14-suite run **timed out** at the execution boundary while the long source-composition suites remained active. 11/14 including `EBM_VST3_Host_Bridge_Contract` were observed passing before timeout. Do not label a full Sanitizer PASS without the outstanding tests.

## Limits / next gate

- This VST3 source is **experimental**. Without the native Windows SDK build it has **not** demonstrated that it compiles/links under MSVC, passes the Steinberg Validator, loads in Studio One or outputs MIDI through a DAW host. CI must run first.
- The **generic host Playback Trigger** parameter is the only registered Controller parameter. GENERATE/VARIATE and root/style/bar/variation actions exist on the SDK-free control-side `Model` but are **not yet connected to VST3 Controller GUI or automation**. Current Processor constructs one original default bass pattern (Classic EBM, eight bars). No fabricated complete GUI or user control claim.
- No custom GUI, no zoom, no branded editor, no file save dialog, no all-control traceability, no GUI lifetime tests, no actual host callback allocation hook, no 125A Plugin Tester, no Studio One musical acceptance. This is **not release-ready**.
- A complete MIDI-only feature must publish its control-side pattern updates through a documented host-safe message/control channel and provide an accessible UI for every required control before shipping. The current host bridge is the boundary, not that complete user interface.
