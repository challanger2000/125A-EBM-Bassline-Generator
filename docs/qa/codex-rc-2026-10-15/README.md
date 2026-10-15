# Local RC evidence — 2026-10-15

These are actual Linux x86_64 results, not Windows or Studio One results.
GCC 14.2, CMake 3.31, Release; AMD EPYC 9V74, 5 available CPUs.

- `ctest.txt`: all 19 suites, no skipped tests.
- `repair-tests.txt`: tuple/control/event/parser/export regressions and zero
  instrumented callback allocations. The paced 750-callback benchmark is a
  one-second core diagnostic at 48 kHz / 64 samples; timer overhead is reported.
  It does not establish Windows DAW CPU performance or a long stress run.
- `asan-tests.txt`: AddressSanitizer/UBSan/leak check, no findings.
- `tsan-tests.txt`: ThreadSanitizer concurrent producer/consumer check, no findings.
  Instrumented timing values are not performance claims.
- `native-process-linux.txt`: actual loaded shared plugin, SDK ProcessData,
  32/64-bit processing and failure/retry/automation cases. An external Linux
  entry-point object was linked for this local diagnostic only; the shipping
  Windows CMake source and plugin identity are unchanged.
- `validator-linux.txt`: Steinberg SDK 3.8.1 validator, 537 pass / 0 fail.
- `workflow-coverage.txt`: workflow includes all local transitive source inputs.
- `expected-negative-control.txt`: deliberate invalid-parser CHECK, expected
  exit 1 in Release. This is evidence that assertions remain active, not a
  functional failure hidden from CTest.
- `music-baseline.txt`, `music-current.txt`: independent executables built from
  original b1d2879 and repaired composer sources; 3,000 deterministic phrases
  have identical FNV-1a-64 digest 68003fbe9b876b02. This is regression evidence,
  not a cryptographic signature.

All three Windows-target translation units (Plugin.cpp, EbmPanel.cpp and
vst3_host_probe.cpp) also passed MinGW SDK syntax checking. Windows execution,
native HWND interaction, MSVC and Studio One results must be stated separately
in the final report. Local SDK builds used the repository-pinned SDK version.
