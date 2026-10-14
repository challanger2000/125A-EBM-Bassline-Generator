# Hierarchical four/eight-bar phrase-form audit — 2026-10-14

**Status:** locally implemented & technically tested; **musical quality NOT VERIFIED**. Not a VST3; not a release. This is a controlled change to *arrangement placement*, not source-note generation and not proof of groove or song-worthiness.

## WENN -> DANN observation

**Problem found in exact branch `f5dc4e970d45f59dc4788bf97c3a321ae44066c8`:** the first positive variation in an eight-bar composition was placed at bar **4** (zero-based index 3) for all three styles, leaving the actual final bar (bar 8, index 7) as a verbatim copy of the B source (or A in Minimal) at intensity 15. Thus the generator offered a variation of the *middle* while the ending contained no answering development. Higher intensities had a rigid arrangement that did not consistently distinguish midpoint, preparation and conclusion.

The following is a **documented compositional design hypothesis** (EMPIRICALLY TUNED), not an assertion that all EBM must use this form. Intentional Minimal exact repetition is retained at variation 0. The existing tonal intervals, source rhythms and note roles remain frozen.

**Intended 8-bar structure:** introduce the original one/two-bar motif; 1–24 performs a *terminal answering note* at bar 8; 25–49 also introduces a deliberate midpoint contrast in bar 4; 50–74 adds a preparatory approach in bar 7 for Minimal/Classic, or sparse answering space in bar 6 for Dark; 75–100 adds a secondary contrast in bar 6. Form schematically: A/AB introduction -> midpoint contrast -> preparation -> closing answer. The **first source motif(s) are never changed**.

**Intended 4-bar structure:** reserve the original first A(/B) as an identifiable introduction. At positive variation, final bar 4 carries an answer; upper levels add silence, anticipation or a second answer in the remaining permitted bars. No fixed cadence is imposed on the last note, and previously documented chromatic targets remain intact. Both lengths use the same five discrete intensity classes (0, 1–24, 25–49, 50–74, 75–100).

## Test evidence

- GCC C++17 Release compiled with strict warnings; `ctest` **6/6 PASS**. The new `tests/phrase_form_tests.cpp` evaluates 3 styles × 300 fixed seeds × 2 lengths × 5 levels = **9,000 compositions**, 7,200 adjacent-level comparisons, and the exact typed-edit placement for each style/length/intensity.
- GCC Debug AddressSanitizer and UndefinedBehaviorSanitizer with leak detection: `ctest` **6/6 PASS**. No reported issues in this suite.
- **Independent negative baseline test:** compiling the *new* phrase-form regression against the previous `Composer.cpp` fails at an expected positional assertion; this establishes that the test detects the reported defect rather than merely reciting output counts.
- **Fixed seed before/after:** 8-bar intensity 15, 300 seeds/style (seeds `314159+1019*style+i`, i=0..299): previously **0/300** terminal bars changed in each style; revised **300/300** terminal bars changed in each style. At this intensity the revised bars 1–7 remain exact repeats of their motif sources.
- **Unmodified-mode golden regression:** standalone independent renderer for three styles, 300 seeds, 4/8 bars, intensity 0: both the GitHub-head-matched pre-change working Composer and the new Composer produced **1,800 files**, aggregate **679,896 MIDI bytes**, identical FNV-1a 64-bit digest `a85f73001b25c2e1`. The **earlier pre-tonal-fix historical baseline** has another digest and must not be confused with the exact pre-change working source.
- Lineage reconstruction, source-device identity, tonal palette, valid note durations and cross-bar tension targets remain covered by the unchanged original suites.
- Tests were run locally on Linux. **GitHub Actions not run.** Windows / Studio One / VST3 not tested.

## What was NOT proved

- A distinct closing answer can be a *worse* riff than strict repetition. The new 8-bar pattern is one hypothesized formal device, not the exclusive proper form of EBM.
- The five steps are discrete, not a continuously responsive slider. A future GUI must present five meaningful settings or implement genuine continuity; 1% and 24% still yield the same result for a fixed seed.
- There are no level-matched, independently rated listening scores of the new full eight-bar arrangement. More structural intention is not a substitute for demonstrated musical usefulness.
- The source melody grammar and generic fixed 4/4 kick reference are still musically constrained; further evolution must be driven by evidence, not maximizing a metric.

**Next acceptance gate:** select at most three blinded A/B examples with identical seed, synthesizer and kick. Ask whether the changed **placement** makes the final bar more useful, not merely whether there are more altered events. Preserve results that favor the old form.
