#include "dfre/models.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char** argv) {
    if(argc<2){std::cerr<<"Usage: models_probe MODEL...\n";return 2;}
    const bool matrix_cases=std::string(argv[1])=="--character-matrix-cases-json";
    const bool matrices=std::string(argv[1])=="--character-matrices-json";
    const bool prepare=std::string(argv[1])=="--character-prepared";
    const bool deltas=std::string(argv[1])=="--character-deltas-json";
    const bool character=std::string(argv[1])=="--character-poses-json";
    if((character||deltas||prepare||matrices||matrix_cases) && argc!=3){std::cerr<<"Usage: models_probe --character-poses-json CHR\n";return 2;}
    const bool origins=std::string(argv[1])=="--origins-json";
    if(origins && argc!=3){std::cerr<<"Usage: models_probe --origins-json MODEL\n";return 2;}
    bool failed=false;
    for(int i=(origins||character||deltas||prepare||matrices||matrix_cases)?2:1;i<argc;++i)try {
        std::ifstream file(argv[i],std::ios::binary);
        if(!file)throw std::runtime_error("cannot open model");
        std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
        if(matrix_cases) {
            auto chr=dfre::prepare_character_runtime(dfre::decode_character(bytes));std::cout<<"[";bool first=true;
            for(const auto& animation:chr.animations)for(std::uint32_t heading:{0u,1u,0x40000000u,0x7fffffffu,0x80000000u,0xffffffffu}) {
                auto pose=dfre::character_frame_pose(animation.frames[0]);
                for(std::size_t j=0;j<pose.size();++j)for(std::size_t k=0;k<3;++k)pose[j].angles[k]=(pose[j].angles[k]+std::int32_t(12345*(j+1)*(k+1)))&0x1ffffff;
                const auto world=dfre::model_local_matrix({1234567,-2345678,3456789},{0x12345678u,0x3456789au,0x56789abcu});
                const bool use_world=(heading&1)!=0;
                const auto values=dfre::character_bone_matrices(chr,pose,heading,use_world?&world:nullptr);
                if(!first)std::cout<<",";first=false;
                std::cout<<"{\"slot\":"<<animation.slot<<",\"frame\":0,\"heading\":"<<heading<<",\"pose\":[";bool first_bone=true;
                for(const auto& p:pose){if(!first_bone)std::cout<<",";first_bone=false;std::cout<<"["<<p.angles[0]<<","<<p.angles[1]<<","<<p.angles[2]<<","<<p.scale[0]<<","<<p.scale[1]<<","<<p.scale[2]<<"]";}
                std::cout<<"],\"world\":[";if(use_world)for(std::size_t k=0;k<world.size();++k){if(k)std::cout<<",";std::cout<<world[k];}
                std::cout<<"],\"matrices\":[";bool first_matrix=true;
                for(const auto& matrix:values){if(!first_matrix)std::cout<<",";first_matrix=false;std::cout<<"[";for(std::size_t k=0;k<matrix.size();++k){if(k)std::cout<<",";std::cout<<matrix[k];}std::cout<<"]";}
                std::cout<<"]}";
            }
            std::cout<<"]\n";continue;
        }
        if(prepare) {
            auto prepared=dfre::prepare_character_runtime(dfre::decode_character(bytes));
            std::cout.write(reinterpret_cast<const char*>(prepared.raw.data()),static_cast<std::streamsize>(prepared.raw.size()));continue;
        }
        if(character||deltas||matrices) {
            auto chr=dfre::decode_character(bytes);if(matrices)chr=dfre::prepare_character_runtime(chr);std::cout<<"[";bool first=true;
            for(const auto& animation:chr.animations)for(std::size_t j=0;j<animation.frames.size();++j) {
                if(!first)std::cout<<",";first=false;
                const auto target=dfre::character_frame_pose(animation.frames[j]);
                if(matrices) {
                    const auto values=dfre::character_bone_matrices(chr,target);
                    std::cout<<"{\"slot\":"<<animation.slot<<",\"frame\":"<<j<<",\"matrices\":[";bool first_matrix=true;
                    for(const auto& matrix:values){if(!first_matrix)std::cout<<",";first_matrix=false;std::cout<<"[";for(std::size_t k=0;k<matrix.size();++k){if(k)std::cout<<",";std::cout<<matrix[k];}std::cout<<"]";}
                    std::cout<<"]}";continue;
                }
                auto values=target;std::vector<dfre::CharacterBonePose> advanced;
                std::cout<<"{\"slot\":"<<animation.slot<<",\"frame\":"<<j;
                if(deltas) {
                    const auto& header=animation.frames[j].raw_header;
                    const auto duration=std::uint16_t(header[8]|(std::uint16_t(header[9])<<8));
                    const auto current=dfre::character_frame_pose(animation.frames[j?j-1:animation.frames.size()-1]);
                    values=dfre::character_pose_delta(current,target,duration);advanced=dfre::advance_character_pose(current,values);
                    std::cout<<",\"duration\":"<<duration<<",\"delta\":[";
                }else std::cout<<",\"pose\":[";
                auto write_pose=[](const auto& poses) {bool first_bone=true;for(const auto& pose:poses){if(!first_bone)std::cout<<",";first_bone=false;std::cout<<"["<<pose.angles[0]<<","<<pose.angles[1]<<","<<pose.angles[2]<<","<<pose.scale[0]<<","<<pose.scale[1]<<","<<pose.scale[2]<<"]";}};
                write_pose(values);std::cout<<"]";
                if(deltas){std::cout<<",\"advanced\":[";write_pose(advanced);std::cout<<"]";}
                std::cout<<"}";
            }
            std::cout<<"]\n";continue;
        }
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
