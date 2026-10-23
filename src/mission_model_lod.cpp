#include "dfre/mission_model_lod.hpp"
#include <stdexcept>
namespace dfre {
namespace {
using U = std::uint32_t;
std::int64_t s(U v) { return v < 0x80000000u ? v : std::int64_t(v)-0x100000000ll; }
U sar(U v, unsigned n) { return (v >> n) | ((v & 0x80000000u) ? (~U(0) << (32-n)) : 0); }
}
U mission_model_lod_load(const MissionModelLodBindings& b,U args,U f) {
    if (!b.read || !b.write) throw std::runtime_error("model LOD memory provider required");
    if ((f&3u) || f>0xffffffffu-39u || args>0xffffffffu-11u)
        throw std::runtime_error("model LOD frame or argument range");
    auto r=[&](U a,unsigned w=4){return b.read(a,w);};
    auto w=[&](U a,U v,unsigned n=4){b.write(a,n,n==4?v:v&((U(1)<<(n*8))-1));};
    auto call=[&](U a,std::vector<U> v){if(!b.call) throw std::runtime_error("model LOD call provider required");return b.call(a,v);};
    U count=r(0xc65dcc);
    if(s(count)>600) return 0;
    U rec=0xc65dd0u+count*0xb8u;
    call(0x499280,{rec,0xb8});
    U size=r(rec+0x14);
    U allocation=call(0x497f60,{size,0x20,0,rec});
    w(rec+0x18,allocation);
    if(!allocation) return 0;
    size=r(rec+0x14);
    call(0x499280,{allocation,size});
    U n=r(rec+0x80), p=r(rec+0x18); w(rec+0x84,p); p+=n*8u;
    n=r(rec+0x88); U next=r(rec+0x90); w(rec+0x8c,p); p+=n*8u;
    w(rec+0x94,p); p+=next*0x48u;
    n=r(rec+0x98); w(rec+0x9c,p); next=r(rec+0xa0); p+=n*0x70u;
    w(rec+0xa4,p); n=r(rec+0xb0); p+=next*12u;
    w(rec+0xb4,p); p+=n*0x48u; w(rec+0xac,p);
    // Four separate source passes. Counts and bases remain current.
    const U source_fields[4]={0x84,0x94,0x8c,0xb4};
    const U dest_fields[4]={8,0x10,0x18,0x20};
    const U count_fields[4]={4,12,20,28};
    const U strides[4]={8,0x48,8,0x48};
    for(unsigned pass=0;pass<4;++pass){
        U limit=r(rec+0x98), i=0,total=0,off=0;
        if(s(limit)>0) do {
            U base=r(rec+source_fields[pass]); U value=base+total*strides[pass];
            base=r(rec+0x9c); w(base+off+dest_fields[pass],value);
            off+=0x70; base=r(rec+0x9c); U amount=r(base+off-0x70+count_fields[pass]);
            limit=r(rec+0x98); total+=amount; ++i;
        } while(s(i)<s(limit));
    }
    U limit=r(rec+0x98); w(f+0x14,0);
    if(s(limit)>0){
        w(f+8,0);
        do {
            U off=r(f+8); w(f+0x1c,off); U i=0,faceoff=0;
            for(;;){
                off=r(f+0x1c); U mesh=r(rec+0x9c)+off;
                if(s(i)>=s(r(mesh+12))) break;
                U faces=r(mesh+0x10); U index=r(faces+faceoff+0x44); U material=r(rec+0xac);
                faceoff+=0x48; ++i; w(faces+faceoff-4,material+index*0x4cu);
            }
            off=r(f+8); U i2=r(f+0x14); limit=r(rec+0x98);
            off+=0x70; ++i2; w(f+8,off); w(f+0x14,i2);
            if(s(i2)>=s(limit)) break;
        } while(true);
    }
    limit=r(rec+0xa8); w(f+0x10,0);
    if(s(limit)>0){
        w(f+0x18,0);
        do {
            U off=r(f+0x18); U material=r(rec+0xac)+off;
            if(r(material+0x11,1)&0x40u){
                if(!b.material_ticks) throw std::runtime_error("model LOD x87 block provider required");
                b.material_ticks(material,f+0x24);
                U converted=r(f+0x24); w(material+0x1c,converted,2);
            }
            U bank=r(args+8); U flags=r(material+0x10,1); w(material+0x1a,bank,2);
            if(flags&1u){
                U level=r(0xc80d10), index=r(args), end=r(args+4);
                U key=r(material+0x34+level,1);
                if(s(index)<s(end)){
                    U row=index*0x24u, stop=end*0x24u;
                    do {
                        U sequence=sar(r(row+0xc5449a),16);
                        if(key==sequence){
                            w(material+0x18,index,2);
                            U value=r(row+0xc544a4); w(material+0x44,value);
                            value=r(row+0xc544a8); w(material+0x48,value);
                            if(r(row+0xc5449e,1)&0x20u){value=r(material+0x11,1);w(material+0x11,value|2u,1);}
                            U descriptor=index*0x24u;
                            w(material+0x20,0,2); w(f+12,descriptor);
                            for(U bit=0;bit<16;++bit){
                                descriptor=r(f+12); U dimension=sar(r(descriptor+0xc5449e),16);
                                if(s(dimension)>=s(U(1)<<bit)) w(material+0x20,bit,2);
                            }
                            descriptor=index*0x24u; w(material+0x22,0,2); w(f+0x20,descriptor);
                            for(U bit=0;bit<16;++bit){
                                descriptor=r(f+0x20); U dimension=sar(r(descriptor+0xc544a0),16);
                                if(s(dimension)>=s(U(1)<<bit)) w(material+0x22,bit,2);
                            }
                            U held=r(f+0x18); w(material+0x24,0,2);
                            for(U bit=0;;++bit){
                                U base=r(rec+0xac); U bits=sar(r(base+held+0x1e),16);
                                if(s(bit)>=s(bits)) break;
                                U value2=r(material+0x24,2); w(material+0x24,value2|(U(1)<<(bit&31u)),2);
                            }
                            held=r(f+0x18); w(material+0x26,0,2);
                            for(U bit=0;;++bit){
                                U base=r(rec+0xac); U bits=sar(r(base+held+0x20),16);
                                if(s(bit)>=s(bits)) break;
                                U value2=r(material+0x26,2); w(material+0x26,value2|(U(1)<<(bit&31u)),2);
                            }
                            break;
                        }
                        row+=0x24; ++index;
                        if(s(row)>=s(stop)) break;
                    }while(true);
                }
                if(s(index)>=s(r(args+4))){U value=r(material+0x10,1);w(material+0x10,value&0xfeu,1);}
            }
            off=r(f+0x18); U i=r(f+0x10); limit=r(rec+0xa8);
            off+=0x4c; ++i; w(f+0x18,off); w(f+0x10,i);
            if(s(i)>=s(limit)) break;
        }while(true);
    }
    if(r(rec+0x10,1)&1u){
        U i=r(0xc80d18);
        if(!i){
            U limit2=r(rec+0x80), off=0;
            if(s(limit2)>0) do {
                U vertices=r(rec+0x84); U meshindex=sar(r(vertices+off+4),16);
                U mesh=r(rec+0x9c)+meshindex*0x70u;
                U x=sar(r(mesh+0x34),8); w(f,x); U y=r(mesh+0x38); U z=sar(r(mesh+0x3c),8); w(f+4,z);
                x=r(f); U value=r(vertices+off,2); w(vertices+off,value-x,2);
                vertices=r(rec+0x84); value=r(vertices+off+2,2); w(vertices+off+2,value-sar(y,8),2);
                vertices=r(rec+0x84); z=r(f+4); value=r(vertices+off+4,2); w(vertices+off+4,value-z,2);
                ++i; limit2=r(rec+0x80); off+=8;
            }while(s(i)<s(limit2));
            limit2=r(rec+0x98); i=0; off=0;
            if(s(limit2)>0) do {
                U base=r(rec+0x9c), mesh=base+off, parentoffset=r(mesh+0x24)*0x70u;
                w(f+0x24,base+parentoffset); U parent=r(f+0x24);
                U own=r(mesh+0x34), other=r(parent+0x34); w(mesh+0x28,own-other);
                base=r(rec+0x9c); mesh=base+off; w(f+0x24,base+parentoffset); parent=r(f+0x24);
                own=r(mesh+0x38); other=r(parent+0x38); w(mesh+0x2c,own-other);
                base=r(rec+0x9c); mesh=base+off; parent=base+parentoffset;
                own=r(mesh+0x3c); other=r(parent+0x3c); w(mesh+0x30,own-other);
                ++i; limit2=r(rec+0x98); off+=0x70;
            }while(s(i)<s(limit2));
        }
    }
    count=r(0xc65dcc); w(0xc65dcc,count+1); return rec;
}
}
