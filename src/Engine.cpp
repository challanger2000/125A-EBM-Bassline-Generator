#include "ebm/Engine.hpp"
#include <algorithm>
#include <stdexcept>

namespace ebm {
namespace {
// Fixed PRNG/selection arithmetic: deterministic across standard libraries.
class Random {
public:
    explicit Random(std::uint64_t seed) : state_(seed ? seed : 0x9e3779b97f4a7c15ULL) {}
    std::uint32_t next() {
        std::uint64_t x = state_;
        x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
        state_ = x;
        return static_cast<std::uint32_t>((x * 2685821657736338717ULL) >> 32);
    }
    int range(int upperExclusive) { return static_cast<int>(next() % static_cast<std::uint32_t>(upperExclusive)); }
    bool chance(int percentage) { return range(100) < percentage; }
private:
    std::uint64_t state_;
};
struct Cell { int step, interval, velocity, duration; };
using Motif = std::vector<Cell>;

int weightedPick(Random& rng, const int* values, const int* weights, int count) {
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += weights[i];
    int roll = rng.range(sum);
    for (int i = 0; i < count; ++i) {
        if (roll < weights[i]) return values[i];
        roll -= weights[i];
    }
    return values[count - 1];
}

// Bass-first contour. Controlled intervals are *not* recordings of existing songs.
int intervalFor(Random& rng, Style style, int step, int ordinal, int prev) {
    if (step == 0) return 0;
    if (style == Style::MinimalBody) {
        constexpr int choices[] = {0, 0, 0, 3, 7, 12, -1};
        constexpr int weights[] = {20, 16, 16, 8, 8, 4, 3};
        int interval = weightedPick(rng, choices, weights, 7);
        // Rare passing semitone belongs near the end of the bar.
        if (interval == -1 && step < 12) interval = 0;
        return interval;
    }
    if (style == Style::ClassicEBM) {
        constexpr int choices[] = {0, 3, 7, 12, -1};
        const int weights[] = {step % 4 == 0 ? 20 : 31, 11, 11,
                               step % 4 == 0 ? 19 : 10, step >= 12 ? 5 : 0};
        return weightedPick(rng, choices, weights, 5);
    }
    // Dark Electro: contour and directed stepwise movement instead of independent notes.
    constexpr int choices[] = {0, 3, 5, 7, 10, 12, -1};
    int weights[] = {14, 15, 7, 13, 6, 8, step >= 12 ? 7 : 0};
    if (prev >= 10) { weights[0] += 9; weights[2] += 8; weights[5] = 1; }
    if (ordinal >= 4 && step >= 12) { weights[0] += 18; weights[6] += 8; }
    return weightedPick(rng, choices, weights, 7);
}

Motif composeBar(Random& rng, Style style, int barIndex) {
    Motif result;
    const int n = style == Style::MinimalBody ? 7 + rng.range(3)
                : style == Style::ClassicEBM ? 10 + rng.range(3) : 6 + rng.range(3);
    bool active[16] = {};
    active[0] = true;
    if (style == Style::ClassicEBM) { active[4] = true; active[8] = true; active[12] = true; }
    if (style == Style::MinimalBody) { active[8] = true; }
    if (style == Style::DarkElectro) { active[8] = true; }
    int selected = 0;
    for (bool a : active) if (a) ++selected;
    // Weighted without replacement, enforcing diversity in rhythmic onset placement.
    int weights[16] = {};
    for (int i = 1; i < 16; ++i) {
        if (style == Style::MinimalBody) weights[i] = (i % 2 == 0 ? 12 : 5) + (i % 4 == 0 ? 5 : 0);
        else if (style == Style::ClassicEBM) weights[i] = (i % 4 == 0 ? 12 : 10) + (i % 4 == 3 ? 9 : 0);
        else weights[i] = (i % 4 == 0 ? 15 : (i % 4 == 3 ? 12 : 7));
    }
    while (selected < n) {
        int total = 0;
        for (int i = 1; i < 16; ++i) if (!active[i]) total += weights[i];
        int k = rng.range(total);
        for (int i = 1; i < 16; ++i) {
            if (active[i]) continue;
            if (k < weights[i]) { active[i] = true; ++selected; break; }
            k -= weights[i];
        }
    }
    int ordinal = 0, lastInterval = 0;
    for (int s = 0; s < 16; ++s) {
        if (!active[s]) continue;
        int interval = intervalFor(rng, style, s, ordinal, lastInterval);
        if (style == Style::DarkElectro && barIndex == 1 && s == 0) interval = 7;
        const int velocity = 72 + rng.range(22) + (s % 4 == 0 ? 20 : (s % 2 == 0 ? 8 : 0));
        const int maximum = style == Style::DarkElectro ? 3 : (style == Style::MinimalBody ? 2 : 1);
        result.push_back({s, interval, std::min(127, velocity), 1 + rng.range(maximum)});
        lastInterval = interval;
        ++ordinal;
    }
    // Force a recognizable melodic counterpoint if stochastic choices all hit the pedal.
    const int anchorPitch = result.front().interval;
    const bool anyContrast = std::any_of(result.begin(), result.end(),
        [anchorPitch](const Cell& c) { return c.interval != anchorPitch; });
    if (!anyContrast) result[result.size() / 2].interval =
        style == Style::DarkElectro ? 3 : 7;
    // One voice, no overlapping note-ons: truncate note lengths at next onset.
    for (std::size_t i = 0; i < result.size(); ++i) {
        const int next = i + 1 < result.size() ? result[i + 1].step : 16;
        result[i].duration = std::min(result[i].duration, next - result[i].step);
    }
    // Tonal resolution is intentionally explicit for the final beat of bar 2.
    if (style == Style::DarkElectro && barIndex == 1) result.back().interval = 0;
    return result;
}

Motif develop(const Motif& source, Random& rng, Style style, int intensity, bool cadence) {
    Motif out = source;
    if (intensity == 0) return out;
    // Preserve the onset grid and most pitches; change up to two late motif notes.
    const int changes = (intensity >= 65 && out.size() >= 8) ? 2 : 1;
    for (int i = 0; i < changes; ++i) {
        if (!rng.chance(intensity)) continue;
        const std::size_t first = out.size() / 2;
        const std::size_t pick = first + static_cast<std::size_t>(rng.range(static_cast<int>(out.size() - first)));
        auto& c = out[pick];
        constexpr int minimal[] = {0, 3, 7, 12};
        constexpr int classic[] = {0, 3, 7, 12};
        constexpr int dark[] = {0, 3, 5, 7, 10, 12, -1};
        if (style == Style::MinimalBody) c.interval = minimal[rng.range(4)];
        else if (style == Style::ClassicEBM) c.interval = classic[rng.range(4)];
        else c.interval = dark[rng.range(7)];
        c.velocity = std::min(127, c.velocity + (rng.chance(50) ? 9 : -9));
    }
    if (cadence) out.back().interval = 0; // predictable end-of-phrase landing.
    return out;
}
} // namespace

const char* styleName(Style style) {
    switch (style) {
        case Style::MinimalBody: return "MINIMAL_BODY";
        case Style::ClassicEBM: return "CLASSIC_EBM";
        case Style::DarkElectro: return "DARK_ELECTRO";
    }
    return "INVALID";
}

Phrase generate(const Config& config) {
    if (config.bars != 4 && config.bars != 8) throw std::invalid_argument("bars must be 4 or 8");
    if (config.rootMidi < 24 || config.rootMidi > 60) throw std::invalid_argument("rootMidi must be 24..60");
    if (config.variation < 0 || config.variation > 100) throw std::invalid_argument("variation must be 0..100");
    if (config.style != Style::MinimalBody && config.style != Style::ClassicEBM &&
        config.style != Style::DarkElectro) throw std::invalid_argument("invalid style");
    Random rng(config.seed);
    const int motifBars = config.style == Style::MinimalBody ? 1 : 2;
    Motif original[2] = {composeBar(rng, config.style, 0), {}};
    if (motifBars == 2) original[1] = composeBar(rng, config.style, 1);
    Phrase phrase{config.bars, {}};
    phrase.notes.reserve(static_cast<std::size_t>(config.bars * 14));
    for (int bar = 0; bar < config.bars; ++bar) {
        const int localBar = bar % motifBars;
        const bool ending = bar == config.bars - 1;
        const bool alternate = bar >= motifBars && ((bar % 4 == 3) || (bar >= 4 && bar % 2 == 1));
        const Motif cells = alternate ? develop(original[localBar], rng, config.style,
                                               config.variation, ending) : original[localBar];
        for (const auto& c : cells) {
            phrase.notes.push_back({bar * 16 + c.step, config.rootMidi + c.interval,
                                    c.velocity, c.duration});
        }
    }
    return phrase;
}
} // namespace ebm
