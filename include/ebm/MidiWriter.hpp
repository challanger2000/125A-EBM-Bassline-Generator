#pragma once
#include "ebm/Engine.hpp"
#include <cstdint>
#include <vector>
namespace ebm {
// SMF format 0, 480 PPQ, 120 BPM, 4/4; tempo metadata independent of DAW tempo.
std::vector<std::uint8_t> toMidi(const Phrase&,int gatePercent=100);
}
