#include "dfre/game_messages.hpp"
#include "dfre/mission_peer_frame.hpp"
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
int main(){
    using U=std::uint32_t;
    U seed;
    while(std::cin>>seed){
        const U mode=seed%4,flags=seed/4;
        dfre::GameMessagePeerBuffer buffer;
        for(U i=0;i<8192;++i)buffer.bytes_8ec1cc[i]=std::uint8_t(seed+i*7);
        const U counts[]={1,8191,8192,0x7fffffffu,0x80000000u,0xffffffffu,16,1024};
        buffer.count_8ee1cc=mode==0?counts[flags%8]:0;
        std::map<U,U> cells{{0x67f520,mode>=2?1u:0u},{0x50133c,0xa0010000u+seed},{0x501344,0xb0020000u+seed},
            {0x50135c,0x10000000u+seed},{0x501360,seed%8193},{0x8e61b4,0x11111111},{0x8e61b8,0x22222222},{0x8e61bc,0x33333333}};
        std::vector<std::vector<U>> writes,calls;
        U step=0;
        dfre::MissionPeerFrameBindings b;
        b.cell=[&](U a){return cells.at(a);};
        b.store=[&](U a,U v){cells[a]=v;writes.push_back({a,v});++step;
            if(flags&1){cells[0x50133c]=0xc0000000u+step;cells[0x501344]=0xc1000000u+step;cells[0x50135c]=0x10001000u+step;cells[0x501360]=100+step;}
            if(flags&2)cells[0x8e61b4]=0xd0000000u+step;
            if(flags&4)buffer.count_8ee1cc=70+step;
            if(flags&8)buffer.bytes_8ec1cc[0]=std::uint8_t(step);
        };
        b.receive_497790=[&]{calls.push_back({0x497790,buffer.count_8ee1cc,cells[0x8e61b4]});
            if(flags&16){cells[0x501344]=0xe0010000u+seed;cells[0x50135c]=0x10002000u+seed;cells[0x501360]=200+seed;buffer.count_8ee1cc=55;}
            return mode==3?0x12345678u:0u;};
        const auto result=dfre::mission_peer_frame_receive(buffer,b);
        auto lists=[](const std::vector<std::vector<U>>& a){std::cout<<'[';bool comma=false;for(const auto& row:a){if(comma)std::cout<<',';comma=true;std::cout<<'[';for(std::size_t i=0;i<row.size();++i){if(i)std::cout<<',';std::cout<<row[i];}std::cout<<']';}std::cout<<']';};
        std::cout<<"{\"result\":"<<result<<",\"count\":"<<buffer.count_8ee1cc<<",\"first\":"<<unsigned(buffer.bytes_8ec1cc[0])<<",\"cells\":[";
        bool comma=false;for(const auto& c:cells){if(comma)std::cout<<',';comma=true;std::cout<<'['<<c.first<<','<<c.second<<']';}
        std::cout<<"],\"bytes\":\"";
        static constexpr char hex[]="0123456789abcdef";
        for(const auto byte:buffer.bytes_8ec1cc)std::cout<<hex[byte>>4]<<hex[byte&15];
        std::cout<<"\",\"writes\":";lists(writes);std::cout<<",\"calls\":";lists(calls);std::cout<<"}\n";
    }
}
