#include "dfre/models.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv) {
    if(argc<2){std::cerr<<"Usage: models_probe MODEL...\n";return 2;}
    bool failed=false;
    for(int i=1;i<argc;++i)try {
        std::ifstream file(argv[i],std::ios::binary);
        if(!file)throw std::runtime_error("cannot open model");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        auto model=dfre::decode_model(bytes);std::size_t vertices=0,faces=0;
        for(const auto& lod:model.lods){vertices+=lod.vertices.size();faces+=lod.faces.size();}
        std::cout<<argv[i]<<"\t"<<unsigned(model.version)<<"\t"<<model.lods.size()<<"\t"<<vertices<<"\t"<<faces<<"\n";
    }catch(const std::exception& e){std::cerr<<argv[i]<<": "<<e.what()<<"\n";failed=true;}
    return failed?1:0;
}
