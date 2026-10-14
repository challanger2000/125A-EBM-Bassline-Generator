#include "ebm/Composer.hpp"
#include <algorithm>
#include <initializer_list>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

namespace ebm::v2 {
bool Event::operator==(const Event& o) const {
    return id == o.id && step == o.step && interval == o.interval &&
           duration == o.duration && velocity == o.velocity && role == o.role &&
           targetId == o.targetId && targetBarDelta == o.targetBarDelta;
}
bool Bar::operator==(const Bar& o) const {
    return motif == o.motif && device == o.device && transformed == o.transformed && notes == o.notes;
}
const char* roleName(Role r) {
    switch (r) {
        case Role::Anchor: return "ANCHOR";
        case Role::Drive: return "DRIVE";
        case Role::Pickup: return "PICKUP";
        case Role::Answer: return "ANSWER";
        case Role::Tension: return "TENSION";
        case Role::Resolution: return "RESOLUTION";
    }
    return "UNKNOWN";
}
const char* operationName(Operation op) {
    switch (op) {
        case Operation::Repeat: return "REPEAT";
        case Operation::Omit: return "OMIT";
        case Operation::Anticipate: return "ANTICIPATE";
        case Operation::Answer: return "ANSWER";
        case Operation::Reaccent: return "REACCENT";
    }
    return "UNKNOWN";
}
const char* deviceName(Device device) {
    switch(device) {
        case Device::PedalGap: return "PEDAL_GAP";
        case Device::DelayedBackbeat: return "DELAYED_BACKBEAT";
        case Device::CrossPulse: return "CROSS_PULSE";
        case Device::MotorLock: return "MOTOR_LOCK";
        case Device::BreakAndShout: return "BREAK_AND_SHOUT";
        case Device::OctaveInterlock: return "OCTAVE_INTERLOCK";
        case Device::ClimbRelease: return "CLIMB_RELEASE";
        case Device::LateSuspension: return "LATE_SUSPENSION";
        case Device::BrokenAnswer: return "BROKEN_ANSWER";
    }
    return "UNKNOWN";
}
namespace {
class Rng {
public:
    explicit Rng(std::uint64_t seed) : x_(seed ? seed : 0x9e3779b97f4a7c15ULL) {}
    std::uint32_t next() {
        x_ ^= x_ >> 12; x_ ^= x_ << 25; x_ ^= x_ >> 27;
        return static_cast<std::uint32_t>((x_ * 2685821657736338717ULL) >> 32);
    }
    int choose(int count) { return static_cast<int>(next() % static_cast<std::uint32_t>(count)); }
    int among(std::initializer_list<int> values) {
        const auto it = values.begin();
        return it[choose(static_cast<int>(values.size()))];
    }
private:
    std::uint64_t x_;
};
Event make(int id,int pos,int pitch,Role role,int velocity,int gate = 1) {
    return Event{id,pos,pitch,gate,velocity,role,-1,0};
}
void target(Event& e,int id,int barDelta) { e.targetId=id; e.targetBarDelta=barDelta; }
void finish(Bar& bar) {
    std::sort(bar.notes.begin(),bar.notes.end(),[](const Event& a,const Event& b){return a.step<b.step;});
    for(std::size_t i=0;i<bar.notes.size();++i) {
        const int next=(i+1<bar.notes.size())?bar.notes[i+1].step:16;
        bar.notes[i].duration=std::min(bar.notes[i].duration,next-bar.notes[i].step);
    }
}
// A role and beat relationship always determines the next event; never sample each of 16 cells.
// Devices are compositional schemas with deliberate beat relationships.
// Variation *inside* a schema changes placement within named musical roles,
// rather than flipping 16 independent step probabilities.
Bar minimal(Rng& rng) {
    Bar a; a.motif='A';
    const int grammar = rng.choose(3);
    int lowEcho, syncopated, secondAnchor, answerStep, leadingStep;
    switch(grammar) {
        case 0: // A gap at the kick on 4; an answer after the kick on 8.
            a.device=Device::PedalGap;
            lowEcho=rng.among({1,2,3});
            syncopated=rng.among({5,6});
            secondAnchor=8;
            answerStep=rng.among({10,11,12,13});
            leadingStep=rng.among({14,15});
            break;
        case 1: // Delay the second anchor until beat 4; fill previous space.
            a.device=Device::DelayedBackbeat;
            lowEcho=rng.among({1,2,3});
            syncopated=rng.among({6,7,8});
            secondAnchor=12;
            answerStep=rng.among({13,14});
            leadingStep=15;
            break;
        default: // Cross-pulse answer arrives much later than the anchor.
            a.device=Device::CrossPulse;
            lowEcho=rng.among({1,2,3});
            syncopated=4; // Contrasting bass/kick co-onset, unlike PEDAL_GAP.
            secondAnchor=8;
            answerStep=rng.among({12,13});
            leadingStep=rng.among({14,15});
            break;
    }
    const int upper = rng.among({3,5,7,12});
    a.notes.push_back(make(1,0,0,Role::Anchor,120,rng.among({1,2})));
    a.notes.push_back(make(2,lowEcho,0,Role::Drive,91,1));
    a.notes.push_back(make(3,syncopated,0,Role::Drive,106,1));
    a.notes.push_back(make(4,secondAnchor,0,Role::Anchor,116,rng.among({1,2})));
    a.notes.push_back(make(5,answerStep,upper,Role::Answer,103,1));
    // PedalGap/DelayedBackbeat conclude with air; a compulsory chromatic
    // pickup every bar would turn a purposeful hypnotic rest into a cliche.
    // CrossPulse deliberately carries an approach into the next downbeat.
    if(a.device==Device::CrossPulse) {
        auto note=make(6,leadingStep,-1,Role::Tension,85,1);
        target(note,1,1); a.notes.push_back(note);
    } else if(a.device==Device::PedalGap) {
        // Three deliberate conclusions: leave space, an early tonic return
        // followed by a rest, or a tonic pickup just before the downbeat.
        // The latter two are *rhythmic* choices, not the mandatory chromatic
        // -1 semitone found in every source bar of the previous prototype.
        const int ending=rng.choose(3);
        if(ending==1 && answerStep<=12)
            a.notes.push_back(make(6,14,0,Role::Pickup,85));
        else if(ending==2)
            a.notes.push_back(make(6,15,0,Role::Pickup,88));
    }
    finish(a);
    return a;
}
std::pair<Bar,Bar> classic(Rng& rng) {
    Bar a,b;a.motif='A';b.motif='B';
    // Structural rhythm devices are deliberate accent/rest patterns, not the
    // same constant grid with a changed onset-density parameter.
    const int grammar=rng.choose(3);
    std::array<int,8> at{};
    std::array<int,9> bt{};
    switch(grammar) {
        case 0: // Pedal pulse and offbeat fifth answers.
            a.device=b.device=Device::MotorLock;
            at={{0,2,4,7,10,12,14,15}};
            bt={{0,3,4,6,7,9,11,12,15}};
            break;
        case 1: // Strongly interrupted phrase and a more active answering bar.
            a.device=b.device=Device::BreakAndShout;
            at={{0,3,4,7,9,12,14,15}};
            bt={{0,2,4,6,7,8,10,12,15}};
            break;
        default: // Upper answer coincides with the kick on 8, not as a fill.
            a.device=b.device=Device::OctaveInterlock;
            at={{0,2,4,6,8,12,13,15}};
            bt={{0,3,4,6,7,9,10,12,15}};
            break;
    }
    // Within each device, choose between written *phrase positions*.
    // Each choice preserves the intentional empty beats and guarantees that
    // a later ANTICIPATE can move the anchor at step 12 into free step 11.
    at[1]=rng.among({1,2,3});
    at[3]=rng.among({6,7});
    at[6]=rng.among({13,14});
    bt[1]=rng.among({1,2,3});
    bt[3]=rng.among({5,6});
    // Device-defining syncopation is *invariant* under the other choices.
    // MOTOR skips kick-8 in its answering bar, BREAK lands on kick-8,
    // INTERLOCK places an upper response on kick-8 in the first bar.
    if(grammar==0) { at[4]=10;bt[5]=9;bt[6]=11; }
    else if(grammar==1) { at[4]=9;bt[5]=8;bt[6]=10; }
    else { at[4]=8;bt[5]=9;bt[6]=11; }
    const int upper=rng.among({7,12});
    const int color=rng.among({3,5,7});
    const int bPulse=rng.among({0,5});
    a.notes={make(1,at[0],0,Role::Anchor,121),
             make(2,at[1],upper,Role::Answer,108),
             make(3,at[2],0,Role::Anchor,117,2),
             make(4,at[3],0,Role::Pickup,113),
             make(5,at[4],upper,Role::Answer,104),
             make(6,at[5],0,Role::Anchor,120),
             make(7,at[6],color,Role::Answer,97),
             make(8,at[7],0,Role::Pickup,106)};
    b.notes={make(101,bt[0],bPulse,Role::Anchor,121,2),
             make(102,bt[1],color,Role::Answer,103),
             make(103,bt[2],bPulse,Role::Anchor,115),
             make(104,bt[3],upper,Role::Answer,107),
             make(105,bt[4],bPulse,Role::Pickup,108),
             make(106,bt[5],bPulse,Role::Drive,93),
             make(107,bt[6],upper,Role::Answer,108),
             make(108,bt[7],bPulse,Role::Anchor,120,2)};
    // BREAK_AND_SHOUT ends on its weighty low anchor and leaves steps 14-15
    // silent. MOTOR/INTERLOCK are driven *toward* the following downbeat.
    if(a.device!=Device::BreakAndShout) {
        auto leading=make(109,bt[8],-1,Role::Tension,87);
        target(leading,1,1);b.notes.push_back(leading);
    }
    finish(a);finish(b);return {a,b};
}
std::pair<Bar,Bar> dark(Rng& rng) {
    Bar a,b;a.motif='A';b.motif='B';
    const int grammar=rng.choose(3);
    // Distinct contour tension-release *placements*, with matching answer form.
    int earlyStep,climbStep,tenseStep,resolveStep,answerStep,descendStep,secondAnswer,lowAnswer;
    switch(grammar) {
        case 0: // Tension before the backbeat resolution.
            a.device=b.device=Device::ClimbRelease;
            earlyStep=rng.among({3,4});climbStep=rng.among({6,7});
            tenseStep=rng.among({9,10});resolveStep=12;
            answerStep=rng.among({4,5});descendStep=rng.among({7,8});
            secondAnswer=10;lowAnswer=13;
            break;
        case 1: // Delayed release on beat 4+, a deliberately suspended shape.
            a.device=b.device=Device::LateSuspension;
            earlyStep=rng.among({3,4});climbStep=rng.among({7,8});
            tenseStep=rng.among({10,11});resolveStep=13;
            answerStep=rng.among({4,5});descendStep=rng.among({8,9});
            secondAnswer=11;lowAnswer=14;
            break;
        default: // Early climb with a gap before a broken syncopated response.
            a.device=b.device=Device::BrokenAnswer;
            earlyStep=rng.among({2,3});climbStep=rng.among({5,6});
            tenseStep=rng.among({8,9});resolveStep=11;
            answerStep=rng.among({3,4});descendStep=rng.among({6,7});
            secondAnswer=10;lowAnswer=13;
            break;
    }
    const int targetPitch=rng.among({3,5,7});
    // Natural-minor contour with a named destination. The upper voice must
    // rise ABOVE its later target instead of accidentally remaining level.
    const int candidateSummit=rng.among({7,10,12});
    const int summit=candidateSummit>targetPitch?candidateSummit:10;
    // The answering bar uses an explicitly selected scale tone, never an
    // arithmetic max() that can invent a major sixth (+9) in this minor idiom.
    const int candidateAnswer=rng.among({7,10,12});
    const int upperAnswer=candidateAnswer>targetPitch?candidateAnswer:10;
    a.notes={make(1,0,0,Role::Anchor,117,3),
             make(2,earlyStep,3,Role::Answer,101,2),
             make(3,climbStep,summit,Role::Drive,107),
             make(4,tenseStep,summit+rng.among({-1,1}),Role::Tension,110,1),
             make(5,resolveStep,summit,Role::Resolution,107,2)};
    target(a.notes[3],5,0);
    auto beforeBar=make(6,15,targetPitch-1,Role::Tension,91);
    target(beforeBar,101,1);a.notes.push_back(beforeBar);
    b.notes={make(101,0,targetPitch,Role::Resolution,115,3),
             make(102,answerStep,upperAnswer,Role::Answer,107),
             make(103,descendStep,targetPitch,Role::Drive,113,2),
             make(104,secondAnswer,3,Role::Answer,101,2),
             make(105,lowAnswer,0,Role::Anchor,112)};
    // The sustained/suspended answer must be allowed to breathe: unlike
    // CLIMB_RELEASE or BROKEN_ANSWER it does not require a chromatic pickup
    // into the next A. An A->B resolution remains explicitly present.
    if(a.device!=Device::LateSuspension) {
        auto beforeA=make(106,15,-1,Role::Tension,89);
        target(beforeA,1,1);b.notes.push_back(beforeA);
    }
    finish(a);finish(b);return {a,b};
}
Event& byId(Bar& b,int id) {
    const auto it=std::find_if(b.notes.begin(),b.notes.end(),[id](const Event& e){return e.id==id;});
    if(it==b.notes.end())throw std::logic_error("unknown event id");
    return *it;
}
void omit(Composition& c,int bar,int id) {
    Bar& b=c.bars[static_cast<std::size_t>(bar)];
    const auto it=std::find_if(b.notes.begin(),b.notes.end(),[id](const Event& e){return e.id==id;});
    if(it==b.notes.end()||it->role!=Role::Drive)throw std::logic_error("invalid OMIT");
    const int step=it->step;
    b.notes.erase(it);b.transformed=true;
    c.edits.push_back({bar,Operation::Omit,id,step,-1});
    finish(b);
}
void anticipate(Composition& c,int bar,int id) {
    Bar& b=c.bars[static_cast<std::size_t>(bar)];
    Event& e=byId(b,id);if(e.role!=Role::Anchor)throw std::logic_error("invalid ANTICIPATE");
    const int old=e.step, next=old-1;
    if(next<1||std::any_of(b.notes.begin(),b.notes.end(),[&](const Event& x){return x.step==next;}))
        throw std::logic_error("ANTICIPATE destination occupied");
    e.step=next;e.role=Role::Pickup;e.duration=1;
    b.transformed=true;c.edits.push_back({bar,Operation::Anticipate,id,old,next});finish(b);
}
void answer(Composition& c,int bar,int id,int newInterval) {
    Bar& b=c.bars[static_cast<std::size_t>(bar)];
    Event& e=byId(b,id);
    if(e.role!=Role::Answer||e.interval==newInterval)throw std::logic_error("invalid ANSWER");
    const int previousInterval=e.interval;
    e.interval=newInterval;b.transformed=true;
    c.edits.push_back({bar,Operation::Answer,id,e.step,e.step,previousInterval,newInterval});
}
// A two-event *accent exchange* creates a deliberate bass/kick dialogue.
// It does not insert hits, change rhythm/pitch or raise the sum of velocities.
// The accent that was anchored on a four-on-floor kick moves to a weak-step
// answering event, keeping the actual chosen source motif unmodified.
void reaccent(Composition& c,int bar,int anchorId,int responseId) {
    Bar& b=c.bars[static_cast<std::size_t>(bar)];
    Event& anchor=byId(b,anchorId);
    Event& response=byId(b,responseId);
    if((anchor.role!=Role::Anchor && anchor.role!=Role::Resolution) || anchor.step%4!=0 ||
       response.role!=Role::Answer || response.step%4==0 ||
       anchor.velocity<=response.velocity)
        throw std::logic_error("REACCENT requires a stronger on-kick anchor and an offbeat answer");
    Edit record;
    record.bar=bar; record.operation=Operation::Reaccent;
    record.eventId=anchorId; record.partnerEventId=responseId;
    record.previousStep=anchor.step; record.newStep=response.step;
    record.previousVelocity=anchor.velocity;
    record.newVelocity=response.velocity;
    record.partnerPreviousVelocity=response.velocity;
    record.partnerNewVelocity=anchor.velocity;
    std::swap(anchor.velocity,response.velocity);
    b.transformed=true;
    c.edits.push_back(record);
}
void checkConfig(const Config& cfg) {
    if(cfg.bars!=4&&cfg.bars!=8)throw std::invalid_argument("bars must be 4 or 8");
    if(cfg.rootMidi<24||cfg.rootMidi>60)throw std::invalid_argument("rootMidi must be 24..60");
    if(cfg.variation<0||cfg.variation>100)throw std::invalid_argument("variation must be 0..100");
    if(cfg.style!=Style::MinimalBody&&cfg.style!=Style::ClassicEBM&&cfg.style!=Style::DarkElectro)
        throw std::invalid_argument("unknown style");
}
} // namespace

Composition compose(const Config& cfg) {
    checkConfig(cfg);
    Rng rng(cfg.seed);
    Composition out;out.settings=cfg;
    Bar a,b;
    if(cfg.style==Style::MinimalBody)a=minimal(rng);
    else if(cfg.style==Style::ClassicEBM){auto motifs=classic(rng);a=motifs.first;b=motifs.second;}
    else {auto motifs=dark(rng);a=motifs.first;b=motifs.second;}
    for(int i=0;i<cfg.bars;++i) {
        out.bars.push_back((cfg.style==Style::MinimalBody||i%2==0)?a:b);
    }
    // Phrase-level arc, not a per-step random fill: preserve the source A(/B)
    // cell, make the last bar a recognizable response, and only at greater
    // intensity change the middle and the approach to that response.
    // In 8 bars: A/A-B intro (bars 1-2), midpoint contrast (bar 4),
    // preparation (bars 6/7) and terminal answer (bar 8).
    // In 4 bars: opening A(/B) remains original; final bar 4 responds.
    // 0: exact repeat, 1..24: terminal answer, 25..49: midpoint contrast,
    // 50..74: further approach movement, 75..100: second contrast.
    // The units are deliberate discrete musical classes, not smooth percent.
    const int last=cfg.bars-1;
    if(cfg.variation>0) {
        if(cfg.style==Style::MinimalBody) {
            const int old=byId(out.bars[static_cast<std::size_t>(last)],5).interval;
            answer(out,last,5,old==3?7:3);
        } else if(cfg.style==Style::ClassicEBM) {
            const int old=byId(out.bars[static_cast<std::size_t>(last)],107).interval;
            answer(out,last,107,old==7?12:7);
        } else {
            const int old=byId(out.bars[static_cast<std::size_t>(last)],102).interval;
            answer(out,last,102,old==10?12:10);
        }
    }
    if(cfg.variation>=25) {
        // At eight bars this contrast is half-way, not another forced fill
        // in the end; four-bar forms use the final response bar as contrast.
        if(cfg.style==Style::MinimalBody) omit(out,3,3);
        else if(cfg.style==Style::ClassicEBM) omit(out,3,106);
        else {
            const int old=byId(out.bars[3],104).interval;
            answer(out,3,104,old==3?5:3);
        }
    }
    if(cfg.variation>=50) {
        // The last A/low-pulse before the reply prepares the final answer.
        // No random edits to the very first motif, and no double On at a step.
        if(cfg.style==Style::MinimalBody) anticipate(out,cfg.bars==8?6:2,4);
        else if(cfg.style==Style::ClassicEBM) anticipate(out,cfg.bars==8?6:2,6);
        else omit(out,cfg.bars==8?5:3,103);
    }
    if(cfg.variation>=75) {
        if(cfg.style==Style::MinimalBody) {
            if(cfg.bars==8) omit(out,5,2);
            else { const int old=byId(out.bars[2],5).interval;
                   answer(out,2,5,old==3?7:3); }
        } else if(cfg.style==Style::ClassicEBM) {
            const int bar=cfg.bars==8?5:3;
            const int old=byId(out.bars[static_cast<std::size_t>(bar)],102).interval;
            answer(out,bar,102,old==3?5:3);
        } else {
            if(cfg.bars==8) {
                const int old=byId(out.bars[5],102).interval;
                answer(out,5,102,old==10?12:10);
            } else {
                // At four bars, alter a non-source question tone without
                // erasing the B-bar closing answer or losing tonal contour.
                const int old=byId(out.bars[2],2).interval;
                answer(out,2,2,old==3?5:3);
            }
        }
    }
    // High-intensity phrase response: swap emphasis from a downbeat bass
    // anchor to a deliberately offbeat answering note in the final bar.
    // Minimal Body deliberately keeps its hypnotic repeated accent contour.
    if(cfg.variation>=75 && cfg.style==Style::ClassicEBM)
        reaccent(out,last,108,107);
    if(cfg.variation>=75 && cfg.style==Style::DarkElectro)
        reaccent(out,last,101,104);
    validate(out);
    return out;
}
// Repeated VARIATE must not swap the underlying source idea or scatter unrelated
// random notes. These four alternative phrase treatments keep the selected seed,
// the source A(/B) cell, the grammar, and the existing intensity-level edits.
// The strict verifier below independently replays each newly logged edit.
Composition composeAlternative(const Config& cfg,int alternative) {
    if(alternative<0 || alternative>4)
        throw std::invalid_argument("alternative index must be 0..4");
    Composition out=compose(cfg);
    if(alternative==0)return out;
    if(cfg.variation==0)
        throw std::invalid_argument("0% is neutral; choose an intensity before VARIATE");
    const int final=cfg.bars-1;
    const auto swapAnswer=[&](int bar,int id,int first,int second) {
        const int old=byId(out.bars[static_cast<std::size_t>(bar)],id).interval;
        // Both destinations are chosen scale/role-compatible response tones;
        // unlike arithmetic transposition these do not invent chromatic notes.
        answer(out,bar,id,old==first?second:first);
    };
    switch(alternative) {
        case 1: // Rephrase the ending; leave its rhythm and anchors intact.
            if(cfg.style==Style::MinimalBody)swapAnswer(final,5,7,5);
            else if(cfg.style==Style::ClassicEBM)swapAnswer(final,107,7,12);
            else swapAnswer(final,102,10,12);
            break;
        case 2: // Mid-phrase question/response instead of a last-bar flourish.
            if(cfg.style==Style::MinimalBody)
                swapAnswer(cfg.bars==8?5:2,5,7,5);
            else if(cfg.style==Style::ClassicEBM)
                swapAnswer(2,2,7,12);
            else swapAnswer(2,2,3,5);
            break;
        case 3: // Remove a named optional drive to create a deliberate gap.
            if(cfg.style==Style::MinimalBody)omit(out,1,3);
            else if(cfg.style==Style::ClassicEBM) {
                if(cfg.bars==4 && cfg.variation>=25)swapAnswer(3,102,3,5);
                else omit(out,cfg.bars==8?5:3,106);
            } else {
                if(cfg.bars==4 && cfg.variation>=50)swapAnswer(3,104,3,5);
                else omit(out,3,103);
            }
            break;
        case 4: // Anticipate a selected later anchor; explicitly vacate its old slot.
            if(cfg.style==Style::MinimalBody)anticipate(out,1,4);
            else if(cfg.style==Style::ClassicEBM) {
                // At 4 bars and intensity >=50 this anchor was already advanced
                // by the canonical phrase arc. Do not "anticipate" a Pickup twice.
                if(cfg.bars==4 && cfg.variation>=50)swapAnswer(2,7,5,7);
                else anticipate(out,2,6);
            }
            else anticipate(out,3,105);
            break;
        default:break;
    }
    validate(out);
    return out;
}
// Independent event-lineage audit. A later bar is legally derived only by
// replaying its explicit operations against the untouched A or B source motif.
// This deliberately does NOT use the mutation helpers above, so the check
// cannot silently follow an undocumented side effect of an implementation.
static void verifyLineage(const Composition& c) {
    const int sourceCount=c.settings.style==Style::MinimalBody?1:2;
    for(std::size_t barIndex=0;barIndex<c.bars.size();++barIndex) {
        const Bar& actual=c.bars[barIndex];
        const Bar& source=c.bars[barIndex%static_cast<std::size_t>(sourceCount)];
        Bar reconstructed=source;
        bool changed=false;
        if(barIndex < static_cast<std::size_t>(sourceCount)) {
            for(const Edit& edit:c.edits)
                if(edit.bar==static_cast<int>(barIndex))
                    throw std::invalid_argument("source motif may not be edited");
            if(actual.transformed || actual.notes!=source.notes)
                throw std::invalid_argument("modified source motif");
            continue;
        }
        for(const Edit& edit:c.edits) {
            if(edit.bar!=static_cast<int>(barIndex))continue;
            auto it=std::find_if(reconstructed.notes.begin(),reconstructed.notes.end(),
                [&](const Event& event){return event.id==edit.eventId;});
            if(it==reconstructed.notes.end())
                throw std::invalid_argument("edit references missing or already omitted event");
            // Unused metadata must not silently hide an unrelated edit.
            if(edit.operation!=Operation::Reaccent &&
               (edit.partnerEventId!=-1 || edit.previousVelocity!=0 ||
                edit.newVelocity!=0 || edit.partnerPreviousVelocity!=0 ||
                edit.partnerNewVelocity!=0))
                throw std::invalid_argument("unexpected paired velocity payload");
            switch(edit.operation) {
                case Operation::Omit:
                    if(edit.previousStep!=it->step || edit.newStep!=-1 ||
                       edit.previousInterval!=0 || edit.newInterval!=0 ||
                       it->role!=Role::Drive)
                        throw std::invalid_argument("unverifiable OMIT");
                    reconstructed.notes.erase(it);
                    break;
                case Operation::Anticipate:
                    if(edit.previousStep!=it->step || edit.newStep!=it->step-1 ||
                       edit.previousInterval!=0 || edit.newInterval!=0 ||
                       it->role!=Role::Anchor || edit.newStep<=0 ||
                       std::any_of(reconstructed.notes.begin(),reconstructed.notes.end(),
                           [&](const Event& another){return another.id!=it->id && another.step==edit.newStep;}))
                        throw std::invalid_argument("unverifiable ANTICIPATE");
                    it->step=edit.newStep;
                    it->role=Role::Pickup;
                    it->duration=1;
                    break;
                case Operation::Answer:
                    if(edit.previousStep!=it->step || edit.newStep!=it->step ||
                       it->role!=Role::Answer || it->interval!=edit.previousInterval ||
                       edit.previousInterval==edit.newInterval || edit.partnerEventId!=-1 ||
                       edit.previousVelocity!=0 || edit.newVelocity!=0 ||
                       edit.partnerPreviousVelocity!=0 || edit.partnerNewVelocity!=0)
                        throw std::invalid_argument("unverifiable ANSWER");
                    it->interval=edit.newInterval;
                    break;
                case Operation::Reaccent: {
                    auto partner=std::find_if(reconstructed.notes.begin(),reconstructed.notes.end(),
                        [&](const Event& event){return event.id==edit.partnerEventId;});
                    const bool eligible=c.settings.variation>=75 &&
                        barIndex==c.bars.size()-1 &&
                        ((c.settings.style==Style::ClassicEBM && edit.eventId==108 &&
                          edit.partnerEventId==107) ||
                         (c.settings.style==Style::DarkElectro && edit.eventId==101 &&
                          edit.partnerEventId==104));
                    if(!eligible || partner==reconstructed.notes.end() || partner==it ||
                       edit.previousInterval!=0 || edit.newInterval!=0 ||
                       (it->role!=Role::Anchor && it->role!=Role::Resolution) || it->step%4!=0 ||
                       partner->role!=Role::Answer || partner->step%4==0 ||
                       edit.previousStep!=it->step || edit.newStep!=partner->step ||
                       edit.previousVelocity!=it->velocity ||
                       edit.partnerPreviousVelocity!=partner->velocity ||
                       edit.previousVelocity<=edit.partnerPreviousVelocity ||
                       edit.newVelocity!=edit.partnerPreviousVelocity ||
                       edit.partnerNewVelocity!=edit.previousVelocity)
                        throw std::invalid_argument("unverifiable paired REACCENT");
                    it->velocity=edit.newVelocity;
                    partner->velocity=edit.partnerNewVelocity;
                    break;
                }
                default:
                    throw std::invalid_argument("unsupported or fake motif operation");
            }
            // Length can be shortened by a shifted onset; it is never
            // silently lengthened by removing a subsequent event.
            finish(reconstructed);
            changed=true;
        }
        reconstructed.transformed=changed;
        if(!(reconstructed==actual))
            throw std::invalid_argument("bar differs from documented motif transformations");
    }
}

void validate(const Composition& c) {
    checkConfig(c.settings);
    if(c.bars.size()!=static_cast<std::size_t>(c.settings.bars))
        throw std::invalid_argument("wrong bar count");
    for(int k=0;k<4;++k)if(c.kickSteps[k]!=k*4)
        throw std::invalid_argument("kick reference changed");
    // The first source motif(s) must match their *named* rhythmic device.
    // This is an independent structural contract: a changed device label with
    // unchanged notes is not a different musical grammar.
    const auto sourceEvent=[](const Bar& b,int id)->const Event& {
        const auto it=std::find_if(b.notes.begin(),b.notes.end(),
            [id](const Event& e){return e.id==id;});
        if(it==b.notes.end())throw std::invalid_argument("missing source role event");
        return *it;
    };
    const auto requireDevice=[](bool predicate) {
        if(!predicate)throw std::invalid_argument("source device rhythm violated");
    };
    const auto hasId=[](const Bar& bar,int id) {
        return std::any_of(bar.notes.begin(),bar.notes.end(),
            [id](const Event& e){return e.id==id;});
    };
    const Bar& sourceA=c.bars.front();
    requireDevice(!sourceA.transformed && sourceA.motif=='A');
    const auto& opening=sourceEvent(sourceA,1);
    requireDevice(opening.step==0 && opening.role==Role::Anchor && opening.interval==0);
    if(c.settings.style==Style::MinimalBody) {
        const auto& pickup=sourceEvent(sourceA,3);
        const auto& anchor=sourceEvent(sourceA,4);
        const auto& reply=sourceEvent(sourceA,5);
        requireDevice(anchor.role==Role::Anchor && reply.role==Role::Answer &&
                 pickup.role==Role::Drive);
        switch(sourceA.device) {
            case Device::PedalGap:
                requireDevice(anchor.step==8 && pickup.step>=5 && pickup.step<=6 &&
                         reply.step>=10 && reply.step<=13);
                if(hasId(sourceA,6)) {
                    const auto& ending=sourceEvent(sourceA,6);
                    requireDevice(ending.role==Role::Pickup && ending.interval==0 &&
                        ending.targetId==-1 && ending.targetBarDelta==0 &&
                        (ending.step==15 || (ending.step==14 && reply.step<=12)));
                }
                break;
            case Device::DelayedBackbeat:
                requireDevice(anchor.step==12 && pickup.step>=6 && pickup.step<=8 &&
                         reply.step>=13 && reply.step<=14 && !hasId(sourceA,6));break;
            case Device::CrossPulse:
                requireDevice(anchor.step==8 && pickup.step==4 &&
                         reply.step>=12 && reply.step<=13 && hasId(sourceA,6) &&
                         sourceEvent(sourceA,6).role==Role::Tension);break;
            default: throw std::invalid_argument("invalid minimal device");
        }
    } else {
        const Bar& sourceB=c.bars[1];
        requireDevice(!sourceB.transformed && sourceB.motif=='B' &&
                 sourceB.device==sourceA.device);
        const auto& responseStart=sourceEvent(sourceB,101);
        requireDevice(responseStart.step==0 &&
                      (responseStart.role==Role::Anchor || responseStart.role==Role::Resolution));
        if(c.settings.style==Style::ClassicEBM) {
            const auto& aAnswer=sourceEvent(sourceA,5);
            const auto& bDrive=sourceEvent(sourceB,106);
            const auto& bAnswer=sourceEvent(sourceB,107);
            requireDevice(aAnswer.role==Role::Answer && bDrive.role==Role::Drive &&
                     bAnswer.role==Role::Answer);
            switch(sourceA.device) {
                case Device::MotorLock:
                    requireDevice(aAnswer.step==10 && bDrive.step==9 && bAnswer.step==11 &&
                                  hasId(sourceB,109));break;
                case Device::BreakAndShout:
                    requireDevice(aAnswer.step==9 && bDrive.step==8 && bAnswer.step==10 &&
                                  !hasId(sourceB,109));break;
                case Device::OctaveInterlock:
                    requireDevice(aAnswer.step==8 && bDrive.step==9 && bAnswer.step==11 &&
                                  hasId(sourceB,109));break;
                default: throw std::invalid_argument("invalid classic device");
            }
        } else {
            const auto& resolution=sourceEvent(sourceA,5);
            const auto& summit=sourceEvent(sourceA,3);
            const auto& firstTarget=sourceEvent(sourceB,101);
            requireDevice(summit.role==Role::Drive &&
                          firstTarget.role==Role::Resolution &&
                          summit.interval>firstTarget.interval);
            const auto& answer=sourceEvent(sourceB,104);
            const auto& lowAnchor=sourceEvent(sourceB,105);
            requireDevice(resolution.role==Role::Resolution && answer.role==Role::Answer &&
                     lowAnchor.role==Role::Anchor);
            switch(sourceA.device) {
                case Device::ClimbRelease:
                    requireDevice(resolution.step==12 && answer.step==10 && lowAnchor.step==13 &&
                                  hasId(sourceB,106));break;
                case Device::LateSuspension:
                    requireDevice(resolution.step==13 && answer.step==11 && lowAnchor.step==14 &&
                                  !hasId(sourceB,106));break;
                case Device::BrokenAnswer:
                    requireDevice(resolution.step==11 && answer.step==10 && lowAnchor.step==13 &&
                                  hasId(sourceB,106));break;
                default: throw std::invalid_argument("invalid dark device");
            }
        }
    }
    for(std::size_t index=0;index<c.bars.size();++index) {
        const Bar& b=c.bars[index];
        const int device=static_cast<int>(b.device);
        const int low=c.settings.style==Style::MinimalBody?0:
                      c.settings.style==Style::ClassicEBM?3:6;
        if(device<low || device>=low+3)
            throw std::invalid_argument("device incompatible with chosen style");
        if(index>0 && b.device!=c.bars.front().device)
            throw std::invalid_argument("device switched within composed phrase");
        if(c.settings.style==Style::DarkElectro) {
            // This PROVISIONAL grammar is explicitly natural-minor anchored;
            // notes outside the palette are allowed only as tagged chromatic
            // TENSION with a resolvable target checked below.
            constexpr std::array<int,6> palette{{0,3,5,7,10,12}};
            for(const Event& event:b.notes) {
                if(event.role!=Role::Tension &&
                   std::find(palette.begin(),palette.end(),event.interval)==palette.end())
                    throw std::invalid_argument("unmotivated dark-electro chromatic pitch");
            }
            if(b.motif=='B') {
                const auto targetIt=std::find_if(b.notes.begin(),b.notes.end(),
                    [](const Event& event){return event.id==101;});
                const auto answerIt=std::find_if(b.notes.begin(),b.notes.end(),
                    [](const Event& event){return event.id==102;});
                if(targetIt==b.notes.end()||answerIt==b.notes.end()||
                   targetIt->role!=Role::Resolution||answerIt->role!=Role::Answer||
                   answerIt->interval<=targetIt->interval)
                    throw std::invalid_argument("dark-electro answer does not rise above target");
            }
        }
        if(b.notes.empty()||(b.motif!='A' && b.motif!='B'))throw std::invalid_argument("invalid motif");
        int nextFree=0;bool hasAnchor=false;
        for(const auto& e:b.notes) {
            if(e.id<0||e.step<nextFree||e.step<0||e.step>=16||
               e.duration<=0||e.step+e.duration>16||e.velocity<1||e.velocity>127||
               e.interval+c.settings.rootMidi<0||e.interval+c.settings.rootMidi>127)
                throw std::invalid_argument("note outside valid bounds or overlap");
            if(std::any_of(b.notes.begin(),b.notes.end(),[&](const Event& x){return x.id==e.id && &x!=&e;}))
                throw std::invalid_argument("duplicate motif ID");
            if(e.role==Role::Anchor||e.role==Role::Resolution)hasAnchor=true;
            if(e.role==Role::Tension) {
                if(e.targetId<0||e.targetBarDelta<0||e.targetBarDelta>1)
                    throw std::invalid_argument("tension without target");
                const std::size_t dest=(index+static_cast<std::size_t>(e.targetBarDelta))%c.bars.size();
                const auto& notes=c.bars[dest].notes;
                const auto it=std::find_if(notes.begin(),notes.end(),[&](const Event& x){return x.id==e.targetId;});
                if(it==notes.end()||(it->role!=Role::Resolution&&it->role!=Role::Anchor)||
                   std::abs(it->interval-e.interval)!=1||
                   (e.targetBarDelta==0&&it->step<=e.step))
                    throw std::invalid_argument("unresolved chromatic approach");
            } else if(e.targetId!=-1||e.targetBarDelta!=0)throw std::invalid_argument("unexpected target");
            nextFree=e.step+e.duration;
        }
        if(!hasAnchor)throw std::invalid_argument("motif has no anchor");
        if(b.notes.front().step!=0)throw std::invalid_argument("no opening motif identity");
    }
    for(const auto& op:c.edits){
        if(op.bar<0||op.bar>=c.settings.bars||!c.bars[static_cast<std::size_t>(op.bar)].transformed)
            throw std::invalid_argument("invalid operation log");
        if(op.operation==Operation::Anticipate) {
            if(op.newStep!=op.previousStep-1)throw std::invalid_argument("fake anticipation");
            const auto& notes=c.bars[static_cast<std::size_t>(op.bar)].notes;
            if(std::any_of(notes.begin(),notes.end(),[&](const Event& e){return e.step==op.previousStep;}))
                throw std::invalid_argument("original onset not vacated");
            auto it=std::find_if(notes.begin(),notes.end(),[&](const Event& e){return e.id==op.eventId;});
            if(it==notes.end()||it->step!=op.newStep||it->role!=Role::Pickup)
                throw std::invalid_argument("anticipation not real");
        }
        if(op.operation==Operation::Omit) {
            if(op.newStep!=-1)throw std::invalid_argument("fake omission");
            const auto& notes=c.bars[static_cast<std::size_t>(op.bar)].notes;
            if(std::any_of(notes.begin(),notes.end(),[&](const Event& e){return e.id==op.eventId;}))
                throw std::invalid_argument("omission not real");
        }
    }
    verifyLineage(c);
}
Phrase render(const Composition& c) {
    validate(c);
    Phrase out;out.bars=c.settings.bars;
    for(std::size_t i=0;i<c.bars.size();++i)
        for(const auto& note:c.bars[i].notes)
            out.notes.push_back({static_cast<int>(i)*16+note.step,
                c.settings.rootMidi+note.interval,note.velocity,note.duration});
    return out;
}
} // namespace ebm::v2
