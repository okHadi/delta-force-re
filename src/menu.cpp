#include "dfre/menu.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <utility>

namespace dfre {
namespace {
std::string lower(std::string_view value) {
    std::string out(value);
    for (auto& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    return out;
}
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}
std::string name(std::string value) {
    value = trim(std::move(value));
    return lower(value) == "null" ? std::string{} : value;
}
std::vector<std::string> split(std::string_view line) {
    std::vector<std::string> out;
    std::size_t at = 0;
    do {
        const auto end = line.find(',', at);
        out.push_back(name(std::string(line.substr(at, end == std::string::npos ? end : end - at))));
        if (end == std::string::npos) break;
        at = end + 1;
    } while (at <= line.size());
    return out;
}
int number(const std::string& value) {
    if (value.empty()) return 0;
    // The original decimal reader accepts a numeric prefix (DF.MNU contains 200x).
    try { return std::stoi(value); }
    catch (const std::exception&) { throw AssetError("Invalid menu number: " + value); }
}
std::uint32_t u32(const AssetBytes& data, std::size_t at) {
    if (at > data.size() || data.size() - at < 4) throw AssetError("Truncated RTXT word");
    return data[at] | (std::uint32_t(data[at + 1]) << 8) |
           (std::uint32_t(data[at + 2]) << 16) | (std::uint32_t(data[at + 3]) << 24);
}
std::string cstring(const AssetBytes& data, std::size_t at, std::size_t end) {
    if (end > data.size() || at >= end) throw AssetError("RTXT string outside file");
    const auto zero = std::find(data.begin() + at, data.begin() + end, 0);
    if (zero == data.begin() + end) throw AssetError("Unterminated RTXT string");
    return {data.begin() + at, zero};
}
template<class T> const T& find(const std::unordered_map<std::string, T>& table,
                                std::string_view key, const char* kind) {
    const auto found = table.find(lower(key));
    if (found == table.end()) throw AssetError(std::string("Unknown menu ") + kind + ": " + std::string(key));
    return found->second;
}
template<class T> void insert(std::unordered_map<std::string, T>& table,
                              const std::string& key, T value) {
    if (key.empty() || !table.emplace(lower(key), std::move(value)).second)
        throw AssetError("Duplicate or empty menu name: " + key);
}
std::uint32_t rgb(const PcxImage& image, std::uint8_t pixel) {
    if (image.palette.size() != 768) throw AssetError("Menu PCX requires an RGB palette");
    const auto at = std::size_t(pixel) * 3;
    return 0xff000000u | (std::uint32_t(image.palette[at]) << 16) |
           (std::uint32_t(image.palette[at + 1]) << 8) | image.palette[at + 2];
}
std::uint32_t strip_blend(std::uint32_t source, std::uint32_t target) {
    // Original 40EC30: MMX masked halves, quarter target, then packed dword adds.
    const auto half = (target & 0xfefefefeu) >> 1;
    return 0xff000000u | (((source & 0x00fefefeu) >> 1) + half +
                         ((half & 0xfefefefeu) >> 1));
}
std::uint32_t tint(std::uint32_t target, std::uint32_t color, unsigned intensity) {
    std::uint32_t out = 0xff000000u;
    for (unsigned pair : {0u, 16u}) {
        std::uint32_t base=0,delta_words=0;
        for(unsigned word=0;word<2;++word) {
            const auto shift=pair+word*8;
            const int a=(target>>shift)&255,b=(color>>shift)&255;
            const int product=(b-a)*int(intensity);
            const int delta=product>=0?product/128:-((-product+127)/128);
            base|=std::uint32_t(a)<<(word*16);
            delta_words|=std::uint32_t(std::uint16_t(delta))<<(word*16);
        }
        // Original 44C3AC uses PADDD, so a low-word carry changes the next channel.
        const auto sum=base+delta_words;
        for(unsigned word=0;word<2;++word) {
            const int value=std::int16_t(sum>>(word*16));
            if(pair+word*8<24)out|=std::uint32_t(std::clamp(value,0,255))<<(pair+word*8);
        }
    }
    return out;
}
} // namespace

MenuData parse_menu(const AssetBytes& data) {
    MenuData out;
    std::vector<std::vector<std::string>> attachments;
    std::istringstream stream(std::string(data.begin(), data.end()));
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find('\0') != std::string::npos || line.size() > 4096) throw AssetError("Invalid DF.MNU line");
        line = trim(line.substr(0, line.find(';')));
        if (line.empty() || line[0] == '[') continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        const auto kind = lower(trim(line.substr(0, equals)));
        auto f = split(line.substr(equals + 1));
        const auto need = [&](std::size_t size) { if (f.size() < size) throw AssetError("Truncated menu " + kind); };
        if (kind == "rgb" || kind == "rgba") {
            need(4); std::uint32_t color = 0xff000000u;
            for (unsigned i = 1; i < 4; ++i) {
                const auto value = number(f[i]);
                if (value < 0 || value > 255) throw AssetError("Menu color outside byte range");
                color |= std::uint32_t(value) << (8 * (3 - i));
            }
            insert(out.colors, f[0], color);
        } else if (kind == "font") {
            need(8); insert(out.fonts, f[0], MenuFont{f[0], f[1], f[2], f[4], f[6]});
        } else if (kind == "image") {
            need(7); insert(out.images, f[0], MenuImage{f[0], f[1], number(f[2]), number(f[3]),
                                                     number(f[4]), number(f[5]), number(f[6]) != 0});
        } else if (kind == "button") {
            need(20); MenuButton b;
            b.name=f[0]; b.type=number(f[1]); b.attributes=number(f[2]); b.text_key=f[3];
            b.handler=f[4]; b.linked_button=f[5]; b.hover_handler=f[6]; b.x=number(f[7]); b.y=number(f[8]);
            b.font=f[9]; b.image=f[10]; b.slider_size=number(f[11]); b.slider_position=number(f[12]);
            b.width=number(f[13]); b.height=number(f[14]); b.control_state=number(f[15]); b.state=number(f[16]);
            b.hotkey=number(f[17]); b.user_id=number(f[18]);
            // DF.MNU has 20 fields: user draw is the final field after user id.
            b.draw_handler=f[19];
            insert(out.buttons, b.name, b);
        } else if (kind == "menu") {
            need(12); MenuDefinition m;
            m.name=f[0]; m.background=f[1]; m.font=f[2]; m.accept_handler=f[3]; m.accept_button=f[4];
            m.back_handler=f[5]; m.back_button=f[6];
            for (unsigned i=0;i<4;++i) m.arrow_buttons[i]=f[7+i];
            m.string_group=f[11]; out.menu_order.push_back(m.name); insert(out.menus,m.name,m);
        } else if (kind == "menubutton") attachments.push_back(std::move(f));
    }
    for (const auto& row : attachments) {
        if (row.empty()) continue;
        auto found=out.menus.find(lower(row[0]));
        if (found==out.menus.end()) throw AssetError("Attachment names an unknown menu");
        for (std::size_t i=1;i<row.size() && lower(row[i])!=".stop";++i) {
            if (row[i].empty()) continue;
            if(out.buttons.find(lower(row[i]))!=out.buttons.end()) found->second.buttons.push_back(row[i]);
        }
    }
    return out;
}

