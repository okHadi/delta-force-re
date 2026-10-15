#include "dfre/models.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using Bytes=std::vector<std::uint8_t>;
void put(Bytes& b,std::size_t p,std::uint32_t v){for(int i=0;i<4;++i)b.at(p+i)=std::uint8_t(v>>(8*i));}
Bytes triangle(unsigned version) {
    const std::size_t hs=version==5?184:176,ms=version==2?88:(version==5?112:104),base=128+hs;
    Bytes b(base+24+8+72+ms+76);b[0]='3';b[1]='D';b[2]='I';b[3]=version;put(b,20,1);
    put(b,128+20,std::uint32_t(b.size()-base));put(b,128+128,3);put(b,128+136,1);put(b,128+144,1);put(b,128+152,1);put(b,128+168,1);
    b[base]=255;b[base+1]=255;b[base+8+2]=10;b[base+16+4]=20;
    const auto face=base+32;b[face+30]=1;b[face+32]=2;
    const auto mesh=face+72;put(b,mesh+4,3);put(b,mesh+12,1);put(b,mesh+20,1);
    return b;
}
Bytes character() {
    const std::size_t map=120,bone=124,table=216,animation=1236,frame=1256;
    Bytes b(1312);put(b,0,0x31524843);put(b,4,bone);put(b,8,1);put(b,12,map);put(b,16,1);put(b,20,table);put(b,24,1);
    put(b,bone+24,1);put(b,table,animation);put(b,animation,1);put(b,animation+4,1);
    put(b,frame+28,1311);put(b,frame+32,1311);b[frame+52]=255;b[frame+53]=128;b[frame+54]=1;
    return b;
}
void rejects_character(const Bytes& b){try{dfre::decode_character(b);std::cerr<<"accepted invalid character\n";std::exit(1);}catch(const std::invalid_argument&) {}}
void check(bool result,const char* message){if(!result){std::cerr<<message<<'\n';std::exit(1);}}
void rejects(const Bytes& b){try{dfre::decode_model(b);check(false,"accepted invalid model");}catch(const std::invalid_argument&) {}}
}
int main() {
    for(unsigned version: {2,3,4,5}) {
        const auto b=triangle(version);const auto m=dfre::decode_model(b);
        check(m.version==version && m.lods.size()==1,"version or LOD changed");
        check(m.lods[0].vertices[0].position[0]==-1,"signed vertex changed");
        check(m.lods[0].faces[0].vertices[2]==2,"triangle index changed");
        for(std::size_t n=0;n<b.size();++n)rejects(Bytes(b.begin(),b.begin()+n));
        auto bad=b;bad.push_back(0);rejects(bad);
        bad=b;put(bad,128+128,0xffffffff);rejects(bad);
        bad=b;put(bad,128+20,0xffffffff);rejects(bad);
        bad=b;const std::size_t face=128+(version==5?184:176)+32;bad[face+28]=3;rejects(bad);
        bad=b;bad[face+34]=1;rejects(bad);
        bad=b;put(bad,face+68,1);rejects(bad);
        bad=b;put(bad,bad.size()-76+16,0x4000);bad[bad.size()-76+30]=2;rejects(bad);
        bad[bad.size()-76+30]=1;bad[bad.size()-76+31]=127;const auto animated=dfre::decode_model(bad);
        check(animated.lods[0].materials[0].frame_count==1 && animated.lods[0].materials[0].animation_parameter==127,"material frame fields changed");
    }
    auto b=triangle(5);b[3]=8;rejects(b);b=triangle(5);b[0]=0;rejects(b);
    b=triangle(5);put(b,124,0xffffffff);rejects(b);b=triangle(5);put(b,20,0xffffffff);rejects(b);
    // Texture size and dimensions are separate fields; verify safe payload preservation.
    b=triangle(5);Bytes texture(1061);texture[0]='T';put(texture,16,1);texture[20]=7;texture[24]=1;texture[26]=1;texture[36]=42;texture[37]=123;
    b.insert(b.begin()+128,texture.begin(),texture.end());put(b,124,1);
    put(b,b.size()-76+16,1);b[b.size()-76+52]=7;
    auto m=dfre::decode_model(b);check(m.textures[0].pixels[0]==42 && m.textures[0].palette_rgb_pad[0]==123,"texture data changed");
    check(m.lods[0].materials[0].texture_candidates==std::vector<std::size_t>{0},"texture sequence binding changed");
    b[128+16]=255;b[128+17]=255;b[128+18]=255;b[128+19]=255;rejects(b);
    auto raw=dfre::decode_model(triangle(5)).lods[0];raw.flags=1;
    raw.meshes[0].origin={-257,2147483647,(-2147483647-1)};
    auto local=dfre::localize_model_lod(raw);
    check(local.origins_localized,"localized marker missing");
    try{dfre::localize_model_lod(local);check(false,"accepted repeated model localization");}catch(const std::invalid_argument&){}
    check(local.vertices[0].position==std::array<std::int16_t,3>{1,1,0},"signed origin shift or word wrap changed");
    check(local.meshes[0].delta_origin==std::array<std::int32_t,3>{0,0,0},"self parent delta changed");
    check(raw.vertices[0].position[0]==-1,"origin step changed its input");
    raw.meshes[0].parent_index=1;
    try{dfre::localize_model_lod(raw);check(false,"accepted invalid parent");}catch(const std::invalid_argument&){}
    raw.meshes[0].parent_index=0;raw.vertices[0].mesh=1;
    try{dfre::localize_model_lod(raw);check(false,"accepted invalid vertex mesh");}catch(const std::invalid_argument&){}
    raw.flags=0;local=dfre::localize_model_lod(raw);
    check(local.vertices[0].position==raw.vertices[0].position,"unflagged LOD changed");
    b=character();auto chr=dfre::decode_character(b);
    check(chr.bones.size()==1 && chr.bone_map[0]==0 && chr.animations.size()==1,"character table changed");
    const auto pose=dfre::character_frame_pose(chr.animations[0].frames[0]);
    check(pose[0].angles==std::array<std::int32_t,3>{33423360,16777216,131072} && pose[0].scale==std::array<std::int32_t,3>{65536,65536,65536},"packed character pose changed");
    for(std::size_t n=0;n<b.size();++n)rejects_character(Bytes(b.begin(),b.begin()+n));
    auto bad=b;put(bad,8,0xffffffff);rejects_character(bad);
    bad=b;put(bad,120,1);rejects_character(bad);
    bad=b;put(bad,20,0xffffffff);rejects_character(bad);
    bad=b;put(bad,124+12,0xffffffff);rejects_character(bad);
    bad=b;put(bad,1236,2);rejects_character(bad);
    bad=b;put(bad,1236+4,2);rejects_character(bad);
    bad=b;put(bad,1256+28,0xffffffff);rejects_character(bad);
    bad=b;put(bad,0,0x30524843);rejects_character(bad);
    std::vector<dfre::CharacterBonePose> current{{{33554431,0,0},{2147483647,(-2147483647-1),65536}}};
    std::vector<dfre::CharacterBonePose> target{{{0,33554431,16777216},{(-2147483647-1),2147483647,0}}};
    auto delta=dfre::character_pose_delta(current,target,2);
    check(delta[0].angles==std::array<std::int32_t,3>{0,0,-8388608},"25-bit shortest angle delta changed");
    check(delta[0].scale==std::array<std::int32_t,3>{0,0,-32768},"scale difference wrap or division changed");
    auto advanced=dfre::advance_character_pose(current,delta);
    check(advanced[0].angles[2]==25165824 && advanced[0].scale[2]==32768,"pose tick changed");
    delta[0].angles={2,-3,0};delta[0].scale={1,-1,0};advanced=dfre::advance_character_pose(current,delta);
    check(advanced[0].angles[0]==1 && advanced[0].angles[1]==33554429,"pose angle wrap changed");
    check(advanced[0].scale[0]==(-2147483647-1) && advanced[0].scale[1]==2147483647,"pose scale wrap changed");
    try{dfre::character_pose_delta(current,target,0);check(false,"accepted zero frame duration");}catch(const std::invalid_argument&){}
    try{dfre::character_pose_delta(current,{},1);check(false,"accepted different pose counts");}catch(const std::invalid_argument&){}
    try{dfre::advance_character_pose(current,{});check(false,"accepted different delta counts");}catch(const std::invalid_argument&){}
    b=character();b.resize(1604);put(b,124,1312);put(b,128,1516);put(b,124+12,3);put(b,124+16,1);
    put(b,1312,65536);put(b,1316,0xffffffff);put(b,1320,0xffff0000);
    put(b,1516,1312);put(b,1520,1380);put(b,1524,1448);put(b,1516+40,99);put(b,124+28,0xffffffff);
    auto source_chr=dfre::decode_character(b);auto prepared=dfre::prepare_character_runtime(source_chr);
    auto word=[](const Bytes& data,std::size_t pos){return std::uint32_t(data[pos])|(std::uint32_t(data[pos+1])<<8)|(std::uint32_t(data[pos+2])<<16)|(std::uint32_t(data[pos+3])<<24);};
    check(prepared.runtime_prepared && word(prepared.raw,1312)==0x1d2b0 && word(prepared.raw,1316)==0xfffffffe && word(prepared.raw,1320)==0xfffe2d50,"character fixed scale changed");
    check(word(prepared.raw,1516)==0 && word(prepared.raw,1520)==1 && word(prepared.raw,1524)==2 && word(prepared.raw,1556)==0,"character key preparation changed");
    check(source_chr.raw==b,"character preparation changed source bytes");
    try{dfre::prepare_character_runtime(prepared);check(false,"accepted repeated character preparation");}catch(const std::invalid_argument&){}
    put(source_chr.raw,1516,1313);
    try{dfre::prepare_character_runtime(source_chr);check(false,"accepted unaligned character key");}catch(const std::invalid_argument&){}
    const auto identity=dfre::model_identity_matrix();
    check(dfre::model_transform_point(identity,{-1,123,456})==std::array<std::int32_t,3>{-1,123,456},"identity point changed");
    auto translation=dfre::model_local_matrix({12,-34,56},{0,0,0});
    check(dfre::model_transform_point(translation,{1,2,3})==std::array<std::int32_t,3>{13,-32,59},"matrix translation changed");
    check(dfre::model_multiply_matrix(identity,translation)==translation,"identity multiplication changed");
    auto quarter=dfre::model_local_matrix({0,0,0},{0x40000000u,0,0});
    check(quarter[0]==0 && quarter[1]==-4194303 && quarter[4]==4194303 && quarter[5]==0,"original rotation table changed");
    check(dfre::fixed_sine_cosine(0)==std::array<std::int32_t,2>{0,4194303},"original sine table phase changed");
    auto tiny=identity;tiny[0]=-1;
    check(dfre::model_transform_point(tiny,{1,0,0})[0]==-1,"fixed multiplication rounding changed");
    dfre::CharacterModel skeleton;skeleton.runtime_prepared=true;skeleton.raw.resize(184);skeleton.bone_map={0,1};
    dfre::CharacterBone root;root.offset=0;dfre::CharacterBone child;child.offset=92;skeleton.bones={root,child};
    put(skeleton.raw,56,0xffffffff);put(skeleton.raw,92+56,0);put(skeleton.raw,92+28,256);put(skeleton.raw,92+32,512);put(skeleton.raw,92+36,768);
    std::vector<dfre::CharacterBonePose> zero_pose(2);auto skeleton_matrices=dfre::character_bone_matrices(skeleton,zero_pose);
    check(skeleton_matrices[1][3]==-768 && skeleton_matrices[1][7]==-256 && skeleton_matrices[1][11]==512,"bone hierarchy axes changed");
    dfre::ModelLod mesh_lod;mesh_lod.flags=1;mesh_lod.meshes.resize(2);
    mesh_lod.meshes[1].delta_origin={123,-456,789};
    try{dfre::character_mesh_matrices(skeleton,mesh_lod,zero_pose);check(false,"accepted raw mesh origins");}catch(const std::invalid_argument&){}
    mesh_lod.origins_localized=true;
    auto mesh_matrices=dfre::character_mesh_matrices(skeleton,mesh_lod,zero_pose);
    check(mesh_matrices[0][3]==0 && mesh_matrices[1][3]==123 && mesh_matrices[1][7]==-456 && mesh_matrices[1][11]==789,"mesh draw offsets changed");
    check(mesh_lod.meshes[1].delta_origin==std::array<std::int32_t,3>{123,-456,789},"mesh matrices changed input");
    mesh_lod.meshes.pop_back();
    try{dfre::character_mesh_matrices(skeleton,mesh_lod,zero_pose);check(false,"accepted different mesh counts");}catch(const std::invalid_argument&){}
    put(skeleton.raw,92+56,1);
    try{dfre::character_bone_matrices(skeleton,zero_pose);check(false,"accepted forward bone parent");}catch(const std::invalid_argument&){}
    skeleton.runtime_prepared=false;
    try{dfre::character_bone_matrices(skeleton,zero_pose);check(false,"accepted unprepared character matrices");}catch(const std::invalid_argument&){}
    std::cout<<"model bounds and geometry tests passed\n";
}
