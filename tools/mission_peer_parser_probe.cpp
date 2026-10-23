#include "dfre/mission_peer_parser.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
using U=std::uint32_t;
const std::vector<U> ids={0,1,2,4,9,10,11,0x12,0x13,0x14,0x17,0x21,0x22,0x28,0x2a,0x2c,0x2f,0x32,0x40,0x43,0x45,0x46,0x4c,0x4e,0x50,0x31};
U load(const std::uint8_t* p,U w=4){U v=0;for(U i=0;i<w;++i)v|=U(p[i])<<(8*i);return v;}
void save(std::uint8_t* p,U v,U w=4){for(U i=0;i<w;++i)p[i]=std::uint8_t(v>>(8*i));}
struct Memory {
    std::map<U,std::vector<std::uint8_t>> data,initial;
    void add(U a,U n){data[a]=std::vector<std::uint8_t>(n);}
    std::uint8_t* span(U a,std::size_t n){auto i=data.upper_bound(a);if(i==data.begin())throw std::runtime_error("fixture address");--i;const auto off=std::uint64_t(a)-i->first;if(off>i->second.size()||n>i->second.size()-std::size_t(off))throw std::runtime_error("fixture span");return i->second.data()+off;}
    U read(U a,U w=4){return load(span(a,w),w);}void write(U a,U v,U w=4){save(span(a,w),v,w);}
};
std::string hex(const std::uint8_t* p,std::size_t n){static constexpr char digits[]="0123456789abcdef";std::string s;for(std::size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
void list(const std::vector<std::vector<U>>& rows){bool comma=false;std::cout<<'[';for(const auto& row:rows){if(comma)std::cout<<',';comma=true;std::cout<<'[';for(std::size_t i=0;i<row.size();++i){if(i)std::cout<<',';std::cout<<row[i];}std::cout<<']';}std::cout<<']';}
void run(U seed){
    const U id=ids[seed%ids.size()],v=seed/U(ids.size()),slot=v%17,client=0x6e9a90+slot*0x9b84;
    Memory m;
    for(const auto& r:std::vector<std::pair<U,U>>{{0x6e9a90,17*0x9b84},{0x10000000,4096},{0x1500000,0x400},{0x1600000,0x100},{0x1700000,0x100},{0x7cca80,0x501*0x114},{0x1800000,39*0xa8},{0x1810000,12*0x34},{0xad3dd0,48},{0x8e61b4,12},{0x6e5878,12},{0x67f520,4},{0x67f538,4},{0x67dbc0,4},{0x6e5588,4},{0x50321c,4},{0xad3ed0,4},{0xad3ee4,4}})m.add(r.first,r.second);
    for(U s=0;s<17;++s)m.write(0x6e9a90+s*0x9b84,10+s);
    m.write(client+8,(v&8)?0:0x1500000);m.write(client+0x70,0xfffffff0);m.write(client+0x80,0xfffffffe);m.write(client+0x98,99);m.write(client+0x154,(v&4)?0xfffffff0u:40);m.write(client+0x158,500);m.write(client+0x15c,10);m.write(client+0x16c,0);m.write(client+0x170,1);m.write(client+0x1c4,(v%3==0)?5:(v%3==1)?1:3,2);m.write(client+0x1b8,32000);m.write(client+0x198,(v&16)?5:0);
    for(U i=0;i<12;++i){m.write(client+0xd4+i*4,4);m.write(0xad3dd0+i*4,7+i);}
    for(U i=0;i<64;++i){m.write(client+0x9b04+i*2,0xffff,2);if(v&32)m.write(client+0x204+i*0x264+9,1,1);}
    m.write(client+0x204+2*0x264,0x1234,2);m.write(client+0x204+2*0x264+9,1,1);
    const U seq=v%4==3?0:0x4321;if(v%4==1)m.write(client+0x9b04,seq,2);if(v%4==2)m.write(client+0x9b04+126,seq,2);
    m.write(0x1500000+0x6c,(v&2)?4:0,1);m.write(0x1500200+0x6c,0,1);
    m.write(0xad3ed0,0x1800000);m.write(0xad3ee4,0x1810000);m.write(0x67f538,100);m.write(0x67dbc0,(v&1)?1:0);m.write(0x67f520,0);m.write(0x6e5588,42);m.write(0x50321c,3);
    for(U i=0;i<4096;++i)m.write(0x10000000+i,(i*7+seed)&255,1);
    dfre::GameMessageStage messages;dfre::MissionServerMessageBuffer buffer;auto& table=messages.table();
    const auto d=table.descriptor(std::uint8_t(id));const U payload=0x10000001+((d.flags&6)?2:0);const U n=(d.flags&1)?8:d.length;
    m.write(0x10000000,id,1);if(d.flags&6)m.write(0x10000001,seq,2);
    if(d.flags&1)m.write(payload,8,2);
    if(id==9){m.write(payload,0x10002311);m.write(payload+4,0x20004522);m.write(payload+10,0x1234,2);m.write(payload+12,0xabcd,2);m.write(payload+14,4,1);}
    if(id==10){m.write(payload,0x10002311);m.write(payload+4,0x20004522);m.write(payload+12,(v&4)?0:10);}
    if(id==0x14)m.write(payload,0x1234,2);
    if(id==0x17){const U actor_slot=v%4==3?0xffff:(v%4==2?0x500:v%2);m.write(payload,actor_slot,2);if(actor_slot!=0xffff){const U actor=0x7cca80+actor_slot*0x114;m.write(actor+4,0x1600000);m.write(actor+0xc,0x1700000);m.write(actor+8,0x12345678);m.write(actor+0x24,0x11111111);m.write(actor+0x28,0x22222222);m.write(actor+0x2c,0x33333333);m.write(actor+0x30,0x44445555);m.write(actor+0x34,0x66667777);m.write(actor+0x6c,0x1234,2);m.write(actor+0x70,0x56,1);m.write(actor+0x98,0x78,1);m.write(0x1600000+0x20,0x1234);m.write(0x1600000+0x24,4);m.write(0x1600000+0x28,0x89,1);const char name[]="SOLDIER";for(U i=0;i<sizeof(name);++i)m.write(0x1700004+i,U(name[i]),1);}}
    if(id==0x4c){for(U i=0;i<7;++i)m.write(payload+i,0,1);if(v&1)m.write(payload+6,1,1);}
    if(id==0x4e){m.write(payload,0,2);m.write(payload+2,0,1);if(v&1)m.write(payload,1,2);}
    if(id==0x50){m.write(payload,0x12345678);m.write(payload+4,(v&1)?18:17);}
    m.write(payload+n,0x32,1);m.write(0x8e61b4,(v%29==28)?0:0x10000000);m.write(0x8e61b8,payload+n+1-0x10000000);m.write(0x8e61bc,v%11==10?99:10+slot);
    m.initial=m.data;std::vector<std::vector<U>> calls;std::vector<std::string> payloads;
    dfre::MissionClientOutboxBindings ob;ob.client=[&](std::size_t s){return dfre::MissionClientRecordView{m.span(0x6e9a90+U(s)*0x9b84,0x9b84),0x9b84};};
    ob.cell=[&](U a){if(a==0x50321c&&(v&128)){m.write(client+0x9b04,seq,2);m.write(payload,0x1234,2);}return m.read(a);};
    dfre::MissionClientOutbox outbox(messages,buffer,ob);
    dfre::MissionPeerParserBindings b;b.client=ob.client;b.read=[&](U a,U w){return m.read(a,w);};b.write=[&](U a,U w,U value){m.write(a,value,w);};b.resolve=[&](U a,std::size_t size){return m.span(a,size);};
    b.call=[&](const dfre::MissionPeerParserCall& c){
        std::vector<U> row{c.address};for(U i=0;i<c.count;++i)row.push_back(c.args[i]);calls.push_back(row);
        if(c.address==0x493a80){for(U i=0;i<c.length;++i)c.payload[i]=std::uint8_t(i+seed);payloads.emplace_back();}
        else payloads.push_back(c.payload?hex(c.payload,c.length):"");
        if((v&64)&&c.address==0x47df40){m.write(client+8,0x1500200);m.write(0x67f538,200);m.write(payload+14,8,1);m.write(0xad3dd0+4*4,33);}
        if((v&256)&&c.address==0x445b80){m.write(client+8,0x1500200);m.write(payload+14,9,1);m.write(client+0x154,0x12345678);}
        if((v&512)&&c.address==0x462f20){m.write(client+0x154,0x87654321);m.write(client+0xd4+4*4,77);m.write(0xad3dd0+4*4,44);}
        if((v&1024)&&c.address!=0x493a80&&c.address!=0x4564b0)throw std::runtime_error("fixture handler fault");
        return c.address==0x47df40?0x30000000u:c.address==0x46c180?0x12345678u:0u;
    };
    bool failed=false;try{dfre::mission_peer_parser_parse(outbox,table,b);}catch(const std::runtime_error&){failed=true;}
    std::vector<std::vector<U>> changes;for(const auto& region:m.data){const auto& initial=m.initial.at(region.first);for(U i=0;i<region.second.size();++i)if(region.second[i]!=initial[i])changes.push_back({region.first+i,region.second[i]});}
    std::cout<<"{\"failed\":"<<(failed?"true":"false")<<",\"changes\":";list(changes);std::cout<<",\"calls\":";list(calls);std::cout<<",\"payloads\":[";for(std::size_t i=0;i<payloads.size();++i){if(i)std::cout<<',';std::cout<<'"'<<payloads[i]<<'"';}std::cout<<"]}\n";
}
}
int main(){U seed;while(std::cin>>seed)run(seed);}