RtxtTable::RtxtTable(const AssetBytes& data) {
    if (data.size()<16 || std::string(data.begin(),data.begin()+4)!="RTXT") throw AssetError("Expected RTXT table");
    const std::size_t metadata=u32(data,4), names_bytes=u32(data,8), count=u32(data,12);
    if (count>(data.size()-16)/16 || metadata<16+count*16 || metadata>data.size() ||
        names_bytes>data.size()-metadata) throw AssetError("RTXT ranges outside file");
    const std::size_t text_base=16+count*16, groups=u32(data,metadata);
    if (groups>(data.size()-metadata-4)/8) throw AssetError("Truncated RTXT groups");
    std::size_t group_name=metadata+4+groups*8, index=0;
    for (std::size_t group=0;group<groups;++group) {
        const auto group_text=cstring(data,group_name,data.size()); group_name+=group_text.size()+1;
        const auto group_key=lower(group_text); order_.push_back(group_key);
        auto& table=groups_[group_key];
        std::size_t key_at=metadata+4+u32(data,metadata+4+group*8);
        const std::size_t length=u32(data,metadata+8+group*8);
        if (length>count-index) throw AssetError("RTXT group exceeds text table");
        for (std::size_t entry=0;entry<length;++entry,++index) {
            const auto key=cstring(data,key_at,data.size()); key_at+=key.size()+1;
            const auto offset=u32(data,16+index*16);
            if (offset>=metadata-text_base) throw AssetError("RTXT text offset outside text block");
            table.emplace(lower(key),cstring(data,text_base+offset,metadata));
            ++count_;
        }
    }
    if (index!=count) throw AssetError("RTXT group count differs from text count");
}
std::string RtxtTable::get(std::string_view group,std::string_view key) const {
    const auto found=groups_.find(lower(group)); if(found==groups_.end()) return {};
    const auto value=found->second.find(lower(key)); return value==found->second.end()?std::string{}:value->second;
}
std::string RtxtTable::get(std::string_view key) const {
    for(const auto& group:order_) { auto value=get(group,key); if(!value.empty()) return value; }
    return {};
}

