#include "ebm/HostBridge.hpp"
#include <algorithm>
#include <utility>
#include <stdexcept>

namespace ebm::host {
namespace {
using Bytes=DocumentBytes;
void put8(Bytes& out,std::size_t& i,std::uint8_t x) noexcept {out[i++]=x;}
void put32(Bytes& out,std::size_t& i,std::uint32_t x) noexcept {
    for(int n=0;n<4;++n)put8(out,i,static_cast<std::uint8_t>(x>>(8*n)));
}
void put64(Bytes& out,std::size_t& i,std::uint64_t x) noexcept {
    for(int n=0;n<8;++n)put8(out,i,static_cast<std::uint8_t>(x>>(8*n)));
}
std::uint8_t get8(const std::uint8_t* data,std::size_t& i) noexcept {return data[i++];}
std::uint32_t get32(const std::uint8_t* data,std::size_t& i) noexcept {
    std::uint32_t x=0;for(int n=0;n<4;++n)x|=std::uint32_t(get8(data,i))<<(8*n);return x;
}
std::uint64_t get64(const std::uint8_t* data,std::size_t& i) noexcept {
    std::uint64_t x=0;for(int n=0;n<8;++n)x|=std::uint64_t(get8(data,i))<<(8*n);return x;
}
std::uint32_t checksum(const std::uint8_t* src,std::size_t count) noexcept {
    std::uint32_t hash=2166136261u;
    for(std::size_t i=0;i<count;++i){hash^=src[i];hash*=16777619u;}
    return hash;
}
void storeConfig(Bytes& b,std::size_t& i,const Config& c) noexcept {
    put8(b,i,static_cast<std::uint8_t>(c.style));
    put8(b,i,static_cast<std::uint8_t>(c.bars));
    put8(b,i,static_cast<std::uint8_t>(c.rootMidi));
    put8(b,i,static_cast<std::uint8_t>(c.variation));
    put64(b,i,c.seed);
}
Config loadConfig(const std::uint8_t* b,std::size_t& i) noexcept {
    Config c{};c.style=static_cast<Style>(get8(b,i));c.bars=get8(b,i);
    c.rootMidi=get8(b,i);c.variation=get8(b,i);c.seed=get64(b,i);return c;
}
bool validModel(const Document& d) {
    if(d.mode!=live::Mode::Transport && d.mode!=live::Mode::MidiNote)return false;
    if(!edit::valid(d.editable) || d.gatePercent<0 || d.gatePercent>100)return false;
    v2::Session tmp;
    tmp.restore(d.session); // Full semantic and lineage consistency.
    return true;
}
} // namespace

bool encode(const Document& source,DocumentBytes& target) noexcept {
    try {
        if(source.session.history.size()>kMaximumHistory || !validModel(source))return false;
        DocumentBytes fresh{};
        std::size_t i=0;
        for(char c: {'E','B','M','3'})put8(fresh,i,static_cast<std::uint8_t>(c));
        put32(fresh,i,3);
        storeConfig(fresh,i,source.session.options);
        put8(fresh,i,source.session.hasCurrent?1:0);
        storeConfig(fresh,i,source.session.hasCurrent?source.session.current:Config{});
        put8(fresh,i,static_cast<std::uint8_t>(source.session.alternative));
        put8(fresh,i,static_cast<std::uint8_t>(source.session.history.size()));
        put8(fresh,i,static_cast<std::uint8_t>(source.mode));
        for(const auto& c:source.session.history)storeConfig(fresh,i,c);
        while(i<kLegacyDocumentBytes-4)put8(fresh,i,0);
        const auto& e=source.editable;
        put8(fresh,i,e.enabled?1:0);
        put8(fresh,i,static_cast<std::uint8_t>(e.bars));
        put8(fresh,i,static_cast<std::uint8_t>(e.scale));
        put8(fresh,i,static_cast<std::uint8_t>(e.songStartBar&255));
        put8(fresh,i,static_cast<std::uint8_t>((e.songStartBar>>8)&255));
        for(const auto& step:e.steps){
            put8(fresh,i,step.on?1:0);put8(fresh,i,step.pitch);
            put8(fresh,i,step.velocity);put8(fresh,i,step.length);
            put8(fresh,i,step.locked?1:0);
        }
        put8(fresh,i,static_cast<std::uint8_t>(source.gatePercent));
        put32(fresh,i,checksum(fresh.data(),kDocumentBytes-4));
        if(i!=kDocumentBytes)return false;
        target=fresh;return true;
    }catch(...) {return false;}
}
bool decode(const std::uint8_t* bytes,std::size_t count,Document& dest) noexcept {
    if(!bytes || (count!=kDocumentBytes && count!=kV2DocumentBytes && count!=kLegacyDocumentBytes))return false;
    try {
        std::size_t i=0;
        if(get8(bytes,i)!='E'||get8(bytes,i)!='B'||get8(bytes,i)!='M')return false;
        const auto magic=get8(bytes,i);const auto version=get32(bytes,i);
        const bool legacy=magic=='1'&&version==1&&count==kLegacyDocumentBytes;
        const bool v2=magic=='2'&&version==2&&count==kV2DocumentBytes;
        const bool v3=magic=='3'&&version==3&&count==kDocumentBytes;
        if(!legacy && !v2 && !v3)return false;
        Document candidate{};
        candidate.session.version=2;
        candidate.session.options=loadConfig(bytes,i);
        const auto has=get8(bytes,i);
        if(has>1)return false;
        candidate.session.hasCurrent=has==1;
        const Config storedCurrent=loadConfig(bytes,i);
        candidate.session.current=storedCurrent;
        const auto alt=get8(bytes,i),len=get8(bytes,i),mode=get8(bytes,i);
        if(len>kMaximumHistory || mode>1 || alt>4)return false;
        candidate.session.alternative=alt;
        candidate.mode=static_cast<live::Mode>(mode);
        candidate.session.history.reserve(len);
        for(unsigned n=0;n<len;++n)candidate.session.history.push_back(loadConfig(bytes,i));
        // Strict canonical tail: unused slots may not smuggle state.
        while(i<kLegacyDocumentBytes-4)if(get8(bytes,i)!=0)return false;
        if(!legacy){
            auto& e=candidate.editable;
            const auto enabled=get8(bytes,i);if(enabled>1)return false;
            e.enabled=enabled!=0;
            e.bars=get8(bytes,i);
            e.scale=static_cast<edit::Scale>(get8(bytes,i));
            e.songStartBar=get8(bytes,i);
            e.songStartBar|=static_cast<int>(get8(bytes,i))<<8;
            for(auto& step:e.steps){
                const auto on=get8(bytes,i),pitch=get8(bytes,i),velocity=get8(bytes,i);
                const auto length=get8(bytes,i),locked=get8(bytes,i);
                if(on>1||locked>1)return false;
                step.on=on!=0;step.pitch=pitch;step.velocity=velocity;
                step.length=length;step.locked=locked!=0;
            }
        }
        if(v3)candidate.gatePercent=get8(bytes,i);
        if(get32(bytes,i)!=checksum(bytes,count-4) || i!=count)return false;
        if(!candidate.session.hasCurrent) {
            // Canonical no-current fields were all 0 except Config defaults.
            if(storedCurrent.style!=Style::ClassicEBM || storedCurrent.bars!=8 ||
               (storedCurrent.rootMidi!=36 && storedCurrent.rootMidi!=48) || storedCurrent.seed!=1 ||
               storedCurrent.variation!=30)return false;
        }
        if(!validModel(candidate))return false;
        dest=std::move(candidate);return true;
    }catch(...) {return false;}
}

bool PatternExchange::publish(const PreparedState& source) noexcept {
    const auto& pattern=source.pattern;
    if(!pattern.ready || (pattern.bars!=1&&pattern.bars!=2&&pattern.bars!=4&&pattern.bars!=8) ||
       pattern.lengthSteps!=pattern.bars*16 || source.root<0 || source.root>127 ||
       source.songStart<1 || source.songStart>16 ||
       static_cast<int>(source.scale)<0 || static_cast<int>(source.scale)>=edit::kScaleCount)return false;
    std::lock_guard<std::mutex> guard(writerMutex_);
    for(int i=0;i<3;++i){
        int expected=Free;
        if(!slots_[static_cast<std::size_t>(i)].status.compare_exchange_strong(
            expected,Writing,std::memory_order_acq_rel))continue;
        slots_[static_cast<std::size_t>(i)].data=source;
        slots_[static_cast<std::size_t>(i)].status.store(Ready,std::memory_order_release);
        const int discarded=pending_.exchange(i,std::memory_order_acq_rel);
        if(discarded>=0){
            int oldExpected=Ready;
            (void)slots_[static_cast<std::size_t>(discarded)].status.compare_exchange_strong(
                oldExpected,Free,std::memory_order_acq_rel);
        }
        return true;
    }
    return false;
}
const PreparedState* PatternExchange::acquire(std::uint64_t& revision) noexcept {
    const int candidate=pending_.exchange(-1,std::memory_order_acq_rel);
    if(candidate>=0){
        int expected=Ready;
        if(slots_[static_cast<std::size_t>(candidate)].status.compare_exchange_strong(
           expected,Reading,std::memory_order_acq_rel)){
            const int previous=active_;
            active_=candidate;
            ++revision_;
            if(previous>=0)slots_[static_cast<std::size_t>(previous)].status.store(
                Free,std::memory_order_release);
        }
    }
    revision=revision_;
    return active_<0?nullptr:&slots_[static_cast<std::size_t>(active_)].data;
}
Model::Model(){
    // Default is ready for a transport test even before a GUI exists.
    v2::Session prepared;
    (void)prepared.generate();
    if(!publishSession(prepared))throw std::logic_error("default EBM pattern unavailable");
    session_=std::move(prepared);
}
bool Model::publishSession(const v2::Session& chosen) {
    const int bars=chosen.options().bars;
    Phrase phrase{bars,{}};
    if(chosen.hasCurrent())phrase=v2::render(chosen.current());
    PreparedState compiled;
    compiled.root=chosen.options().rootMidi;
    compiled.scale=edit::Scale::Chromatic;
    compiled.songStart=1;
    return live::prepare(phrase,compiled.pattern) && patterns_.publish(compiled);
}
bool Model::restore(const Document& doc) {
    std::lock_guard<std::mutex> guard(documentMutex_);
    try {
        if(!validModel(doc))return false;
        v2::Session next;
        next.restore(doc.session);
        Phrase phrase=doc.editable.enabled?edit::toPhrase(doc.editable):
            (next.hasCurrent()?v2::render(next.current()):Phrase{next.options().bars,{}});
        PreparedState compiled{};
        compiled.root=doc.session.options.rootMidi;
        compiled.scale=doc.editable.scale;
        compiled.songStart=doc.editable.enabled?doc.editable.songStartBar:1;
        if(!live::prepare(phrase,compiled.pattern) || !patterns_.publish(compiled))return false;
        session_=std::move(next);
        editable_=doc.editable;
        gate_.store(doc.gatePercent,std::memory_order_release);
        songStart_.store(doc.editable.enabled?doc.editable.songStartBar:1,
                         std::memory_order_release);
        mode_.store(doc.mode,std::memory_order_release);
        return true;
    }catch(...) {return false;}
}
Document Model::snapshot() const {
    std::lock_guard<std::mutex> guard(documentMutex_);
    return Document{session_.snapshot(),mode_.load(std::memory_order_acquire),editable_,gate_.load(std::memory_order_acquire)};
}
bool Model::generate(){
    std::lock_guard<std::mutex> guard(documentMutex_);
    try { auto next=session_;(void)next.generate();
        if(!publishSession(next))return false;
        session_=std::move(next);editable_.enabled=false;
        songStart_.store(1,std::memory_order_release);
        return true;
    }catch(...) {return false;}
}
bool Model::variate(int intensity){
    std::lock_guard<std::mutex> guard(documentMutex_);
    try {auto next=session_;if(!next.variate(intensity))return false;
        if(!publishSession(next))return false;
        session_=std::move(next);editable_.enabled=false;
        songStart_.store(1,std::memory_order_release);
        return true;
    }catch(...) {return false;}
}
bool Model::nextVariant(){
    std::lock_guard<std::mutex> guard(documentMutex_);
    try {auto next=session_;if(!next.nextVariant())return false;
        if(!publishSession(next))return false;
        session_=std::move(next);editable_.enabled=false;
        songStart_.store(1,std::memory_order_release);
        return true;
    }catch(...) {return false;}
}
void Model::setMode(live::Mode mode) noexcept {
    if(mode==live::Mode::Transport||mode==live::Mode::MidiNote)
        mode_.store(mode,std::memory_order_release);
}
live::Mode Model::mode() const noexcept {return mode_.load(std::memory_order_acquire);}
void Model::resetRuntime() noexcept {runtime_.reset();}
} // namespace ebm::host
