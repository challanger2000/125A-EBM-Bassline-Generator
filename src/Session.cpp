#include "ebm/Session.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ebm::v2 {
namespace {
void checkConfig(const Config& cfg) {
    // The standalone composer is the single authority for range validation.
    (void)compose(cfg);
}
std::vector<Composition> materialize(const std::vector<Config>& history) {
    std::vector<Composition> out;
    out.reserve(history.size());
    for(const auto& cfg:history)out.push_back(compose(cfg));
    return out;
}
} // namespace
Session::Session(const Config& initial) : options_(initial) {checkConfig(options_);}
const Composition& Session::current() const {
    if(!current_)throw std::logic_error("no generated motif; press GENERATE first");
    return *current_;
}
GenerateInfo Session::generate() {
    // Keeping the bounded source history in Config form makes session save/recall
    // deterministic without exporting full event graphs or ephemeral pointers.
    const auto preceding=materialize(history_);
    auto outcome=composeNovel(options_,preceding);
    const Config chosen=outcome.composition.settings;
    // All potentially throwing work is done before modifying observable state.
    auto nextHistory=history_;
    nextHistory.push_back(chosen);
    if(nextHistory.size()>kMaximumHistory)nextHistory.erase(nextHistory.begin());
    auto nextCurrent=std::move(outcome.composition);
    const GenerateInfo info{chosen.seed,outcome.candidatesEvaluated,
                            outcome.minimumDistance,outcome.meetsTarget};
    options_.seed=chosen.seed+1ULL; // uint64 overflow is defined modulo 2^64.
    history_.swap(nextHistory);
    current_=std::move(nextCurrent);
    alternative_=0;
    return info;
}
bool Session::variate(int intensity) {
    if(!current_)throw std::logic_error("VARIATE requires a generated motif");
    Config selected=current_->settings;
    selected.variation=intensity;
    auto candidate=compose(selected); // Reject invalid intensity before mutation.
    const bool changed=render(candidate).notes!=render(*current_).notes;
    current_=std::move(candidate);
    alternative_=0;
    options_.variation=intensity;
    return changed;
}
bool Session::nextVariant() {
    if(!current_)throw std::logic_error("VARIATE requires a generated motif");
    if(current_->settings.variation==0)return false; // 0% is explicitly neutral.
    const Config selected=current_->settings;
    const Phrase previous=render(*current_);
    // Bounded alternatives; no implicit change of style, root or seed.
    for(std::uint32_t turn=1;turn<=5;++turn) {
        const std::uint32_t next=(alternative_+turn)%5;
        Composition candidate=composeAlternative(selected,static_cast<int>(next));
        if(render(candidate).notes==previous.notes)continue;
        current_=std::move(candidate);
        alternative_=next;
        return true;
    }
    return false; // Explicitly report exhaustion rather than a fake new variation.
}
void Session::setRootMidi(int key) {
    Config changed=options_;changed.rootMidi=key;
    checkConfig(changed);
    if(current_) {
        Config source=current_->settings;source.rootMidi=key;
        current_=composeAlternative(source,static_cast<int>(alternative_));
    }
    options_.rootMidi=key;
}
void Session::setStyle(Style style) {
    Config candidate=options_;candidate.style=style;
    checkConfig(candidate);
    if(options_.style!=style){current_.reset();alternative_=0;options_.style=style;}
}
void Session::setBars(int bars) {
    Config candidate=options_;candidate.bars=bars;
    checkConfig(candidate);
    if(options_.bars!=bars){current_.reset();alternative_=0;options_.bars=bars;}
}
void Session::clearCurrent(){current_.reset();alternative_=0;}
SessionState Session::snapshot() const {
    SessionState state;
    state.options=options_;
    state.hasCurrent=static_cast<bool>(current_);
    if(current_)state.current=current_->settings;
    state.alternative=alternative_;
    state.history=history_;
    return state;
}
void Session::restore(const SessionState& state) {
    if((state.version!=1&&state.version!=2) || state.history.size()>kMaximumHistory ||
       state.alternative>4 || (state.version==1&&state.alternative!=0) ||
       (!state.hasCurrent && state.alternative!=0))
        throw std::invalid_argument("unsupported or malformed session state");
    checkConfig(state.options);
    auto restoredHistory=state.history;
    (void)materialize(restoredHistory); // Validate every saved source before commit.
    std::optional<Composition> restoredCurrent;
    if(state.hasCurrent) {
        if(restoredHistory.empty())throw std::invalid_argument("orphan current riff");
        const auto& last=restoredHistory.back();
        const auto& cur=state.current;
        if(cur.seed!=last.seed||cur.style!=last.style||cur.bars!=last.bars||
           cur.style!=state.options.style||cur.bars!=state.options.bars||
           cur.rootMidi!=state.options.rootMidi||cur.variation!=state.options.variation)
            throw std::invalid_argument("current riff and session state disagree");
        if(cur.variation==0 && state.alternative!=0)
            throw std::invalid_argument("alternative cannot bypass neutral intensity");
        restoredCurrent=composeAlternative(cur,static_cast<int>(state.alternative));
    }
    // Copy/swap into live state only after all validation succeeds.
    history_.swap(restoredHistory);
    current_.swap(restoredCurrent);
    options_=state.options;
    alternative_=state.alternative;
}
} // namespace ebm::v2