FontAtlas scan_font(PcxImage image) {
    if(!image.width || !image.height || image.pixels.size()!=std::size_t(image.width)*image.height)
        throw AssetError("Invalid font atlas dimensions");
    FontAtlas out; out.image=std::move(image); auto& pixels=out.image.pixels;
    std::uint32_t start=0;
    for(unsigned row=0;row<16 && start<out.image.height;++row) {
        auto end=start;
        while(end<out.image.height && pixels[end*out.image.width]!=1) ++end;
        const auto height=end-start;
        if(height>255) throw AssetError("Font glyph height exceeds byte range");
        bool active=false; std::uint32_t glyph_x=0; unsigned column=0;
        for(std::uint32_t x=0;x<out.image.width;++x) {
            bool ink=false;
            for(std::uint32_t y=0;y<height;++y) {
                auto& pixel=pixels[(start+y)*out.image.width+x];
                if(pixel==3) { ink=true; pixel=0; }
                else if(pixel==2) { out.baseline=std::uint16_t(y); pixel=0; }
                else if(pixel>=8) ink=true;
            }
            if(ink && !active) { active=true; glyph_x=x; }
            if(!ink && active) {
                if(column>=16 || x>65535 || start>65535) throw AssetError("Font glyph grid exceeds 16 columns");
                out.glyphs[row*16+column]={std::uint16_t(glyph_x),std::uint16_t(x-glyph_x),
                                         std::uint8_t(height),1,std::uint16_t(start)};
                ++column; active=false;
            }
        }
        out.glyph_count=std::uint8_t(out.glyph_count+column);
        if(end==out.image.height) break;
        start=end+2;
    }
    return out;
}

