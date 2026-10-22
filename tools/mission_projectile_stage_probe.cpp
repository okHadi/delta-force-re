#include "dfre/mission_projectile_stage.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace dfre;
namespace {
std::uint32_t next(std::istream& in) {std::uint64_t n;if(!(in>>n))throw std::invalid_argument("Short stage probe row");return std::uint32_t(n);}
std::int32_t integer(std::istream& in) {return std::int32_t(next(in));}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(i*8));}
std::string hex(const AiActorImage& a) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto n:a)out<<std::setw(2)<<unsigned(n);return out.str();}
AiActorImage readimage(std::istream& in) {std::string raw;in>>raw;if(raw.size()!=0x228)throw std::invalid_argument("Bad actor image");AiActorImage a{};for(unsigned i=0;i<a.size();++i)a[i]=std::uint8_t(std::stoul(raw.substr(i*2,2),nullptr,16));return a;}
std::uint32_t base(std::uint32_t group) {return group==3?0x7cca80:group==1?0x7dde80:0x867e80;}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        std::string line;while(std::getline(std::cin,line)) {
            if(line.empty())continue;std::istringstream in(line);
            MissionProjectileStageState s;BehaviorState random;
            s.host=next(in);s.rules_67f549=std::uint8_t(next(in));s.water=integer(in);s.local_player_identity=next(in);
            random.rng=next(in);s.random=&random;s.alternate_listener=next(in);
            s.alternate_pose={{integer(in),integer(in),integer(in)},next(in),next(in)};
            auto local_present=next(in);auto local=readimage(in);if(local_present)s.local_player={&local,s.local_player_identity};
            auto owner_id=next(in);auto owner=readimage(in);
            std::map<std::uint32_t,MissionAreaDefinition> definitions;
            auto n=next(in);while(n--) {auto id=next(in);definitions[id]={next(in),next(in)};}
            std::map<std::uint32_t,MissionAreaBounds> bounds;n=next(in);while(n--) {auto id=next(in);bounds[id]={integer(in),integer(in)};}
            std::array<std::vector<AiActorImage>,4> images;
            for(auto group:{3u,1u,0u}) {n=next(in);while(n--)images[group].push_back(readimage(in));}
            std::vector<MissionPendingSpawn> pending;n=next(in);
            while(n--)pending.push_back({{integer(in),integer(in),integer(in)},integer(in),next(in)});
            s.pending=&pending;std::vector<std::int32_t> answers;n=next(in);while(n--)answers.push_back(integer(in));
            const auto mutation=next(in);bool appended=false;std::size_t query_index=0;
            auto dump=[&](){std::ostringstream out;out<<random.rng;
                for(auto group:{3u,1u,0u}) {out<<'|'<<images[group].size();for(const auto& a:images[group])out<<','<<hex(a);}
                out<<'|'<<pending.size();for(const auto& p:pending)out<<','<<std::uint32_t(p.position[0])<<','<<std::uint32_t(p.position[1])<<','<<std::uint32_t(p.position[2])<<','<<std::uint32_t(p.type)<<','<<p.owner;
                out<<'|'<<hex(local);return out.str();};
            std::ostringstream trace;auto snapshot=[&](const std::string& tag){trace<<tag<<':'<<dump()<<';';};
            MissionProjectileStageBindings b;
            b.count=[&](auto group){return images.at(group).size();};
            b.actor=[&](auto ref){return MissionAreaActor{&images.at(ref.group).at(ref.index),base(ref.group)+std::uint32_t(ref.index)*0x114};};
            b.owner=[&](auto id){for(auto group:{3u,1u,0u})if(id>=base(group)&&(id-base(group))%0x114==0&&(id-base(group))/0x114<images[group].size())return b.actor({group,(id-base(group))/0x114});if(id==owner_id)return MissionAreaActor{&owner,id};throw std::invalid_argument("Unknown stage owner");};
            b.definition=[&](auto id){return definitions.at(id);};b.bounds=[&](auto id){return bounds.at(id);};
            b.segment=[&](auto ref,const auto& start,const auto& end) {
                snapshot("q,"+std::to_string(base(ref.group)+ref.index*0x114)+","+std::to_string(std::uint32_t(start.x))+","+std::to_string(std::uint32_t(start.y))+","+std::to_string(std::uint32_t(start.z))+","+std::to_string(std::uint32_t(end.x))+","+std::to_string(std::uint32_t(end.y))+","+std::to_string(std::uint32_t(end.z)));
                if(mutation&2)random.rng^=0x12345678;
                if((mutation&32)&&!pending.empty())pending[0].type=1;
                return answers.at(query_index++);
            };
            b.update=[&](auto ref,auto callback,auto message) {
                snapshot("u,"+std::to_string(callback)+","+std::to_string(base(ref.group)+ref.index*0x114)+","+std::to_string(message));
                auto& a=images.at(ref.group).at(ref.index);
                if(mutation&4)a[0x6c]|=4;
                if(mutation&8)put(a,0x74,0x11223344);
                if((mutation&16)&&!pending.empty())pending[0].type=1;
                if((mutation&1)&&!appended) {appended=true;pending.push_back({{0,0,0},4,0});}
            };
            b.local_hit=[&](){snapshot("l");};
            b.listener=[&](const MissionListenerPose& pose){snapshot("v,"+std::to_string(std::uint32_t(pose.position.x))+","+std::to_string(std::uint32_t(pose.position.y))+","+std::to_string(std::uint32_t(pose.position.z))+","+std::to_string(pose.yaw)+","+std::to_string(pose.pitch));};
            if(std::string(argv[1])=="listener")mission_listener_stage(s,b);
            else if(std::string(argv[1])=="apply") {
                auto site=next(in),group=next(in),index=next(in),pending_index=next(in);auto distance=integer(in);
                mission_area_apply(MissionAreaSite(site),{group,index},pending_index,distance,s,b);
            } else if(std::string(argv[1])=="queue") {
                auto present=next(in);if(present)s.incoming_dispatch=MissionAreaDispatch{integer(in),MissionAreaSite(next(in))};
                mission_pending_area_stage(s,b);
            } else throw std::invalid_argument("Unknown stage probe mode");
            std::cout<<dump()<<' '<<query_index<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
        }
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
