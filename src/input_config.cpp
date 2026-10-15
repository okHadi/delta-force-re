#include "dfre/input_config.hpp"

#include <algorithm>
#include <cctype>

namespace dfre {
namespace {

// 0x49afa0: the game's own atoi. Skips isspace, accepts one sign, then folds
// digits into a 32-bit value with plain wrapping multiply/add.
std::int32_t original_atoi(std::string_view text) {
    std::size_t i=0;
    while(i<text.size()&&(text[i]==' '||text[i]=='\t'||text[i]=='\n'||text[i]=='\v'||text[i]=='\f'||text[i]=='\r'))++i;
    bool negative=false;
    if(i<text.size()&&(text[i]=='+'||text[i]=='-'))negative=text[i++]=='-';
    std::uint32_t value=0;
    while(i<text.size()&&text[i]>='0'&&text[i]<='9'){value=value*10u+std::uint32_t(text[i]-'0');++i;}
    return negative?std::int32_t(0u-value):std::int32_t(value);
}

bool token_separator(char c){return c==' '||c=='\t'||c==',';}

// 0x4565f0 tokenizes the line in place inside the reused 0x8e4040 buffer: space,
// tab and comma end tokens, double quotes are removed, and at most 30 tokens
// are recorded. The caller keeps the buffer between lines, so bytes after the
// current line stay from earlier lines exactly like the original.
struct TokenizedLine {
    int count=0;
    std::size_t offsets[30]{};
};

TokenizedLine tokenize_in_place(char* buffer,std::size_t length) {
    TokenizedLine record;
    bool in_quote=false;
    bool start=true;
    // The original loop runs while index <= strlen, so the terminating NUL is
    // processed too; when the line ends with a separator or a closing quote the
    // NUL starts one last empty token at offset strlen.
    for(std::size_t i=0;i<=length;++i){
        const char c=buffer[i];
        if(!in_quote&&token_separator(c)){
            buffer[i]=0;
            start=true;
            if(record.count>=30)return record;
            continue;
        }
        if(c=='"'){
            buffer[i]=0;
            start=true;
            in_quote=!in_quote;
            if(record.count>=30)return record;
            continue;
        }
        if(start){
            if(record.count>=30)return record;
            record.offsets[record.count++]=i;
            start=false;
        }
    }
    return record;
}

std::string_view buffer_text(const char* buffer,std::size_t offset) {
    return std::string_view(buffer+offset);
}

bool ascii_equal_ignore_case(std::string_view a,std::string_view b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i){
        const unsigned char x=static_cast<unsigned char>(a[i]),y=static_cast<unsigned char>(b[i]);
        if(std::tolower(x)!=std::tolower(y))return false;
    }
    return true;
}

void ascii_upper(std::string& value){for(char& c:value)if(c>='a'&&c<='z')c=char(c-32);}

std::string_view trim_trailing_spaces(std::string_view value) {
    while(!value.empty()&&value.back()==' ')value.remove_suffix(1);
    return value;
}

std::string_view trim_leading_spaces(std::string_view value) {
    while(!value.empty()&&value.front()==' ')value.remove_prefix(1);
    return value;
}

struct IntKey {
    std::string_view name;
    std::int32_t GameConfig::* field;
    bool clamp_to_16;
};
struct StringKey {
    std::string_view name;
    std::string GameConfig::* field;
    bool upper;
};

// Key names follow the 0x406640 strcasecmp chain.
constexpr IntKey int_keys[]={
    {"cd_play",&GameConfig::cd_play,false},
    {"menu_track",&GameConfig::menu_track,false},
    {"game_track",&GameConfig::game_track,false},
    {"cd_volume",&GameConfig::cd_volume,false},
    {"fx_volume",&GameConfig::fx_volume,false},
    {"dialog_volume",&GameConfig::dialog_volume,false},
    {"renderer",&GameConfig::renderer,false},
    {"windowed",&GameConfig::windowed,false},
    {"window_size",&GameConfig::window_size,false},
    {"game_res",&GameConfig::game_res,false},
    {"max_res",&GameConfig::max_res,false},
    {"gamma",&GameConfig::gamma,false},
    {"turbo",&GameConfig::turbo,false},
    {"flipmouse",&GameConfig::flipmouse,false},
    {"crosshairs",&GameConfig::crosshairs,false},
    {"mousescale",&GameConfig::mousescale,false},
    {"showgun",&GameConfig::showgun,false},
    {"picinpic",&GameConfig::picinpic,false},
    {"mp_service",&GameConfig::mp_service,false},
    {"mp_gametype",&GameConfig::mp_gametype,false},
    {"maxplayers",&GameConfig::maxplayers,true},
    {"dedicated",&GameConfig::dedicated,false},
    {"com_port",&GameConfig::com_port,false},
    {"baud_rate",&GameConfig::baud_rate,false},
    {"max_lives",&GameConfig::max_lives,false},
    {"max_kills",&GameConfig::max_kills,false},
    {"team_req",&GameConfig::team_req,false},
    {"attributes",&GameConfig::attributes,false},
    {"replay",&GameConfig::replay,false},
    {"time_limit",&GameConfig::time_limit,false},
    {"koth_limit",&GameConfig::koth_limit,false},
    {"ammo_respawn",&GameConfig::ammo_respawn,false},
    {"timeout",&GameConfig::timeout,false},
    {"gpsicon",&GameConfig::gpsicon,false},
    {"showtags",&GameConfig::showtags,false},
    {"contest",&GameConfig::contest,false},
    {"map_infrared",&GameConfig::map_infrared,false},
    {"map_iff",&GameConfig::map_iff,false},
    {"map_AWAC",&GameConfig::map_awac,false},
    {"preempt_pff",&GameConfig::preempt_pff,false},
    {"whichbrowser",&GameConfig::whichbrowser,false},
    {"mp_verbose",&GameConfig::mp_verbose,false},
};

constexpr StringKey string_keys[]={
    {"login_password",&GameConfig::login_password,true},
    {"game_password",&GameConfig::game_password,true},
    {"squad_password",&GameConfig::squad_password,true},
    {"game_name",&GameConfig::game_name,false},
    {"phone_number",&GameConfig::phone_number,false},
    {"internet_address",&GameConfig::internet_address,false},
    {"country",&GameConfig::country,false},
    {"red_password",&GameConfig::red_password,true},
    {"blue_password",&GameConfig::blue_password,true},
};

// 0x49b010/0x49b1d0/0x49b230/0x49b580: the shipped DFDK.KEY reader. Loading
// converts "\r\n" to '\n' + NUL, tabs to spaces and uppercases every byte
// (0x49bf20); the reader then only reads as many NUL-terminated strings as
// there were CRLF pairs, stops at a line starting with '[' and compares the
// key text up to '=' against "KEY".
std::vector<std::string_view> read_key_lines(const std::vector<char>& buffer,std::size_t line_count) {
    std::vector<std::string_view> lines;
    std::size_t begin=0;
    for(std::size_t i=0;i<buffer.size()&&lines.size()<line_count;++i){
        if(buffer[i]!='\0')continue;
        std::size_t length=i-begin;
        if(length>255)length=255;
        lines.emplace_back(buffer.data()+begin,length);
        begin=i+1;
    }
    return lines;
}

bool is_section_header(std::string_view line,std::string_view name) {
    if(line.empty()||line.front()!='[')return false;
    std::size_t end=1;
    while(end<line.size()&&((line[end]>='A'&&line[end]<='Z')||(line[end]>='0'&&line[end]<='9')||line[end]=='_'))++end;
    return line.substr(1,end-1)==name;
}

bool any_section_header(std::string_view line){return !line.empty()&&line.front()=='[';}

std::vector<std::string_view> split_lines(std::string_view text) {
    std::vector<std::string_view> lines;
    std::size_t begin=0;
    while(begin<=text.size()){
        const auto end=text.find('\n',begin);
        if(end==std::string_view::npos){lines.push_back(text.substr(begin));break;}
        lines.push_back(text.substr(begin,end-begin));
        begin=end+1;
    }
    return lines;
}

// strtok(value, ", "): separator runs collapse and empty fields are skipped.
std::vector<std::string_view> split_fields(std::string_view value) {
    std::vector<std::string_view> fields;
    std::size_t i=0;
    while(i<value.size()){
        while(i<value.size()&&(value[i]==','||value[i]==' '))++i;
        const std::size_t start=i;
        while(i<value.size()&&value[i]!=','&&value[i]!=' ')++i;
        if(i>start)fields.push_back(value.substr(start,i-start));
    }
    return fields;
}

const ActionBinding default_actions[action_count]={
    {37,0,0},{39,0,0},{38,0,0},{40,0,0},{34,3,0},{35,0,0},{46,0,0},{32,0,0},
    {17,1,0},{83,2,0},{16,0,0},{8,0,0},{66,0,0},{49,0,0},{50,0,0},{51,0,0},
    {52,0,0},{53,0,0},{54,0,0},{55,0,0},{56,0,0},{57,0,0},{48,0,0},{187,0,0},
    {189,0,0},{84,0,0},{89,0,0},{87,0,0},{65,0,0},{86,0,0},{68,0,0},{77,0,0},
    {104,0,0},{98,0,0},{102,0,0},{100,0,0},{9,0,0},{79,0,0},{82,0,0},{70,0,0},
    {71,0,0},{67,0,0},{72,0,0},{81,0,0},{69,0,0},{36,0,0},
};

constexpr std::string_view action_names[action_count]={
    "Move Left","Move Right","Move Forward","Move Back","Go Prone","Crouch",
    "Stand","Jump","Fire","Scope","Walk","Jump","Binocs","Primary Weapon Func 1",
    "Primary Weapon Func 2","Primary Weapon Func 3","Secondary Weapon Func 1",
    "Secondary Weapon Func 2","Secondary Weapon Func 3","Claymore/Satchel Charge",
    "Claymore/Satchel Charge","Frag Grenade","Laser","Zoom In","Zoom Out","Talk",
    "Squad Talk","Next waypoint","Audio Command","Resolution","Drop",
    "Change Magazine","Look Up","Look Down","Look Right","Look Left","Cycle FOB",
    "Mission Briefings","Recent Messages","Friendly Tags","Goals","Command Map",
    "Turbo Mode","Next Flag","Previous Waypoint","Server Screen",
};

// 0x4fb2fc, stride 0x2c: id, name[0x22], key (+0x24), mouse (+0x26),
// joystick (+0x28), flags (+0x2a).
const HotkeyCommand hotkey_rows[]={
    {0,"null",0,0,0,0},
    {1,"command",192,0,0,0},
    {2,"PrintScreen",80,0,0,64},
    {3,"exit",0,0,0,0},
    {4,"quit",0,0,0,0},
    {5,"sky",0,0,0,0},
    {6,"scope",83,2,0,1},
    {7,"fullScreen",145,0,0,0},
    {8,"resolution",86,0,0,0},
    {9,"help",112,0,0,0},
    {10,"flipmouse",0,0,0,0},
    {11,"hudcolor",0,0,0,0},
    {12,"server",36,0,0,0},
    {13,"restart",0,0,0,0},
    {14,"crosshairs",114,0,0,0},
    {15,"lbsize",123,0,0,0},
    {17,"showgun",115,0,0,0},
    {18,"gamma",0,0,0,0},
    {19,"saturation",0,0,0,0},
    {20,"mousescale",0,0,0,0},
    {21,"escape",27,0,0,0},
    {27,"huddetail",0,0,0,0},
    {28,"tracers",0,0,0,0},
    {29,"pinp",119,0,0,0},
    {30,"fo",120,0,0,0},
    {31,"tab",9,0,0,0},
    {32,"NextWaypoint",87,0,0,0},
    {33,"Drop",68,0,0,0},
    {34,"Pause",19,0,0,0},
    {35,"Binoculars",66,0,0,1},
    {36,"Turbo",72,0,0,0},
    {37,"briefing",79,0,0,0},
    {38,"BigMap",67,0,0,0},
    {39,"OldMessages",82,0,0,0},
    {40,"ShowFriendly",70,0,0,0},
    {41,"MissionGoals",71,0,0,0},
    {42,"NextFlag",81,0,0,0},
    {43,"AudioMacros",65,0,0,1},
    {44,"Punt",0,0,0,0},
    {45,"Bozo",0,0,0,0},
    {46,"",0,0,0,0},
    {47,"PrevWaypoint",69,0,0,0},
    {48,"Verbose",0,0,0,0},
    {49,"SaveScores",0,0,0,0},
    {100,"stalk",89,0,0,0},
    {101,"talk",84,0,0,0},
    {102,"killlist",75,0,0,0},
    {103,"netdelay",0,0,0,0},
    {104,"talkred",0,0,0,0},
    {105,"talkblue",0,0,0,0},
    {106,"resetgames",0,0,0,0},
    {107,"Ban",0,0,0,0},
    {108,"LastGame",0,0,0,0},
    {109,"LockGame",0,0,0,0},
    {110,"LockNames",0,0,0,0},
    {180,"weapon0",48,0,0,1},
    {181,"weapon1",49,0,0,1},
    {182,"weapon2",50,0,0,1},
    {183,"weapon3",51,0,0,1},
    {184,"weapon4",52,0,0,1},
    {185,"weapon5",53,0,0,1},
    {186,"weapon6",54,0,0,1},
    {187,"weapon7",55,0,0,1},
    {188,"weapon8",56,0,0,1},
    {189,"weapon9",57,0,0,1},
    {190,"magazine",77,0,0,1},
    {191,"cycle",8,0,0,1},
    {192,"cycleb",0,0,0,1},
    {120,"hitmewithyourbestshot",0,0,0,16},
    {121,"takeittothelimit",0,0,0,16},
    {122,"iwillsurvive",0,0,0,16},
    {123,"closetoyou",0,0,0,16},
    {124,"raindropskeepfallinonmyhead",0,0,0,16},
    {140,"attack_1",17,1,1,1},
    {141,"attack_2",17,0,0,1},
    {142,"move_back",40,0,0,5},
    {143,"move_forward",38,0,0,5},
    {144,"move_jump",32,0,0,1},
    {145,"look_down",98,0,0,5},
    {146,"look_up",104,0,0,5},
    {147,"slide_left",37,0,0,5},
    {148,"slide_right",39,0,0,5},
    {149,"turn_left",100,0,0,5},
    {150,"turn_right",102,0,0,5},
    {151,"fly_down",0,0,0,5},
    {152,"center_view",101,0,0,1},
    {153,"fly_up",0,0,0,5},
    {154,"toggle_item",0,0,0,0},
    {155,"look_up_abs",0,0,0,5},
    {156,"look_dn_abs",0,0,0,5},
    {157,"turn_left_abs",0,0,0,5},
    {158,"turn_right_abs",0,0,0,5},
    {159,"NoRun",16,0,0,4},
    {160,"Slide",18,0,0,4},
    {161,"Crouch",35,0,0,1},
    {162,"Prone",34,16,0,1},
    {163,"",0,0,0,0},
    {164,"Stand",46,0,0,1},
    {360,"zoomin",187,0,0,4},
    {361,"zoomout",189,0,0,4},
    {362,"mapterrain",121,0,0,0},
    {363,"mapelevation",122,0,0,0},
    {380,"debug_hud",123,0,0,64},
    {381,"d_down",98,0,0,0},
    {382,"d_up",104,0,0,0},
    {383,"d_pgdn",99,0,0,0},
    {384,"d_pgup",105,0,0,0},
    {385,"d_left",100,0,0,0},
    {386,"d_right",102,0,0,0},
    {387,"d_home",103,0,0,0},
    {388,"d_end",97,0,0,0},
    {400,"1stPerson",113,0,0,1},
    {402,"3rdPersonw",117,0,0,1},
    {401,"3rdPersonp",116,0,0,1},
    {403,"3rdPersonl",118,0,0,17},
    {410,"CameraLeft",76,0,0,5},
    {411,"CameraRight",186,0,0,5},
    {412,"CameraUp",80,0,0,5},
    {413,"CameraDown",190,0,0,5},
    {414,"CameraZoomIn",219,0,0,21},
    {415,"CameraZoomOut",221,0,0,21},
};

// 0x407360 writes these exact format strings (0x4c86a8-0x4c8bd8). The original
// CRT opens df.cfg in text mode, so Windows writes CRLF for each LF here.
void write_int(std::string& out,std::string_view format,std::int32_t value) {
    const auto at=format.find("%i");
    out+=format.substr(0,at);
    out+=std::to_string(value);
    out+=format.substr(at+2);
}
void write_string(std::string& out,std::string_view format,const std::string& value) {
    const auto at=format.find("%s");
    out+=format.substr(0,at);
    out+=value;
    out+=format.substr(at+2);
}

} // namespace

