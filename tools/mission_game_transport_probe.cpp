#include "dfre/mission_game_transport.hpp"
#include <array>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
using U=std::uint32_t;
struct Fixture {
    std::array<U,17> row{};std::map<U,U> memory;std::vector<U> frames;
    U reads=0,writes=0,calls=0,sends=0,receives=0,depth=0,heads=0;
    std::ostringstream trace;
    U norm(U at)const{for(std::size_t i=0;i<frames.size();++i)if(at>=frames[i]&&at<frames[i]+4)return 0x30000000u+U(i)*0x100u+(at-frames[i]);return at;}
    U get(U at)const{const auto p=memory.find(at);return p==memory.end()?0:p->second;}
    void initialize(){memory[0x501320]=row[1];memory[0x501398]=row[2]?0x501ab8:0x501a28;for(auto table:{0x501ab8u,0x501a28u}){memory[table+0x54]=table==0x501ab8?0x4b9530:0x4b83b0;memory[table+0x58]=table==0x501ab8?0x4b95d0:0x4b8470;memory[table+0x60]=table==0x501ab8?0x4b96b0:0x4b85b0;}memory[0xd0a540]=row[3];memory[0x501344]=row[4];memory[0x501354]=row[6];memory[0x501364]=0x12345678;memory[0x50135c]=0xfefefefe;memory[0x501360]=0xfdfdfdfd;heads=row[15];memory[0x501328]=heads?0x1200000:0;}
    dfre::MissionGameTransportBindings bindings(){dfre::MissionGameTransportBindings b;
        b.read=[&](U at,U width){if(width!=4)throw std::runtime_error("Unexpected width");trace<<'R'<<norm(at)<<';';if(++reads==row[10])throw std::runtime_error("Read failure");if((row[9]&1)&&at==0x501398)memory[0x501398]=0x501ab8;if((row[9]&4)&&at==0x501344){memory[0x501354]^=0xffffffffu;memory[0xd0a540]^=0x2000;}if((row[9]&256)&&(at==0x501ab8+0x54||at==0x501ab8+0x58||at==0x501ab8+0x60))memory[0x501398]=0x501a28;return get(at);};
        b.write=[&](U at,U width,U value){if(width!=4)throw std::runtime_error("Unexpected width");trace<<'W'<<norm(at)<<','<<norm(value)<<';';if(++writes==row[11])throw std::runtime_error("Store failure");if((row[9]&512)&&norm(at)>=0x30000000u&&norm(at)<0x30010000u)memory[0x501320]^=0xffffffffu;memory[at]=value;};
        b.before_call=[&](U site,const std::vector<U>& args){trace<<'C'<<site;for(auto arg:args)trace<<','<<norm(arg);trace<<';';if(++calls==row[12])throw std::runtime_error("Call failure");};
        b.frame=[&](U site,U size){if(site!=0x4b9530||size!=4)throw std::runtime_error("Unexpected frame");const auto at=0x1300000u+depth++*0x100u;memory[at]=0xa5a5a5a5;frames.push_back(at);return at;};
        b.release=[&](U site,U,U size){if(site!=0x4b9530||size!=4||!depth)std::terminate();--depth;};
        b.call=[&](U site,const std::vector<U>& args)->U{
            if(site==0x4c551a){++receives;if(row[9]&2)memory[0xd0a540]^=0x1000;if(receives<=row[14]||row[4]){memory[args.at(1)]=row[5]+receives;memory[args.at(2)]=receives<=row[14]?0:row[4];return 0;}return 12;}
            if(site==0x4c5514||site==0x4c550e){return ++sends<=row[8]?0x10:row[7];}
            if(site==0x490880){if(row[13]==1)memory[0x501364]=0x800;if(row[13]==2)memory[0x501364]=0;if(row[13]==3)memory[0x501364]=0xffffffffu;return 0;}
            if(site==0x4b9400){if(row[9]&16){memory[0x501320]=0;memory[0xd0a540]^=0x4000;}return 0;}
            if(site==0x4c5562){if(row[9]&32){memory[0x501320]=0;memory[0xd0a540]^=0x8000;}return row[7];}
            if(site==0x4983b0)return row[16];
            if(site==0x4c555c){if(row[9]&64)memory[0xd0a540]^=0x10000;return row[7];}
            if(site==0x498400){if(row[9]&128){memory[0xd0a540]=0x12345678;memory[0x501320]=7;}return 0;}
            if(site==0x4b8da0){if(heads)--heads;memory[0x501328]=heads?get(0x501328)+0x100:0;return 0;}
            if(site==0x4b83b0||site==0x4b8470||site==0x4b85b0)return row[7];
            throw std::runtime_error("Unbound source body");};return b;
    }
};
}
int main(){std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;Fixture f;std::istringstream in(line);for(auto& n:f.row)if(!(in>>n))return 1;f.initialize();auto b=f.bindings();U result=0;bool failed=false;try{switch(f.row[0]){case 0:result=dfre::mission_game_transport_receive(b);break;case 1:result=dfre::mission_game_transport_send_server(b,0x1201000,f.row[5]);break;case 2:result=dfre::mission_game_transport_send_client(b,0x1201000,f.row[5],0x87654321);break;case 3:result=dfre::mission_game_transport_receive_netsock(b);break;case 4:result=dfre::mission_game_transport_send_server_netsock(b,0x1201000,f.row[5]);break;case 5:result=dfre::mission_game_transport_send_client_netsock(b,0x1201000,f.row[5],0x87654321);break;case 6:result=dfre::mission_game_transport_initialize_netsock(b);break;case 7:dfre::mission_game_transport_close_netsock(b);break;default:return 1;}}catch(const std::runtime_error&){failed=true;}if(f.depth)return 2;std::cout<<failed<<' '<<result<<' '<<f.reads<<' '<<f.writes<<' '<<f.calls<<' ';for(U at:{0x501320u,0x501398u,0xd0a540u,0x501344u,0x501354u,0x501364u,0x50135cu,0x501360u,0x501328u})std::cout<<f.get(at)<<',';std::cout<<' ';for(auto at:f.frames)std::cout<<f.get(at)<<',';std::cout<<' '<<f.trace.str()<<'\n';}}
