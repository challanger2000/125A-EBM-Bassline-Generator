#include "ebm/Composer.hpp"
#include <algorithm>
#include <initializer_list>
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
    return motif == o.motif && transformed == o.transformed && notes == o.notes;
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
Bar minimal(Rng& rng) {
    Bar a; a.motif='A';
    const int lowEcho = rng.among({1,2,3});
    const int secondAnchor = rng.among({8,12});
    const int syncopated = secondAnchor==8 ? rng.among({5,6}) : rng.among({5,6,7});
    const int answerStep = secondAnchor==8?rng.among({10,11,12,13}):rng.among({9,10,13,14});
    const int upper = rng.among({3,5,7,12});
    a.notes.push_back(make(1,0,0,Role::Anchor,120,rng.among({1,2})));
    a.notes.push_back(make(2,lowEcho,0,Role::Drive,91,1));
    a.notes.push_back(make(3,syncopated,0,Role::Drive,106,1));
    a.notes.push_back(make(4,secondAnchor,0,Role::Anchor,116,rng.among({1,2})));
    a.notes.push_back(make(5,answerStep,upper,Role::Answer,103,1));
    auto note=make(6,15,-1,Role::Tension,secondAnchor==8?85:88,1);
    target(note,1,1); a.notes.push_back(note);
    finish(a);
    return a;
}
// The A cell and B response share a pitch motif; B is not separately randomized note soup.
std::pair<Bar,Bar> classic(Rng& rng) {
    const int upper = rng.among({7,12});
    const int color = rng.among({3,5,7});
    const int firstAnswer = rng.among({2,3});
    const int beatResponse = rng.among({9,10});
    const int finalResponse=rng.among({13,14});
    const int preBeat = rng.among({6,7});
    const int bPulse = rng.among({0,5});
    const int bResponse = rng.among({10,11});
    Bar a; a.motif='A';
    a.notes={make(1,0,0,Role::Anchor,121),make(2,firstAnswer,upper,Role::Answer,108),
       make(3,4,0,Role::Anchor,117,2),make(4,preBeat,0,Role::Pickup,113),
       make(5,beatResponse,upper,Role::Answer,104),make(6,12,0,Role::Anchor,120),
       make(7,finalResponse,color,Role::Answer,97),make(8,15,0,Role::Pickup,106)};
    // A's final pickup propels bar B on its downbeat.
    Bar b; b.motif='B';
    const int bAnswer=rng.among({2,3,5});
    const int optionalDrive=rng.among({8,9});
    b.notes={make(101,0,bPulse,Role::Anchor,121,2),make(102,bAnswer,color,Role::Answer,103),
       make(103,4,bPulse,Role::Anchor,115),make(104,6,upper,Role::Answer,107),
       make(105,7,bPulse,Role::Pickup,108),make(106,optionalDrive,bPulse,Role::Drive,93),
       make(107,bResponse,upper,Role::Answer,108),make(108,12,bPulse,Role::Anchor,120,2)};
    auto leading=make(109,15,-1,Role::Tension,87); target(leading,1,1); b.notes.push_back(leading);
    finish(a);finish(b);return {a,b};
}
// Directed melodic question / answer with explicit chromatic arrivals.
std::pair<Bar,Bar> dark(Rng& rng) {
    const int targetPitch=rng.among({3,5,7});
    const int summit=rng.among({7,10,12});
    const int earlyStep=rng.among({3,4});
    const int climbStep=rng.among({6,7});
    const int tenseStep=rng.among({9,10});
    const int answerStep=rng.among({4,5});
    const int descendStep=rng.among({7,8});
    const int upperAnswer = std::max(targetPitch+2,rng.among({7,10,12}));
    Bar a; a.motif='A';
    a.notes={make(1,0,0,Role::Anchor,117,3),make(2,earlyStep,3,Role::Answer,101,2),
             make(3,climbStep,summit,Role::Drive,107),
             make(4,tenseStep,summit+1,Role::Tension,110,1),
             make(5,12,summit,Role::Resolution,107,2)};
    target(a.notes[3],5,0);
    auto beforeBar=make(6,15,targetPitch-1,Role::Tension,91,1);
    target(beforeBar,101,1); a.notes.push_back(beforeBar);
    Bar b; b.motif='B';
    b.notes={make(101,0,targetPitch,Role::Resolution,115,3),
             make(102,answerStep,upperAnswer,Role::Answer,107,1),
             make(103,descendStep,targetPitch,Role::Drive,113,2),
             make(104,10,3,Role::Answer,101,2),make(105,13,0,Role::Anchor,112)};
    auto beforeA=make(106,15,-1,Role::Tension,89,1);
    target(beforeA,1,1);b.notes.push_back(beforeA);
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
    e.interval=newInterval;b.transformed=true;
    c.edits.push_back({bar,Operation::Answer,id,e.step,e.step});
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
    // Structural transformations are selected by role and form. Variation is not a
    // percentage likelihood of randomly changing individual notes.
    if(cfg.variation>0) {
        if(cfg.style==Style::MinimalBody) {
            omit(out,3,3);
            if(cfg.bars==8) {
                // second anchor is 8 or 12; the preceding slot is always intentionally free.
                anticipate(out,7,4);
            }
        } else if(cfg.style==Style::ClassicEBM) {
            omit(out,3,106);
            if(cfg.bars==8) anticipate(out,6,6);
        } else {
            // Answer maintains the shared rhythm but changes a designated pitch role.
            const int selected=byId(out.bars[3],102).interval;
            answer(out,3,102,selected==7?10:7);
            if(cfg.bars==8){
                const int old=byId(out.bars[7],104).interval;
                answer(out,7,104,old==3?5:3);
            }
        }
        // Intensity 60+ allows additional meaningful answer contour transformation.
        if(cfg.variation>=60&&cfg.bars==8) {
            if(cfg.style==Style::ClassicEBM) {
                const int old=byId(out.bars[5],107).interval;
                answer(out,5,107,old==7?12:7);
            } else if(cfg.style==Style::MinimalBody) {
                const int old=byId(out.bars[5],5).interval;
                answer(out,5,5,old==3?7:3);
            } else {
                const int old=byId(out.bars[5],102).interval;
                answer(out,5,102,old==7?10:7);
            }
        }
    }
    validate(out);
    return out;
}
void validate(const Composition& c) {
    checkConfig(c.settings);
    if(c.bars.size()!=static_cast<std::size_t>(c.settings.bars))
        throw std::invalid_argument("wrong bar count");
    for(int k=0;k<4;++k)if(c.kickSteps[k]!=k*4)
        throw std::invalid_argument("kick reference changed");
    for(std::size_t index=0;index<c.bars.size();++index) {
        const Bar& b=c.bars[index];
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
