#include "dfre/mission_object_live.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {using U=std::uint32_t;void put(AiActorImage& image,std::size_t at,U n){std::memcpy(image.data()+at,&n,4);}std::string hex(const AiActorImage& image){const char* d="0123456789abcdef";std::string s;for(auto n:image){s+=d[n>>4];s+=d[n&15];}return s;}}
int main(){try{std::string line;while(std::getline(std::cin,line)){if(line.empty())continue;std::istringstream input(line);U v[16];for(auto& n:v){std::uint64_t x;if(!(input>>x))throw MissionTickError("Short veh0 move fixture");n=U(x);}
    MissionTickState state;AiActorTables tables;tables.groups[0].resize(1);tables.groups[0][0].active=true;tables.groups[0][0].definition_index=1;
    init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),{},0x12345678);
    auto image=snapshot_mission_actor(state,{0,0});image[0x6c]=std::uint8_t(v[1]);put(image,0x7c,v[3]);put(image,0x88,v[4]);put(image,0x30,v[5]);put(image,0x8c,v[6]);put(image,0x24,v[14]);put(image,0x28,v[15]);apply_mission_actor_image(state,{0,0},image);
    U axle=v[8];unsigned sine_calls=0,ground_calls=0,definition_reads=0;std::ostringstream trace;MissionObjectLiveBindings b;
    b.definition=[&](auto& current,U index,U at){trace<<'D'<<index<<','<<at<<';';if(at==0xe4){const U result=axle;if(definition_reads++==0&&(v[10]&8))axle=0x40000;return result;}if(at==0xdc){if(v[10]&128)current.tables.groups[0][0].field_6c^=4;return v[7];}throw MissionTickError("Unknown definition field");};
    b.global=[&](auto&,U at){trace<<'G'<<at<<';';if(at<0xbd0068||(at-0xbd0068)%4)throw MissionTickError("Unknown sine address");return 0xb16b00b5u^(((at-0xbd0068)/4)*0x10203u);};
    b.type_slot=[&](auto& current,U slot,U at){trace<<'T'<<slot<<','<<at<<';';if(v[10]&32)current.tables.groups[0][0].field_7c=0x80000000;return v[9];};
    b.call=[&](auto& current,const auto&,const ObjectBehaviorAction& action){trace<<'C'<<action.site;for(auto n:action.args)trace<<','<<n;trace<<';';
        std::array<U,6> values{};
        if(action.site==0x477820){if(sine_calls++==0&&(v[10]&1)){current.tables.groups[0][0].position[0]+=123;current.tables.groups[0][0].position[1]-=456;}values[1]=v[11];values[2]=v[12];return values;}
        if(action.site==0x47df40){const unsigned call=ground_calls++;if(call==0&&(v[10]&2)){current.tables.groups[0][0].position[0]+=1000;current.tables.groups[0][0].position[1]-=2000;}
            if(call==1&&(v[10]&4)){current.tables.groups[0][0].field_7c=0x123;current.tables.groups[0][0].angle_pitch=0x9876;}
            if(call==2&&(v[10]&16))throw MissionTickError("Ground failure");
            values[0]=v[13]+U(call)*0x13579u;return values;}
        throw MissionTickError("Unknown move call");};
    bool failed=false;try{mission_object_veh0_move(state,{0,0},b);}catch(const std::exception&){failed=true;}
    std::cout<<failed<<' '<<hex(snapshot_mission_actor(state,{0,0}))<<' '<<trace.str()<<'\n';
}}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
