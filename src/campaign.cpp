#include "dfre/campaign.hpp"
#include "dfre/mission.hpp"
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace dfre {
namespace {
std::uint32_t word(const AssetBytes& data,std::size_t at){if(at>data.size()||data.size()-at<4)throw AssetError("Campaign word outside file");return data[at]|(std::uint32_t(data[at+1])<<8)|(std::uint32_t(data[at+2])<<16)|(std::uint32_t(data[at+3])<<24);}
void put(AssetBytes& data,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)data.at(at+i)=std::uint8_t(value>>(8*i));}
std::string number(unsigned value){std::ostringstream out;out<<std::setw(2)<<std::setfill('0')<<value;return out.str();}
// Original map centers: table4D0AF4, 0x50-byte campaign stride, 8-byte mission stride.
constexpr int map_centers[5][10][2]={
 {{103,30},{119,50},{154,146},{176,176},{125,204},{70,112}},
 {{108,47},{95,119},{166,96},{142,188},{91,221},{135,151}},
 {{24,129},{44,173},{85,206},{80,139},{116,117},{104,161},{139,154},{214,150}},
 {{30,97},{54,55},{86,71},{89,116},{146,98},{120,147},{147,124},{171,161},{231,150},{166,197}},
 {{213,19},{212,45},{137,36},{108,61},{80,98},{75,125},{33,146},{42,171},{66,195},{167,61}}};
constexpr unsigned mission_counts[5]={6,6,8,10,10};
void validate(const std::vector<CampaignDefinition>& campaigns){
    std::vector<std::uint32_t> indices;
    for(const auto& campaign:campaigns){if(campaign.index<1||campaign.index>5||campaign.missions.empty()||campaign.missions.size()>16||std::find(indices.begin(),indices.end(),campaign.index)!=indices.end())throw AssetError("Invalid campaign catalog");indices.push_back(campaign.index);}
    if(campaigns.empty())throw AssetError("Empty campaign catalog");
}
}
CampaignCatalog::CampaignCatalog(std::vector<CampaignDefinition> campaigns):campaigns_(std::move(campaigns)){validate(campaigns_);}
CampaignCatalog::CampaignCatalog(const AssetStore& assets){
    const RtxtTable menu(assets.read("dfmenu.bin").bytes),descriptions(assets.read("dfcamps.bin").bytes);
    for(unsigned index=1;index<=5;++index){CampaignDefinition campaign;campaign.index=index;const auto file_index=index+1;
        campaign.name=menu.get("Campaign Names","STR_C"+number(file_index)+"NAME");campaign.overview=descriptions.get("Campaign Descriptions","Camp"+std::to_string(file_index));
        campaign.text_file="dfcamp"+number(file_index)+".bin";campaign.map_image="ImageCampMap"+std::to_string(index);
        const RtxtTable text(assets.read(campaign.text_file).bytes);
        for(unsigned i=0;i<mission_counts[index-1];++i){CampaignMission mission;mission.filename="C"+number(file_index)+"M"+number(i+1)+".BMS";
            const auto bytes=assets.read(mission.filename).bytes;if(bytes.size()<268||bytes[0]!='B'||bytes[1]!='M'||bytes[2]!='S'||bytes[3]<=12)throw AssetError("Invalid campaign mission header");
            mission.equipment_flags=word(bytes,0x78);mission.prerequisites=word(bytes,0x7c);mission.map_x=map_centers[index-1][i][0];mission.map_y=map_centers[index-1][i][1];
            mission.name=text.get("Mission Names","STR_VC"+number(file_index)+"M"+number(i+1));mission.overview=descriptions.get("Mission Statements","C"+std::to_string(file_index)+"M"+std::to_string(i+1));campaign.missions.push_back(std::move(mission));
        }
        campaigns_.push_back(std::move(campaign));
    }
    validate(campaigns_);
}
const CampaignDefinition& CampaignCatalog::at(std::uint32_t index) const{for(const auto& campaign:campaigns_)if(campaign.index==index)return campaign;throw AssetError("Campaign index is outside the catalog");}
CampaignState::CampaignState(const CampaignCatalog& catalog,PlayerProfile& profile):catalog_(catalog),profile_(profile){enter();}
void CampaignState::enter(){const auto index=word(profile_.raw(),0x28);campaign_=index<1?1:index;const auto& c=catalog_.at(campaign_);mission_=word(profile_.raw(),0x470+campaign_*4);if(mission_>=c.missions.size())throw AssetError("Stored mission index is outside the campaign");write_selection();}
const CampaignDefinition& CampaignState::definition() const{return catalog_.at(campaign_);}
bool CampaignState::available(std::size_t index) const{const auto required=definition().missions.at(index).prerequisites;return !required||(word(profile_.raw(),0x70+campaign_*0x44)&required)==required;}
bool CampaignState::completed(std::size_t index) const{definition().missions.at(index);return word(profile_.raw(),0x30+campaign_*0x44+index*4)!=0;}
void CampaignState::write_selection(){auto bytes=profile_.encoded();put(bytes,0x28,campaign_);put(bytes,0x470+campaign_*4,mission_);profile_=PlayerProfile::decode(std::move(bytes),ProfileDefaults{});}
void CampaignState::select_campaign(std::uint32_t index){const auto& c=catalog_.at(index);const auto selected=word(profile_.raw(),0x470+index*4);if(selected>=c.missions.size())throw AssetError("Stored mission index is outside the campaign");campaign_=index;mission_=selected;write_selection();}
bool CampaignState::select_mission(std::uint32_t index){if(index>=definition().missions.size()||!available(index))return false;mission_=index;write_selection();return true;}
MissionSelection CampaignState::accept(){
    const auto& item=definition().missions.at(mission_);if(!available(mission_))throw AssetError("Selected mission prerequisites are not complete");auto bytes=profile_.encoded();
    // Original40B4A6–40B607 chooses the last clear restriction bit in each group.
    constexpr std::uint32_t primary_ids[5]={9,23,12,15,11},secondary_ids[4]={1,21,24,20},sidearm_ids[2]={6,5},primary_order[5]={0,4,1,2,3};
    for(unsigned i=0;i<5;++i)if(!(item.equipment_flags&(1u<<i)))put(bytes,0x698,primary_order[i]);
    for(unsigned i=0;i<4;++i)if(!(item.equipment_flags&(1u<<(i+8))))put(bytes,0x69c,i);
    for(unsigned i=0;i<2;++i)if(!(item.equipment_flags&(1u<<(i+16))))put(bytes,0x694,i);
    const auto primary=word(bytes,0x698),secondary=word(bytes,0x69c),sidearm=word(bytes,0x694);
    if(primary>=5||secondary>=4||sidearm>=2)throw AssetError("Profile equipment index is outside original tables");
    put(bytes,0x684,primary_ids[primary]);put(bytes,0x688,secondary_ids[secondary]);put(bytes,0x680,sidearm_ids[sidearm]);profile_=PlayerProfile::decode(std::move(bytes),ProfileDefaults{});
    return {campaign_,mission_,item.equipment_flags,item.prerequisites,item.filename,definition().text_file};
}
std::string expand_briefing(std::string text,const std::function<std::string(std::string_view)>& lookup,std::uint32_t mask){
    for(unsigned pass=0;pass<64;++pass){std::string out;bool changed=false;
        for(std::size_t at=0;at<text.size();){
            const bool add=text.compare(at,5,"{ADD}")==0,conditional=text.compare(at,4,"{CV:")==0;
            if(!add&&!conditional){out+=text[at++];continue;}changed=true;bool include=true;
            if(conditional){const auto end=text.find('}',at+4);if(end==std::string::npos)throw AssetError("Unclosed briefing condition");auto condition=text.substr(at+4,end-at-4);const bool negate=!condition.empty()&&condition[0]=='!';if(negate)condition.erase(0,1);
                const auto bit=std::uint8_t(std::strtol(condition.c_str(),nullptr,10))&31;include=bool(mask&(1u<<bit))!=negate;at=end+1;
            }else at+=5;
            const auto end=text.find(' ',at);const auto stop=end==std::string::npos?text.size():end;const auto key=std::string_view(text).substr(at,stop-at);if(include)out+=lookup(key);at=stop;
            if(conditional&&at+1<text.size()&&text[at+1]=='{')++at;
            if(out.size()>9999)throw AssetError("Expanded briefing exceeds original buffer size");
        }
        if(!changed)return text;text=std::move(out);
    }
    throw AssetError("Briefing expansion exceeds recursion limit");
}
BriefingText parse_briefing(std::string_view expanded){BriefingText out;std::size_t spaces=0;
    for(std::size_t at=0;at<expanded.size();){if(expanded[at]=='{'){const auto end=expanded.find('}',at+1);if(end==std::string_view::npos)throw AssetError("Unclosed briefing link");out.links.push_back({std::string(expanded.substr(at+1,end-at-1)),spaces});at=end+1;}
        else {if(expanded[at]==' ')++spaces;out.plain+=expanded[at++];}}
    if(out.links.size()>128)throw AssetError("Briefing has too many original link records");return out;
}
CampaignMenu::CampaignMenu(MenuRuntime& menu,const std::filesystem::path& folder,PlayerProfile& profile):menu_(menu),assets_(folder),catalog_(assets_),state_(catalog_,profile){}
void CampaignMenu::enter(){menu_.select_menu("CampMenu");state_.enter();ticks_=last_click_=0;labels();}
void CampaignMenu::labels(){
    for(const auto& c:catalog_.campaigns()){const auto name="but_ca_"+std::to_string(c.index+1);menu_.set_button_text(name,c.name);menu_.set_button_state(name,c.index==state_.campaign()?3:1);}
    const auto& selected=state_.definition().missions.at(state_.mission());menu_.set_button_text("but_ca_ovrtxt",menu_.strings().get("CAOVERVIEW")+" {C:TrueGreen}"+selected.name);
}
std::optional<CampaignResult> CampaignMenu::actions(const MenuAction& event){
    if(event.handler=="mnu_camp_select"){
        const auto index=std::uint32_t(menu_.button(event.button).user_id);
        if(event.action==1){state_.select_campaign(index);labels();}
        else if((event.action==6||event.action==7)&&index==state_.campaign())menu_.set_button_state(event.button,3);
        return {};
    }
    if(event.handler=="mnu_camp_map"&&event.action==1){const auto& map=menu_.button(event.button);const auto& missions=state_.definition().missions;
        for(std::size_t i=0;i<missions.size();++i){const auto& mission=missions[i];if(!state_.available(i)||pointer_x_<map.x+mission.map_x-10||pointer_x_>map.x+mission.map_x+10||pointer_y_<map.y+mission.map_y-10||pointer_y_>map.y+mission.map_y+10)continue;
            const bool same=i==state_.mission();if(same&&ticks_-last_click_<20)return CampaignResult{0x4d3dec,state_.accept()};state_.select_mission(std::uint32_t(i));last_click_=ticks_;labels();break;
        }
        return {};
    }
    if(event.action!=2&&event.action!=8)return {};
    if(event.handler=="mnu_camp_accept")return CampaignResult{0x4d3dec,state_.accept()};
    if(event.handler=="mnu_camp_back")return CampaignResult{0x4cf2ec,{}};
    if(event.handler=="mnu_camp_exit")return CampaignResult{0x4e6784,{}};
    return {};
}
void CampaignMenu::paragraph(int x,int y,int width,int height,std::string_view text){const auto& atlas=menu_.font_atlas("LCD10");const auto step=atlas.glyphs['A'].height;const auto bottom=y+height;int column=x;std::istringstream words{std::string(text)};std::string word;
    while(words>>word){if(word[0]=='~'){column=x;y+=step;continue;}word+=' ';int size=0;for(unsigned char c:word)size+=atlas.glyphs[c].width+1;if(column+size>x+width){column=x;y+=step;}if(y+step>bottom)return;menu_.draw_text("LCD10",column,y,word,0xff00ff00);column+=size;}
}
void CampaignMenu::owner_draw(const MenuAction& event){
    auto& button=menu_.button(event.button);
    if(event.handler=="mnu_camp_textbox")menu_.draw_image(button.image,button.x,button.y,0,1,false,true);
    if(event.handler!="mnu_camp_map")return;
    menu_.draw_image(state_.definition().map_image,button.x,button.y);
    const auto& missions=state_.definition().missions;
    for(std::size_t i=0;i<missions.size();++i){if(!state_.available(i))continue;const auto& item=missions[i];const bool selected=i==state_.mission(),hover=pointer_x_>=button.x+item.map_x-10&&pointer_x_<=button.x+item.map_x+10&&pointer_y_>=button.y+item.map_y-10&&pointer_y_<=button.y+item.map_y+10;
        const char* image=state_.completed(i)?"ImageCampMapFinsDot":selected?"ImageCampMapSeltDot":hover?"ImageCampMapPassDot":"ImageCampMapRegDot";
        menu_.draw_image(image,button.x+item.map_x-5,button.y+item.map_y-5,0,1,false,true);
        const auto& atlas=menu_.font_atlas("LCD7");int width=0;for(unsigned char c:item.name)width+=atlas.glyphs[c].width+1;
        menu_.draw_text("LCD7",button.x+item.map_x-width/2,button.y+item.map_y-15,item.name,selected?0xffaaffaa:(hover?menu_.definitions().colors.at("lightamber"):0xff00ff00));
    }
}
const std::vector<std::uint32_t>& CampaignMenu::draw(){const auto& frame=menu_.render([this](const MenuAction& event){owner_draw(event);});const auto& box=menu_.button("but_ca_txtbox");paragraph(box.x+8,box.y+8,240,122,state_.definition().missions.at(state_.mission()).overview);const auto& row=menu_.button("but_ca_"+std::to_string(state_.campaign()+1));menu_.draw_line(row.x,row.y+9,row.x-10,row.y+9,0xff00ff00);menu_.draw_line(row.x-10,row.y+9,row.x-10,box.y+30,0xff00ff00);menu_.draw_line(row.x-10,box.y+30,row.x,box.y+30,0xff00ff00);return frame;}

