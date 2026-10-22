#include "dfre/live_world.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace dfre {
namespace {
std::uint32_t word(const AiActorImage& image,std::size_t at){
    std::uint32_t value;std::memcpy(&value,image.data()+at,4);return value;
}
std::uint16_t half(const AiActorImage& image,std::size_t at){
    std::uint16_t value;std::memcpy(&value,image.data()+at,2);return value;
}
AiActorState decode(const AiActorImage& image,bool active,std::optional<std::size_t> person){
    AiActorState actor;actor.active=active;actor.person_slot=person;
    actor.definition_index=word(image,0);actor.object_id=word(image,8);
    actor.field_10=word(image,0x10);actor.field_14=word(image,0x14);actor.field_18=word(image,0x18);
    for(std::size_t axis=0;axis<3;++axis)actor.position[axis]=word(image,0x24+axis*4);
    actor.angle_yaw=word(image,0x30);actor.angle_pitch=word(image,0x34);actor.field_6c=image[0x6c];
    actor.field_70=half(image,0x70);actor.field_72=half(image,0x72);actor.field_76=half(image,0x76);
    actor.field_78=word(image,0x78);actor.field_7c=word(image,0x7c);actor.field_fc=word(image,0xfc);
    actor.field_104=word(image,0x104);actor.field_108=word(image,0x108);
    actor.field_10c=word(image,0x10c);actor.field_110=word(image,0x110);
    return actor;
}
}
MissionActorImages initial_actor_images(const MissionRuntimeState& state){
    MissionActorImages images;
    for(std::size_t group=0;group<images.size();++group){
        const auto count=group==3?std::max(state.group3_count,state.groups[group].size()):state.groups[group].size();
        images[group].reserve(count);
        for(std::size_t slot=0;slot<count;++slot)images[group].push_back(mission_actor_image(state,group,slot));
    }
    return images;
}
MissionActorSeed seed_mission_actors(const MissionRuntimeState& state){
    MissionActorSeed seed;seed.images=initial_actor_images(state);
    for(std::size_t group=0;group<seed.images.size();++group){
        for(std::size_t slot=0;slot<seed.images[group].size();++slot){
            const auto& image=seed.images[group][slot];
            const bool created=group==3 && (slot>=state.groups[3].size() || mission_actor_created(state,slot));
            std::optional<std::size_t> person;bool active=false;
            if(created){
                active=word(image,0)!=0;
                const auto handle=word(image,0xc);
                if(handle){
                    if(handle<kPersonHandleBase || (handle-kPersonHandleBase)%kAiPersonRecordBytes)
                        throw std::invalid_argument("Created actor has an invalid person handle");
                    person=(handle-kPersonHandleBase)/kAiPersonRecordBytes;
                }
            }else{
                const auto& source=state.groups[group].at(slot);active=source.active;person=source.person_slot;
            }
            if(person && *person>=state.people.size())throw std::invalid_argument("Actor person slot exceeds the pool");
            seed.tables.groups[group].push_back(decode(image,active,person));
            seed.extras[group].push_back({image[0x6d],half(image,0x74),word(image,0x84),word(image,0x94),word(image,0xf8)});
            seed.person_encoding[group].push_back(created?ActorPersonEncoding::handle:ActorPersonEncoding::slot);
        }
    }
    return seed;
}
AiActorImage merge_live_actor_image(const AiActorImage& stored,const AiActorState& actor,
    const BehaviorActorExtra& extra,ActorPersonEncoding encoding){
    auto image=stored;
    const auto put=[&](std::size_t at,const auto& value){std::memcpy(image.data()+at,&value,sizeof(value));};
    const auto index=actor.active?actor.definition_index:0u;
    put(0,index);put(4,index);put(8,actor.object_id);
    std::uint32_t person=0;
    if(actor.person_slot){
        if(*actor.person_slot>=kAiPersonSlotCount)throw std::invalid_argument("Actor person slot exceeds the pool");
        person=encoding==ActorPersonEncoding::handle?person_handle(*actor.person_slot):std::uint32_t(*actor.person_slot);
    }
    put(0xc,person);put(0x10,actor.field_10);put(0x14,actor.field_14);put(0x18,actor.field_18);
    for(std::size_t axis=0;axis<3;++axis)put(0x24+axis*4,actor.position[axis]);
    put(0x30,actor.angle_yaw);put(0x34,actor.angle_pitch);put(0x6c,actor.field_6c);put(0x6d,extra.field_6d);
    put(0x70,actor.field_70);put(0x72,actor.field_72);put(0x74,extra.field_74);put(0x76,actor.field_76);
    put(0x78,actor.field_78);put(0x7c,actor.field_7c);put(0x84,extra.field_84);put(0x94,extra.field_94);put(0xf8,extra.field_f8);
    put(0xfc,actor.field_fc);put(0x104,actor.field_104);put(0x108,actor.field_108);put(0x10c,actor.field_10c);put(0x110,actor.field_110);
    return image;
}
} // namespace dfre
