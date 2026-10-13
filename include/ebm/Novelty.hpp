#pragma once
#include "ebm/Composer.hpp"
#include <cstdint>
#include <vector>

namespace ebm::v2 {
// Count changes of rhythmic onsets and pitch contours in the *source* motif.
// Velocities, transposition, and downstream arrangement edits do not count.
// This is a diagnostic, not a subjective measure of musical quality.
int motifDistance(const Composition& a,const Composition& b);
struct NoveltyResult {
    Composition composition;
    int minimumDistance = 0;
    bool meetsTarget = false;
    int candidatesEvaluated = 0;
};
// Bounded, deterministic and non-realtime selection for a future GENERATE button.
// Compares against the last eight same-style/length phrases, not all historic outputs.
// Makes no promise that the returned phrase is objectively musically compelling.
NoveltyResult composeNovel(const Config& config,const std::vector<Composition>& previous);
} // namespace ebm::v2
