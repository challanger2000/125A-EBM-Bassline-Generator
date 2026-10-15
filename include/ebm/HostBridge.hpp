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
    int gatePercent=100; // version 3; 100 = original LENGTH
};
constexpr std::size_t kMaximumHistory=48;
constexpr std::size_t kLegacyDocumentBytes=8+12+1+12+1+1+1+12*kMaximumHistory+4;
constexpr std::size_t kV2DocumentBytes=kLegacyDocumentBytes+5+edit::kMaxSteps*5;
constexpr std::size_t kDocumentBytes=kV2DocumentBytes+1;
using DocumentBytes=std::array<std::uint8_t,kDocumentBytes>;
bool encode(const Document& source,DocumentBytes& target) noexcept;
bool decode(const std::uint8_t* bytes,std::size_t count,Document& destination) noexcept;

// Single audio-thread consumer; serialized non-realtime producer(s).
// A slot is only ever written while FREE/WRITING, and the audio reader owns
// the READING slot until it adopts the next published pattern. No RT locks,
// heap allocations, atomic refcounts, or copying of large structures.
struct PreparedState {
    live::Pattern pattern{};
    int root=48;
    edit::Scale scale=edit::Scale::Chromatic;
    int songStart=1;
    int gate=100;
    live::Mode mode=live::Mode::Transport;
    std::uint64_t gateTicket=100,modeTicket=0;
    bool resetOnAdopt=false;
};
class PatternExchange final {
public:
    PatternExchange() noexcept = default;
    PatternExchange(const PatternExchange&)=delete;
    PatternExchange& operator=(const PatternExchange&)=delete;
    bool publish(const PreparedState& prepared) noexcept;
    const PreparedState* acquire(std::uint64_t& revision) noexcept;
private:
    enum : int { Free=0, Writing=1, Ready=2, Reading=3 };
    struct Slot {
        std::atomic<int> status{Free};
        PreparedState data{};
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
    bool restore(const Document& doc,bool restoreControls=true);
    Document snapshot() const;
    bool generate();
    bool variate(int intensity);
    bool nextVariant();
    bool setMode(live::Mode mode) noexcept;
    live::Mode mode() const noexcept;
    bool setGate(int value) noexcept;
    int gate() const noexcept;
    int songStartBar() const noexcept {return songStart_.load(std::memory_order_acquire);}
    int playheadStep() const noexcept {return playhead_;}
    void resetRuntime() noexcept;
    bool beginBlock() noexcept {
        activeState_=patterns_.acquire(activeRevision_);
        return activeState_!=nullptr;
    }
    template<class Sink> bool stop(Sink&& sink) noexcept {
        playhead_=-1;
        return runtime_.stop(sink);
    }
    template<class Sink>
    bool processPrepared(live::Block block,const live::Trigger* triggers,int count,Sink&& sink) noexcept {
        if(!activeState_)return false;
        const auto& state=*activeState_;
        // State replacement cleanup belongs to the same published revision.
        // Pattern-only edits do not request a reset of held MIDI ownership.
        if(state.resetOnAdopt&&resetRevision_!=activeRevision_){
            if(!runtime_.stop(sink))return false;
            resetRevision_=activeRevision_;
        }
        block.revision=activeRevision_;
        if(block.ppqValid)block.projectPpq-=4.*(state.songStart-1);
        const auto gateCommand=gateControl_.load(std::memory_order_acquire);
        const auto modeCommand=modeControl_.load(std::memory_order_acquire);
        block.gatePercent=gateCommand==state.gateTicket?state.gate:static_cast<int>(gateCommand&255u);
        const auto mode=modeCommand==state.modeTicket?state.mode:static_cast<live::Mode>(modeCommand&255u);
        block.songRoot=state.root;block.scale=state.scale;
        runtime_.setMode(mode);
        const bool ok=runtime_.process(state.pattern,block,triggers,count,sink);
        playhead_=-1;
        if(mode==live::Mode::MidiNote)playhead_=runtime_.playheadStep();
        else if(ok&&block.playing&&block.ppqValid&&block.tempoValid&&
                std::isfinite(block.projectPpq)&&block.projectPpq>=0.&&block.projectPpq<=1.e8)
            playhead_=static_cast<int>(static_cast<std::int64_t>(std::floor(block.projectPpq*4.))%16);
        return ok;
    }
    template<class Sink>
    bool process(live::Block block,const live::Trigger* triggers,int count,Sink&& sink) noexcept {
        return beginBlock()&&processPrepared(block,triggers,count,sink);
    }

private:
    bool publishSession(const v2::Session& chosen);
    mutable std::mutex documentMutex_;
    v2::Session session_{};
    edit::Pattern editable_{};
    std::atomic<int> songStart_{1};
    // Tickets distinguish a document restore from later independent automation.
    // UI pattern edits preserve the previous control baseline/tickets.
    PreparedState defaults_{}; // documentMutex_, never read by the audio thread
    std::atomic<std::uint64_t> gateControl_{100},modeControl_{0};
    std::atomic<std::uint64_t> gateDefault_{100},modeDefault_{0};
    const PreparedState* activeState_=nullptr;
    std::uint64_t activeRevision_=0,resetRevision_=0;
    int playhead_=-1;
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,"RT controls must be lock free");
    PatternExchange patterns_{};
    live::Runtime runtime_{};
};
} // namespace ebm::host
