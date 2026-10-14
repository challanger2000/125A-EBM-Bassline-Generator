#pragma once
#include "ebm/Novelty.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace ebm::v2 {
// Value representation for later versioned host-state serialization.
// Not a VST3 binary state format and not usable in an audio callback.
struct SessionState {
    std::uint32_t version = 2;
    Config options{};                  // options.seed is the NEXT candidate seed.
    bool hasCurrent = false;
    Config current{};                  // Selected seed, current key and variation.
    std::uint32_t alternative = 0;    // 0 baseline, 1..4 musical alternatives.
    std::vector<Config> history;       // Chosen GEN seeds, newest last; max 48.
};
struct GenerateInfo {
    std::uint64_t selectedSeed = 0;
    int candidatesEvaluated = 0;
    int minimumDistance = 0;
    int minimumRhythmicDistance = 0;
    bool meetsRhythmicTarget = false;
    bool meetsNoveltyTarget = false;
};
class Session {
public:
    explicit Session(const Config& initial = {});
    // GENERATE chooses a new source motif against bounded prior source motifs.
    // Even if the requested threshold is not met, the best candidate is returned
    // and the failure is reported; it is NEVER silently labelled novel.
    GenerateInfo generate();
    // VARIATE keeps the selected seed and source motifs. A repeated discrete
    // intensity band returns false if its rendered MIDI is unchanged.
    bool variate(int intensity);
    // VARIATE button: cycle through real role-based alternatives of one seed.
    // Return false when intensity is neutral, without silently raising it.
    bool nextVariant();
    void setRootMidi(int key);
    void setStyle(Style style);        // Changes the context; requires GENERATE again.
    void setBars(int bars);            // Changes the context; requires GENERATE again.
    void clearCurrent();               // Retains history to discourage repeats.
    bool hasCurrent() const { return current_.has_value(); }
    const Composition& current() const;
    const Config& options() const { return options_; }
    std::uint32_t alternative() const { return alternative_; }
    SessionState snapshot() const;
    // Restores atomically: malformed/outdated snapshots leave existing state untouched.
    void restore(const SessionState& state);
private:
    static constexpr std::size_t kMaximumHistory = 48; // 8 each for 3 styles x 2 lengths
    Config options_;
    std::optional<Composition> current_;
    std::vector<Config> history_;
    std::uint32_t alternative_ = 0;
};
} // namespace ebm::v2
