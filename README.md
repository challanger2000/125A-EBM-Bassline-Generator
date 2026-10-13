# 125A EBM Bassline Generator

**Status: exploratory musical-engine prototype v0.1.0 — NOT a VST3, NOT FINAL.**

Standalone C++17 composition engine to determine whether three provisional EBM-inspired bassline styles can produce useful musical material *before* investing in GUI/VST3 work.

## Output

MINIMAL_BODY, CLASSIC_EBM, DARK_ELECTRO produce deterministic monophonic 4- or 8-bar MIDI phrases from an explicit seed, root note and controlled phrase development. MIDI files are Standard MIDI Files (format 0) with 480 PPQ, one channel and 120 BPM tempo metadata. Generated material is original procedural data, not copied MIDI transcriptions.

## Build and preview

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
    ./build/ebm_batch previews

On Windows multi-config generators the batch tool is typically at build/Release/ebm_batch.exe. The 90 preview MIDIs can be imported directly into a DAW; no terminal is needed to audition them once generated.

See [musical design](docs/MUSICAL-SPEC.md) and [QA gates](docs/QA-STATUS.md). Engineering policy is defined by challanger2000/125A-Engineering/START-HERE.md and its references. No GitHub Actions workflow has been added.
