#pragma once
#include "ebm/Harmony.hpp"
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
    int duration = 0; // gate in sixteenth steps, cached at NoteOn for edit continuity
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
    bool transportValid=false; // host transport state supplied; stopped live input is valid
    bool ppqValid=false;
    bool tempoValid=false;
    // Bump when replacing a prepared pattern. Keep same revision for read-only blocks.
    std::uint64_t revision=0;
    int gatePercent=100;
    int songRoot=48;
    edit::Scale scale=edit::Scale::Chromatic;
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
    int playheadStep() const noexcept {return triggerHeld_?static_cast<int>(std::floor(triggerPhase_))%16:-1;}
    // reset() ONLY after output disconnection/deactivation, not for live edits.
    void reset() noexcept {
        activePitch_=-1; activeBasePitch_=-1;activeDuration_=0;triggerHeld_=false; triggerPitch_=-1;
        phaseRunning_=false;haveTransportState_=false;wasPlaying_=false;
        triggerPhase_=0.;expectedPpq_=0.;haveExpected_=false;
        haveRevision_=false; failedOff_=false; seenMode_=mode_;
    }
    template<class Sink> bool stop(Sink& sink) noexcept {
        triggerHeld_=false;triggerPitch_=-1;phaseRunning_=false;triggerPhase_=0.;haveExpected_=false;
        return release(0,sink);
    }
    template<class Sink>
    bool process(const Pattern& pattern,const Block& block,
                 const Trigger* triggers,int triggerCount,Sink&& sink) noexcept {
        if (block.samples<0 || !std::isfinite(block.sampleRate) || block.sampleRate<=0. ||
            !std::isfinite(block.tempo) || block.tempo<=0. || block.tempo>1000. ||
            triggerCount<0 || triggerCount>maxTriggers ||
            block.gatePercent<0 || block.gatePercent>100 || block.songRoot<0 || block.songRoot>127 ||
            static_cast<int>(block.scale)<0 || static_cast<int>(block.scale)>=edit::kScaleCount ||
            (triggerCount>0 && triggers==nullptr) ||
            !pattern.ready || (pattern.bars!=1 && pattern.bars!=2 && pattern.bars!=4 && pattern.bars!=8) ||
            pattern.lengthSteps!=pattern.bars*16) {
            (void)stop(sink);return false;
        }
        if (failedOff_ && !release(0,sink)) return cancelInput();
        if (block.gatePercent==0 && !release(0,sink)) return cancelInput();
        if (mode_!=seenMode_) {
            if (!release(0,sink)) return cancelInput();
            triggerHeld_=false;triggerPitch_=-1;triggerPhase_=0.;
            phaseRunning_=false;haveTransportState_=false;
            haveExpected_=false;seenMode_=mode_;
        }
        if (!haveRevision_ || block.revision!=seenRevision_) {
            // Same-length revisions must not cut a currently ringing note:
            // keep the end step cached when that NoteOn was emitted.
            // A length change invalidates the cached modulo position, so send
            // one safe release without resetting trigger ownership or phase.
            if (haveRevision_ && seenLength_!=pattern.lengthSteps && !release(0,sink)) return cancelInput();
            haveRevision_=true;seenRevision_=block.revision;seenLength_=pattern.lengthSteps;
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
                if (!release(0,sink)) return cancelInput(); // seek, discontinuity, restart
            }
            expectedPpq_=block.projectPpq+block.samples*block.tempo/(60.*block.sampleRate);
            haveExpected_=true;
            return schedule(pattern,block,block.projectPpq*4.,0,block.samples,rate,sink);
        }
        haveExpected_=false;
        // A NoteOff is a gate closure, not a clock reset. Only an explicit
        // transport running -> stopped edge (or stop/lifecycle/mode/state)
        // ends the sequence. Constant stopped transport permits live playing.
        if(block.transportValid){
            const bool stopped=haveTransportState_&&wasPlaying_&&!block.playing;
            haveTransportState_=true;wasPlaying_=block.playing;
            if(stopped)return stop(sink); // stop dominates chase/input in this callback
        }
        if (triggerCount>0 && block.samples==0) return false;
        std::array<Trigger,maxTriggers> ordered{};
        for (int i=0;i<triggerCount;++i) {
            const Trigger e=triggers[i];
            if (e.offset<0 || e.offset>=block.samples || e.pitch<0 || e.pitch>127) {
                (void)stop(sink);return false;
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
            if (e.offset>pos) {
                if (triggerHeld_ && !schedule(pattern,block,triggerPhase_,pos,e.offset-pos,rate,sink))
                    return cancelInput();
                if (phaseRunning_)advance(e.offset-pos,rate,pattern.lengthSteps);
            }
            pos=e.offset;
            if (!e.on) {
                if (triggerHeld_ && triggerPitch_==e.pitch) {
                    // A MIDI chord change commonly sends old NoteOff and new
                    // NoteOn at the same sample. Keep the established phase.
                    bool replacement=false;
                    for(int next=i+1;next<triggerCount&&ordered[static_cast<std::size_t>(next)].offset==pos;++next)
                        replacement=replacement||ordered[static_cast<std::size_t>(next)].on;
                    if(!replacement){
                        // Close the sound gate immediately, including across
                        // callback boundaries. Keep the silent clock running:
                        // neither a short nor a long gap implies a restart.
                        triggerHeld_=false;triggerPitch_=-1;
                        if (!release(pos,sink)) return cancelInput();
                    }
                }
            } else if(!triggerHeld_) {
                if (!release(pos,sink)) return cancelInput();
                triggerHeld_=true;triggerPitch_=e.pitch;
                if(!phaseRunning_){triggerPhase_=0.;phaseRunning_=true;}
            } else if(e.pitch!=triggerPitch_) {
                // New chord root while a trigger is held: RETUNE, never seek.
                // Preserve the original sounding note's remaining gate.
                const int savedBase=activeBasePitch_,savedVel=activeVelocity_;
                const int savedDuration=activeDuration_;
                const double savedStart=activeStart_;
                if (!release(pos,sink))return cancelInput();
                triggerPitch_=e.pitch;
                if(savedBase>=0 && block.gatePercent>0 &&
                   triggerPhase_+1.e-8 < savedStart+savedDuration*block.gatePercent/100.){
                    const int mapped=harmony::chordPitch(savedBase,block.songRoot,e.pitch,block.scale);
                    if(!sink(Output{pos,mapped,savedVel,true}))return cancelInput();
                    activePitch_=mapped;activeBasePitch_=savedBase;
                    activeVelocity_=savedVel;activeDuration_=savedDuration;activeStart_=savedStart;
                }
            } // Repeated NoteOn for an already held pitch is harmless.
        }
        if (pos<block.samples) {
            if (triggerHeld_ && !schedule(pattern,block,triggerPhase_,pos,block.samples-pos,rate,sink))return cancelInput();
            if(phaseRunning_)advance(block.samples-pos,rate,pattern.lengthSteps);
        }
        return true;
    }