struct MenuRuntime::Impl {
    const AssetStore& assets;
    MenuData data;
    RtxtTable text;
    std::unordered_map<std::string,PcxImage> images;
    std::unordered_map<std::string,FontAtlas> fonts;
    std::unordered_map<std::string,std::string> overrides;
    std::string current,hover,pressed;
    int pointer_x=0,pointer_y=0;
    std::vector<std::uint32_t> frame=std::vector<std::uint32_t>(640*480,0xff000000u);
    explicit Impl(const AssetStore& source):assets(source),data(parse_menu(source.read("df.mnu").bytes)),
        text(source.read("dfmenu.bin").bytes) {}
    MenuButton& button(std::string_view n) {
        auto i=data.buttons.find(lower(n)); if(i==data.buttons.end()) throw AssetError("Unknown menu button: "+std::string(n));
        return i->second;
    }
    const MenuDefinition& menu() const {return find(data.menus,current,"definition");}
    const PcxImage& image(std::string_view n) {
        const auto& definition=find(data.images,n,"image"); const auto key=lower(definition.filename);
        auto found=images.find(key); if(found==images.end()) found=images.emplace(key,assets.load_image(definition.filename).image).first;
        return found->second;
    }
    const FontAtlas& font(std::string_view n) {
        const auto& definition=find(data.fonts,n,"font"); const auto key=lower(definition.filename);
        auto found=fonts.find(key); if(found==fonts.end()) found=fonts.emplace(key,scan_font(assets.load_image(definition.filename).image)).first;
        return found->second;
    }
    std::string label(const MenuButton& b) const {
        const auto found=overrides.find(lower(b.name)); if(found!=overrides.end()) return found->second;
        return text.get(b.text_key);
    }
    int text_width(const FontAtlas& atlas,std::string_view value) const {
        int width=0;
        for(std::size_t i=0;i<value.size();) {
            if(value.substr(i,5)=="{hot}") {i+=5;continue;}
            if(value.substr(i,3)=="{C:") {const auto end=value.find('}',i);if(end!=std::string_view::npos){i=end+1;continue;}}
            const auto byte=std::uint8_t(value[i++]); const auto& g=atlas.glyphs[atlas.glyphs[byte].valid?byte:0]; width+=g.width+1;
        }
        return width;
    }
    void dimensions(MenuButton& b) {
        if(!b.image.empty()) {const auto& picture=image(b.image);b.width=int(picture.width);b.height=int(picture.height)/((b.type==1 && !(b.attributes&2048))?4:1);}
        else if(!b.font.empty()) {const auto& atlas=font(b.font);b.width=text_width(atlas,label(b));b.height=atlas.glyphs[65].height;}
    }
    bool enabled(const MenuButton& b) const {return b.state!=0 && b.state!=4 && !(b.attributes&2);}
    std::string hit(int x,int y) {
        for(const auto& n:menu().buttons) {
            auto& b=button(n);if(b.attributes&512)continue;dimensions(b);
            int left=b.x;
            if(b.image.empty() && !(b.attributes&32) && !(b.attributes&8)) left-=(b.attributes&16)?b.width:b.width/2;
            if(x>=left && y>=b.y && x<left+b.width && y<b.y+b.height) return b.name;
        }
        return {};
    }
    void emit(std::vector<MenuAction>& events,MenuButton& b,int action) {
        if(!b.handler.empty()) events.push_back({b.name,b.handler,action});
    }
    void blit(std::string_view name,int x,int y,int frame_index,int frames,bool blend,bool transparent,bool half_blend=false) {
        const auto& picture=image(name);
        if(frames<1 || frame_index<0 || frame_index>=frames || picture.height%unsigned(frames)) throw AssetError("Invalid menu image strip");
        const int height=int(picture.height)/frames;
        for(int row=0;row<height;++row)for(int col=0;col<int(picture.width);++col) {
            if(x+col<0 || x+col>=640 || y+row<0 || y+row>=480)continue;
            const auto pixel=picture.pixels[(row+frame_index*height)*picture.width+col];if(transparent && !pixel)continue;
            auto& target=frame[(y+row)*640+x+col]; const auto color=rgb(picture,pixel);target=half_blend?(0xff000000u|(((color&0xfefefeu)>>1)+((target&0xfefefeu)>>1))):(blend?strip_blend(color,target):color);
        }
    }
    void draw_text(const MenuButton& b,int x,int y,std::string_view value,std::uint32_t color) {
        const auto& atlas=font(b.font);
        bool has_hot=false;int hot_x=0;std::uint32_t hot_color=color;
        for(std::size_t index=0;index<value.size();) {
            if(value.substr(index,5)=="{hot}"){has_hot=true;hot_x=x;hot_color=color;index+=5;continue;}
            if(value.substr(index,3)=="{C:") {const auto end=value.find('}',index);if(end!=std::string_view::npos){
                const auto found=data.colors.find(lower(value.substr(index+3,end-index-3)));if(found!=data.colors.end())color=found->second;
                index=end+1;continue;}}
            const auto byte=std::uint8_t(value[index++]);const auto& g=atlas.glyphs[atlas.glyphs[byte].valid?byte:0];
            for(unsigned row=0;row<g.height;++row)for(unsigned col=0;col<g.width;++col) {
                const int xx=x+int(col),yy=y+int(row);if(xx<0||xx>=640||yy<0||yy>=480)continue;
                const auto pixel=atlas.image.pixels[(g.y+row)*atlas.image.width+g.x+col];if(!pixel)continue;
                auto& target=frame[yy*640+xx];target=tint(target,color,atlas.image.palette.at(std::size_t(pixel)*3)>>1);
            }
            x+=g.width+1;
        }
        if(has_hot)draw_text(b,hot_x,y+1,"_",hot_color);
    }
};

