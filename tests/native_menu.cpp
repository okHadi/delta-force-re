#include "dfre/menu.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
dfre::AssetBytes bytes(std::string_view value){return {value.begin(),value.end()};}
void put(dfre::AssetBytes& data,std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)data.at(at+i)=std::uint8_t(value>>(8*i));}
dfre::AssetBytes rtxt() {
    dfre::AssetBytes data(16+16+6,0);std::copy_n("RTXT",4,data.begin());put(data,4,data.size());put(data,12,1);
    std::copy_n("Hello",5,data.begin()+32);const auto meta=data.size();data.resize(meta+12);
    put(data,meta,1);put(data,meta+4,14);put(data,meta+8,1);
    const auto strings=bytes("Group");data.insert(data.end(),strings.begin(),strings.end());data.push_back(0);
    const auto key=bytes("Name");data.insert(data.end(),key.begin(),key.end());data.push_back(0);
    put(data,meta+4,14);put(data,8,data.size()-meta-4);
    return data;
}
void test_rtxt() {
    auto data=rtxt();dfre::RtxtTable table(data);require(table.size()==1,"RTXT count");require(table.get("group","NAME")=="Hello","RTXT group/key");
    require(table.get("missing").empty(),"RTXT missing key");
    put(data,16,1000);bool refused=false;try{dfre::RtxtTable bad(data);}catch(const dfre::AssetError&){refused=true;}require(refused,"RTXT bad offset");
}
void test_font() {
    dfre::PcxImage image;image.width=8;image.height=5;image.pixels.assign(40,0);image.palette.assign(768,0);
    image.pixels[1]=3;image.pixels[2]=20;image.pixels[9]=20;image.pixels[10]=20;image.pixels[16]=1;
    const auto atlas=dfre::scan_font(image);const auto g=atlas.glyphs[0];
    require(g.valid && g.x==1 && g.width==2 && g.height==2 && g.y==0,"Original font run bounds");
    require(atlas.image.pixels[1]==0 && image.pixels[1]==3,"Font marker copy");
}
void test_menu() {
    const auto data=dfre::parse_menu(bytes("RGB=C,0,200x,0\nFONT=F,f.pcx,C,C,C,C,C,C\n"
        "IMAGE=I,i.pcx,1,4,0,0,1\nBUTTON=B,1,192,T,Action,NULL,NULL,17,29,F,I,0,0,0,0,0,1,13,9,NULL\n"
        "MENU=M,I,F,NULL,B,NULL,B,NULL,NULL,NULL,NULL,Group\nMENUBUTTON=M,B,.STOP\n"));
    require(data.colors.at("c")==0xff00c800u,"Original numeric prefix");
    require(data.buttons.at("b").x==17 && data.buttons.at("b").state==1,"Button fields");
    require(data.menus.at("m").buttons==std::vector<std::string>{"B"},"Menu attachment order");
    bool refused=false;try{dfre::parse_menu(bytes("BUTTON=bad\n"));}catch(const dfre::AssetError&){refused=true;}require(refused,"Truncated menu row");
}
dfre::AssetBytes pcx(unsigned width,unsigned height,dfre::AssetBytes pixels) {
    dfre::AssetBytes data(128,0);data[0]=10;data[1]=5;data[2]=1;data[3]=8;data[65]=1;
    data[8]=std::uint8_t(width-1);data[9]=std::uint8_t((width-1)>>8);
    data[10]=std::uint8_t(height-1);data[11]=std::uint8_t((height-1)>>8);
    data[66]=std::uint8_t(width);data[67]=std::uint8_t(width>>8);
    for(auto pixel:pixels){if(pixel>=192)data.push_back(193);data.push_back(pixel);}
    data.push_back(12);
    for(unsigned i=0;i<256;++i)for(unsigned channel=0;channel<3;++channel)data.push_back(std::uint8_t(i));
    return data;
}
void archive(const std::filesystem::path& path,const std::vector<std::pair<std::string,dfre::AssetBytes>>& members) {
    dfre::AssetBytes data(20,0),records;put(data,0,20);std::copy_n("PFF3",4,data.begin()+4);
    put(data,8,members.size());put(data,12,32);
    for(const auto& member:members){const auto at=records.size();records.resize(at+32);put(records,at+4,data.size());
        put(records,at+8,member.second.size());std::copy(member.first.begin(),member.first.end(),records.begin()+at+16);
        data.insert(data.end(),member.second.begin(),member.second.end());}
    put(data,16,data.size());data.insert(data.end(),records.begin(),records.end());
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),data.size());
    require(bool(out),"Write menu fixture");
}
void test_runtime() {
    const auto folder=std::filesystem::temp_directory_path()/
        ("dfre-menu-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(folder);
    struct Cleanup {std::filesystem::path p;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(p,ignored);}} cleanup{folder};
    const auto config=bytes("RGB=G,0,255,0\nFONT=F,f.pcx,G,G,G,G,G,G\nIMAGE=BG,bg.pcx,1,1,0,0,1\n"
        "IMAGE=I,i.pcx,1,4,0,0,1\nBUTTON=B,1,192,Name,Action,NULL,NULL,17,29,F,I,0,0,0,0,0,1,65,9,Draw\n"
        "BUTTON=P,6,0,NULL,NULL,NULL,NULL,50,29,F,I,0,0,0,0,0,1,0,0,NULL\n"
        "MENU=M,BG,F,NULL,B,NULL,B,NULL,NULL,NULL,NULL,Group\nMENUBUTTON=M,B,P,.STOP\n");
    dfre::AssetBytes font(40,0);font[1]=255;font[9]=255;font[16]=1;
    archive(folder/"Df.pff",{});
    archive(folder/"Dfbase.pff",{{"DF.MNU",config},{"DFMENU.BIN",rtxt()},
        {"BG.PCX",pcx(640,480,dfre::AssetBytes(640*480,20))},
        {"I.PCX",pcx(8,8,dfre::AssetBytes(64,40))},{"F.PCX",pcx(8,5,font)}});
    const dfre::AssetStore store(folder);dfre::MenuRuntime runtime(store);runtime.select_menu("m");
    require(runtime.button_text("b")=="Hello","Original RTXT button label");
    require(runtime.hit_test(17,29)=="B" && runtime.hit_test(25,29).empty(),"Button exclusive right edge");
    require(runtime.hit_test(50,36)=="P" && runtime.hit_test(50,37).empty(),"Full-height panel hit area");
    require(runtime.hit_test(17,30)=="B" && runtime.hit_test(17,31).empty(),"Four-frame button hit area");
    auto actions=runtime.pointer_down(17,29);require(actions.size()==2 && actions[0].action==1 && actions[1].action==6,"Mouse down action order");
    actions=runtime.pointer_up(17,29);require(actions.size()==2 && actions[0].action==2,"Mouse release action");
    actions=runtime.key(13);require(actions.size()==2 && actions[0].handler=="Action" && actions[1].action==2,"Keyboard accept actions");
    runtime.set_button_state("B",4);require(runtime.key(13).empty(),"Disabled button input");
    runtime.set_button_state("B",1);runtime.set_button_text("B","Changed");require(runtime.button_text("B")=="Changed","Dynamic original label");
    require(runtime.render().size()==640*480 && runtime.render()[0]==0xff141414u,"Menu RGB background");
    runtime.set_button_text("B","H");
    require(runtime.render()[30*640+20]==0xff00fe00u,"Original MMX color carry");
}
}
int main(){try{test_rtxt();test_font();test_menu();test_runtime();std::cout << "menu checks passed\n";return 0;}catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}}
