#include "dfre/mission_post_menu.hpp"
namespace dfre {
namespace {
using U=std::uint32_t;
struct Access {
    const MissionPostMenuBindings& b;
    U r(U at)const{if(!b.read)throw MissionPostMenuError("Post Menu reader is not bound");return b.read(at,4);}
    void w(U at,U value)const{if(!b.write)throw MissionPostMenuError("Post Menu writer is not bound");b.write(at,4,value);}
    U call(U at,std::initializer_list<U> args={})const{if(!b.call)throw MissionPostMenuError("Post Menu body is not bound");return b.call(at,args);}
};
}
void mission_post_menu_enter(const MissionPostMenuBindings& b){
    const Access m{b};m.call(0x407850);m.call(0x40d550);
    const U menu=m.call(0x40bfd0,{0x4cb4e0});m.w(0x68d530,menu);
    m.call(0x40bca0,{menu});m.call(0x40e4d0,{m.r(0x68d530)});
    m.call(0x422ef0,{1});m.call(0x4230a0);m.call(0x40dd80,{1,0});
    if(!m.r(0x67f53c))m.call(0x42af00);
    m.call(0x40b960);
}
U mission_post_menu_update(const MissionPostMenuBindings& b){
    const Access m{b};const U status=m.r(0x67dbbc);
    if(status==2){m.call(0x461340,{0});return 1;}
    if(status==5){const U row=m.call(0x49a092,{m.r(0x50322c),0x4cb4fc,0x4cb4ec});const U text=m.r(row);m.call(0x401d00,{0,text});m.call(0x461340,{0});return 1;}
    if(status==1){
        const U multiplayer=m.r(0x67f520);
        if(!multiplayer){m.call(0x4c4f40,{multiplayer,status});m.call(0x461340,{0x4e7a58});}
        else{const U host=m.r(0x67f528);m.call(0x461340,{host?0x4e6608u:0x4d6b78u});m.call(0x4c4f40,{0,2});}
        m.call(0x41e2e0);return 1;
    }
    if(m.r(0x67f520)&&m.r(0x67f528)){
        const U mode=m.r(0x67f534);
        if(mode==1){m.call(0x461340,{0x4e6878});return mode;}
        if(mode==2){m.call(0x407fd0);m.call(0x461340,{0x4e6878});return 1;}
        (void)m.r(0x67f53c);m.call(0x461340,{0x4e6608});
    }else m.call(0x461340,{0x4cf2ec});
    m.call(0x41e2e0);return 1;
}
void mission_post_menu_render(const MissionPostMenuBindings& b){
    const Access m{b};if(!m.r(0x68d530))return;
    if(m.r(0x4e9bf0)&&!m.r(0x4e9bdc)){m.call(m.r(0x4e9c18));m.call(0x496c50);return;}
    m.call(0x40fa90,{m.r(0x68d530)});
    if(m.r(0x4e9bf0))m.call(0x496c50);else m.call(0x4230e0);
}
void mission_post_menu_leave(const MissionPostMenuBindings&){}
}
