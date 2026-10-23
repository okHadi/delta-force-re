#include "dfre/mission_scene_projectiles.hpp"
#include <stdexcept>
#include <utility>
namespace dfre {
std::vector<SceneEffectRecord> mission_scene_projectile_records(const std::vector<ProjectileEntity>& source){
    if(source.size()!=512)throw std::invalid_argument("Projectile scene needs512 physical records");
    std::vector<SceneEffectRecord> result;result.reserve(source.size());
    for(std::size_t i=0;i<source.size();++i){
        const auto& current=source[i];SceneEffectRecord record;
        record.identity=scene_model_identity(0x9f3098u+std::uint32_t(i)*0x114u);
        record.position={current.position.x,current.position.y,current.position.z};
        record.model=std::uint32_t(current.item_field);record.counter_fc=current.ttl;
        record.image=snapshot_projectile_record(current);result.push_back(std::move(record));
    }
    return result;
}
}
