#include "dfre/actor_collision.hpp"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace dfre;
namespace {
constexpr std::uint32_t actor_id=0x2201000,target_id=0x2202000,carried_id=0x2203000;
std::uint32_t next(std::istream& in) {std::uint64_t value;if(!(in>>value))throw std::invalid_argument("Short collision probe row");return std::uint32_t(value);}
std::int32_t integer(std::istream& in) {return std::int32_t(next(in));}
std::uint32_t word(const AiActorImage& a,unsigned at) {return std::uint32_t(a[at])|std::uint32_t(a[at+1])<<8|std::uint32_t(a[at+2])<<16|std::uint32_t(a[at+3])<<24;}
void put(AiActorImage& a,unsigned at,std::uint32_t n) {for(unsigned i=0;i<4;++i)a[at+i]=std::uint8_t(n>>(8*i));}
std::string hex(const AiActorImage& a) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto n:a)out<<std::setw(2)<<unsigned(n);return out.str();}
AiActorImage readimage(std::istream& in) {std::string raw;in>>raw;if(raw.size()!=0x228)throw std::invalid_argument("Bad actor image");AiActorImage a{};for(unsigned i=0;i<a.size();++i)a[i]=std::uint8_t(std::stoul(raw.substr(i*2,2),nullptr,16));return a;}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        std::string line;while(std::getline(std::cin,line)) {
            if(line.empty())continue;std::istringstream in(line);auto a=readimage(in),t=readimage(in),carried=readimage(in);
            ActorCollisionDefinition definition{next(in),next(in)},carried_definition{next(in),next(in)};
            ActorCollisionBindings b;b.host=next(in)!=0;auto incoming=next(in),seed=next(in),mutation=next(in);
            std::ostringstream trace;unsigned calls=0;
            b.definition=[&](auto id){return id==0x2205000?carried_definition:definition;};
            b.actor=[&](auto id){if(id!=carried_id)throw std::invalid_argument("Unknown carried actor");return ActorCollisionActor{&carried,id};};
            b.call=[&](const ActorCollisionCall& request) {
                trace<<std::uint32_t(request.site);for(unsigned i=0;i<request.count;++i)trace<<','<<request.arguments[i];
                trace<<':'<<hex(a)<<':'<<hex(t)<<':'<<hex(carried)<<';';++calls;
                if((mutation&1)&&(request.site==ActorCollisionSite::refill||request.site==ActorCollisionSite::heal))put(a,0x74,0x01020304);
                if((mutation&2)&&request.site==ActorCollisionSite::pickup)put(a,0x90,target_id);
                if((mutation&8)&&request.site==ActorCollisionSite::objective)put(t,0x24,word(t,0x24)+123);
                if((mutation&16)&&request.site==ActorCollisionSite::objective_effect)t[0x6c]|=0x20;
                if((mutation&32)&&(request.site==ActorCollisionSite::deliver_side_one||request.site==ActorCollisionSite::deliver_side_two))put(a,0x90,0x12345678);
                if(request.site==ActorCollisionSite::can_use&&(mutation&4))return 0u;
                return seed^std::uint32_t(request.site)^calls;
            };
            std::uint32_t code;
            if(std::string(argv[1])=="trigger")code=actor_collision_trigger({&a,actor_id},{&t,target_id},b,incoming);
            else if(std::string(argv[1])=="collision") {
                auto radius=integer(in);auto present=next(in),state_present=next(in),count=next(in);
                std::vector<ActorCollisionMesh> meshes;
                for(unsigned i=0;i<count;++i) {
                    ActorCollisionMesh mesh;for(auto& n:mesh.matrix)n=integer(in);for(auto& n:mesh.bounds)n=integer(in);
                    auto volumes=next(in);
                    for(unsigned j=0;j<volumes;++j) {
                        std::array<std::uint8_t,72> raw{};
                        for(unsigned k=0;k<18;++k) {auto n=next(in);for(unsigned v=0;v<4;++v)raw[k*4+v]=std::uint8_t(n>>(v*8));}
                        mesh.volumes.push_back(decode_collision_volume(raw));
                    }
                    meshes.push_back(std::move(mesh));
                }
                b.model=[&](auto id){trace<<"m,"<<id<<';';return ActorCollisionModel{radius,present!=0};};
                b.state=[&](auto id,auto)->const std::vector<ActorCollisionMesh>* {trace<<"s,"<<id<<':'<<hex(a)<<':'<<hex(t)<<';';return state_present?&meshes:nullptr;};
                code=actor_target_collision({&a,actor_id},{&t,target_id},b);
            } else throw std::invalid_argument("Unknown collision probe mode");
            std::cout<<code<<' '<<hex(a)<<' '<<hex(t)<<' '<<hex(carried)<<' '<<(trace.str().empty()?"-":trace.str())<<'\n';
        }
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