GameConfig default_game_config(std::int32_t cpu_family) {
    GameConfig config;
    const bool fast_cpu=cpu_family>5;
    config.cd_play=0;
    config.menu_track=2;
    config.game_track=2;
    config.cd_volume=255;
    config.fx_volume=255;
    config.dialog_volume=200;
    config.game_res=3;
    config.window_size=1;
    config.windowed=0;
    config.renderer=fast_cpu?1:0;
    config.max_res=fast_cpu?4:3;
    config.gamma=0x80;
    config.turbo=0;
    config.flipmouse=0;
    config.crosshairs=1;
    config.mousescale=0x80;
    config.showgun=1;
    config.picinpic=0;
    config.login_password.clear();
    config.game_password.clear();
    config.squad_password.clear();
    config.game_name.clear();
    config.phone_number.clear();
    config.internet_address="0.0.0.0";
    config.mp_service=4;
    config.mp_gametype=0;
    config.maxplayers=0x10;
    config.dedicated=0;
    config.com_port=0;
    config.baud_rate=0;
    config.max_lives=0x14;
    config.max_kills=0x14;
    config.team_req=1;
    config.attributes=2;
    config.replay=0;
    config.time_limit=0x1e;
    config.koth_limit=0x1e;
    config.ammo_respawn=2;
    config.timeout=0;
    config.gpsicon=1;
    config.showtags=0;
    config.contest=0;
    config.map_infrared=1;
    config.map_iff=1;
    config.map_awac=1;
    config.preempt_pff=0;
    config.whichbrowser=0;
    config.country.clear();
    config.red_password.clear();
    config.blue_password.clear();
    config.mp_verbose=1;
    config.runtime_ced58=0;
    config.runtime_ced60=0;
    config.runtime_ced64=0;
    config.runtime_ced68=0;
    config.window_scale=0x10000;
    config.map_scale_lo=0;
    config.map_scale_hi=1;
    return config;
}

