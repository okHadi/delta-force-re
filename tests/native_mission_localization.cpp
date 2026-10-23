#include "dfre/mission_localization.hpp"
#include <stdexcept>
namespace {void check(bool value){if(!value)throw std::runtime_error("Localization check failed");}}
int main(){
    using U=std::uint32_t;dfre::MissionLocalizationBindings b;U table=0,flags=0,entry=0,text=0,reads=0,calls=0;
    b.read=[&](U at,U width){++reads;if(at==0x503230){check(width==4);return table;}if(at==0x67dbc9){check(width==1);table=9;return flags;}check(at==entry&&width==4);return text;};
    check(dfre::mission_localized_text(b,1,2)==0x4ced16&&reads==1);
    table=7;flags=0x20;check(dfre::mission_localized_text(b,1,2)==0x4ced14&&reads==3);
    table=7;flags=0;bool failed=false;try{dfre::mission_localized_text(b,1,2);}catch(const std::runtime_error&){failed=true;}check(failed&&table==9);
    b.lookup_49a092=[&](U current,U group,U key){check(current==7&&group==1&&key==2);++calls;return entry;};
    table=7;check(dfre::mission_localized_text(b,1,2)==0x4ced16&&calls==1);
    entry=0x2200000;table=7;check(dfre::mission_localized_text(b,1,2)==0x4ced16&&calls==2);
    text=0x2300000;table=7;check(dfre::mission_localized_text(b,1,2)==text&&calls==3);
}