MenuRuntime::MenuRuntime(const AssetStore& assets):impl_(std::make_unique<Impl>(assets)){}
MenuRuntime::~MenuRuntime()=default;
MenuRuntime::MenuRuntime(MenuRuntime&&) noexcept=default;
MenuRuntime& MenuRuntime::operator=(MenuRuntime&&) noexcept=default;
const MenuData& MenuRuntime::definitions() const noexcept{return impl_->data;}
const RtxtTable& MenuRuntime::strings() const noexcept{return impl_->text;}
void MenuRuntime::select_menu(std::string_view n){find(impl_->data.menus,n,"definition");impl_->current=std::string(n);impl_->hover.clear();impl_->pressed.clear();}
const MenuDefinition& MenuRuntime::menu() const{return impl_->menu();}
MenuButton& MenuRuntime::button(std::string_view n){return impl_->button(n);}
std::string MenuRuntime::button_text(std::string_view n) const{return impl_->label(find(impl_->data.buttons,n,"button"));}
void MenuRuntime::set_button_text(std::string_view n,std::string text){impl_->button(n);impl_->overrides[lower(n)]=std::move(text);}
void MenuRuntime::set_button_state(std::string_view n,int state){if(state<0||state>4)throw AssetError("Invalid menu button state");impl_->button(n).state=state;}
void MenuRuntime::set_button_value(std::string_view n,int value){impl_->button(n).slider_position=value;}
std::string MenuRuntime::hit_test(int x,int y){return impl_->hit(x,y);}
std::vector<MenuAction> MenuRuntime::pointer_move(int x,int y) {
    impl_->pointer_x=x;impl_->pointer_y=y;std::vector<MenuAction> events;const auto hit=impl_->hit(x,y);
    if(lower(hit)!=lower(impl_->hover)) {
        if(!impl_->hover.empty()){auto& old=impl_->button(impl_->hover);if(impl_->enabled(old)){impl_->emit(events,old,7);}}
        impl_->hover=hit;
        if(!hit.empty()){auto& next=impl_->button(hit);if(impl_->enabled(next))impl_->emit(events,next,6);}
    }
    return events;
}
std::vector<MenuAction> MenuRuntime::pointer_down(int x,int y) {
    auto events=pointer_move(x,y);const auto hit=impl_->hit(x,y);impl_->pressed.clear();
    if(!hit.empty()){auto& b=impl_->button(hit);if(impl_->enabled(b)){
        if(!events.empty() && events.back().button==b.name && events.back().action==6)events.pop_back();
        impl_->pressed=hit;impl_->emit(events,b,1);impl_->emit(events,b,6);}}
    return events;
}
std::vector<MenuAction> MenuRuntime::pointer_up(int x,int y) {
    auto events=pointer_move(x,y);const auto hit=impl_->hit(x,y);
    if(!impl_->pressed.empty() && lower(hit)==lower(impl_->pressed)){auto& b=impl_->button(impl_->pressed);if(impl_->enabled(b)){
        if(!events.empty() && events.back().button==b.name && events.back().action==6)events.pop_back();
        impl_->emit(events,b,2);impl_->emit(events,b,6);}}
    impl_->pressed.clear();return events;
}
std::vector<MenuAction> MenuRuntime::key(int key) {
    const auto& m=impl_->menu();std::string target;
    if(key==13)target=m.accept_button;else if(key==27)target=m.back_button;
    else if(key==38)target=m.arrow_buttons[0];else if(key==40)target=m.arrow_buttons[1];
    else if(key==37)target=m.arrow_buttons[2];else if(key==39)target=m.arrow_buttons[3];
    else for(const auto& n:m.buttons){auto& b=impl_->button(n);int hotkey=b.hotkey;
        if(!hotkey){const auto value=impl_->label(b);const auto hot=value.find("{hot}");
            if(hot!=std::string::npos && hot+5<value.size())hotkey=std::toupper(static_cast<unsigned char>(value[hot+5]));}
        if(key && hotkey==key){target=n;break;}}
    std::vector<MenuAction> events;if(!target.empty()){auto& b=impl_->button(target);if(impl_->enabled(b)){impl_->emit(events,b,1);impl_->emit(events,b,2);}}
    return events;
}
std::vector<MenuAction> MenuRuntime::draw_actions() const {
    std::vector<MenuAction> events;
    for(const auto& name:impl_->menu().buttons){const auto& b=find(impl_->data.buttons,name,"button");
        if(b.state && (b.attributes&32) && !b.draw_handler.empty())events.push_back({b.name,b.draw_handler,3});}
    return events;
}
const std::vector<std::uint32_t>& MenuRuntime::render(){return render({});}
const std::vector<std::uint32_t>& MenuRuntime::render(const std::function<void(const MenuAction&)>& owner_draw) {
    auto& p=*impl_;const auto& m=p.menu();std::fill(p.frame.begin(),p.frame.end(),0xff000000u);
    if(!m.background.empty())p.blit(m.background,0,0,0,1,false,false);
    for(const auto& n:m.buttons) {
        auto& b=p.button(n);if(!b.state)continue;p.dimensions(b);
        if(owner_draw && (b.attributes&32) && !b.draw_handler.empty())owner_draw({b.name,b.draw_handler,3});
        int x=b.x,y=b.y;
        if(!b.image.empty() && b.type==1) {
            const int count=(b.attributes&2048)?1:4;const int cell=count==1?0:b.state-1;
            p.blit(b.image,x,y,cell,count,!(b.attributes&4096),bool(b.attributes&4096));
        } else if(!b.image.empty() && (b.type==2 || b.type==3) && b.state!=4 && !(b.attributes&2)) {
            if(b.type==2 && b.width)x+=int(double(b.slider_size)/b.width);
            if(b.type==3 && b.height)y+=int(double(b.slider_size)/b.height);
            p.blit(b.image,x,y,0,1,false,true);
        }
        if((b.attributes&64) && !b.font.empty()) {
            const auto& atlas=p.font(b.font);const auto value=p.label(b);const auto width=p.text_width(atlas,value);
            if(b.attributes&128) {
                if(b.attributes&8)x+=6;else if(b.attributes&16)x+=b.width-6;else x+=b.width/2;
                y+=(b.height-atlas.glyphs[65].height)/2;
            }
            if(b.attributes&16)x-=width;else if(!(b.attributes&8))x-=width/2;
            const auto& f=find(p.data.fonts,b.font,"font");
            const auto& c=b.state==4?f.disabled_color:((lower(p.hover)==lower(b.name))?f.lit_color:f.normal_color);
            p.draw_text(b,x,y,value,find(p.data.colors,c,"color"));
        }
    }
    return p.frame;
}
void MenuRuntime::draw_image(std::string_view n,int x,int y,int frame,int frames,bool blend,bool transparent,bool half_blend){impl_->blit(n,x,y,frame,frames,blend,transparent,half_blend);}
const FontAtlas& MenuRuntime::font_atlas(std::string_view name){return impl_->font(name);}
void MenuRuntime::draw_pcx(const PcxImage& image,int x,int y,bool transparent) {
    if(image.pixels.size()!=std::size_t(image.width)*image.height)throw AssetError("Invalid menu picture pixels");
    for(unsigned row=0;row<image.height;++row)for(unsigned col=0;col<image.width;++col) {
        const auto xx=std::int64_t(x)+col,yy=std::int64_t(y)+row;if(xx<0||xx>=640||yy<0||yy>=480)continue;
        const auto pixel=image.pixels[row*image.width+col];if(transparent && !pixel)continue;
        impl_->frame[std::size_t(yy)*640+std::size_t(xx)]=rgb(image,pixel);
    }
}
void MenuRuntime::draw_text(std::string_view font,int x,int y,std::string_view text,std::uint32_t color) {
    MenuButton b;b.font=std::string(font);impl_->draw_text(b,x,y,text,color);
}
void MenuRuntime::draw_line(int x1,int y1,int x2,int y2,std::uint32_t color) {
    if(std::abs(std::int64_t(x2)-x1)>10000||std::abs(std::int64_t(y2)-y1)>10000)throw AssetError("Menu line exceeds drawing limit");
    const int dx=std::abs(x2-x1),sx=x1<x2?1:-1,dy=-std::abs(y2-y1),sy=y1<y2?1:-1;int error=dx+dy;
    while(true){if(x1>=0&&x1<640&&y1>=0&&y1<480)impl_->frame[y1*640+x1]=0xff000000u|(((color&0xfefefeu)>>1)+((impl_->frame[y1*640+x1]&0xfeu)>>1));
        if(x1==x2&&y1==y2)break;const auto twice=error*2;if(twice>=dy){error+=dy;x1+=sx;}if(twice<=dx){error+=dx;y1+=sy;}}
}

} // namespace dfre