std::size_t load_game_config_text(GameConfig& config,std::string_view text) {
    std::size_t matched=0;
    // The original tokenizer keeps one static buffer and one record slot for the
    // third token across all lines; model both so a line with fewer than three
    // tokens reuses the earlier third-token position, like 0x4565f0.
    std::vector<char> buffer(1004,'\0');
    std::size_t third_offset=0;
    bool third_known=false;
    for(const auto raw_line:split_lines(text)){
        const std::size_t length=std::min<std::size_t>(raw_line.size(),1000);
        std::copy_n(raw_line.begin(),length,buffer.data());
        buffer[length]='\0';
        const auto record=tokenize_in_place(buffer.data(),length);
        if(record.count==0)continue;
        const auto key=buffer_text(buffer.data(),record.offsets[0]);
        if(key.empty())continue;
        if(key.front()=='/')continue;
        if(record.count>=3){third_offset=record.offsets[2];third_known=true;}
        // A line with two tokens re-reads the stale third slot; when the slot was
        // never set the original dereferences a null pointer and the port uses "".
        const std::string_view value=third_known?buffer_text(buffer.data(),third_offset):std::string_view();
        bool found=false;
        for(const auto& entry:int_keys){
            if(!ascii_equal_ignore_case(key,entry.name))continue;
            auto parsed=original_atoi(value);
            if(entry.clamp_to_16&&parsed>0x10)parsed=0x10;
            config.*entry.field=parsed;
            ++matched;
            found=true;
            break;
        }
        if(found)continue;
        for(const auto& entry:string_keys){
            if(!ascii_equal_ignore_case(key,entry.name))continue;
            std::string parsed(value);
            if(entry.upper)ascii_upper(parsed);
            config.*entry.field=std::move(parsed);
            ++matched;
            break;
        }
    }
    return matched;
}

