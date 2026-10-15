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
        r.range(pos,hs);ModelLod lod;lod.raw_header=r.copy(pos,hs);
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
            ModelMaterial m;m.name=r.name(pos);m.flags=r.u32(pos+16);m.frame_count=bytes[pos+31];m.timer=bytes[pos+30];m.texture_key=bytes[pos+((m.flags&0x8000)&&!(m.flags&1)?55:52)];std::copy_n(bytes.begin()+pos,76,m.raw.begin());std::copy_n(bytes.begin()+pos+40,3,m.flat_rgb.begin());
            if((m.flags&0x8001) && m.texture_key!=255)for(std::size_t j=0;j<model.textures.size();++j)if((model.textures[j].sequence&255)==m.texture_key)m.texture_candidates.push_back(j);
            lod.materials.push_back(std::move(m));
        }
        model.lods.push_back(std::move(lod));
    }
    require(pos==bytes.size(),"model has trailing data");
    return model;
}
}
