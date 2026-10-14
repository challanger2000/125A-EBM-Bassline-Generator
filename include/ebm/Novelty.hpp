#pragma once
#include "ebm/Composer.hpp"
#include <cstdint>
#include <vector>

namespace ebm::v2 {
int motifDistance(const Composition& a,const Composition& b);
// Number of source-motif onset positions that differ (0 = same rhythm).
// Independent of transposition, phrase variation and melodic pitch choices.
int motifRhythmDistance(const Composition& a,const Composition& b);
struct NoveltyResult {
    Composition composition;
    int minimumDistance = 0;     // Existing pitch/onset structural score.
    int minimumRhythmicDistance = 0; // Independent source-onset difference.
    bool meetsTarget = false;      // BOTH structure and rhythm gates met.
    bool meetsRhythmicTarget = false; // Rhythm screen alone met; may still fail structure.

    int candidatesEvaluated = 0;
};
NoveltyResult composeNovel(const Config& config,const std::vector<Composition>& previous);
} // namespace ebm::v2
