#include "dfre/mission_round_checks.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U n){std::int32_t v;std::memcpy(&v,&n,4);return v;}
U read(const MissionRoundCheckBindings& b,U a,U width=4){if(!b.read)throw MissionTickError("Round checks need current memory reads");return b.read(a,width);}
void write(const MissionRoundCheckBindings& b,U a,U n,U width=4){if(!b.write)throw MissionTickError("Round checks need current memory writes");b.write(a,width,n);}
U call(const MissionRoundCheckBindings& b,U site,std::vector<U> args={}){if(!b.call)throw MissionTickError("Round checks need body "+std::to_string(site));return b.call(site,args);}
U absolute(U n){const auto sign=U(si(n)>>31);return (n^sign)-sign;}
U square(U n){return U(std::uint64_t(std::int64_t(si(n))*std::int64_t(si(n)))>>16);}
// Marker scan as reached by modes3 and4. Each read remains at its source point.
U marker_scan(const MissionRoundCheckBindings& b,U client){
    U index=0;
    if(si(read(b,0x8cf900))>0){U offset=0;for(;;){const auto definition=read(b,0x8ace84+offset);bool inside=false;
        if(definition&&read(b,definition+0x20)==0x1776){
            const auto actor_x=read(b,client+0x6e9a98);const auto marker_x=read(b,offset+0x8acea4);const auto x=absolute(marker_x-read(b,actor_x+0x24));
            const auto actor_y=read(b,client+0x6e9a98);const auto marker_y=read(b,offset+0x8acea8);const auto y=absolute(marker_y-read(b,actor_y+0x28));
            const auto radius=read(b,offset+0x8acefc);
            if(x<=radius&&y<=radius){const auto distance=call(b,0x477620,{square(y)+square(x)})<<8;inside=distance<=read(b,offset+0x8acefc);}
        }
        if(inside)break;const auto count=read(b,0x8cf900);++index;offset+=0x114;if(si(index)>=si(count))break;
    }}return index;
}
}
void mission_round_broadcast(const MissionRoundCheckBindings& b,U type,std::uint8_t* payload,U length){
    if(!b.enqueue_441750)throw MissionTickError("Round status needs441750");
    b.enqueue_441750(0,type,payload,length);
    for(U slot=1,offset=0x9b84;offset!=0xa53c4;++slot,offset+=0x9b84){if(read(b,offset+0x6e9c1c,1)&&read(b,offset+0x6e9c54,2)==5)b.enqueue_441750(slot,type,payload,length);}
}
void mission_round_check_end(const MissionRoundCheckBindings& b){
    const auto initial=read(b,0x67dbc0);if(initial)return;
    if(!read(b,0x67f520)){const auto actor=read(b,0xc23820);if(read(b,actor+0x6c,1)&4)call(b,0x444ca0,{2});return;}
    const auto mode=read(b,0x67f53c);if(!mode)return;
    if(mode==1){
        if(read(b,0x67f544)){const auto limit=read(b,0x67f544);U index=0,offset=0;for(;;){if(read(b,offset+0x6e9c1c,1)){const auto score=read(b,offset+0x6e9c30),loss=read(b,offset+0x6e9c44);if(si(score-loss)>=si(limit))break;}offset+=0x9b84;++index;if(si(offset)>=0xa53c4)break;}
            if(si(index)<17)call(b,0x444ca0,{0});}
        const auto timer=read(b,0x67dbe8);if(!timer)call(b,0x444ca0,{timer});return;
    }
    if(mode==2){call(b,0x446da0);const auto limit=read(b,0x67f544);if(limit){
        auto score=read(b,0x6e8144);const auto penalty=read(b,0x6e8154),loss=read(b,0x6e8158);score=score-loss-(penalty<<2);if(si(score)>=si(limit)){call(b,0x444ca0,{1});return;}
        score=read(b,0x6e81ac);const auto penalty2=read(b,0x6e81bc),loss2=read(b,0x6e81c0);score=score-loss2-(penalty2<<2);if(si(score)>=si(limit)){call(b,0x444ca0,{2});return;}}
        if(read(b,0x67dbe8))return;
        auto first=read(b,0x6e8144);const auto penalty=read(b,0x6e8154),loss=read(b,0x6e8158);const auto loss2=read(b,0x6e81c0);first=first-loss-(penalty<<2);auto second=read(b,0x6e81ac);const auto penalty2=read(b,0x6e81bc);second=second-loss2-(penalty2<<2);
        call(b,0x444ca0,{si(second)>si(first)?2u:si(first)>si(second)?1u:0u});return;
    }
    if(mode==3||mode==4){
        for(U offset=initial;offset!=0xa53c4;offset+=0x9b84){if(!read(b,offset+0x6e9c1c,1)||read(b,offset+0x6e9c54,2)!=5)continue;
            const auto marker=marker_scan(b,offset);const auto actor=read(b,offset+0x6e9a98);const auto flags=read(b,actor+0x6c,1);
            if(flags&4){if(mode==3){const auto count=read(b,offset+0x6e9c24);if(si(count)>0)write(b,offset+0x6e9c24,count-1);}write(b,offset+0x6e9c2c,0);}
            else if(si(marker)<si(read(b,0x8cf900))){const auto count=read(b,offset+0x6e9c24);write(b,offset+0x6e9c2c,1);write(b,offset+0x6e9c24,count+1);}
            else if(mode==3){const auto count=read(b,offset+0x6e9c24);write(b,offset+0x6e9c24,si(count)>0?count-1:0);write(b,offset+0x6e9c2c,0);}
            else write(b,offset+0x6e9c2c,0);
        }
        if(mode==3){for(U offset=0;offset!=0xa53c4;offset+=0x9b84){if(!read(b,offset+0x6e9c1c,1)||read(b,offset+0x6e9c54,2)!=5)continue;const auto limit=read(b,0x67f554)*60;const auto count=read(b,offset+0x6e9c24);if(si(limit)<=si(count))call(b,0x444ca0,{0});}
            const auto timer=read(b,0x67dbe8);if(!timer)call(b,0x444ca0,{timer});return;}
        for(U team=0,record=0;team<64;++team,record+=0x68){U index=0,offset=0;for(;;){if(read(b,offset+0x6e9c1c,1)&&read(b,offset+0x6e9c54,2)==5&&read(b,offset+0x6e9c2c)&&read(b,offset+0x6e9b2c)==team)break;offset+=0x9b84;++index;if(si(offset)>=0xa53c4)break;}
            if(si(index)<17)write(b,record+0x6e80d4,read(b,record+0x6e80d4)+1);else write(b,record+0x6e80d4,0);}
        call(b,0x446da0);const auto limit=read(b,0x67f554)*60,first=read(b,0x6e813c);if(si(limit)<=si(first)){call(b,0x444ca0,{1});return;}
        const auto second=read(b,0x6e81a4);if(si(limit)<=si(second)){call(b,0x444ca0,{2});return;}const auto timer=read(b,0x67dbe8);if(timer)return;
        call(b,0x444ca0,{si(second)>si(first)?2u:si(first)>si(second)?1u:timer});return;
    }
    if(mode==5){const auto first=read(b,0x6e815c),limit1=read(b,0x6e5768);if(si(first)>=si(limit1)){call(b,0x444ca0,{1});return;}
        const auto second=read(b,0x6e81c4),limit2=read(b,0x6e5764);if(si(second)>=si(limit2)){call(b,0x444ca0,{2});return;}const auto timer=read(b,0x67dbe8);if(timer)return;
        if(!limit2){call(b,0x444ca0,{2});return;}if(!limit1){call(b,0x444ca0,{1});return;}
        const auto current_first=read(b,0x6e815c);call(b,0x444ca0,{si(second)>si(current_first)?2u:si(current_first)>si(second)?1u:timer});
    }
}
void mission_round_check_clients(const MissionRoundCheckBindings& b){
    if(!read(b,0x67f520))return;
    for(U slot=1,offset=0x9b84;slot<17;++slot,offset+=0x9b84){if(!read(b,offset+0x6e9c1c,1)||read(b,offset+0x6e9c54,2)!=5)continue;
        const auto score=read(b,offset+0x6e9c30),penalty=read(b,offset+0x6e9c40),loss=read(b,offset+0x6e9c44);
        if(si(score-loss-(penalty<<2))<=-9){call(b,0x447fb0,{slot});continue;}
        if(read(b,0x67f53c)!=5)continue;
        const auto actor=read(b,offset+0x6e9a98);U linked=0,definition=0,kind=0;if(actor)linked=read(b,actor+0x90);if(linked)definition=read(b,linked+4);if(definition)kind=read(b,definition+0x20);
        if(!actor||!linked||!definition||(kind!=0xfc3&&kind!=0xfc4)){write(b,offset+0x6e9c50,0);continue;}
        const auto count=read(b,offset+0x6e9c50)+1;write(b,offset+0x6e9c50,count);if(si(count)<600)continue;
        const auto current_link=read(b,actor+0x90);call(b,0x4604c0,{actor});call(b,0x4605d0,{current_link});const auto current_actor=read(b,offset+0x6e9a98);write(b,current_actor+0x74,0xffff,2);
    }
}
void mission_round_publish_status(const MissionRoundCheckBindings& b){if(!read(b,0x67f528))return;if(read(b,0x67f53c))return;const auto value=read(b,0x6e5884,1);call(b,0x4423f0,{0x30,value,1});}
MissionRoundCheckBindings mission_round_check_bindings(MissionTickState& state,MissionRoundCheckBindings host){
    MissionRoundCheckBindings b;b.call=host.call;b.enqueue_441750=host.enqueue_441750;
    b.read=[&state,host](U at,U width)->U{
        if(width!=1&&width!=2&&width!=4)throw MissionTickError("Round-check memory width is invalid");
        if(width==4){
            if(at==0x67dbc0)return state.globals.menu_flag_67dbc0;
            if(at==0x67f520)return state.globals.multiplayer_67f520;
            if(at==0x67f528)return state.globals.mission_67f528;
            if(at==0x67f53c)return state.game_type_67f53c;
            if(at==0x67dbe8)return state.mission_timer_67dbe8;
            if(at==0xc23820){if(!state.config.local_actor)return 0;return mission_actor_source_address(3,*state.config.local_actor);}
            if(at==0x8cf900){const auto count=state.tables.groups[2].size();if(count>0x7fffffffu)throw MissionTickError("Marker count exceeds signed range");return U(count);}
        }
        for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base)continue;const auto delta=at-base,slot=delta/0x114,offset=delta%0x114;const U capacity=group==3?256:group==2?512:1024;if(slot>=capacity||slot>=state.tables.groups[group].size()||offset+width>0x114)continue;
            if(offset<8&&offset+width>4)break; // Physical definition pointer belongs to the host.
            const auto image=snapshot_mission_actor(state,{group,slot});U value=0;for(U i=0;i<width;++i)value|=U(image[offset+i])<<(8*i);return value;
        }
        return read(host,at,width);
    };
    b.write=[&state,host](U at,U width,U value){
        if(width==2){for(std::uint8_t group=0;group<4;++group){const auto base=mission_actor_source_address(group,0);if(at<base)continue;const auto delta=at-base,slot=delta/0x114,offset=delta%0x114;const U capacity=group==3?256:group==2?512:1024;if(slot>=capacity||slot>=state.tables.groups[group].size()||offset!=0x74)continue;auto image=snapshot_mission_actor(state,{group,slot});image[0x74]=std::uint8_t(value);image[0x75]=std::uint8_t(value>>8);apply_mission_actor_image(state,{group,slot},image);return;}}
        write(host,at,value,width);
    };
    return b;
}

}
