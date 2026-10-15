#pragma once
#include "ebm/Engine.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace ebm::live {
// Transport adapter: compile on the control thread, publish a stable snapshot
// before process(); neither generate(), compose() nor file I/O belongs here.
struct GridCell {
    int onPitch = -1;
    int offPitch = -1;
    int velocity = 0;
};
struct Pattern {
    static constexpr int maxSteps = 128; // 8 bars, 16 sixteenths per bar
    int bars = 0;
    int lengthSteps = 0;
    bool ready = false;
    std::array<GridCell,maxSteps> cells{};
};
// Failure leaves the old snapshot intact. No vector construction or allocation.
bool prepare(const Phrase& phrase,Pattern& dest) noexcept;

enum class Mode : std::uint8_t { Transport, MidiNote };
struct Trigger { int offset=0, pitch=0; bool on=false; };
struct Output { int offset=0, pitch=0, velocity=0; bool on=false; };
struct Block {
    int samples=0;
    double sampleRate=44100.;
    double tempo=120.;
    double projectPpq=0.; // absolute DAW quarter notes
    bool playing=false;
    bool ppqValid=false;
    bool tempoValid=false;
    // Bump when replacing a prepared pattern. Keep same revision for read-only blocks.
    std::uint64_t revision=0;
};

// No allocations, locks, I/O or exceptions in process(). Sink must also be
// noexcept and bounded; failure of NoteOff is retried before further NoteOn.
class Runtime final {
public:
    static constexpr int maxTriggers=64;
    static constexpr int maxBoundariesPerBlock=256;
    void setMode(Mode mode) noexcept { mode_=mode; }
    Mode mode() const noexcept { return mode_; }
    int activePitch() const noexcept { return activePitch_; }
    bool triggerHeld() const noexcept { return triggerHeld_; }
    // reset() ONLY after output disconnection/deactivation, not for live edits.
    void reset() noexcept {
        activePitch_=-1; triggerHeld_=false; triggerPitch_=-1;
        triggerPhase_=0.;expectedPpq_=0.;haveExpected_=false;
        haveRevision_=false; failedOff_=false; seenMode_=mode_;
    }
    template<class Sink>
    bool process(const Pattern& pattern,const Block& block,
                 const Trigger* triggers,int triggerCount,Sink&& sink) noexcept {
        if (block.samples<0 || !std::isfinite(block.sampleRate) || block.sampleRate<=0. ||
            !std::isfinite(block.tempo) || block.tempo<=0. || block.tempo>1000. ||
            triggerCount<0 || triggerCount>maxTriggers ||
            (triggerCount>0 && triggers==nullptr) ||
            !pattern.ready || (pattern.bars!=1 && pattern.bars!=2 && pattern.bars!=4 && pattern.bars!=8) ||
            pattern.lengthSteps!=pattern.bars*16) {
            haveExpected_=false;
            if (!release(0,sink)) return false;
            triggerHeld_=false;triggerPitch_=-1;return false;
        }
        if (failedOff_ && !release(0,sink)) return false;
        if (mode_!=seenMode_) {
            if (!release(0,sink)) return false;
            triggerHeld_=false;triggerPitch_=-1;triggerPhase_=0.;
            haveExpected_=false;seenMode_=mode_;
        }
        if (!haveRevision_ || block.revision!=seenRevision_) {
            if (haveRevision_ && !release(0,sink)) return false; // keep trigger/phase, release only voice
            haveRevision_=true;seenRevision_=block.revision;
            // In MidiNote mode retain the held trigger and musical phase.
        }
        const double rate=(block.tempo*4.)/(60.*block.sampleRate); // sixteenths per sample
        if (!std::isfinite(rate) || rate<=0.) return false;
        if (mode_==Mode::Transport) {
            triggerHeld_=false;triggerPitch_=-1;
            if (!block.playing || !block.ppqValid || !block.tempoValid ||
                !std::isfinite(block.projectPpq) || std::abs(block.projectPpq)>1.e8) {
                haveExpected_=false;
                return release(0,sink);
            }
            if (haveExpected_ && std::abs(block.projectPpq-expectedPpq_)>1.e-5) {
                if (!release(0,sink)) return false; // seek, discontinuity, restart
            }
            expectedPpq_=block.projectPpq+block.samples*block.tempo/(60.*block.sampleRate);
            haveExpected_=true;
            return schedule(pattern,block.projectPpq*4.,0,block.samples,rate,sink);
        }
        haveExpected_=false;
        if (triggerCount>0 && block.samples==0) return false;
        std::array<Trigger,maxTriggers> ordered{};
        for (int i=0;i<triggerCount;++i) {
            const Trigger e=triggers[i];
            if (e.offset<0 || e.offset>=block.samples || e.pitch<0 || e.pitch>127) {
                if (!release(0,sink)) return false;
                triggerHeld_=false;triggerPitch_=-1;triggerPhase_=0.;return false;
            }
            int j=i;
            while (j>0 && (ordered[static_cast<std::size_t>(j-1)].offset>e.offset ||
                   (ordered[static_cast<std::size_t>(j-1)].offset==e.offset &&
                    ordered[static_cast<std::size_t>(j-1)].on && !e.on))) {
                ordered[static_cast<std::size_t>(j)]=ordered[static_cast<std::size_t>(j-1)];--j;
            }
            ordered[static_cast<std::size_t>(j)]=e;
        }
        int pos=0;
        for (int i=0;i<triggerCount;++i) {
            const Trigger e=ordered[static_cast<std::size_t>(i)];
            if (triggerHeld_ && e.offset>pos) {
                if (!schedule(pattern,triggerPhase_,pos,e.offset-pos,rate,sink)) return false;
                advance(e.offset-pos,rate,pattern.lengthSteps);
            }
            pos=e.offset;
            if (!e.on) {
                if (triggerHeld_ && triggerPitch_==e.pitch) {
                    if (!release(pos,sink)) return false;
                    triggerHeld_=false;triggerPitch_=-1;triggerPhase_=0.;
                }
            } else {
                if (!release(pos,sink)) return false;
                triggerHeld_=true;triggerPitch_=e.pitch;triggerPhase_=0.;
            }
        }
        if (triggerHeld_ && pos<block.samples) {
            if (!schedule(pattern,triggerPhase_,pos,block.samples-pos,rate,sink))return false;
            advance(block.samples-pos,rate,pattern.lengthSteps);
        }
        return true;
    }
private:
    template<class Sink> bool release(int offset,Sink& sink) noexcept {
        if (activePitch_<0) { failedOff_=false;return true; }
        if (!sink(Output{offset,activePitch_,0,false})) { failedOff_=true;return false; }
        activePitch_=-1;failedOff_=false;return true;
    }
    void advance(int samples,double rate,int length) noexcept {
        triggerPhase_=std::fmod(triggerPhase_+samples*rate,static_cast<double>(length));
    }
    template<class Sink>
    bool schedule(const Pattern& pattern,double start,int offset,int count,double rate,Sink& sink) noexcept {
        if (count<=0)return true;
        if (!std::isfinite(start) || std::abs(start)>4.e8) return false;
        const double end=start+count*rate;
        // A grid event between samples N-1 and N belongs to sample N.
        // Include the preceding sub-sample interval to avoid missing it at block boundaries.
        const double firstDouble=std::ceil(start-rate+1.e-9);
        if (end-firstDouble>maxBoundariesPerBlock) {
            (void)release(offset,sink);return false;
        }
        const auto first=static_cast<std::int64_t>(firstDouble);
        for (int i=0;i<maxBoundariesPerBlock;++i) {
            const auto grid=first+i;
            if (static_cast<double>(grid)>=end-1.e-8)break;
            if (grid<0)continue;
            const double relative=(static_cast<double>(grid)-start)/rate;
            const int local=static_cast<int>(std::ceil(relative-1.e-8));
            if (local<0 || local>=count)continue;
            const auto& cell=pattern.cells[static_cast<std::size_t>(grid%pattern.lengthSteps)];
            if (cell.offPitch>=0 && cell.offPitch==activePitch_) {
                if (!release(offset+local,sink))return false;
            }
            if (cell.onPitch>=0) {
                if (!release(offset+local,sink))return false;
                if (!sink(Output{offset+local,cell.onPitch,cell.velocity,true}))return false;
                activePitch_=cell.onPitch;
            }
        }
        return true;
    }
    Mode mode_=Mode::Transport,seenMode_=Mode::Transport;
    int activePitch_=-1,triggerPitch_=-1;
    bool triggerHeld_=false,haveExpected_=false,failedOff_=false,haveRevision_=false;
    double triggerPhase_=0.,expectedPpq_=0.;
    std::uint64_t seenRevision_=0;
};
} // namespace ebm::live
