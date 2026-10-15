#include "dfre/models.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv) {
    if(argc<2){std::cerr<<"Usage: models_probe MODEL...\n";return 2;}
    const bool origins=std::string(argv[1])=="--origins-json";
    if(origins && argc!=3){std::cerr<<"Usage: models_probe --origins-json MODEL\n";return 2;}
    bool failed=false;
    for(int i=origins?2:1;i<argc;++i)try {
        std::ifstream file(argv[i],std::ios::binary);
        if(!file)throw std::runtime_error("cannot open model");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        auto model=dfre::decode_model(bytes);
        if(origins) {
            std::cout<<"[";bool first_lod=true;
            for(const auto& raw:model.lods) {
                auto lod=dfre::localize_model_lod(raw);
                if(!first_lod)std::cout<<",";first_lod=false;
                std::cout<<"{\"vertices\":[";bool first=true;
                for(const auto& v:lod.vertices){if(!first)std::cout<<",";first=false;std::cout<<"["<<v.position[0]<<","<<v.position[1]<<","<<v.position[2]<<"]";}
                std::cout<<"],\"delta_origin\":[";first=true;
                for(const auto& m:lod.meshes){if(!first)std::cout<<",";first=false;std::cout<<"["<<m.delta_origin[0]<<","<<m.delta_origin[1]<<","<<m.delta_origin[2]<<"]";}
                std::cout<<"]}";
            }
            std::cout<<"]\n";continue;
        }
        std::size_t vertices=0,faces=0;
        for(const auto& lod:model.lods){vertices+=lod.vertices.size();faces+=lod.faces.size();}
        std::cout<<argv[i]<<"\t"<<unsigned(model.version)<<"\t"<<model.lods.size()<<"\t"<<vertices<<"\t"<<faces<<"\n";
    }catch(const std::exception& e){std::cerr<<argv[i]<<": "<<e.what()<<"\n";failed=true;}
    return failed?1:0;
}
