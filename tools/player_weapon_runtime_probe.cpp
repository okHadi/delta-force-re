#include "dfre/player_weapon_runtime.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
namespace {
using U=std::uint32_t;
std::vector<std::string> split(const std::string& s,char sep){std::stringstream in(s);std::vector<std::string> out;std::string v;while(std::getline(in,v,sep))out.push_back(v);return out;}
std::int32_t signed_word(U v){std::int32_t out;std::memcpy(&out,&v,4);return out;}
struct Fixture {
    dfre::WeaponState weapons;dfre::PlayerStepState player;dfre::PlayerAimState aim;
    dfre::WeaponRandom rng;std::array<U,2> bursts{};std::array<U,2> person{0,1},weapon{};
    std::array<dfre::PlayerGunActor,2> actors{{{{100,200,300},{0x12345678,0x23456789,0x3456789a}},{{1100,1200,1300},{0x456789ab,0x56789abc,0x6789abcd}}}};
    U mutation=0,camera=0,steady=0,crt=0,held=0;
    std::vector<std::vector<U>> calls;
    U index(U actor)const{if(actor==0x1200000)return 0;if(actor==0x1201000)return 1;throw std::runtime_error("actor fixture");}
    U& burst(U actor){return bursts[person[index(actor)]];}
    void change(U actor){
        if(mutation==1)burst(actor)=9;
        if(mutation==3){person[index(actor)]=1;player.local_actor_c23820=0x1201000;}
        if(mutation==9){std::array<std::uint8_t,dfre::kWeaponSlotCount> slots{};slots.fill(std::uint8_t((weapons.current_weapon()+1)%dfre::kWeaponCount));weapons.set_slot_map(slots);}
    }
    U random(){calls.push_back({0x477860});if(mutation==4)bursts[0]=3;return rng.next();}
    dfre::PlayerShotBindings shot_bindings(U actor){
        dfre::PlayerShotBindings b;b.random=[&]{return random();};b.burst_storage=[&,actor]()->U&{return burst(actor);};
        b.projectile=[&,actor](U kind,dfre::MuzzleResult& m){calls.push_back({0x46fb40,kind,burst(actor)});change(actor);if(mutation==2){m.aim.yaw+=11;m.aim.pitch+=13;}};
        b.melee=[&,actor]{calls.push_back({0x470b00});change(actor);};return b;
    }
    dfre::PlayerGunBindings view_bindings(){
        dfre::PlayerGunBindings b;
        b.steady=[&]{calls.push_back({0x484740});if(mutation==5){player.local_actor_c23820=0x1201000;player.eye_c23a18+=256;}return steady!=0;};
        b.actor=[&](U actor)->std::optional<dfre::PlayerGunActor>{return actors[index(actor)];};
        b.animated=[&](U actor){calls.push_back({0x45e7a0,actor});change(actor);};
        b.muzzle=[&]{calls.push_back({0x482d90});calls.push_back({0x4847d0});if(mutation==6)weapon[0]=(weapon[0]+1)%dfre::kWeaponCount;return dfre::MuzzleResult{{111,222,333},{444,555,666}};};
        b.shot=[&](U site,U actor,dfre::MuzzleResult& m){calls.push_back({0x45ea00,site,actor,U(m.origin.x),U(m.origin.y),U(m.origin.z),m.aim.yaw,m.aim.pitch,m.aim.roll});dfre::execute_player_shot(weapon[index(actor)],m,shot_bindings(actor));};
        b.crt_random=[&]{calls.push_back({0x49af6c});if(mutation==7){player.local_actor_c23820=0x1201000;actors[1].angles.yaw=0x40000000;player.recoil_c239e8[0]=123;}return crt;};
        b.translate_actor=[&](U actor,std::array<std::int32_t,2> d){auto& a=actors[index(actor)];a.position.x=signed_word(U(a.position.x)+U(d[0]));a.position.y=signed_word(U(a.position.y)+U(d[1]));};
        b.place=[&](U item,dfre::Fixed3 p,U yaw){calls.push_back({0x470f60,item,U(p.x),U(p.y),U(p.z),yaw});change(player.local_actor_c23820);};return b;
    }
    void setup(U id){std::array<std::uint8_t,dfre::kWeaponSlotCount> slots{};slots.fill(std::uint8_t(id));weapons.set_slot_map(slots);weapons.resupply(false);player.local_actor_c23820=0x1200000;player.eye_c23a18=0xb333;weapon={id,id};}
    dfre::PlayerWeaponBindings bindings(){
        dfre::PlayerWeaponBindings b;b.burst_storage=[&]()->U&{return burst(player.local_actor_c23820);};
        b.input_query=[&](U command){calls.push_back({0x47a290,command});if(mutation==8)weapons.set_aim_block_raw(1);return held!=0;};
        b.effect=[&](const dfre::WeaponEvent& e){U address=0;switch(e.kind){case dfre::WeaponEventKind::sound:address=0x43e7d0;break;case dfre::WeaponEventKind::sound2:address=0x43e720;break;case dfre::WeaponEventKind::queue:address=0x46f3e0;break;case dfre::WeaponEventKind::message:address=0x479f00;break;default:throw std::runtime_error("unexpected fixture shot");}calls.push_back({address,U(e.value)});change(player.local_actor_c23820);};
        b.view_body=[&](dfre::WeaponViewKind kind,U id){dfre::execute_player_view(kind,id,camera,player,view_bindings());};return b;
    }
    std::vector<U> snapshot(){std::vector<U> out{U(weapons.fire_timer()),U(weapons.reload_timer()),U(weapons.raise_counter()),weapons.aim_block_raw(),bursts[0],bursts[1],person[0],player.local_actor_c23820,player.eye_c23a18};for(auto r:player.recoil_c239e8)out.push_back(r);for(auto& a:actors){out.push_back(U(a.position.x));out.push_back(U(a.position.y));out.push_back(U(a.position.z));out.push_back(a.angles.yaw);out.push_back(a.angles.pitch);out.push_back(a.angles.roll);}out.push_back(rng.state);for(int i=0;i<int(dfre::kWeaponClassCount);++i){out.push_back(U(weapons.magazine(i)));out.push_back(U(weapons.reserve(i)));out.push_back(U(weapons.pending(i)));}out.push_back(weapons.current_weapon());return out;}
};
void output(const std::vector<U>& state,const std::vector<std::vector<U>>& calls){std::cout<<"{\"state\":[";for(std::size_t i=0;i<state.size();++i)std::cout<<(i?",":"")<<state[i];std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}std::cout<<"]}\n";}
void run(const std::string& line){
    const auto p=split(line,'|');if(p.size()!=2)throw std::runtime_error("mode|words");std::vector<U> v;for(const auto& word:split(p[1],','))v.push_back(U(std::stoull(word)));
    Fixture f;
    if(p[0]=="aim"){
        if(v.size()!=5)throw std::runtime_error("aim size");f.weapons.set_aim_block_raw(v[0]);f.aim={v[1],v[2]};f.rng.state=v[3];f.mutation=v[4];
        dfre::toggle_player_aim(f.weapons,f.aim,[&]{f.calls.push_back({0x477860});if(f.mutation)f.weapons.set_aim_block_raw(7);return f.rng.next();});
        output({f.weapons.aim_block_raw(),f.aim.sway_c23b68,f.aim.sway_c23b6c,f.rng.state},f.calls);
    } else if(p[0]=="shot"){
        if(v.size()!=8)throw std::runtime_error("shot size");f.setup(v[0]);f.bursts={v[1],v[2]};f.person[0]=v[3];f.rng.state=v[6];f.mutation=v[7];dfre::MuzzleResult m{{},{v[4],v[5],0}};
        dfre::execute_player_shot(v[0],m,f.shot_bindings(0x1200000));output({f.bursts[0],f.bursts[1],f.person[0],m.aim.yaw,m.aim.pitch,f.rng.state},f.calls);
    } else if(p[0]=="view"){
        if(v.size()!=7)throw std::runtime_error("view size");f.setup(v[0]);f.camera=v[1];f.steady=v[2];f.crt=v[3];f.mutation=v[4];f.bursts[0]=v[5];f.rng.state=v[6];
        const auto b=f.bindings();f.weapons.burst_storage=b.burst_storage;f.weapons.on_event=b.effect;f.weapons.view_body=b.view_body;f.weapons.shot_mode=dfre::WeaponShotMode::live_body;f.weapons.fire();output(f.snapshot(),f.calls);
    } else if(p[0]=="prefix"){
        if(v.size()!=13)throw std::runtime_error("prefix size");f.setup(v[0]);f.weapons.restore_timing({signed_word(v[1]),signed_word(v[2]),signed_word(v[3])});f.bursts[0]=v[4];f.weapons.set_aim_block_raw(v[5]);f.weapons.set_unlimited_ammo(v[6]!=0);f.held=v[7];f.mutation=v[8];f.camera=v[9];f.steady=v[10];f.crt=v[11];f.rng.state=v[12];
        dfre::player_weapon_step(f.weapons,f.player,f.bindings(),[&](U a)->std::optional<dfre::PlayerStepActor>{const auto& actor=f.actors[f.index(a)];return dfre::PlayerStepActor{{U(actor.position.x),U(actor.position.y)},0,0};});output(f.snapshot(),f.calls);
    } else throw std::runtime_error("mode");
}
}
int main(){std::string line;try{while(std::getline(std::cin,line))if(!line.empty())run(line);}catch(const std::exception& e){std::cerr<<e.what()<<" line="<<line<<'\n';return 1;}}
