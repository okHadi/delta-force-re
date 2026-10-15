#include "dfre/models.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace dfre {
namespace {
class Reader {
public:
    const std::vector<std::uint8_t>& b;
    explicit Reader(const std::vector<std::uint8_t>& bytes): b(bytes) {}
    void range(std::size_t p, std::size_t n) const {
        if(p>b.size() || n>b.size()-p) throw std::invalid_argument("model range outside file");
    }
    std::uint16_t u16(std::size_t p) const {range(p,2);return b[p]|(std::uint16_t(b[p+1])<<8);}
    std::int16_t i16(std::size_t p) const {auto v=u16(p);return v<=32767 ? std::int16_t(v) : std::int16_t(std::int32_t(v)-65536);}
    std::uint32_t u32(std::size_t p) const {range(p,4);return b[p]|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
    std::int32_t i32(std::size_t p) const {auto v=u32(p);return v<=2147483647 ? std::int32_t(v) : std::int32_t(std::int64_t(v)-4294967296LL);}
    std::string name(std::size_t p) const {range(p,12);auto end=std::find(b.begin()+p,b.begin()+p+12,0);return std::string(b.begin()+p,end);}
    std::vector<std::uint8_t> copy(std::size_t p,std::size_t n) const {range(p,n);return {b.begin()+p,b.begin()+p+n};}
};
void require(bool condition,const char* message) {if(!condition)throw std::invalid_argument(message);}
}
Model decode_model(const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);r.range(0,128);
    require(bytes[0]=='3' && bytes[1]=='D' && bytes[2]=='I',"unsupported model magic");
    Model model;model.version=bytes[3];model.name=r.name(4);
    require(model.version>=2 && model.version<=5,"unsupported model version");
    const std::size_t hs=model.version==5?184:176, ms=model.version==2?88:(model.version==5?112:104);
    auto lc=r.u32(20),tc=r.u32(124);
    require(lc<=bytes.size()/hs,"model LOD count outside file");
    require(tc<=(bytes.size()-128)/1060,"model texture count outside file");
    std::size_t pos=128;
    for(std::uint32_t i=0;i<tc;++i) {
        r.range(pos,36);auto size=r.u32(pos+16);r.range(pos+36,std::size_t(size)+1024);
        ModelTexture t;t.name=r.name(pos);t.sequence=r.u16(pos+20);t.flags=bytes[pos+22];t.width=r.u16(pos+24);t.height=r.u16(pos+26);
        t.pixels=r.copy(pos+36,size);std::copy_n(bytes.begin()+pos+36+size,1024,t.palette_rgb_pad.begin());
        model.textures.push_back(std::move(t));pos+=36+std::size_t(size)+1024;
    }
    for(std::uint32_t l=0;l<lc;++l) {
        r.range(pos,hs);ModelLod lod;lod.flags=r.u32(pos+16);lod.raw_header=r.copy(pos,hs);
        const auto vc=r.u32(pos+128),nc=r.u32(pos+136),fc=r.u32(pos+144),mc=r.u32(pos+152),hc=r.u32(pos+160),ac=r.u32(pos+168),cc=model.version==5?r.u32(pos+176):0;
        const std::uint64_t total=std::uint64_t(vc)*8+std::uint64_t(nc)*8+std::uint64_t(fc)*72+std::uint64_t(mc)*ms+std::uint64_t(hc)*12+std::uint64_t(cc)*72+std::uint64_t(ac)*76;
        require(total==r.u32(pos+20),"model LOD block size mismatch");
        require(total<=std::numeric_limits<std::size_t>::max(),"model block size overflow");
        pos+=hs;r.range(pos,static_cast<std::size_t>(total));
        for(std::uint32_t i=0;i<vc;++i,pos+=8)lod.vertices.push_back({{r.i16(pos),r.i16(pos+2),r.i16(pos+4)},r.u16(pos+6)});
        for(std::uint32_t i=0;i<nc;++i,pos+=8)lod.normals.push_back({{r.i16(pos),r.i16(pos+2),r.i16(pos+4)},r.u16(pos+6)});
        const auto face_start=pos;pos+=std::size_t(fc)*72;
        std::uint32_t vb=0,nb=0,fb=0;
        for(std::uint32_t i=0;i<mc;++i,pos+=ms) {
            ModelMesh m;m.vertex_start=vb;m.normal_start=nb;m.face_start=fb;m.vertex_count=r.u32(pos+4);m.face_count=r.u32(pos+12);m.normal_count=r.u32(pos+20);m.raw=r.copy(pos,ms);
            if(model.version==5) {m.has_v5_transform=true;m.parent_index=r.u32(pos+36);for(std::size_t k=0;k<3;++k){m.delta_origin[k]=r.i32(pos+40+k*4);m.origin[k]=r.i32(pos+52+k*4);}}
            require(m.vertex_count<=vc-vb && m.normal_count<=nc-nb && m.face_count<=fc-fb,"model mesh count outside geometry");
            for(std::uint32_t j=0;j<m.face_count;++j) {
                const auto p=face_start+std::size_t(fb+j)*72;ModelFace f{};f.flags=r.u16(p);f.face_normal=r.u16(p+2);f.material=r.u32(p+68);
                require(f.material<ac,"model face material outside table");
                for(std::size_t k=0;k<3;++k) {
                    const auto v=r.i16(p+28+k*2),n=r.i16(p+34+k*2);
                    require(v>=0 && std::uint32_t(v)<m.vertex_count,"model face vertex outside mesh");
                    require(n>=0 && std::uint32_t(n)<m.normal_count,"model face normal outside mesh");
                    f.vertices[k]=vb+v;f.normals[k]=nb+n;f.u[k]=r.i32(p+4+k*4);f.v[k]=r.i32(p+16+k*4);
                }
                lod.faces.push_back(f);
            }
            vb+=m.vertex_count;nb+=m.normal_count;fb+=m.face_count;lod.meshes.push_back(std::move(m));
        }
        require(vb==vc && nb==nc && fb==fc,"model mesh totals differ from geometry");
        for(std::uint32_t i=0;i<hc;++i,pos+=12)lod.hardpoints.push_back({r.i32(pos),r.i32(pos+4),r.i32(pos+8)});
        for(std::uint32_t i=0;i<cc;++i,pos+=72) {std::array<std::uint8_t,72> v;std::copy_n(bytes.begin()+pos,72,v.begin());lod.collision_volumes.push_back(v);}
        for(std::uint32_t i=0;i<ac;++i,pos+=76) {
            ModelMaterial m;m.name=r.name(pos);m.flags=r.u32(pos+16);m.frame_count=bytes[pos+30];m.animation_parameter=bytes[pos+31];
            require(!(m.flags&0x4000) || m.frame_count<=ac-i,"model material animation outside table");m.texture_key=bytes[pos+((m.flags&0x8000)&&!(m.flags&1)?55:52)];std::copy_n(bytes.begin()+pos,76,m.raw.begin());std::copy_n(bytes.begin()+pos+40,3,m.flat_rgb.begin());
            if((m.flags&0x8001) && m.texture_key!=255)for(std::size_t j=0;j<model.textures.size();++j)if((model.textures[j].sequence&255)==m.texture_key)m.texture_candidates.push_back(j);
            lod.materials.push_back(std::move(m));
        }
        model.lods.push_back(std::move(lod));
    }
    require(pos==bytes.size(),"model has trailing data");
    return model;
}
ModelLod localize_model_lod(const ModelLod& source) {
    ModelLod lod=source;
    if(!(lod.flags&1))return lod;
    for(const auto& mesh:lod.meshes) {
        require(mesh.has_v5_transform,"origin step requires v5 meshes");
        require(mesh.parent_index<lod.meshes.size(),"mesh parent outside table");
    }
    for(auto& vertex:lod.vertices) {
        require(vertex.mesh<32768 && vertex.mesh<lod.meshes.size(),"vertex mesh outside table");
        for(std::size_t k=0;k<3;++k) {
            const std::int64_t origin=lod.meshes[vertex.mesh].origin[k];
            const auto shifted=origin>=0?origin/256:-((-origin+255)/256);
            const auto wrapped=std::uint16_t(std::int64_t(vertex.position[k])-shifted);
            vertex.position[k]=wrapped<=32767?std::int16_t(wrapped):std::int16_t(std::int32_t(wrapped)-65536);
        }
    }
    for(auto& mesh:lod.meshes)for(std::size_t k=0;k<3;++k) {
        const auto value=std::uint32_t(mesh.origin[k])-std::uint32_t(lod.meshes[mesh.parent_index].origin[k]);
        mesh.delta_origin[k]=value<=2147483647?std::int32_t(value):std::int32_t(std::int64_t(value)-4294967296LL);
    }
    return lod;
}

