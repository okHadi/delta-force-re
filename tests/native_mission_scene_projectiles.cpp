#include "dfre/mission_scene_projectiles.hpp"
#include <stdexcept>
namespace {void check(bool ok){if(!ok)throw std::runtime_error("Projectile scene check failed");}}
int main(){std::vector<dfre::ProjectileEntity> source(512);
    auto& first=source.front();first.item_main=7;first.item_field=0x71000001;first.position={123,-456,-65537};first.aim_yaw=0x87654321;first.aim_pitch=0xabcdef01;first.ttl=9;
    auto& last=source.back();auto image=dfre::snapshot_projectile_record(last);for(std::size_t i=0;i<image.size();++i)image[i]=std::uint8_t(i*37+11);for(std::size_t i=0xf8;i<0xfc;++i)image[i]=0;dfre::apply_projectile_record(last,image);
    const auto records=dfre::mission_scene_projectile_records(source);check(records.size()==512&&records[0].image&&records[511].image);check(!first.active()&&records[0].counter_fc==9);check(dfre::scene_model_handle(records[0].identity)==0x9f3098&&dfre::scene_model_handle(records[511].identity)==0x9f3098+511*0x114);check(records[0].position==std::array<std::int32_t,3>{123,-456,-65537});check(records[0].model==0x71000001);check(*records[511].image==image);
    source[0].aim_yaw=1;source[0].position.x=2;source[0].ttl=-1;const auto next=dfre::mission_scene_projectile_records(source);check(next[0].position[0]==2&&next[0].counter_fc==-1&&*next[0].image!=*records[0].image);check(records[0].position[0]==123);check(next[0].identity==records[0].identity);
    for(const auto count:{0u,511u,513u}){bool failed=false;try{dfre::mission_scene_projectile_records(std::vector<dfre::ProjectileEntity>(count));}catch(const std::invalid_argument&){failed=true;}check(failed);}
}
