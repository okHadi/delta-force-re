#include "dfre/mission_actor_create.hpp"
#include "dfre/mission_actor_lifecycle.hpp"
#include "dfre/mission_client_finalize.hpp"
#include "dfre/mission_client_live.hpp"
#include <cstring>
#include <iostream>
#include <map>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool n){if(!n)throw MissionActorCreateError("Current actor creation check failed");}
struct Fixture {
    MissionTickState state;MissionClientLive clients;CharacterModel model;std::unique_ptr<MissionAnimations> animations;
    std::map<U,U> cells;U highwater=1;bool allow_character=true;MissionActorCreateBindings b;
    Fixture(){
        AiActorTables tables;tables.groups[3].resize(256);tables.groups[3][0].active=true;tables.groups[3][0].definition_index=1;
        std::vector<AiPersonState> people(2);people[0].owner=0x7cca80;
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable({AiItemDefinition{},AiItemDefinition{}}),std::move(people),0);
        model.runtime_prepared=true;model.bone_map={0};animations=std::make_unique<MissionAnimations>(std::vector<MissionAnimationBinding>{{kCharacterInstanceBase,&model,{}}});
        cells={{0x8cf97c,0},{0x8cf982,0},{0xc23810,0x18000000},{0x1800013c+0x6c,23},{0x1800013c+0x74,31},{0x1800013c+0x84,0x45bdf0},{0x1800013c+0x90,0x45b590},{0x1800013c+0x94,20000},{0x67f549,1}};
        b.read=[&](U at,U width)->U{
            if(at==0x67f528&&width==4)return state.globals.mission_67f528;
            if(clients.claims(at,width))return clients.read(at,width);
            if(at==0x8cfa18&&width==4)return highwater;
            if(at>=0x7cca80&&std::uint64_t(at-0x7cca80)+width<=256*0x114){const U delta=at-0x7cca80,slot=delta/0x114,offset=delta%0x114;check(offset+width<=0x114);if(offset==0x0c&&width==4){const auto person=state.tables.groups[3][slot].person_slot;return person?0x794680+U(*person)*0xb4:0;}if(offset==4&&width==4)return state.tables.groups[3][slot].active?0x18000000+state.tables.groups[3][slot].definition_index*0x13c:0;const auto image=snapshot_mission_actor(state,{3,slot});U value=0;for(U i=0;i<width;++i)value|=U(image[offset+i])<<(8*i);return value;}
            const auto p=cells.find(at);if(p==cells.end())throw MissionActorCreateError("Fixture read has no current cell");return width==1?p->second&255:width==2?p->second&65535:p->second;
        };
        b.write=[&](U at,U width,U value){
            if(clients.claims(at,width)){clients.write(at,width,value);return;}
            if(at>=0x794680&&std::uint64_t(at-0x794680)+width<=state.people.size()*0xb4){const U delta=at-0x794680,offset=delta%0xb4;check(width==1&&offset>=4&&offset<0x24);auto& name=state.people[delta/0xb4].name;const auto index=std::size_t(offset-4);if(!value)name.resize(index);else{if(name.size()<=index)name.resize(index+1,0);name[index]=char(value);}return;}
            if(at==0x8cfa18&&width==4){highwater=value;return;}
            if(at<0x7cca80||std::uint64_t(at-0x7cca80)+width>256*0x114)throw MissionActorCreateError("Fixture write has no current actor");
            const U delta=at-0x7cca80,slot=delta/0x114,offset=delta%0x114;auto& actor=state.tables.groups[3][slot];
            switch(offset){
                case 0:check(width==4&&value==1);actor.active=true;actor.definition_index=value;break;
                case 4:check(width==4&&value==0x18000000+actor.definition_index*0x13c);break;
                case 8:check(width==4);actor.object_id=value;break;case 0x70:check(width==2);actor.field_70=std::uint16_t(value);break;
                case 0x10:actor.field_10=value;break;case 0x14:actor.field_14=value;break;
                case 0x18:check(animations->pose(value)!=nullptr);actor.field_18=value;break;
                case 0x0c:{check(value>=0x794680&&(value-0x794680)%0xb4==0);const auto person=(value-0x794680)/0xb4;check(person<state.people.size()&&state.people[person].owner==0x7cca80+slot*0x114);actor.person_slot=person;break;}
                case 0x6c:check(width==4&&value==3);actor.field_6c=3;state.extra_for({3,slot}).field_6d=0;state.runtime_for({3,slot}).byte_6e=0;break;
                case 0x72:check(width==2);actor.field_72=std::uint16_t(value);break;
                case 0x74:check(width==2);state.extra_for({3,slot}).field_74=std::uint16_t(value);break;
                case 0x98:check(width==4);std::memcpy(state.stored_actor_images[3][slot].data()+offset,&value,4);break;
                case 0x110:actor.field_110=value;break;case 0x10c:actor.field_10c=value;break;
                default:throw MissionActorCreateError("Fixture construction field has no current owner");
            }
        };
        b.call=[&](U site,const std::vector<U>& args)->U{
            if(site==0x4828c0){check(args==std::vector<U>{0x138b});return 1;}
            if(site==0x49a620){check(args.at(1)==0&&args.at(2)==0x114);const auto slot=(args.at(0)-0x7cca80)/0x114;state.tables.groups[3].at(slot)={};state.stored_actor_images[3].at(slot)={};state.actor_extras[3].at(slot)={};state.runtimes[3].at(slot).reset();return args.at(0);}
            if(site==0x42cce0){if(!allow_character)throw MissionActorCreateError("Current character allocation is not bound");check(args==std::vector<U>{31}&&animations->pose(kCharacterInstanceBase));return kCharacterInstanceBase;}
            if(site==0x44a0c0){for(U slot=0;slot<state.people.size();++slot)if(!state.people[slot].owner){state.people[slot].owner=args.at(0);return 0x794680+slot*0xb4;}throw MissionActorCreateError("Current person pool is full");}
            throw MissionActorCreateError("Fixture actor service is not bound");
        };
    }
};
}
int main(){try{
    Fixture f;const auto raw=mission_actor_create(f.b,7,0);check(raw==mission_actor_source_address(3,1)&&f.highwater==2);const auto& actor=f.state.tables.groups[3][1];
    check(actor.active&&actor.definition_index==1&&actor.person_slot==1&&actor.field_18==kCharacterInstanceBase&&actor.field_10==23&&actor.field_6c==3&&actor.field_72==1);
    check(f.state.people[1].owner==raw&&f.state.extra_for({3,1}).field_74==14464);const auto image=snapshot_mission_actor(f.state,{3,1});U face=0;std::memcpy(&face,image.data()+0x98,4);check(face==7);
    Fixture joined;joined.clients.write(mission_client_source_base,4,77);joined.clients.write(mission_client_source_base+0x18c,1,1);joined.clients.write(mission_client_source_base+0xa0,4,7);joined.clients.write(mission_client_source_base+0x9c,2,2);
    const char* player="player";for(U i=0;i<=6;++i)joined.clients.write(mission_client_source_base+0x0c+i,1,std::uint8_t(player[i]));
    MissionClientAddedBindings finalize;finalize.read=joined.b.read;finalize.write=joined.b.write;finalize.call=[&](U site,const std::vector<U>& args)->U{if(site==0x45d520)return mission_actor_create(joined.b,args.at(0),args.at(1));if(site==0x43f610){check(args==std::vector<U>{mission_client_source_base});joined.state.tables.groups[3][1].position[0]=123;return 0;}throw MissionActorCreateError("Joined finalization service is not bound");};
    mission_client_finalize(finalize);check(joined.clients.read(mission_client_source_base+8,4)==mission_actor_source_address(3,1)&&joined.state.tables.groups[3][1].object_id==77&&joined.state.tables.groups[3][1].field_70==2&&joined.state.tables.groups[3][1].position[0]==123&&joined.state.people[1].name=="player");
    Fixture missing;missing.allow_character=false;bool failed=false;try{mission_actor_create(missing.b,7,0);}catch(const MissionActorCreateError&){failed=true;}check(failed&&missing.highwater==2&&missing.state.tables.groups[3][1].active&&missing.state.tables.groups[3][1].field_18==0&&!missing.state.tables.groups[3][1].person_slot);
    for(auto& a:f.state.tables.groups[3]){a.active=true;a.definition_index=1;}check(mission_actor_create(f.b,7,0)==0&&f.highwater==2);
    std::cout<<"Canonical actor/CHR/person construction passed with explicit allocation fixtures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
