#include "dfre/items.hpp"
#include "dfre/scripts.hpp"
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void invalid(std::string_view text){try{dfre::ItemCatalog catalog(text);}catch(const dfre::AssetError&){return;}throw std::runtime_error("Invalid item definitions accepted");}
}
int main(){
    check(dfre::decode_game_script({}).empty(),"Empty script changed");
    check(dfre::decode_game_script({0})==std::string(1,'\x6a'),"One-byte script changed");
    check(dfre::decode_game_script({0,1})==std::string("\x6b\xa8",2),"Script reversal changed");
    const dfre::ItemCatalog catalog("/ header\r\nBEGIN \"first item\"\nid 100000\ntype marker\nend\n"
        "begin second\nid 101001\ntype decoration\ngraphic barrel / comment\nhp 4 0\nattrib: a b\nattrib: c\nend\n");
    check(catalog.entries().size()==2 && catalog.find(0).name=="first item","Item identity changed");
    const auto& item=catalog.find(1001);
    check(item.type=="decoration" && item.value("graphic")=="barrel" && item.value("missing").empty(),"Item graphic binding changed");
    check(item.fields.back().key=="attrib:" && item.fields.back().values==std::vector<std::string>{"c"},"Repeated item fields changed");
    check(!catalog.find_exact(999) && catalog.find(999).id==0,"Original missing-item fallback changed");
    for(const auto* bad:{"", "id 1", "begin x\nid 1\nend", "begin x\nid -1\ntype marker\nend", "begin x\nid 1x\ntype marker\nend",
        "begin x\nid 1\nid 2\ntype marker\nend", "begin x\nid 1\ntype marker", "begin \"x", "begin x\nbegin y",
        "begin x\nid 1\ntype marker\nend\nbegin y\nid 1\ntype marker\nend", "begin \"x\"tail\nid 1\ntype marker\nend"})invalid(bad);
    std::cout<<"item and script checks passed\n";
}