std::string save_game_config_text(const GameConfig& config) {
    std::string out;
    out+="// MUSIC\n";
    write_int(out,"cd_play          = %i\n",config.cd_play);
    write_int(out,"menu_track       = %i\n",config.menu_track);
    write_int(out,"game_track       = %i\n",config.game_track);
    write_int(out,"cd_volume        = %i\n",config.cd_volume);
    write_int(out,"fx_volume        = %i\n",config.fx_volume);
    write_int(out,"dialog_volume    = %i\n",config.dialog_volume);
    out+="\n";
    out+="// DISPLAY\n";
    write_int(out,"game_res         = %i\n",config.game_res);
    write_int(out,"window_size      = %i\n",config.window_size);
    write_int(out,"windowed         = %i\n",config.windowed);
    write_int(out,"renderer         = %i\n",config.renderer);
    write_int(out,"max_res          = %i\n",config.max_res);
    write_int(out,"gamma            = %i\n",config.gamma);
    write_int(out,"turbo            = %i\n",config.turbo);
    out+="\n";
    out+="// CONTROLS\n";
    write_int(out,"flipmouse        = %i\n",config.flipmouse);
    write_int(out,"crosshairs       = %i\n",config.crosshairs);
    write_int(out,"mousescale       = %i\n",config.mousescale);
    write_int(out,"showgun          = %i\n",config.showgun);
    write_int(out,"picinpic         = %i\n",config.picinpic);
    out+="\n";
    out+="// MULTIPLAYER\n";
    write_string(out,"login_password   = \"%s\"\n",config.login_password);
    write_string(out,"game_password    = \"%s\"\n",config.game_password);
    write_string(out,"squad_password   = \"%s\"\n",config.squad_password);
    write_string(out,"game_name        = \"%s\"\n",config.game_name);
    write_string(out,"phone_number     = \"%s\"\n",config.phone_number);
    write_string(out,"internet_address = \"%s\"\n",config.internet_address);
    write_int(out,"mp_service       = %i\n",config.mp_service);
    write_int(out,"mp_gametype      = %i\n",config.mp_gametype);
    write_int(out,"maxplayers       = %i\n",config.maxplayers);
    write_int(out,"dedicated        = %i\n",config.dedicated);
    write_int(out,"com_port         = %i\n",config.com_port);
    write_int(out,"baud_rate        = %i\n",config.baud_rate);
    write_int(out,"max_lives        = %i\n",config.max_lives);
    write_int(out,"max_kills        = %i\n",config.max_kills);
    write_int(out,"team_req         = %i\n",config.team_req);
    write_int(out,"attributes       = %i\n",config.attributes);
    write_int(out,"replay           = %i\n",config.replay);
    write_int(out,"time_limit       = %i\n",config.time_limit);
    write_int(out,"koth_limit       = %i\n",config.koth_limit);
    write_int(out,"ammo_respawn     = %i\n",config.ammo_respawn);
    write_int(out,"timeout          = %i\n",config.timeout);
    write_int(out,"gpsicon          = %i\n",config.gpsicon);
    write_int(out,"showtags         = %i\n",config.showtags);
    write_int(out,"contest          = %i\n",config.contest);
    out+="\n";
    out+="// MAP\n";
    write_int(out,"map_infrared     = %i\n",config.map_infrared);
    write_int(out,"map_iff          = %i\n",config.map_iff);
    write_int(out,"map_AWAC         = %i\n",config.map_awac);
    out+="\n";
    out+="// SYSTEM\n";
    write_int(out,"preempt_pff      = %i\n",config.preempt_pff);
    write_int(out,"whichbrowser     = %i\n",config.whichbrowser);
    write_string(out,"country\t\t\t = \"%s\"\n",config.country);
    write_string(out,"red_password     = \"%s\"\n",config.red_password);
    write_string(out,"blue_password    = \"%s\"\n",config.blue_password);
    write_int(out,"mp_verbose       = %i\n",config.mp_verbose);
    out+="\n";
    return out;
}

