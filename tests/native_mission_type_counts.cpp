#include "dfre/mission_type_counts.hpp"
#include <cassert>
using namespace dfre;
int main(){
    MissionTickState state;AiTypeSlotTable types;
    for(auto group:{1,3,0}){state.tables.groups[group].resize(2);for(auto& a:state.tables.groups[group]){a.field_72=3;a.active=false;}}
    auto base=mission_type_count_bindings(state,types);base.write(0x7936b8,4,0xffffffff);base.write(0x7936a8+3*64,4,123);
    mission_type_counts(base);assert(base.read(0x7936ac+3*64,4)==6);assert(base.read(0x7936a8+3*64,4)==123);assert(base.read(0x7936b8,4)==0);assert(base.read(0x7936ac,4)==0);
    // Group1 caches its second count; group3 reads each loop bound again.
    unsigned c1=0,c3=0;auto b=base;b.read=[&](auto at,auto width){if(at==0x8cf904){++c1;return c1==1?2u:1u;}if(at==0x8cfa18){++c3;return c3==1?2u:1u;}return base.read(at,width);};mission_type_counts(b);assert(c1==2&&c3==2&&base.read(0x7936ac+3*64,4)==4);
    // The age value is read before the adjacent count write.
    const auto prior_age=base.read(0x7936b8,4);bool written=false;b=base;b.write=[&](auto at,auto width,auto n){base.write(at,width,n);if(at==0x7936ac&&!written){base.write(0x7936b8,4,99);written=true;}};mission_type_counts(b);assert(written&&base.read(0x7936b8,4)==prior_age+1);
    bool failed=false;try{mission_type_counts({});}catch(const MissionTickError&){failed=true;}assert(failed);
    state.tables.groups[1][0].field_72=0xffff;failed=false;try{mission_type_counts(base);}catch(const MissionTickError&){failed=true;}assert(failed&&base.read(0x7936ac+3*64,4)==0);
    // A failed age store preserves the preceding count clear.
    base.write(0x7936ac,4,123);b=base;b.write=[&](auto at,auto width,auto n){if(at==0x7936b8)throw MissionTickError("Store failed");base.write(at,width,n);};failed=false;try{mission_type_counts(b);}catch(const MissionTickError&){failed=true;}assert(failed&&base.read(0x7936ac,4)==0);
}
