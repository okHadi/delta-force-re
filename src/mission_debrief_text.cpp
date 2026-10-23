#include "dfre/mission_debrief_text.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Debrief text reached a missing provider");return f;}
U read(const MissionDebriefBindings& b,U at,U width=4){return need(b.read)(at,width);}
void write(const MissionDebriefBindings& b,U at,U n,U width=4){need(b.write)(at,width,n);}
U call(const MissionDebriefBindings& b,U site,const std::vector<U>& args={}){return need(b.call)(site,args);}
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
void copy(const MissionDebriefBindings& b,U destination,U source){
    for(;;){
        auto c=read(b,source,1)&255u;write(b,destination,c,1);if(!c)return;
        c=read(b,source+1,1)&255u;source+=2;write(b,destination+1,c,1);destination+=2;if(!c)return;
    }
}
U length(const MissionDebriefBindings& b,U text){const auto start=text;while(read(b,text++,1)&255u){}return text-start-1;}
void append(const MissionDebriefBindings& b,U destination,U source){while(read(b,destination,1)&255u)++destination;copy(b,destination,source);}
}
void mission_debrief_campaign_audio(const MissionDebriefBindings& b){
    U count=0,offset=0;
    for(;;){
        const auto profile=read(b,0x4cf2e4);const auto campaign=read(b,profile+0x28);
        const auto bound=read(b,0x4d0d08+campaign*4);
        if(si(count)>=si(bound)){call(b,0x4c4f40,{2,3});return;}
        if(!read(b,profile+campaign*0x44+0x30+offset))return;
        offset+=4;++count;
    }
}
void mission_debrief_won_audio(const MissionDebriefBindings& b){
    if(!read(b,0x67f718))call(b,0x4c4f40,{2,2});
    const auto profile=read(b,0x4cf2e4);const auto campaign=read(b,profile+0x28);
    const auto mission=read(b,0x68ba34);const auto completion=read(b,profile+campaign*0x44+0x30+mission*4);
    if(completion){write(b,0x4e6cfc,1);call(b,0x4c4f40,{2,0});}
    else{write(b,0x4e6cfc,completion);call(b,0x4c4f40,{2,1});}
}
void mission_debrief_text(const MissionDebriefBindings& b,U frame){
    const auto scratch=call(b,0x497f60,{0x1000,4,0,0x4cad10});write(b,frame+0x124,scratch);
    if(!scratch){call(b,0x4564b0,{0x4cad1c});return;}
    const auto first_mission=read(b,0x68ba34)+1;const auto won=read(b,0x4e6cfc);
    call(b,0x493a80,{frame,won?0x4cad48u:0x4cadd0u,first_mission});
    const auto current_mission=read(b,0x68ba34)+1;call(b,0x493a80,{scratch,0x4cad50,current_mission});
    const auto table=read(b,0x503238);const auto initial=call(b,0x49a092,{table,scratch,frame});
    if(initial){const auto text=read(b,initial);copy(b,scratch,text);}
    else{const auto missing_mission=read(b,0x68ba34)+1;call(b,0x493a80,{scratch,won?0x4cad5cu:0x4cadd8u,missing_mission});}
    if(won){
        const auto payload=read(b,frame+0x124);const auto row=read(b,0x4e7550);const auto kills=read(b,0x4e756c+row*0x24);
        const auto general=read(b,0x503234);const auto kills_entry=call(b,0x49a092,{general,0x4cad98,0x4cad8c});const auto kills_text=read(b,kills_entry);
        const auto current_general=read(b,0x503234);const auto stats_entry=call(b,0x49a092,{current_general,0x4cad98,0x4cada8});const auto stats_text=read(b,stats_entry);
        const auto destination=read(b,0x68bef0);call(b,0x493a80,{destination,0x4cadbc,stats_text,kills_text,kills,payload});
    }else{
        const auto payload=read(b,frame+0x124);const auto general=read(b,0x503234);const auto entry=call(b,0x49a092,{general,0x4cad98,0x4cada8});const auto stats=read(b,entry);
        const auto destination=read(b,0x68bef0);call(b,0x493a80,{destination,0x4cae08,stats,payload});
    }
    const auto clear=read(b,frame+0x124);call(b,0x49a620,{clear,0,0x1000});
    auto input=read(b,0x68bef0);const auto first=read(b,input,1)&255u;auto output=read(b,frame+0x124);
    if(first){
        for(;;){
            bool add=(read(b,input,1)&255u)==0x7b;
            if(add)add=(read(b,input+1,1)&255u)==0x41;
            if(add){const auto third=read(b,input+2,1)&255u;add=third==0x44;if(add)add=third==(read(b,input+3,1)&255u);}
            if(add)add=(read(b,input+4,1)&255u)==0x7d;
            if(add){
                auto key=frame+0xe4;const auto after=read(b,input+5,1)&255u;input+=5;
                if(after!=0x20){
                    for(;;){
                        if(!(read(b,input,1)&255u))break;
                        const auto c=read(b,input,1)&255u;write(b,key,c,1);++key;
                        const auto next=read(b,input+1,1)&255u;++input;if(next==0x20)break;
                    }
                }
                write(b,key,0,1);
                const auto mission=read(b,0x68ba34)+1;call(b,0x493a80,{frame+0x64,0x4cad50,mission});
                const auto current_table=read(b,0x503238);++input;const auto entry=call(b,0x49a092,{current_table,frame+0x64,frame+0xe4});
                if(entry){const auto text=read(b,entry);append(b,output,text);const auto current_text=read(b,entry);output+=length(b,current_text);}
                else{call(b,0x493a80,{frame+0xa4,0x4cae14,frame+0xe4});append(b,output,frame+0xa4);output+=length(b,frame+0xa4);}
            }else{
                ++output;const auto c=read(b,input,1)&255u;++input;write(b,output-1,c,1);
            }
            if(!(read(b,input,1)&255u))break;
        }
    }
    const auto source=read(b,frame+0x124);const auto destination=read(b,0x68bef0);const auto released=read(b,frame+0x124);
    copy(b,destination,source);call(b,0x4980d0,{released});
}
}
