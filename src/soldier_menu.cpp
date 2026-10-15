#include "dfre/soldier_menu.hpp"
#include <sstream>

namespace dfre {
namespace {
std::string stem(std::size_t index){return "but_ns_"+std::to_string(index+1);}
constexpr std::uint32_t green=0xff00ff00;
}
SoldierMenu::SoldierMenu(MenuRuntime& menu,std::filesystem::path game,std::filesystem::path save):
    menu_(menu),assets_(game),profiles_(std::move(game),std::move(save)){for(std::size_t i=0;i<portraits_.size();++i)portraits_[i]=assets_.load_image(portrait_filename(std::uint32_t(i))).image;}
void SoldierMenu::enter(){menu_.select_menu("NewSoldierMenu");if(!initialized_||!profiles_.at(selected_).active())selected_=profiles_.first_active();initialized_=true;preview_=selected_;edit_=Edit::none;confirm_=false;prepare_draw();}
void SoldierMenu::prepare_draw(){
    for(std::size_t i=0;i<9;++i){const auto name=stem(i);const auto& profile=profiles_.at(i);
        menu_.set_button_text(name+"txt",i==selected_&&edit_!=Edit::none?input_:(profile.active()?profile.name():menu_.strings().get("NSEMPTY")));
        menu_.set_button_state(name,i==selected_?3:1);menu_.set_button_state(name+"l",i==selected_?3:1);
    }
    const bool active=profiles_.at(selected_).active(),normal=edit_==Edit::none;
    menu_.set_button_state("but_ns_actxt",active&&normal?1:4);
    menu_.set_button_state("but_ns_plt",active&&normal?1:0);menu_.set_button_state("but_ns_prt",active&&normal?1:0);
    menu_.set_button_state("but_ns_dey",confirm_?1:0);menu_.set_button_state("but_ns_den",confirm_?1:0);
}
void SoldierMenu::start_edit(Edit mode){edit_=mode;preview_.reset();confirm_=false;input_=mode==Edit::rename?profiles_.at(selected_).name():"";prepare_draw();}
void SoldierMenu::finish_edit(bool commit){
    if(commit){if(edit_==Edit::create){if(input_.empty())profiles_.erase(selected_);else profiles_.create(selected_,input_);}
        else if(edit_==Edit::rename&&!input_.empty())profiles_.at(selected_).set_name(input_);
        if(!input_.empty())profiles_.save(selected_);
    }
    edit_=Edit::none;preview_=selected_;input_.clear();prepare_draw();
}
bool SoldierMenu::key(int key){
    if(edit_==Edit::none)return false;
    if(key==13){finish_edit(true);return true;}if(key==27){finish_edit(false);return true;}
    if(key==8){if(!input_.empty())input_.pop_back();}
    prepare_draw();return true;
}
bool SoldierMenu::text(std::string_view utf8){
    if(edit_==Edit::none)return false;
    // The original scanner accepts byte characters. UTF-8 conversion beyond ASCII requires a code-page check.
    for(unsigned char c:utf8)if(c>=32&&c<127&&input_.size()<15)input_.push_back(char(c));
    prepare_draw();return true;
}
std::optional<GameScreen> SoldierMenu::actions(const MenuAction& event){
    if(event.action==1||event.action==6||event.action==7){
        auto& button=menu_.button(event.button);
        if(event.handler=="mnu_ns_namebutton1"){
            const auto slot=std::size_t(button.user_id);
            if(slot<9){if(event.action==1)preview_=slot;const int state=slot==selected_?3:(event.action==1?3:(!confirm_&&event.action==6?2:1));menu_.set_button_state(stem(slot),state);menu_.set_button_state(stem(slot)+"l",state);}
        }else if(button.state!=0&&button.state!=4)menu_.set_button_state(event.button,event.action==1?3:(event.action==6?2:1));
        return {};
    }
    if(event.action!=2&&event.action!=8)return {};
    if(event.handler=="mnu_ns_exit")return GameScreen::end;
    if(event.handler=="mnu_ns_accept"&&profiles_.at(selected_).active()&&edit_==Edit::none){profiles_.save_all();return GameScreen::main_menu;}
    if(event.handler=="mnu_ns_namebutton1"){
        const auto slot=std::size_t(menu_.button(event.button).user_id);if(slot<9&&slot!=selected_){if(edit_!=Edit::none)finish_edit(true);selected_=slot;preview_=slot;confirm_=false;if(!profiles_.at(slot).active())start_edit(Edit::create);}
    }
    else if(event.handler=="mnu_ns_edit"&&profiles_.at(selected_).active())start_edit(Edit::rename);
    else if(event.handler=="mnu_ns_delete"&&profiles_.at(selected_).active())confirm_=true;
    else if(event.handler=="mnu_ns_deleteyes")start_edit(Edit::create);
    else if(event.handler=="mnu_ns_deleteno")confirm_=false;
    else if((event.handler=="mnu_ns_picleft"||event.handler=="mnu_ns_picright")&&profiles_.at(selected_).active()){
        auto& profile=profiles_.at(selected_);const auto index=profile.portrait()%11;
        profile.set_portrait(event.handler=="mnu_ns_picleft"?(index+10)%11:(index+1)%11);
    }
    prepare_draw();return {};
}
void SoldierMenu::paragraph(int x,int y,int width,int height,std::string_view text){
    const auto& font=menu_.font_atlas("LCD10");const int line=font.glyphs['A'].height;int at=x;
    std::istringstream words{std::string(text)};std::string word;
    while(words>>word){if(word[0]=='~'){at=x;y+=line;continue;}
        word+=' ';int size=0;for(unsigned char c:word)size+=font.glyphs[c].width+1;
        if(at+size>x+width){at=x;y+=line;}if(y+line>menu_.button("but_ns_info").y+height)return;
        menu_.draw_text("LCD10",at,y,word,green);at+=size;
    }
}
void SoldierMenu::owner_draw(const MenuAction& event){
    const auto& profile=profiles_.at(selected_);const auto& detail=profiles_.at(preview_.value_or(selected_));const auto& info=menu_.button("but_ns_info");
    if(event.handler=="mnu_ns_statusbox"){
        if(confirm_){menu_.draw_line(info.x,info.y,info.x+320,info.y,green);menu_.draw_line(info.x,info.y+62,info.x+320,info.y+62,green);menu_.draw_line(info.x,info.y,info.x,info.y+62,green);menu_.draw_line(info.x+320,info.y,info.x+320,info.y+62,green);paragraph(info.x+8,info.y+8,312,62,menu_.strings().get("NSDELETEDIALOG"));}
        else if(edit_!=Edit::none){menu_.draw_image("ImageOperPlt3",info.x,info.y,0,1,false,false,true);paragraph(info.x+8,info.y+8,310,60,menu_.strings().get(edit_==Edit::create?"NSCREATEHELP":"NSDELETEHELP"));if(edit_==Edit::create){const auto& row=menu_.button(stem(selected_)+"l");menu_.draw_line(row.x+296,row.y+9,row.x+313,row.y+9,green);menu_.draw_line(row.x+313,row.y+9,row.x+313,info.y+9,green);menu_.draw_line(row.x+313,info.y+9,row.x+296,info.y+9,green);}}
        else if(preview_&&detail.active()){
            menu_.draw_image("ImageOperPlt",info.x,info.y,0,1,false,false,true);const auto s=detail.stats();
            const auto label=[&](std::string_view key){return menu_.strings().get(key);};
            const auto left=label("NSOPERATIVENAME")+": "+detail.name()+" ~ "+label("NSSPKILLS")+": "+std::to_string(s.campaign_kills)+" ~ "+label("NSSPSPK")+": "+std::to_string(s.completed_missions);
            const auto right=label("NSMULTIPLAYERKILLS")+": "+std::to_string(s.multiplayer_kills)+" ~ "+label("NSMPDEATHS")+": "+std::to_string(s.multiplayer_deaths)+" ~ "+label("NSMPSPK")+": "+std::to_string(s.multiplayer_team_games_won);
            paragraph(info.x+8,info.y+11,258,71,left);paragraph(info.x+268,info.y+11,260,71,right);
        }
    }else if(event.handler=="mnu_ns_rightbox"&&profile.active()&&edit_==Edit::none){
        const auto& box=menu_.button("but_ns_rtbox");menu_.draw_image("ImageOperPlt2",box.x,box.y,0,1,false,false,true);
        menu_.draw_pcx(portraits_.at(profile.portrait()),box.x+15,box.y+14);
        menu_.draw_line(box.x+15,box.y+14,box.x+142,box.y+14,green);menu_.draw_line(box.x+15,box.y+141,box.x+142,box.y+141,green);
        menu_.draw_line(box.x+15,box.y+14,box.x+15,box.y+141,green);menu_.draw_line(box.x+142,box.y+14,box.x+142,box.y+141,green);
        const auto& row=menu_.button(stem(selected_)+"l");const auto& edit=menu_.button("but_ns_ed");const auto& del=menu_.button("but_ns_de");
        menu_.draw_line(row.x+296,row.y+9,397,row.y+9,green);menu_.draw_line(397,row.y+9,397,edit.y+9,green);menu_.draw_line(397,row.y+9,397,box.y+87,green);
        menu_.draw_line(397,box.y+87,box.x-1,box.y+87,green);menu_.draw_line(397,edit.y+9,box.x-1,edit.y+9,green);menu_.draw_line(397,del.y+9,box.x-1,del.y+9,green);
    }
}
const std::vector<std::uint32_t>& SoldierMenu::draw(){const auto& frame=menu_.render([this](const MenuAction& event){owner_draw(event);});
    if(preview_&&profiles_.at(*preview_).active()&&!confirm_&&edit_==Edit::none){const auto& row=menu_.button(stem(*preview_));const auto y=row.y+row.height/2;const auto bottom=352+row.height/2;menu_.draw_line(row.x,y,row.x-12,y,green);menu_.draw_line(row.x-12,y,row.x-12,bottom,green);menu_.draw_line(row.x-12,bottom,row.x,bottom,green);}
    return frame;}
}
