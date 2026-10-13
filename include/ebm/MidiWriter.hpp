#pragma once
#include "ebm/Engine.hpp"
#include <cstdint>
#include <vector>
namespace ebm {
// Standard MIDI File format 0; PPQ 480; tempo 120 BPM; channel 1.
std::vector<std::uint8_t> toMidi(const Phrase& phrase);
} // namespace ebm