private:
    bool cancelInput() noexcept {
        triggerHeld_=false;triggerPitch_=-1;phaseRunning_=false;triggerPhase_=0.;return false;
    }
    template<class Sink> bool release(int offset,Sink& sink) noexcept {
        if (activePitch_<0) { failedOff_=false;return true; }
        if (!sink(Output{offset,activePitch_,0,false})) { failedOff_=true;return false; }
        activePitch_=-1;activeBasePitch_=-1;activeDuration_=0;failedOff_=false;return true;
    }
    void advance(int samples,double rate,int length) noexcept {
        triggerPhase_+=samples*rate;
        // Keep floating-point precision stable even for multi-day MIDI runs.
        if(triggerPhase_>8.*length){
            const double shift=std::floor(triggerPhase_/length-4.)*length;
            triggerPhase_-=shift;
            if(activePitch_>=0)activeStart_-=shift;
        }
    }
    template<class Sink>
    bool releaseDue(const Block& block,double segmentStart,double rate,
                    int offset,int sampleLimit,Sink& sink) noexcept {
        if(activePitch_<0)return true;
        const double end=activeStart_+activeDuration_*block.gatePercent/100.;
        const double due=std::max(0.,std::ceil((end-segmentStart)/rate-1.e-8));
        if(due<=sampleLimit)return release(offset+static_cast<int>(due),sink);
        return true;
    }
    template<class Sink>
    bool schedule(const Pattern& pattern,const Block& block,double start,int offset,
                  int count,double rate,Sink& sink) noexcept {
        if(count<=0)return true;
        if(!std::isfinite(start)||std::abs(start)>4.e8)return false;
        const double end=start+count*rate;
        const double firstDouble=std::ceil(start-rate+1.e-9);
        if(end-firstDouble>maxBoundariesPerBlock){
            (void)release(offset,sink);return false;
        }
        const auto first=static_cast<std::int64_t>(firstDouble);
        for(int i=0;i<maxBoundariesPerBlock;++i){
            const auto grid=first+i;
            if(static_cast<double>(grid)>=end-1.e-8)break;
            if(grid<0)continue;
            const int local=static_cast<int>(std::ceil((static_cast<double>(grid)-start)/rate-1.e-8));
            if(local<0||local>=count)continue;
            // A fractional NoteOff takes precedence over the next NoteOn.
            if(!releaseDue(block,start,rate,offset,local,sink))return false;
            const auto& cell=pattern.cells[static_cast<std::size_t>(grid%pattern.lengthSteps)];
            if(cell.onPitch>=0 && block.gatePercent>0){
                if(!release(offset+local,sink))return false;
                const int pitch=mode_==Mode::MidiNote && triggerHeld_?
                    harmony::chordPitch(cell.onPitch,block.songRoot,triggerPitch_,block.scale):cell.onPitch;
                if(!sink(Output{offset+local,pitch,cell.velocity,true}))return false;
                activePitch_=pitch;activeBasePitch_=cell.onPitch;
                activeVelocity_=cell.velocity;activeDuration_=cell.duration;
                activeStart_=static_cast<double>(grid);
            }
        }
        return releaseDue(block,start,rate,offset,count-1,sink);
    }

    Mode mode_=Mode::Transport,seenMode_=Mode::Transport;
    int activePitch_=-1,activeBasePitch_=-1,activeVelocity_=0,activeDuration_=0;
    int triggerPitch_=-1,seenLength_=0;
    double activeStart_=0.;
    bool phaseRunning_=false,haveTransportState_=false,wasPlaying_=false;
    bool triggerHeld_=false,haveExpected_=false,failedOff_=false,haveRevision_=false;
    double triggerPhase_=0.,expectedPpq_=0.;
    std::uint64_t seenRevision_=0;
};
} // namespace ebm::live
