#pragma once
#include "ebm/Engine.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace ebm::v2 {
// Purely musical intermediate representation. One tick is a 16th in 4/4.
enum class Role : std::uint8_t { Anchor, Drive, Pickup, Answer, Tension, Resolution };
enum class Operation : std::uint8_t { Repeat, Omit, Anticipate, Answer };
// Idiom-specific structural devices, not genre fingerprints or artist presets.
enum class Device : std::uint8_t {
    PedalGap, DelayedBackbeat, CrossPulse,
    MotorLock, BreakAndShout, OctaveInterlock,
    ClimbRelease, LateSuspension, BrokenAnswer
};
struct Event {
    int id = -1;           // Stable within a motif, survives transformations.
    int step = 0;          // 0..15 inside the bar.
    int interval = 0;      // Semitones relative to root.
    int duration = 1;      // 16th-notes, deliberately includes rests.
    int velocity = 100;
    Role role = Role::Drive;
    int targetId = -1;    // For Tension, must resolve at step > this note.
    int targetBarDelta = 0; // 0 = same bar; 1 = following bar (loop aware).
    bool operator==(const Event& o) const;
};
struct Bar {
    char motif = 'A';     // Identifies the phrase motif source, not a reference recording.
    Device device = Device::PedalGap;  // Intentional rhythmic grammar of this motif.
    bool transformed = false;
    std::vector<Event> notes;
    bool operator==(const Bar& o) const;
};
struct Edit {
    int bar = 0;
    Operation operation = Operation::Repeat;
    int eventId = -1;
    int previousStep = -1;
    int newStep = -1;
};
struct Composition {
    Config settings{};
    std::vector<Bar> bars;
    std::vector<Edit> edits;
    int kickSteps[4] = {0,4,8,12};
};
// Independent stylistic grammars compose original event relationships.
Composition compose(const Config& config);
Phrase render(const Composition& composition);
// Throws std::invalid_argument for a broken musical contract.
void validate(const Composition& composition);
const char* roleName(Role role);
const char* operationName(Operation operation);
const char* deviceName(Device device);
} // namespace ebm::v2
