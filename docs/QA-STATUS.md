# Quality gate tracking – exploratory v0.1.0

| Gate | Status |
|---|---|
| C++17 engine, three provisional styles | IMPLEMENTED; Linux Release C++17 build PASS (GCC 14.2) |
| 4/8 bars, seed reproducibility, transposition | PASS local CTest: 1 suite, 180 seed/length cases plus adversarial contracts |
| Monophonic MIDI validity | PASS: independent Python mido parse of 90 files; monophony/timing/tempo/end-of-track verified |
| 90 unique 8-bar MIDI demonstrations | PASS technical: 90 unique MIDI byte hashes; 30 per style, generated locally |
| Musical usefulness and authenticity | NOT VERIFIED – user/music evaluation required |
| Steinberg VST3 validator | NOT APPLICABLE (no VST3 yet) |
| GUI, lifecycle, automation, Studio One | NOT IMPLEMENTED |
| Windows native build | NOT VERIFIED |
| FINAL / release | NOT READY |

No GitHub Actions builds are configured: conserve paid Windows CI until the musical candidate has earned it.
