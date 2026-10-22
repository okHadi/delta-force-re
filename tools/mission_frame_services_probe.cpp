#include "dfre/mission_frame_services.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
using namespace dfre;
namespace {
using U=std::uint32_t;
std::int32_t integer(U v){std::int32_t r;std::memcpy(&r,&v,4);return r;}
void output(const std::vector<U>& values,const std::vector<std::vector<U>>& calls){
    std::cout<<"{\"state\":[";for(std::size_t i=0;i<values.size();++i)std::cout<<(i?",":"")<<values[i];
    std::cout<<"],\"calls\":[";for(std::size_t i=0;i<calls.size();++i){std::cout<<(i?",[":"[");
        for(std::size_t j=0;j<calls[i].size();++j)std::cout<<(j?",":"")<<calls[i][j];std::cout<<']';}
    std::cout<<"]}\n";
}
void run(const std::string& line){
    const auto separator=line.find('|');const auto mode=line.substr(0,separator);
    std::stringstream input(line.substr(separator+1));std::string value;std::vector<U> v;
    while(std::getline(input,value,','))v.push_back(U(std::stoull(value)));
    U requested=0,stat=0;MissionProfilerCells profiler{0,0,std::vector<U>(8)};
    Fixed3 center{};MissionCameraCells camera{center,{},0,0x1100000};
    MissionScriptQueueCells scripts{0,0,{}};std::array<U,3> timers{};
    MissionFrameServiceBindings bindings;std::vector<std::vector<U>> calls;std::vector<U> result;
    MissionStageCall call;
    if(mode=="profile"){
        if(v.size()!=13)throw std::runtime_error("profile needs thirteen words");
        profiler.current_4f8718=v[1];profiler.previous_4f871c=v[2];
        for(U i=0;i<8;++i)profiler.totals_8e3f40[i]=v[5+i];
        bindings.rdtsc=[&]{return std::uint64_t(v[3])|(std::uint64_t(v[4])<<32);};
        call.address=0x456e30;call.argument_count=1;call.args[0]=v[0];
    }else if(mode=="camera"){
        if(v.size()!=8)throw std::runtime_error("camera needs eight words");
        center={integer(v[0]),integer(v[1]),integer(v[2])};camera.yaw_6ba3e8=v[3];
        bindings.actor_word=[&](U handle,U offset){if(handle!=camera.actor_6ba3f0)throw std::runtime_error("actor");return v.at(4+(offset-0x24)/4);};
        call.address=0x42ab70;
    }else if(mode=="timers"){
        if(v.size()!=3)throw std::runtime_error("timers needs three words");
        std::copy(v.begin(),v.end(),timers.begin());call.address=0x479ed0;
    }else if(mode=="stat"){
        stat=v.at(0);call.address=0x4702a0;call.argument_count=1;call.args[0]=0x1100000;
        bindings.store_word=[&](U target,U word){calls.push_back({target,word});};
    }else if(mode=="request"){
        requested=v.at(0);call.address=0x461340;call.argument_count=1;call.args[0]=v.at(1);
    }else if(mode=="scripts"){
        if(v.size()!=15+v.at(4)*4)throw std::runtime_error("script words");
        scripts.enabled_8e3d68=v[0];scripts.scale_8e3d64=v[1];
        for(U i=0;i<v[4];++i)scripts.entries_8e3d74.push_back({v[15+4*i],v[16+4*i],v[17+4*i],v[18+4*i]});
        bindings.script_count=[&](U resource){return v.at(5+resource);};
        bindings.script_delay=[&](U resource,U index){return std::uint8_t(v.at(7+resource*4+index));};
        bool mutated=false;
        bindings.script_call=[&](U site,U resource,U index){
            calls.push_back({site,resource,index});
            if(site==0x456110 && !mutated){mutated=true;
                if(v[3]==1)scripts.entries_8e3d74[0].index=1;
                if(v[3]==2){v[5+resource]=3;v[7+resource*4+index]=251;scripts.scale_8e3d64=0xffffffff;}
                if(v[3]==4)scripts.entries_8e3d74.push_back({1,0,1,0});
            }
            if(site==0x456000){
                for(auto it=scripts.entries_8e3d74.begin();it!=scripts.entries_8e3d74.end();++it)
                    if(it->resource+4==resource){scripts.entries_8e3d74.erase(it);break;}
            }
        };
        call.address=0x4562a0;call.argument_count=1;call.args[0]=v[2];
    }else throw std::runtime_error("unknown mode");
    MissionFrameServices services({requested,profiler,timers,stat,camera,scripts},bindings);services.stage(call);
    if(mode=="profile"){result={profiler.current_4f8718,profiler.previous_4f871c};result.insert(result.end(),profiler.totals_8e3f40.begin(),profiler.totals_8e3f40.end());}
    if(mode=="camera"){result={U(center.x),U(center.y),U(center.z)};result.insert(result.end(),camera.delta_6ba3e0_e4_dc.begin(),camera.delta_6ba3e0_e4_dc.end());result.push_back(camera.yaw_6ba3e8);}
    if(mode=="timers")result.assign(timers.begin(),timers.end());
    if(mode=="stat")result={stat};
    if(mode=="request"){call.address=0x461350;result={*services.stage(call)};}
    if(mode=="scripts"){
        result={scripts.scale_8e3d64,U(scripts.entries_8e3d74.size()),v[5],v[6]};
        for(const auto& entry:scripts.entries_8e3d74)result.insert(result.end(),{entry.resource,entry.index,entry.delay,entry.armed});
    }
    output(result,calls);
}
}
int main(){std::string line;try{while(std::getline(std::cin,line))if(!line.empty())run(line);}
catch(const std::exception& e){std::cerr<<e.what()<<" line="<<line<<'\n';return 1;}}
