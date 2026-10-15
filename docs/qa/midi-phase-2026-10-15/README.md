# Phase repair evidence

Actual local Linux x86_64, GCC14.2 Release, CMake4.4.4, AMD EPYC9V74,
5 available CPUs. Separate from forthcoming Windows/Studio One evidence.

- ctest.txt / repair.txt: all19 suites and new60-case exact timing oracle.
- baseline-negative.txt: intentional expected failure of the old RC behavior;
  unused transportValid field added only to compile the new baseline fixture.
- asan.txt / tsan.txt: ASan/UBSan/leak and TSan suites, no findings.
  Instrumented timings are not production performance claims.
- native-process.txt: loaded real Linux module with actual SDK ProcessData in
  both formats; phase/root/overlap/stop cases and earlier processor checks.
- validator.txt: 537 pass /0 fail; trailing whitespace normalized only.
- workflow.txt: transitive-input coverage after adding the phase fixture.

Windows syntax checks also completed with exit0 against the pinned SDK using
MinGW. The external local Linux entry object, build commands and measurement
limits are the same as prior RC evidence. No Linux binary is shipped. The
one-second paced benchmark reports timer overhead and is not real DAW stress.