MissionSetupMenu::MissionSetupMenu(MenuRuntime& menu,const std::filesystem::path& folder,PlayerProfile& profile):menu_(menu),assets_(folder),profile_(profile){}
void MissionSetupMenu::center(int x,int y){target_x_=std::clamp(x-128,0,768);target_y_=std::clamp(y-128,0,768);menu_.set_button_value("but_mi_hscrsl",target_x_);menu_.set_button_value("but_mi_vscrsl",target_y_);}
void MissionSetupMenu::enter(const MissionSelection& selection){
    selection_=selection;menu_.select_menu("MissionMenu");text_=std::make_unique<RtxtTable>(assets_.read(selection.text_file).bytes);const auto group="Briefing"+std::to_string(selection.mission+1);
    const auto expanded=expand_briefing(text_->get(group,"Mission"+std::to_string(selection.mission+1)+"a"),[&](std::string_view key){return text_->get(group,key);},word(profile_.raw(),0x70+selection.campaign*0x44));briefing_=parse_briefing(expanded);
    map_=assets_.load_image("C"+std::to_string(selection.campaign+1)+"M"+std::to_string(selection.mission+1)+"sm.pcx").image;if(map_.width!=256||map_.height!=256)throw AssetError("Original mission map must be 256 by256");
    map_items_.clear();center_x_=center_y_=512;const auto mission=load_mission(assets_,selection.filename);
    for(unsigned group_index=0;group_index<4;++group_index)for(const auto& object:mission.object_groups[group_index]){
        const bool excluded=bool(object.flags&0x50);const auto x=(object.position_raw[0]>>18)+512,y=512-(object.position_raw[1]>>18);
        const auto icon=object.raw[76];if(icon&&!excluded&&map_items_.size()<200)map_items_.push_back({icon,object.byte_67,x,y});
        if(group_index==2&&object.type_id==6038&&(!icon||!excluded)){center_x_=x;center_y_=y;}
    }
    center(center_x_,center_y_);map_x_=target_x_;map_y_=target_y_;scroll_line_=lines_=0;links_.clear();link_center_.reset();link_text_.clear();
}
void MissionSetupMenu::update() noexcept{}
std::optional<CampaignResult> MissionSetupMenu::actions(const MenuAction& event){
    if(event.handler=="mnu_mis_scrollmap"&&(event.action==1||event.action==8)){const auto id=menu_.button(event.button).user_id;if(id==0)target_y_-=32;if(id==1)target_y_+=32;if(id==2)target_x_-=32;if(id==3)target_x_+=32;if(id==6)center(center_x_,center_y_);target_x_=std::clamp(target_x_,0,768);target_y_=std::clamp(target_y_,0,768);menu_.set_button_value("but_mi_hscrsl",target_x_);menu_.set_button_value("but_mi_vscrsl",target_y_);return {};}
    if(event.handler=="mnu_mis_pl1"&&event.action==1){const auto& button=menu_.button(event.button);center(pointer_x_-button.x+target_x_,pointer_y_-button.y+target_y_);return {};}
    if(event.handler=="mnu_mis_scrollbar"){
        if(event.action==1)menu_.set_button_state(event.button,3);
        else if(event.action==6)menu_.set_button_state(event.button,2);
        else if(event.action==7)menu_.set_button_state(event.button,1);
        else if(event.action==2){menu_.set_button_state(event.button,1);const auto& atlas=menu_.font_atlas(menu_.button("but_mi_pl2").font);const auto visible=272/std::max(1,int(atlas.glyphs['A'].height));const auto maximum=std::max(0,lines_-visible);
            if(event.button=="but_mi_up")scroll_line_=std::max(0,scroll_line_-1);
            if(event.button=="but_mi_dn")scroll_line_=std::min(maximum,scroll_line_+1);
            menu_.set_button_value("but_mi_sl",scroll_line_);
        }else if(event.action==3)scroll_line_=menu_.button("but_mi_sl").slider_position;
        return {};
    }
    if(event.handler=="mnu_mis_pl2"&&event.action==2){for(const auto& hit:links_)if(pointer_x_>=hit.x1&&pointer_x_<=hit.x2&&pointer_y_>=hit.y1&&pointer_y_<=hit.y2){const auto& link=briefing_.links.at(hit.index);const auto group="Briefing"+std::to_string(selection_.mission+1);link_text_=text_->get(group,link.key);if(const auto point=text_->coordinates(group,link.key)){link_center_=std::make_pair(int(point->first),int(point->second));center(point->first,point->second);}break;}return {};}
    if(event.action!=2&&event.action!=8)return {};
    if(event.handler=="mnu_mis_accept")return CampaignResult{0x4e6878,selection_};
    if(event.handler=="mnu_mis_back")return CampaignResult{0x4d0cd4,{}};
    if(event.handler=="mnu_mis_exit")return CampaignResult{0x4e6784,{}};
    if(event.handler=="mnu_mis_loadout")return CampaignResult{0x4ecf88,selection_};
    return {};
}
namespace {
bool clip_map_line(int& x1,int& y1,int& x2,int& y2,int left,int top,int right,int bottom){
    const auto code=[&](int x,int y){return (x<left?1:0)|(x>right?2:0)|(y<top?4:0)|(y>bottom?8:0);};
    for(unsigned pass=0;pass<8;++pass){const auto a=code(x1,y1),b=code(x2,y2);if(!(a|b))return true;if(a&b)return false;const auto edge=a?a:b;int x=0,y=0;
        if(edge&4){if(y2==y1)return false;y=top;x=x1+int(std::int64_t(x2-x1)*(top-y1)/(y2-y1));}
        else if(edge&8){if(y2==y1)return false;y=bottom;x=x1+int(std::int64_t(x2-x1)*(bottom-y1)/(y2-y1));}
        else if(edge&2){if(x2==x1)return false;x=right;y=y1+int(std::int64_t(y2-y1)*(right-x1)/(x2-x1));}
        else {if(x2==x1)return false;x=left;y=y1+int(std::int64_t(y2-y1)*(left-x1)/(x2-x1));}
        if(edge==a){x1=x;y1=y;}else{x2=x;y2=y;}
    }
    return false;
}
}
void MissionSetupMenu::draw_map(){
    const auto advance=[](int& current,int target){if(current>target)current-=std::min(32,(current-target)>>1);if(current<target)current+=std::min(32,(target-current)>>1);};advance(map_x_,target_x_);advance(map_y_,target_y_);
    const auto& button=menu_.button("but_mi_pl1");auto view=map_;for(int y=0;y<256;++y)for(int x=0;x<256;++x)view.pixels[y*256+x]=map_.pixels[((map_y_+y)&255)*256+((map_x_+1+x)&255)];menu_.draw_pcx(view,button.x,button.y);
    const int left=button.x,top=button.y,right=left+255,bottom=top+255;menu_.draw_line(left,top,right,top,0xff00ff00);menu_.draw_line(left,bottom,right,bottom,0xff00ff00);menu_.draw_line(left,top,left,bottom,0xff00ff00);menu_.draw_line(right,top,right,bottom,0xff00ff00);
    std::optional<std::pair<int,int>> previous;
    for(const auto& item:map_items_)if(item.icon==1||item.icon==3){const int x=left+item.x-map_x_,y=top+item.y-map_y_;if(previous){int x1=previous->first,y1=previous->second,x2=x,y2=y;if(clip_map_line(x1,y1,x2,y2,left,top,right,bottom))menu_.draw_line(x1,y1,x2,y2,0xffffe000);}previous=std::make_pair(x,y);}
    const auto inside=[&](const MapItem& item){return item.x-4>=map_x_+4&&item.x-4<=map_x_+247&&item.y-4>=map_y_+4&&item.y-4<=map_y_+247;};
    for(const auto& item:map_items_)if(item.icon!=1&&item.icon!=3&&inside(item)&&item.team<3){const char* image=item.team==0?"ImageGreenIcons":item.team==1?"ImageBlueIcons":"ImageRedIcons";menu_.draw_image(image,left+item.x-map_x_-4,top+item.y-map_y_-4,int(item.icon)-1,8,false,true);}
    unsigned number=0;for(const auto& item:map_items_)if(item.icon==1||item.icon==3){++number;if(item.x-4>=map_x_-4&&item.x-4<=map_x_+235&&item.y-4>=map_y_+4&&item.y-4<=map_y_+247)menu_.draw_text("LCD7",left+item.x-map_x_+4,top+item.y-map_y_-6,std::to_string(number),0xffaaffaa);if(inside(item))menu_.draw_image("ImageYellowIcons",left+item.x-map_x_-4,top+item.y-map_y_-4,int(item.icon)-1,8,false,true);}
    if(link_center_&&link_center_->first>=map_x_&&link_center_->first<=map_x_+254&&link_center_->second>=map_y_&&link_center_->second<=map_y_+254){const auto x=left+link_center_->first-map_x_,y=top+link_center_->second-map_y_;menu_.draw_line(x,top,x,bottom,0xffffe000);menu_.draw_line(left,y,right,y,0xffffe000);}
}
void MissionSetupMenu::draw_briefing(){
    const auto& panel=menu_.button("but_mi_pl2");const auto& atlas=menu_.font_atlas(panel.font);const int height=atlas.glyphs['A'].height,left=panel.x+8,right=panel.x+218,top=panel.y+8,bottom=panel.y+280;
    int x=left,line=0;std::size_t index=0,link_index=0;links_.clear();std::istringstream input(briefing_.plain);std::string token;
    while(input>>token){if(token[0]=='~'){x=left;++line;++index;continue;}token+=' ';int width=0;for(unsigned char c:token)width+=atlas.glyphs[atlas.glyphs[c].valid?c:0].width+1;
        if(index&&x+width>right){x=left;++line;}const auto y=top+(line-scroll_line_)*height;std::uint32_t color=0xff00ff00;const bool linked=link_index<briefing_.links.size()&&briefing_.links[link_index].word==index;
        if(linked){if(line>=scroll_line_&&y+height<=bottom){const LinkHit hit{link_index,x,y,x+width,y+height-1};links_.push_back(hit);color=pointer_x_>=hit.x1&&pointer_x_<=hit.x2&&pointer_y_>=hit.y1&&pointer_y_<=hit.y2?0xffff0000:0xffddffdd;}++link_index;}
        if(line>=scroll_line_&&y+height<=bottom)menu_.draw_text(panel.font,x,y,token,color);x+=width;++index;
    }
    lines_=line+1;const auto maximum=std::max(0,lines_-((bottom-top)/std::max(1,height)));scroll_line_=std::clamp(scroll_line_,0,maximum);menu_.button("but_mi_sl").slider_size=maximum;menu_.set_button_value("but_mi_sl",scroll_line_);
}
void MissionSetupMenu::owner_draw(const MenuAction& event){const auto& button=menu_.button(event.button);
    if(event.handler=="mnu_mis_scrollbar"){actions(event);return;}
    if(event.handler=="mnu_mis_pl1"){draw_map();return;}
    if(event.handler=="mnu_mis_pl2"){menu_.draw_image(button.image,button.x,button.y,0,1,false,true);draw_briefing();return;}
    if(event.handler=="mnu_mis_pl3"){menu_.draw_image(button.image,button.x,button.y,0,1,false,true);if(link_text_.empty())return;const auto& atlas=menu_.font_atlas("LCD7");int x=button.x+8,y=button.y+8;const int right=button.x+button.width-8,bottom=button.y+button.height-8;std::istringstream input(link_text_);std::string token;while(input>>token){if(token[0]=='~'){x=button.x+8;y+=atlas.glyphs['A'].height;continue;}token+=' ';int width=0;for(unsigned char c:token)width+=atlas.glyphs[c].width+1;if(x+width>right){x=button.x+8;y+=atlas.glyphs['A'].height;}if(y+atlas.glyphs['A'].height>bottom)break;menu_.draw_text("LCD7",x,y,token,0xff00ff00);x+=width;}}
}
const std::vector<std::uint32_t>& MissionSetupMenu::draw(){return menu_.render([this](const MenuAction& event){owner_draw(event);});}
}