ActionBindings default_action_bindings() {
    ActionBindings bindings{};
    std::copy(std::begin(default_actions),std::end(default_actions),bindings.begin());
    return bindings;
}

std::string_view action_name(std::size_t id) {
    return id<action_count?action_names[id]:std::string_view();
}

ActionLoadResult load_action_bindings_text(ActionBindings& bindings,std::string_view text) {
    ActionLoadResult result;
    // 0x49b010 preprocessing: CRLF -> '\n' + NUL (and one line counted per
    // pair), tabs -> spaces, toupper on every byte (0x49bf20).
    std::vector<char> buffer(text.begin(),text.end());
    std::size_t line_count=0;
    for(std::size_t i=0;i+1<buffer.size();++i){
        if(buffer[i]=='\r'&&buffer[i+1]=='\n'){buffer[i+1]='\0';buffer[i]='\n';++line_count;}
    }
    for(char& c:buffer){
        if(c=='\t')c=' ';
        if(c>='a'&&c<='z')c=c-32;
    }
    const auto lines=read_key_lines(buffer,line_count);
    std::size_t i=0;
    for(;i<lines.size();++i){
        if(is_section_header(lines[i],"KEYS")){result.section_found=true;++i;break;}
    }
    if(!result.section_found)return result;
    for(;i<lines.size();++i){
        const auto line=lines[i];
        if(any_section_header(line))break;
        // The reader splits at the first '=' but also stops at CR/LF, so a name
        // without a following '=' never yields a value; the original then reads
        // the previous scan buffer and the port rejects the line.
        const auto stop=line.find_first_of("=\r\n");
        if(stop==std::string_view::npos)continue;
        const auto key=trim_trailing_spaces(line.substr(0,stop));
        if(key!="KEY")continue;
        if(line[stop]!='='){++result.rejected;continue;}
        auto value=line.substr(stop+1);
        const auto end=value.find_first_of("\r\n;");
        if(end!=std::string_view::npos)value=value.substr(0,end);
        value=trim_leading_spaces(value);
        const auto fields=split_fields(value);
        if(fields.size()<4){++result.rejected;continue;}
        const std::int32_t index=original_atoi(fields[0]);
        if(index<0||index>=std::int32_t(action_count)){++result.rejected;continue;}
        ActionBinding binding;
        binding.key=std::uint32_t(original_atoi(fields[1]));
        binding.mouse=std::uint32_t(original_atoi(fields[2]));
        binding.joystick=std::uint32_t(original_atoi(fields[3]));
        bindings[std::size_t(index)]=binding;
        ++result.applied;
    }
    return result;
}

const std::vector<HotkeyCommand>& hotkey_commands() {
    static const std::vector<HotkeyCommand> commands(std::begin(hotkey_rows),std::end(hotkey_rows));
    return commands;
}

const HotkeyCommand* find_hotkey_command(std::uint32_t id) {
    for(const auto& command:hotkey_commands())if(command.id==id)return &command;
    return nullptr;
}

bool hotkey_active(const HotkeyCommand& command,const std::array<std::uint8_t,256>& key_state,
                   std::uint32_t mouse_buttons,std::uint32_t joystick_buttons) {
    if(command.key!=0&&command.key<256&&key_state[command.key]!=0)return true;
    if((mouse_buttons&command.mouse)!=0)return true;
    if((joystick_buttons&command.joystick)!=0)return true;
    return false;
}

} // namespace dfre
