#pragma once
#include "ebm/LiveMidi.hpp"
#include "ebm/Session.hpp"
#include "ebm/EditPattern.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace ebm::host {
// Versioned, fixed-size persisted host document.  The code uses little endian
// explicitly and never serializes STL objects, padding or raw pointers.
struct Document {
    v2::SessionState session;
    live::Mode mode=live::Mode::Transport;
    edit::Pattern editable{};
};
constexpr std::size_t kMaximumHistory=48;
constexpr std::size_t kLegacyDocumentBytes=8+12+1+12+1+1+1+12*kMaximumHistory+4;
constexpr std::size_t kDocumentBytes=kLegacyDocumentBytes+5+edit::kMaxSteps*5;
using DocumentBytes=std::array<std::uint8_t,kDocumentBytes>;
bool encode(const Document& source,DocumentBytes& target) noexcept;
bool decode(const std::uint8_t* bytes,std::size_t count,Document& destination) noexcept;

// Single audio-thread consumer; serialized non-realtime producer(s).
// A slot is only ever written while FREE/WRITING, and the audio reader owns
// the READING slot until it adopts the next published pattern. No RT locks,
// heap allocations, atomic refcounts, or copying of large structures.
class PatternExchange final {
public:
    PatternExchange() noexcept = default;
    PatternExchange(const PatternExchange&)=delete;
    PatternExchange& operator=(const PatternExchange&)=delete;
    bool publish(const live::Pattern& prepared) noexcept;
    const live::Pattern* acquire(std::uint64_t& revision) noexcept;
private:
    enum : int { Free=0, Writing=1, Ready=2, Reading=3 };
    struct Slot {
        std::atomic<int> status{Free};
        live::Pattern data{};
    };
    std::array<Slot,3> slots_{};
    std::atomic<int> pending_{-1};
    std::mutex writerMutex_; // never acquired in acquire() or process()
    int active_=-1;         // owned by the one process thread
    std::uint64_t revision_=0;
};

// VST3-facing, host-neutral boundary. GUI/host state edits run off the audio
// callback; process() only consumes a published immutable event plan.
class Model final {
public:
    Model();
    Model(const Model&)=delete;
    Model& operator=(const Model&)=delete;
    bool restore(const Document& doc);
    Document snapshot() const;
    bool generate();
    bool variate(int intensity);
    bool nextVariant();
    void setMode(live::Mode mode) noexcept;
    live::Mode mode() const noexcept;
    int songStartBar() const noexcept {return songStart_.load(std::memory_order_acquire);}
    void resetRuntime() noexcept; // only after output bus disconnect
    template<class Sink>
    bool process(live::Block block,const live::Trigger* triggers,int count,Sink&& sink) noexcept {
        std::uint64_t rev=0;
        const auto* pattern=patterns_.acquire(rev);
        if(!pattern)return false;
        block.revision=rev;
        runtime_.setMode(mode());
        return runtime_.process(*pattern,block,triggers,count,sink);
    }
private:
    bool publishSession(const v2::Session& chosen);
    mutable std::mutex documentMutex_;
    v2::Session session_{};
    edit::Pattern editable_{};
    std::atomic<int> songStart_{1};
    std::atomic<live::Mode> mode_{live::Mode::Transport};
    PatternExchange patterns_{};
    live::Runtime runtime_{};
};
} // namespace ebm::host