CharacterModel decode_character(const std::vector<std::uint8_t>& bytes) {
    Reader r(bytes);r.range(0,120);
    require(r.u32(0)==0x31524843,"unsupported character magic");
    CharacterModel model;model.raw=bytes;
    const auto bone_count=r.u32(8),map_count=r.u32(16),map_offset=r.u32(12),animation_table=r.u32(20);
    model.declared_animation_count=r.u32(24);
    require(bone_count>0 && bone_count<=bytes.size()/60,"character bone count outside file");
    require(map_count>0 && map_count<=bytes.size()/4,"character bone map count outside file");
    r.range(map_offset,std::size_t(map_count)*4);r.range(animation_table,255*4);
    for(std::uint32_t i=0;i<map_count;++i) {
        auto index=r.u32(map_offset+std::size_t(i)*4);
        require(index<bone_count,"character bone map outside table");model.bone_map.push_back(index);
    }
    std::size_t pos=r.u32(4);
    for(std::uint32_t i=0;i<bone_count;++i) {
        r.range(pos,60);CharacterBone bone;bone.offset=static_cast<std::uint32_t>(pos);bone.flags=r.u32(pos+24);
        const auto stride=(bone.flags&1)?92:60;r.range(pos,stride);bone.raw=r.copy(pos,stride);
        const std::array<std::uint32_t,3> track_strides{68,88,24};
        for(std::size_t k=0;k<3;++k) {
            auto& track=bone.tracks[k];track.offset=r.u32(pos+k*4);track.count=r.u32(pos+12+k*4);track.stride=track_strides[k];
            require(track.count<=bytes.size()/track.stride,"character track count outside file");
            r.range(track.offset,std::size_t(track.count)*track.stride);
        }
        model.bones.push_back(std::move(bone));pos+=stride;
    }
    for(std::uint32_t slot=0;slot<255;++slot) {
        const auto offset=r.u32(animation_table+slot*4);if(!offset)continue;
        r.range(offset,20);const auto count=r.u32(offset),mapped=r.u32(offset+4);
        require(count>0 && count<=bytes.size()/52,"character frame count outside file");
        require(mapped==map_count,"character animation bone count differs from map");
        CharacterAnimation animation;animation.slot=slot;animation.offset=offset;std::copy_n(bytes.begin()+offset,20,animation.raw_header.begin());
        pos=std::size_t(offset)+20;
        for(std::uint32_t i=0;i<count;++i) {
            r.range(pos,52);const auto stride=r.u32(pos);const auto payload=std::size_t(map_count)*4;
            r.range(pos+52,payload);
            require((i+1==count && stride==0) || stride>=52+payload,"character frame stride is too small");
            if(stride)r.range(pos,stride);
            for(std::size_t field:{28,32,40}) {
                auto pointer=r.u32(pos+field);
                if(pointer || field!=40)r.range(pointer,1);
            }
            CharacterFrame frame;frame.offset=static_cast<std::uint32_t>(pos);frame.next_stride=stride;std::copy_n(bytes.begin()+pos,52,frame.raw_header.begin());
            for(std::uint32_t k=0;k<map_count;++k) {
                std::array<std::uint8_t,4> packed;std::copy_n(bytes.begin()+pos+52+std::size_t(k)*4,4,packed.begin());frame.packed_angles.push_back(packed);
            }
            animation.frames.push_back(std::move(frame));pos+=stride;
        }
        model.animations.push_back(std::move(animation));
    }
    return model;
}
std::vector<CharacterBonePose> character_frame_pose(const CharacterFrame& frame) {
    std::vector<CharacterBonePose> result;
    for(const auto& packed:frame.packed_angles) {
        CharacterBonePose pose;for(std::size_t k=0;k<3;++k){pose.angles[k]=std::int32_t(packed[k])<<17;pose.scale[k]=65536;}
        result.push_back(pose);
    }
    return result;
}

}
