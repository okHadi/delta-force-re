#include "dfre/mission_wet_effect.hpp"
#include "dfre/mission_actor_insert_live.hpp"
#include "dfre/mission_scene_transient.hpp"
#include <cassert>
#include <cstring>
#include <map>
#include <stdexcept>

namespace {
using U=std::uint32_t;
constexpr U frame=0x1300000,definition=0x1210000;
struct Memory {
    std::map<U,std::uint8_t> bytes;
    U read(U at,U width)const{U value=0;for(U i=0;i<width;++i){const auto p=bytes.find(at+i);if(p==bytes.end())throw std::runtime_error("Memory range has no owner");value|=U(p->second)<<(8*i);}return value;}
    void write(U at,U width,U value){for(U i=0;i<width;++i)bytes[at+i]=std::uint8_t(value>>(8*i));}
    void setup(){for(U i=0;i<0x130;++i)bytes[frame+i]=0xa5;write(frame+0x120,4,2);write(frame+0x124,4,0x11111111);write(frame+0x128,4,0x22222222);write(frame+0x12c,4,0x33333333);write(0x8e44b8,4,1);write(0xc23810,4,definition-0x13c);write(definition+0x6c,4,0x71000001);write(definition+0x84,4,0x453810);write(definition+0x90,4,dfre::kAiDefaultCallback);}
};
U word(const dfre::AiActorImage& image,U at){U result;std::memcpy(&result,image.data()+at,4);return result;}
}
int main(){using namespace dfre;
    Memory memory;memory.setup();MissionTickState state;std::vector<AiItemDefinition> definitions(2);definitions[1].field_9c=4;
    init_mission_tick(state,{},AiActorTables{},AiItemDefinitionTable(definitions),{},1);prepare_mission_extra_actor_pool(state);state.behavior.rng=0x12345678;U count=0;
    MissionActorInsertBindings delegated;delegated.read=[&](U at,U width){return memory.read(at,width);};delegated.write=[&](U at,U width,U value){memory.write(at,width,value);};
    auto insert=mission_actor_insert_live_bindings(state,count,delegated,[](U at){assert(at==definition);return 1u;});
    MissionWetEffectBindings b;b.read=delegated.read;b.write=delegated.write;unsigned randoms=0,insertions=0;
    b.call=[&](U site,const std::array<U,3>& args){
        if(site==0x49a620){assert(args[0]==frame&&args[1]==0&&args[2]==0x114);for(U i=0;i<args[2];++i)memory.bytes[frame+i]=0;memory.write(frame+0x124,4,0xabcdef01);memory.write(0xc23810,4,0x1230000);return frame;}
        if(site==0x477860){++randoms;assert(memory.read(frame+4,4)==definition);if(randoms==1)memory.write(definition+0x84,4,0x453930);else memory.write(definition+0x90,4,kAiDefaultCallback);return behavior_rng_next(state.behavior);}
        assert(site==0x45d970&&args[0]==frame&&args[1]==1);++insertions;return mission_actor_insert(insert,args[0],args[1]);
    };
    WeaponRandom rng;rng.seed(0x12345678);const U yaw=U(rng.next())<<16,scale=U(rng.next())+0x8000;
    mission_wet_effect(b,frame);assert(randoms==2&&insertions==1&&count==1&&state.behavior.rng==rng.state);
    auto image=snapshot_mission_actor(state,{4,0});assert(word(image,0)==1&&word(image,4)==1&&word(image,0x10)==0x71000001);assert(word(image,0x24)==0xabcdef01&&word(image,0x28)==0x22222222&&word(image,0x2c)==0x33333333);assert(word(image,0x30)==yaw&&word(image,0x7c)==scale&&word(image,0x6c)==0&&word(image,0x110)==0x453930&&word(image,0x10c)==kAiDefaultCallback);assert(insert.read(0x822e84,4)==definition);
    MissionRuntimeState world;world.definitions.resize(2);const auto records=mission_scene_transient_records(world,state);assert(records.size()==1&&records[0].group==4&&records[0].slot==0&&records[0].image==image);
    // A reached missing random body keeps the initial scratch fields and does not insert.
    memory.setup();b.call=[&](U site,const std::array<U,3>& args)->U{if(site==0x49a620){for(U i=0;i<args[2];++i)memory.bytes[args[0]+i]=0;return args[0];}throw std::runtime_error("Random body is not bound");};bool failed=false;try{mission_wet_effect(b,frame);}catch(const std::runtime_error&){failed=true;}assert(failed&&count==1&&memory.read(frame+0x24,4)==0x11111111&&memory.read(frame+0x30,4)==0&&memory.read(frame+0x110,4)==0);
    // The index read precedes the clear. An unbound source cannot clear its frame.
    memory.setup();b.read={};failed=false;try{mission_wet_effect(b,frame);}catch(const std::runtime_error&){failed=true;}assert(failed&&memory.read(frame,4)==0xa5a5a5a5);
}
