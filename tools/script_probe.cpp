#include "dfre/scripts.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream source(argv[1],std::ios::binary);
    if(!source)return 1;
    dfre::AssetBytes bytes((std::istreambuf_iterator<char>(source)),{});
    const auto text=dfre::decode_game_script(bytes);
    std::cout.write(text.data(),text.size());
    return std::cout?0:1;
}
