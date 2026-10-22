#include "dfre/mission_client_outbox.hpp"
#include <cstring>
#include <stdexcept>
#include <utility>
namespace dfre {
namespace {
std::uint32_t get(const std::uint8_t* bytes,unsigned at,unsigned width=4){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(bytes[at+i])<<(8*i);return value;}
void put(std::uint8_t* bytes,unsigned at,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));}
std::int32_t si(std::uint32_t n){return n<=0x7fffffffu?std::int32_t(n):std::int32_t(std::int64_t(n)-0x100000000LL);}
std::int32_t short_value(std::uint32_t n){return n&0x8000u?std::int32_t(n&0xffffu)-65536:std::int32_t(n&0xffffu);}
template<class F> const F& need(const F& f,const char* message){if(!f)throw std::runtime_error(message);return f;}
void copy_record_payload(std::uint8_t* target,const std::uint8_t* source,std::uint32_t length){
    while(length>=4){std::uint32_t value;std::memcpy(&value,source,4);std::memcpy(target,&value,4);source+=4;target+=4;length-=4;}
    while(length--)*target++=*source++;
}
}
MissionClientOutbox::MissionClientOutbox(GameMessageStage& messages,MissionServerMessageBuffer& buffer,MissionClientOutboxBindings bindings)
    :messages_(messages),buffer_(buffer),bindings_(std::move(bindings)){}
MissionClientRecordView MissionClientOutbox::client(std::size_t slot) const {
    if(slot>=17)throw std::runtime_error("Client outbox slot exceeds17 records");
    const auto view=need(bindings_.client,"Client outbox records are not bound")(slot);
    if(!view.bytes||view.length<mission_client_record_size)throw std::runtime_error("Client outbox record is incomplete");return view;
}
std::uint32_t MissionClientOutbox::cell(std::uint32_t address) const{return need(bindings_.cell,"Client outbox cells are not bound")(address);}
const GameMessageDescriptor& MissionClientOutbox::descriptor(std::int32_t id) const{
    if(id<0||std::uint32_t(id)>=messages_.table().count())throw std::runtime_error("Server message descriptor exceeds its table");
    return messages_.table().descriptor(std::uint8_t(id));
}
bool MissionClientOutbox::enqueue(std::size_t slot,std::int32_t id,const std::uint8_t* payload,std::uint32_t length){
    if(!id)return false;
    auto* bytes=client(slot).bytes;unsigned at=0x204;
    for(unsigned index=0;;++index,at+=0x264){if(index==64)return false;if(!bytes[at+9])break;}
    if(length>kGameMessagePayloadSize||(!payload&&length))throw std::runtime_error("Client message payload is invalid");
    bytes[at+9]=1;put(bytes,at+4,length,2);bytes[at+8]=std::uint8_t(id);
    const auto sequence=get(bytes,0x1b8);put(bytes,0x1b8,sequence+1);put(bytes,at,sequence,2);
    if(si(get(bytes,0x1b8))>32000)put(bytes,0x1b8,1);
    const auto flags=descriptor(id).flags;
    put(bytes,at+2,(flags&2)?1001:((flags&4)?45:1),2);put(bytes,at+6,cell(0x50321c),2);
    copy_record_payload(bytes+at+10,payload,length);return true;
}
void MissionClientOutbox::broadcast(std::int32_t id,const std::uint8_t* payload,std::uint32_t length){
    for(std::size_t slot=0;slot<17;++slot){auto* bytes=client(slot).bytes;
        if(bytes[0x18c]&&get(bytes,0x1c4,2)==5&&!bytes[0x18d])enqueue(slot,id,payload,length);
    }
}
void MissionClientOutbox::begin() noexcept{buffer_.count_8e81c0=0;}
void MissionClientOutbox::append(std::int32_t id,std::uint16_t sequence,const std::uint8_t* payload,std::int32_t length){
    const auto initial=descriptor(id);
    if(!(initial.flags&1)&&short_value(initial.length)!=length)
        need(bindings_.diagnostic_4564b0,"Server message diagnostic is not bound")(id);
    if(si(buffer_.count_8e81c0+std::uint32_t(length)+16)>8192)return;
    auto count=buffer_.count_8e81c0;
    if(count>=buffer_.bytes_8e61c0.size())throw std::runtime_error("Server message cursor exceeds its buffer");
    buffer_.count_8e81c0=count+1;buffer_.bytes_8e61c0[count]=std::uint8_t(id);
    if(descriptor(id).flags&6){count=buffer_.count_8e81c0;
        if(count>8190)throw std::runtime_error("Server message sequence exceeds its buffer");
        put(buffer_.bytes_8e61c0.data(),count,sequence,2);buffer_.count_8e81c0+=2;
    }
    if(payload){count=buffer_.count_8e81c0;
        if(length>0){if(count>8192||std::uint32_t(length)>8192-count)throw std::runtime_error("Server message payload exceeds its buffer");
            for(std::int32_t i=0;i<length;++i)buffer_.bytes_8e61c0[count+std::uint32_t(i)]=payload[i];}
        buffer_.count_8e81c0+=std::uint32_t(length);
    }
}
std::uint32_t MissionClientOutbox::flush(std::uint32_t peer){
    const auto count=buffer_.count_8e81c0;if(!count)return 0;
    if(count>8192)throw std::runtime_error("Server flush count exceeds its buffer");
    if(peer==cell(0x501340)){
        const auto& echo=messages_.pending_local_echo();
        if(si(std::uint32_t(echo.size())+count)<8192){
            std::vector<std::uint8_t> joined(echo);joined.insert(joined.end(),buffer_.bytes_8e61c0.begin(),buffer_.bytes_8e61c0.begin()+count);
            messages_.submit_local_echo(joined.data(),joined.size());
        }
    }else need(bindings_.transport_4977e0,"Server message transport is not bound")(buffer_.bytes_8e61c0.data(),count,peer);
    const auto sent=buffer_.count_8e81c0;buffer_.count_8e81c0=0;return sent;
}
void MissionClientOutbox::tick(){
    for(std::size_t slot=0;slot<17;++slot){auto* bytes=client(slot).bytes;if(!bytes[0x18c])continue;
        bytes[0x18f]=0;begin();
        for(unsigned index=0;index<64;++index){const auto at=0x204+index*0x264;if(!bytes[at+9])continue;
            ++bytes[0x18f];
            if(short_value(get(bytes,at+6,2))<1){
                if(bytes[at+2]&1)append(std::int8_t(bytes[at+8]),std::uint16_t(get(bytes,at,2)),bytes+at+10,short_value(get(bytes,at+4,2)));
                const auto repeats=std::uint16_t(get(bytes,at+2,2)-1);put(bytes,at+2,repeats,2);
                if(short_value(repeats)<1)std::memset(bytes+at,0,0x264);
            }else put(bytes,at+6,get(bytes,at+6,2)-1,2);
        }
        const auto sent=flush(get(bytes,0));put(bytes,0x6c,get(bytes,0x6c)+sent);put(bytes,0x7c,get(bytes,0x7c)+1);
        unsigned target=0;
        for(unsigned index=0;index<64;++index){auto* record=bytes+0x204+index*0x264;if(!record[9])continue;
            if(target!=index){std::memcpy(bytes+0x204+target*0x264,record,0x264);std::memset(record,0,0x264);}++target;
        }
    }
}
} // namespace dfre
