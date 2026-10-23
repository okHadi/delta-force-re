#include "dfre/mission_localization.hpp"
#include <iostream>
#include <sstream>
int main(){using U=std::uint32_t;U seed;while(std::cin>>seed){
    U table=seed%5?0x2000000u+seed*4:0,flags=(seed*17)&255,entry=seed%7?0x2100000:0,text=seed%11?0x2200000u+seed*16:0;
    std::ostringstream trace;dfre::MissionLocalizationBindings b;
    b.read=[&](U at,U width){trace<<'R'<<at<<','<<width<<';';if(at==0x503230)return table;if(at==0x67dbc9){if(seed%3==0)table^=0xff00;return flags;}if(at==entry)return text;throw std::runtime_error("Unowned localization read");};
    b.lookup_49a092=[&](U current,U group,U key){trace<<'C'<<current<<','<<group<<','<<key<<';';if(seed%13==0)text^=0xffff0000;return entry;};
    const auto value=dfre::mission_localized_text(b,0x12345678^seed,0x87654321+seed);
    std::cout<<value<<' '<<table<<' '<<trace.str()<<'\n';
}}
