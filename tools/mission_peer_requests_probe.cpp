#include "dfre/mission_peer_requests.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;
constexpr U ACTORS=0x7dde80,OWNERS=0x7cca80,CLIENTS=0x6e9a90,STRIDE=0x9b84,DEFS=0x1600000,PACKET=0x10000000,CB1=0x410500,CB2=0x410600;
struct Memory {
    std::map<U,std::vector<std::uint8_t>> data,initial;
    void add(U a,U n){data[a]=std::vector<std::uint8_t>(n);}
    std::uint8_t* span(U a,U n){auto i=data.upper_bound(a);if(i==data.begin())throw std::runtime_error("fixture address");--i;const U offset=a-i->first;if(offset>i->second.size()||n>i->second.size()-offset)throw std::runtime_error("fixture span");return i->second.data()+offset;}
    U read(U a,U w=4){const auto* p=span(a,w);U v=0;for(U i=0;i<w;++i)v|=U(p[i])<<(8*i);return v;}
    void put(U a,U v,U w=4){auto* p=span(a,w);for(U i=0;i<w;++i)p[i]=std::uint8_t(v>>(8*i));}
};
void rows(const std::vector<std::vector<U>>& data){std::cout<<'[';for(std::size_t i=0;i<data.size();++i){if(i)std::cout<<',';std::cout<<'[';for(std::size_t j=0;j<data[i].size();++j){if(j)std::cout<<',';std::cout<<data[i][j];}std::cout<<']';}std::cout<<']';}
void run(const std::string& op,U seed){
    const U v=seed/(op=="ledger"?5u:6u),slot=seed%17,client=CLIENTS+slot*STRIDE;
    Memory m;for(const auto& r:std::vector<std::pair<U,U>>{{ACTORS,8*0x114},{OWNERS,4*0x114},{CLIENTS,17*STRIDE},{DEFS,8*0x13c},{PACKET,15},{0x8cfa1c,4},{0x6e4760,3604},{0x6e5778,0x110},{0x50321c,4}})m.add(r.first,r.second);
    for(U i=0;i<17;++i)m.put(CLIENTS+i*STRIDE,10+i);
    for(U i=0;i<8;++i){const auto a=ACTORS+i*0x114,d=DEFS+i*0x13c;m.put(a,i==4?0:1);m.put(a+4,d);m.put(a+0x6c,i==2?2:i==3?4:0,1);m.put(a+0x74,0xabcd1234);m.put(a+0xf8,i==1?OWNERS+0x114:OWNERS);m.put(a+0x110,i==5?CB2:CB1);m.put(d+0x20,i==6?999:i==5?0xfb4:0xfc8);}
    const U actor_counts[]={0,1,6,8,0xffffffffu,0x80000000u};m.put(0x8cfa1c,actor_counts[seed%6]);
    m.put(OWNERS+0x24,0x11110000);m.put(OWNERS+0x28,0x22220000);m.put(OWNERS+0x78,0x11111111);m.put(OWNERS+0x114+0x24,0x33330000);m.put(OWNERS+0x114+0x28,0x44440000);m.put(OWNERS+0x114+0x78,0x22222222);
    m.put(client+8,OWNERS);m.put(client+0x1b8,32000);m.put(0x50321c,3);
    const U remaining[]={0,1,2,0xffffffffu,0x80000000u,0x7fffffffu};m.put(0x6e5884,remaining[seed%6]);
    const U free_slot=v%17,free_at=0x6e5778+free_slot*16;for(U i=0;i<16;++i)m.put(0x6e5784+i*16,1);
    if(free_slot<16)m.put(free_at+12,(v/17)%3==0?0:(v/17)%3==1?0xffffffffu:0x80000000u);
    if(v&128)for(U i=0;i<64;++i)m.put(client+0x204+i*0x264+9,1,1);
    const U counts[]={0,1,99,100,0x7fffffffu},count=counts[seed%5],record=0x6e4760+count*36;m.put(0x6e5570,count);
    const U slots[]={0,1,0xffff,0x8000,0x7fff};m.put(PACKET,0x12345680);m.put(PACKET+4,0x876543ff);m.put(PACKET+8,slots[v%5],2);m.put(PACKET+10,0x1234,2);m.put(PACKET+12,0xabcd,2);m.put(PACKET+14,4,1);
    m.put(client+0x1a4,0xffffffffu);const auto second=CLIENTS+((slot+1)%17)*STRIDE;m.put(second+8,OWNERS+0x114);m.put(second+0x1a4,10);
    if(op=="ledger")m.put(client+8,OWNERS);
    m.initial=m.data;std::vector<std::vector<U>> writes,calls;
    auto after_write=[&](U a){
        if(op=="trigger"&&a>=ACTORS&&a<ACTORS+8*0x114){if(v&2)m.put(a-0x74+0x110,CB2);if(v&32)throw std::runtime_error("fixture write fault");}
        if(op=="strike"){
            if(a==free_at+12&&(v&1))m.put(client+8,OWNERS+0x114);
            if(a==free_at&&(v&2))m.put(client+8,OWNERS);
            if(a==free_at+4&&(v&4))m.put(client+8,OWNERS+0x114);
            if(a==free_at+8&&(v&8))m.put(0x6e5884,50);
            if(a==0x6e5884&&(v&16))m.put(0x6e5884,99);
            if(a==free_at+12&&(v&32))throw std::runtime_error("fixture write fault");
        }
        if(op=="ledger"){
            if(a==record&&(v&1)){m.put(PACKET,0x99aa0011);m.put(PACKET+4,0xbbcc0022);}
            if(a==record+4&&(v&2)){m.put(record+4,0xdead0000);m.put(PACKET+4,0xcafe0099);}
            if(a==record+8&&(v&4)){m.put(record+4,0xbeef0000);m.put(PACKET+10,0x7777,2);m.put(PACKET+12,0x8888,2);}
            if(a==record+0x20&&(v&16))m.put(0x6e5570,88);
            if(a==0x6e5570&&(v&32))m.put(PACKET+8,1,2);
            if(a>=CLIENTS&&a<CLIENTS+17*STRIDE&&(v&64))m.put(a-0x1a4+8,OWNERS+0x114);
            if(a==record+0x10&&(v&256))throw std::runtime_error("fixture write fault");
        }
    };
    dfre::MissionPeerRequestBindings b;b.read=[&](U a,U w){return m.read(a,w);};b.write=[&](U a,U w,U value){m.put(a,value,w);writes.push_back({a,w,value});after_write(a);};
    b.call=[&](U site,const std::vector<U>& args){auto row=std::vector<U>{site};row.insert(row.end(),args.begin(),args.end());calls.push_back(row);
        if(op=="trigger"){
            if(v&1){m.put(0x8cfa1c,8);m.put(ACTORS+5*0x114+0x6c,2,1);}
            if(v&4){m.put(ACTORS+0x114+0xf8,OWNERS);m.put(DEFS+6*0x13c+0x20,0xfb4);}
            if(v&16)m.put(0x8cfa1c,0xffffffffu);
            if(v&8)throw std::runtime_error("fixture call fault");
        }
        if(op=="ledger"){
            if(v&8){m.put(0x6e5570,50);m.put(PACKET+8,1,2);m.put(PACKET+14,9,1);}
            if(v&128)throw std::runtime_error("fixture call fault");
        }
        return site==0x47df40?0x40000000u+seed:0u;
    };
    dfre::GameMessageStage messages;dfre::MissionServerMessageBuffer buffer;dfre::MissionClientOutboxBindings ob;
    ob.client=[&](std::size_t s){return dfre::MissionClientRecordView{m.span(CLIENTS+U(s)*STRIDE,STRIDE),STRIDE};};ob.cell=[&](U a){if(op=="strike"&&(v&64))m.put(0x6e5884,77);return m.read(a);};dfre::MissionClientOutbox outbox(messages,buffer,ob);
    bool failed=false;try{if(op=="trigger")dfre::mission_peer_trigger_charges(b,OWNERS);else if(op=="strike")dfre::mission_peer_request_strike(b,outbox,slot);else dfre::mission_peer_record_shot(b,m.span(PACKET,15),15);}catch(const std::runtime_error&){failed=true;}
    std::vector<std::vector<U>> changes;for(const auto& r:m.data){const auto& before=m.initial.at(r.first);for(U start=0;start<r.second.size();start+=4096){const U n=U(std::min<std::size_t>(4096,r.second.size()-start));if(std::equal(r.second.begin()+start,r.second.begin()+start+n,before.begin()+start))continue;for(U i=start;i<start+n;++i)if(r.second[i]!=before[i])changes.push_back({r.first+i,r.second[i]});}}
    std::cout<<"{\"failed\":"<<(failed?"true":"false")<<",\"changes\":";rows(changes);std::cout<<",\"writes\":";rows(writes);std::cout<<",\"calls\":";rows(calls);std::cout<<"}\n";
}
}
int main(){std::string op;U seed;while(std::cin>>op>>seed)run(op,seed);}
