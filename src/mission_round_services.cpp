#include "dfre/mission_round_services.hpp"

namespace dfre {
namespace {
using U=std::uint32_t;
template<class F> const F& need(const F& f,const char* error){if(!f)throw MissionRoundServiceError(error);return f;}
std::int32_t signed32(U v){return v<=0x7fffffffu?std::int32_t(v):std::int32_t(std::int64_t(v)-0x100000000LL);}
U cell(const MissionRoundServiceBindings& b,U a){return need(b.cell,"Missing round cell reader")(a,4);}
U client(const MissionRoundServiceBindings& b,U i,U a,U width=4){return need(b.client,"Missing round client reader")(i,a,width);}
U squad(const MissionRoundServiceBindings& b,U i,U a){return need(b.squad,"Missing round squad reader")(i,a,4);}
void put_squad(const MissionRoundServiceBindings& b,U i,U a,U v){need(b.squad_store,"Missing round squad writer")(i,a,4,v);}
void print(const MissionRoundServiceBindings& b,U file,U fmt,std::vector<MissionRoundLogValue> args={}){need(b.file_print,"Missing round file print")(file,fmt,args);}
U score(const MissionRoundServiceBindings& b,U slot,U mode){
    if(mode==5)return client(b,slot,0x1bc);
    if(mode==3||mode==4)return client(b,slot,0x194);
    const U value=client(b,slot,0x1a0),penalty=client(b,slot,0x1b0),loss=client(b,slot,0x1b4);
    return value-loss-(penalty<<2);
}
}
void mission_round_set_client_updates(const MissionRoundServiceBindings& b,U enabled){need(b.store,"Missing round cell writer")(0x6e5588,4,enabled);}
void mission_round_clear_frame_flag(const MissionRoundServiceBindings& b){need(b.store,"Missing round cell writer")(0x6e5570,4,0);}
void mission_round_collect_squads(const MissionRoundServiceBindings& b){
    for(U i=0;i<64;++i)for(U offset=0x48;offset<=0x60;offset+=4)put_squad(b,i,offset,0);
    for(U i=0;i<17;++i){
        if(!client(b,i,0x18c,1)||client(b,i,0x1c4,2)!=5)continue;
        const U team=client(b,i,0x9c),first=client(b,i,0x19c);
        put_squad(b,team,0x48,squad(b,team,0x48)+first);
        for(U offset=0x4c;offset<=0x60;offset+=4){const U prior=squad(b,team,offset),value=client(b,i,0x19c+offset-0x48);put_squad(b,team,offset,prior+value);}
    }
}
std::int32_t mission_round_compare_clients(const MissionRoundServiceBindings& b,U first,U second){
    const U mode=cell(b,0x67f53c);if(mode>5)return 0;
    U left,right;
    if(mode<=2){
        const U value=client(b,first,0x1a0),penalty=client(b,first,0x1b0),loss=client(b,first,0x1b4);
        left=value-loss-(penalty<<2);
        const U value2=client(b,second,0x1a0),loss2=client(b,second,0x1b4),penalty2=client(b,second,0x1b0);
        right=value2-loss2-(penalty2<<2);
    }else{const U offset=mode==5?0x1bc:0x194;left=client(b,first,offset);right=client(b,second,offset);}
    if(left!=right)return signed32(left)>signed32(right)?1:-1;
    left=client(b,first,0x1ac);right=client(b,second,0x1ac);
    if(left!=right)return signed32(left)>signed32(right)?1:-1;
    left=client(b,first,0x1a8);right=client(b,second,0x1a8);
    if(left!=right)return signed32(left)<signed32(right)?1:-1;
    return 0;
}
void mission_round_log(const MissionRoundServiceBindings& b,U mode,std::optional<U> inherited_edi){
    if(!cell(b,0x503428)||!cell(b,0x67f520))return;
    mission_round_collect_squads(b);
    const U file=need(b.file_open,"Missing round file open")("scores.txt","a");if(!file)return;
    print(b,file,0x4cc72c,{need(b.text,"Missing round game name")(0x67f478)});
    const auto time=need(b.local_time,"Missing round local time")();
    print(b,file,0x4cc748,{U(time[1]),U(time[3]),U(time[0]),U(time[4]),U(time[5])});
    const U game=cell(b,0x67f53c);
    switch(game){case 1:print(b,file,0x4cc7e8);break;case 2:print(b,file,0x4cc764);break;case 3:print(b,file,0x4cc7c8);break;case 4:print(b,file,0x4cc784);break;case 5:print(b,file,0x4cc7a8);break;default:break;}
    const U current_game=cell(b,0x67f53c);
    if(current_game==2||current_game==4||current_game==5){
        print(b,file,mode==1?0x4cc804:mode==2?0x4cc81c:0x4cc830);
        if(!inherited_edi)throw MissionRoundServiceError("Team log needs the source incoming EDI");
        const U index=*inherited_edi;
        const U blue=cell(b,0x6e8144),blue_mode=cell(b,0x67f53c),blue_loss=squad(b,index,0x60),blue_penalty=squad(b,index,0x5c);
        const U blue_score=blue-blue_loss-(blue_penalty<<2),blue_time=cell(b,0x6e813c),blue_flags=cell(b,0x6e815c);
        if(blue_mode==2)print(b,file,0x4cc840,{blue_score});
        else if(blue_mode==4){const auto t=signed32(blue_time);print(b,file,0x4cc854,{U(t/60),U(t%60)});}
        else if(blue_mode==5)print(b,file,0x4cc86c,{blue_flags});
        const U red_mode=cell(b,0x67f53c),red=cell(b,0x6e81ac),red_loss=squad(b,index,0x60),red_penalty=squad(b,index,0x5c);
        const U red_time=cell(b,0x6e81a4),red_score=red-red_loss-(red_penalty<<2),red_flags=cell(b,0x6e81c4);
        if(red_mode==2)print(b,file,0x4cc884,{red_score});
        else if(red_mode==4){const auto t=signed32(red_time);print(b,file,0x4cc898,{U(t/60),U(t%60)});}
        else if(red_mode==5)print(b,file,0x4cc8b0,{red_flags});
    }
    print(b,file,0x4cc8c8);
    for(U slot=0;slot<17;++slot){
        if(!client(b,slot,0x18c,1)|| (client(b,slot,0x18d,1)&&!cell(b,0x67f52c)))continue;
        const auto name=need(b.client_name,"Missing round client name")(slot);
        if(name.size()>31)throw MissionRoundServiceError("Client name exceeds the source log stack buffer");
        const U row_mode=cell(b,0x67f53c),value=score(b,slot,row_mode),format_mode=cell(b,0x67f53c),deaths=client(b,slot,0x1a8),kills=client(b,slot,0x1ac);
        if(format_mode==3||format_mode==4){const auto t=signed32(value);print(b,file,0x4cc8f8,{slot,name,U(t/60),U(t%60),kills,deaths});}
        else print(b,file,0x4cc918,{slot,name,value,kills,deaths});
    }
    print(b,file,0x4cc934);need(b.file_close,"Missing round file close")(file);
}
const char* mission_round_log_format(U address){
    switch(address){
    case 0x4cc72c:return "Game Stats for Game: %s\n";
    case 0x4cc748:return "  Date: %i/%i/%i, %i:%02i\n";
    case 0x4cc764:return "  Game type: Team Deathmatch\n";
    case 0x4cc784:return "  Game type: Team King of the Hill\n";
    case 0x4cc7a8:return "  Game type: Capture the Flag\n";
    case 0x4cc7c8:return "  Game type: King of the Hill\n";
    case 0x4cc7e8:return "  Game type: Deathmatch\n";
    case 0x4cc804:return "  Winner: Team Blue\n";
    case 0x4cc81c:return "  Winner: Team Red\n";
    case 0x4cc830:return "  Winner: None\n";
    case 0x4cc840:return "  Blue Score: %i\n";
    case 0x4cc854:return "  Blue Score: %2i:%02i\n";
    case 0x4cc86c:return "  Blue Score: %i flags\n";
    case 0x4cc884:return "  Red Score:   %i\n";
    case 0x4cc898:return "  Red Score:  %2i:%02i\n";
    case 0x4cc8b0:return "  Red Score:  %i flags\n";
    case 0x4cc8c8:return "  Players ( id, name, score, kills, deaths )\n";
    case 0x4cc8f8:return "    %2i %20s %2i:%02i %3i %3i\n";
    case 0x4cc918:return "    %2i %20s %3i %3i %3i\n";
    case 0x4cc934:return "\n\n";
    default:throw MissionRoundServiceError("Unknown round log format");
    }
}
} // namespace dfre
