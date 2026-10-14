#pragma once
#include "ebm/Composer.hpp"
#include <cstdint>
#include <vector>

namespace ebm::v2 {
int motifDistance(const Composition& a,const Composition& b);
// Number of source-motif onset positions that differ (0 = same rhythm).
// Independent of transposition, phrase variation and melodic pitch choices.
int motifRhythmDistance(const Composition& a,const Composition& b);
// Change in the rhythmic backbone (Anchor/Drive/Answer/Resolution), ignoring
// decorative Pickup and Tension. This is NOT a subjective groove grade.
int motifCoreRhythmDistance(const Composition& a,const Composition& b);
struct NoveltyResult {
    Composition composition;
    int minimumDistance = 0;     // Existing pitch/onset structural score.
    int minimumRhythmicDistance = 0; // Independent source-onset difference.
    int minimumCoreRhythmicDistance = 0; // Independent non-ornamental source-onset difference.
    bool meetsTarget = false;      // All THREE structure, full-rhythm and backbone gates met.
    bool meetsRhythmicTarget = false; // Full rhythm screen alone met.
    bool meetsCoreRhythmicTarget = false; // Backbone screen alone met.

    int candidatesEvaluated = 0;
};
NoveltyResult composeNovel(const Config& config,const std::vector<Composition>& previous);
} // namespace ebm::v2
