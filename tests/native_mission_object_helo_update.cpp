#include "dfre/mission_object_helo_update.hpp"
#include "dfre/mission_live.hpp"
#include "dfre/mission_sound_registration.hpp"
#include "dfre/mission_audio_frame.hpp"
#include <iostream>
#include <stdexcept>
using namespace dfre;using U=std::uint32_t;
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
template<class B> U get(const B& bytes,U at,U width=4){U n=0;for(U i=0;i<width;++i)n|=U(bytes.at(at+i))<<(8*i);return n;}
template<class B> void put(B& bytes,U at,U n,U width=4){for(U i=0;i<width;++i)bytes.at(at+i)=std::uint8_t(n>>(8*i));}
struct Fixture {
    static constexpr U a=0x7dde80,p=0x794680,d=0x1302000,f=0x1304000,ambient=0x1305000,entry=0x4f2ea8+0x1f6*40;
    MissionTickState state;std::array<std::uint8_t,0x13c> definition{};
    std::array<std::uint8_t,0x44> frame{};std::array<std::uint8_t,0x50> sample{};
    Fixed3 listener{};U yaw=0;MissionAudioFrameCells audio{0,listener,yaw};
    U identity=1,damage=3;unsigned tracks=0;bool change_damage=false;
    Fixture(){
        AiActorTables tables;tables.groups[0].resize(1);auto& actor=tables.groups[0][0];
        actor.active=true;actor.definition_index=1;actor.person_slot=0;actor.field_110=0x450e40;
        init_mission_tick(state,{},std::move(tables),AiItemDefinitionTable(std::vector<AiItemDefinition>(2)),std::vector<AiPersonState>(2),123);
        state.globals.mission_67f528=1;state.extra_for({0,0}).field_74=100;
        state.person_extra(0).field_44=0x7cca80;state.person_extra(0).field_40=20;state.person_extra(0).field_2c=10;
        auto image=snapshot_mission_actor(state,{0,0});put(image,0x98,0x14000000);apply_mission_actor_image(state,{0,0},image);
        put(definition,0x20,0x7e2);put(sample,0x18,99);frame.fill(0xa5);
    }
    AiActorImage raw_actor(){auto image=snapshot_mission_actor(state,{0,0});put(image,4,d);const auto person=state.tables.groups[0][0].person_slot;put(image,0xc,person?p+U(*person)*0xb4:0);return image;}
    AiPersonImage person(U slot){auto image=ai_person_image(state.people.at(slot));const auto& e=state.person_extra(slot);
        put(image,0x2c,e.field_2c);put(image,0x38,e.field_38);put(image,0x40,e.field_40);put(image,0x44,e.field_44);return image;}
    MissionObjectHeloBindings bindings(){MissionObjectHeloBindings b;
        b.read=[&](U at,U width){
            if(at>=a&&std::uint64_t(at-a)+width<=0x114)return get(raw_actor(),at-a,width);
            if(at>=p&&std::uint64_t(at-p)+width<=0xb4*2){const auto offset=at-p;return get(person(offset/0xb4),offset%0xb4,width);}
            if(at>=d&&std::uint64_t(at-d)+width<=definition.size())return get(definition,at-d,width);
            if(at>=f&&std::uint64_t(at-f)+width<=frame.size())return get(frame,at-f,width);
            if(at>=ambient&&std::uint64_t(at-ambient)+width<=sample.size())return get(sample,at-ambient,width);
            if(at>=0x6e0700&&std::uint64_t(at-0x6e0700)+width<=sizeof(audio.registrations_6e0700)){U n=0;const auto* bytes=reinterpret_cast<const std::uint8_t*>(audio.registrations_6e0700.data());for(U i=0;i<width;++i)n|=U(bytes[at-0x6e0700+i])<<(8*i);return n;}
            if(at==0x67f528&&width==4)return state.globals.mission_67f528;
            if(at==0xa158c0&&width==4)return damage;
            if(at==entry&&width==4)return ambient;
            if(at==entry+0x20&&width==1)return 2u;
            if(at==0x4f7ea4&&width==4)return identity;
            throw MissionTickError("Native helicopter update read has no owner");
        };
        b.write=[&](U at,U width,U n){
            if(at>=a&&std::uint64_t(at-a)+width<=0x114){auto image=snapshot_mission_actor(state,{0,0});put(image,at-a,n,width);apply_mission_actor_image(state,{0,0},image);if(change_damage&&at==a+0xd4)damage=6;return;}
            if(at>=p&&std::uint64_t(at-p)+width<=0xb4*2){const auto offset=at-p,slot=offset/0xb4;auto image=person(slot);put(image,offset%0xb4,n,width);auto& e=state.person_extra(slot);
                e.field_2c=get(image,0x2c);e.field_38=get(image,0x38);e.field_40=get(image,0x40);e.field_44=get(image,0x44);
                if(at==p+0x40)state.tables.groups[0][0].field_fc=57;return;}
            if(at>=f&&std::uint64_t(at-f)+width<=frame.size()){put(frame,at-f,n,width);return;}
            if(at>=0x6e0700&&std::uint64_t(at-0x6e0700)+width<=sizeof(audio.registrations_6e0700)){auto* bytes=reinterpret_cast<std::uint8_t*>(audio.registrations_6e0700.data());for(U i=0;i<width;++i)bytes[at-0x6e0700+i]=std::uint8_t(n>>(8*i));return;}
            if(at==0x4f7ea4&&width==4){identity=n;return;}
            throw MissionTickError("Native helicopter update write has no owner");
        };
        const auto read=b.read;const auto write=b.write;
        b.call=[&,read,write](U site,const auto& args)->U{
            if(site==0x4503f0||site==0x450710){check(args==std::vector<U>{a},"wrong target fixture actor");++tracks;return 0;}
            if(site==0x43ef80){MissionSoundBankBindings bank{read,write,{}};mission_sound_attach(bank,args.at(0),args.at(1),args.at(2),args.at(3));
                state.tables.groups[0][0].person_slot=1;state.tables.groups[0][0].field_fc=32;return 0;}
            throw MissionTickError("Native helicopter update required body has no owner");
        };return b;
    }
};
}
int main(){try{
    {
        Fixture f;bind_mission_object_helo_update(f.state,f.bindings(),Fixture::f,f.frame.size());
        AiActorAction action{AiActorActionKind::update,{0,0},0x450e40};f.state.update_callbacks.at(0x450e40)(f.state,{0,0},action);
        check(f.tracks==2&&f.state.tables.groups[0].size()==1&&f.state.people.size()==2,"helicopter update used another actor/person owner");
        check(f.audio.registrations_6e0700[0].actor==Fixture::a&&f.audio.registrations_6e0700[0].definition==Fixture::ambient,"helicopter sound did not use shared registrations");
        check(get(f.raw_actor(),0x1c)==0x6e0700&&f.identity==2,"helicopter registration did not publish its current actor pointer");
        check(f.state.tables.groups[0][0].person_slot==1&&f.state.person_extra(0).field_40==52&&f.state.person_extra(0).field_2c==67,"helicopter timers lost captured person or current delay reads");
        check(f.state.person_extra(1).field_40==0&&f.state.person_extra(1).field_2c==0,"helicopter timer writes reached the replacement person");
        check(get(f.frame,0x3c)==0x7cca80&&get(f.frame,0x40)==1,"helicopter frame cells did not use the current memory owner");
    }
    {
        Fixture f;f.change_damage=true;mission_object_helo_update(f.bindings(),Fixture::a,1,Fixture::f,0x44);
        check(get(f.raw_actor(),0xd4)==1&&get(f.raw_actor(),0xd8)==1&&get(f.raw_actor(),0x74,2)==102,"damage update did not reread the current type");
    }
    {
        Fixture f;f.state.globals.mission_67f528=0;f.state.tables.groups[0][0].field_6c=4;
        auto image=snapshot_mission_actor(f.state,{0,0});put(image,0x1c,0x6e0700);apply_mission_actor_image(f.state,{0,0},image);
        bool failed=false;try{mission_object_helo_update(f.bindings(),Fixture::a,0,Fixture::f,0x44);}catch(const MissionTickError&){failed=true;}
        check(failed&&f.state.tables.groups[0][0].field_fc==8,"missing sound-stop body changed the source failure prefix");
        failed=false;try{mission_object_helo_update(f.bindings(),Fixture::a,0,Fixture::f,0x40);}catch(const MissionTickError&){failed=true;}check(failed,"short helicopter source frame was accepted");
    }
    std::cout<<"mission object helicopter update native checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
