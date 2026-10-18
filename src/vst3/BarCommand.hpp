#pragma once
#include "ebm/Session.hpp"
#include "ebm/EditPattern.hpp"

namespace ebm::vst3 {
// One policy shared by the BARS menu and Ctrl+BARS command.
// Engine composition must cover all visible bars, including bars 5-8.
inline void applyBars(v2::Session& session,edit::Pattern& pattern,int requestedBars) {
    const int engineBars=requestedBars<=4?4:8;
    if(session.options().bars!=engineBars)session.setBars(engineBars);
    if(!session.hasCurrent())(void)session.generate();
    pattern.bars=requestedBars;
}
}
