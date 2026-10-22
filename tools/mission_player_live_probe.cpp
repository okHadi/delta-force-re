#include "dfre/mission_player_live.hpp"
#include "dfre/world_runtime.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace {
using namespace dfre;
using U=std::uint32_t;
std::int32_t signed_word(U v){std::int32_t out;std::memcpy(&out,&v,4);return out;}
std::vector<std::string> split(const std::string& s,char separator){
    std::stringstream stream(s);std::string v;std::vector<std::string> out;
    while(std::getline(stream,v,separator))out.push_back(v);return out;
}
void output(const std::vector<U>& state,const std::vector<std::vector<U>>& calls){
    std::cout<<"{\"state\":[";for(std::size_t i=0;i<state.size();++i)std::cout<<(i?",":"")<<state[i];
    std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");
        for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}
    std::cout<<"]}\n";
}
void run(const std::string& line){
    const auto pieces=split(line,'|');if(pieces.size()!=2)throw std::runtime_error("mode|words");
    std::vector<U> v;for(const auto& item:split(pieces[1],','))v.push_back(U(std::stoull(item)));
    MissionTickState state;MissionPlayerCells cells;
    MissionAnimations animations{std::vector<MissionAnimationBinding>{}};
    MissionMuzzle muzzle{MissionMuzzleResources{{RuntimeItemDefinition{}},{},{}},animations};
    std::vector<std::vector<U>> calls;
    MissionStageCall call;
    if(pieces[0]=="aim"){
        if(v.size()!=5 || v[4])throw std::runtime_error("aim needs five words and pure shared RNG");
        state.weapons.set_aim_block_raw(v[0]);cells.aim={v[1],v[2]};state.behavior.rng=v[3];
        if(!v[0])calls.push_back({0x477860});
        MissionPlayerLive live(state,cells,muzzle,{});call.address=0x484c20;live.stage(call);
        output({state.weapons.aim_block_raw(),cells.aim.sway_c23b68,cells.aim.sway_c23b6c,state.behavior.rng},calls);return;
    }
    const bool fire=pieces[0]=="view";
    if((!fire && pieces[0]!="prefix") || v.size()!=13)throw std::runtime_error("prefix/view needs thirteen words");
    AiActorTables tables;tables.groups[3].resize(2);
    for(U i=0;i<2;++i){auto& a=tables.groups[3][i];a.active=true;a.definition_index=1;a.person_slot=i;
        a.position[0]=100+i*1000;a.position[1]=200+i*1000;a.position[2]=300+i*1000;
        a.angle_yaw=i?0x456789ab:0x12345678;a.angle_pitch=i?0x56789abc:0x23456789;
    }
    MissionTickConfig config;config.local_actor=0;
    init_mission_tick(state,config,std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),
        std::vector<AiPersonState>(2),v[12]);
    state.runtime_for({3,0}).roll_38=0x3456789a;state.runtime_for({3,1}).roll_38=0x6789abcd;
    state.extra_for({3,0}).field_94=v[0];state.extra_for({3,1}).field_94=v[0];
    std::array<std::uint8_t,kWeaponSlotCount> map{};map.fill(std::uint8_t(v[0]));
    state.weapons.set_slot_map(map);state.weapons.resupply(false);state.weapons.take_events();
    state.weapons.restore_timing({signed_word(v[1]),signed_word(v[2]),signed_word(v[3])});
    state.person_extra(0).field_6c=v[4];state.weapons.set_aim_block_raw(v[5]);
    state.weapons.set_unlimited_ammo(v[6]!=0);cells.camera_mode_6ba3ec=v[9];state.scope_c23a24=v[10];
    const auto change=[&]{
        if(v[8]==1)mission_player_burst(state)=9;
        if(v[8]==3){state.tables.groups[3][*state.config.local_actor].person_slot=1;state.config.local_actor=1;}
        if(v[8]==9){map.fill(std::uint8_t((state.weapons.current_weapon()+1)%kWeaponCount));state.weapons.set_slot_map(map);}
    };
    state.world.weapon_effect=[&](auto&,const WeaponEvent& event){U address=0;
        switch(event.kind){case WeaponEventKind::sound:address=0x43e7d0;break;
        case WeaponEventKind::sound2:address=0x43e720;break;case WeaponEventKind::queue:address=0x46f3e0;break;
        case WeaponEventKind::message:address=0x479f00;break;default:throw std::runtime_error("fixture event");}
        calls.push_back({address,U(event.value)});change();return true;
    };
    state.world.trigger_held_47a290=[&](U command){calls.push_back({0x47a290,command});
        if(v[8]==8)state.weapons.set_aim_block_raw(1);return v[7]!=0;
    };
    MissionPlayerProviders providers;
    providers.projectile=[&](auto&,auto&,U actor,U,U kind,MuzzleResult& result){
        std::size_t slot=actor==actor_handle(3,0)?0:1;
        const auto person=*state.tables.groups[3][slot].person_slot;
        calls.push_back({0x46fb40,kind,state.person_extra(person).field_6c,
            U(result.origin.x),U(result.origin.y),U(result.origin.z),result.aim.yaw,result.aim.pitch,result.aim.roll});
        if(v[8]==1)state.person_extra(person).field_6c=9;
        if(v[8]==3){state.tables.groups[3][slot].person_slot=1;state.config.local_actor=1;}
        if(v[8]==9){map.fill(std::uint8_t((state.weapons.current_weapon()+1)%kWeaponCount));state.weapons.set_slot_map(map);}
        if(v[8]==2){result.aim.yaw+=11;result.aim.pitch+=13;}
    };
    providers.melee=[&](auto&,auto&,U,U){calls.push_back({0x470b00});change();};
    providers.crt_random=[&](auto&,auto& current){calls.push_back({0x49af6c});
        if(v[8]==7){state.config.local_actor=1;state.tables.groups[3][1].angle_yaw=0x40000000;current.recoil_c239e8[0]=123;}
        return v[11];
    };
    providers.place=[&](auto&,auto&,U item,Fixed3 p,U yaw){
        calls.push_back({0x470f60,item,U(p.x),U(p.y),U(p.z),yaw});change();
    };
    MissionPlayerLive live(state,cells,muzzle,providers);
    if(fire){
        live.install();state.weapons.fire();
    }else{call.address=0x482b00;live.stage(call);}
    const auto normalized_local=state.config.local_actor?0x1200000u+U(*state.config.local_actor)*0x1000:0;
    std::vector<U> out{U(state.weapons.fire_timer()),U(state.weapons.reload_timer()),U(state.weapons.raise_counter()),
        state.weapons.aim_block_raw(),state.person_extra(0).field_6c,state.person_extra(1).field_6c,
        U(*state.tables.groups[3][0].person_slot),normalized_local,cells.eye_c23a18};
    for(auto value:cells.recoil_c239e8)out.push_back(value);
    for(U i=0;i<2;++i){const auto& a=state.tables.groups[3][i];for(auto value:a.position)out.push_back(value);
        out.push_back(a.angle_yaw);out.push_back(a.angle_pitch);out.push_back(state.runtime_for({3,i}).roll_38);}
    out.push_back(state.behavior.rng);
    for(int i=0;i<int(kWeaponClassCount);++i){out.push_back(U(state.weapons.magazine(i)));
        out.push_back(U(state.weapons.reserve(i)));out.push_back(U(state.weapons.pending(i)));}
    out.push_back(state.weapons.current_weapon());output(out,calls);
}
}
int main(){std::string line;try{while(std::getline(std::cin,line))if(!line.empty())run(line);}
catch(const std::exception& e){std::cerr<<e.what()<<" line="<<line<<'\n';return 1;}}
