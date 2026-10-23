#include "dfre/mission_wet_effect.hpp"
#include <array>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
using U=std::uint32_t;
constexpr U frame=0x2007ee4,definition=0x1210000;
struct Fixture {
    std::map<U,std::uint8_t> memory;
    std::array<U,14> input{};
    std::string trace;
    U reads=0,writes=0,calls=0,randoms=0;
    U get(U at)const{U n=0;for(U i=0;i<4;++i){const auto p=memory.find(at+i);if(p!=memory.end())n|=U(p->second)<<(8*i);}return n;}
    void put(U at,U n){for(U i=0;i<4;++i)memory[at+i]=std::uint8_t(n>>(8*i));}
    void mutate_read(U at){if((input[10]&8)&&at==definition+0x84)put(definition+0x90,get(definition+0x90)^0x12345678);if((input[10]&16)&&at==frame+0x124)put(frame+0x128,get(frame+0x128)^0x87654321);}
    dfre::MissionWetEffectBindings bindings(){return {
        [&](U at,U width){if(width!=4)throw std::runtime_error("Unexpected read width");trace+='R'+std::to_string(at)+';';if(++reads==input[11])throw std::runtime_error("Read failure");mutate_read(at);return get(at);},
        [&](U at,U width,U value){if(width!=4)throw std::runtime_error("Unexpected store width");trace+='W'+std::to_string(at)+','+std::to_string(value)+';';if(++writes==input[12])throw std::runtime_error("Store failure");put(at,value);},
        [&](U site,const std::array<U,3>& args){trace+='C'+std::to_string(site);for(auto n:args)trace+=','+std::to_string(n);trace+=';';if(++calls==input[13])throw std::runtime_error("Call failure");
            if(site==0x49a620){for(U i=0;i<args[2];++i)memory[args[0]+i]=std::uint8_t(args[1]);if(input[10]&1){put(frame+0x124,get(frame+0x124)^0xabcdef01);put(frame+0x128,get(frame+0x128)^0x13579bdf);put(frame+0x12c,get(frame+0x12c)^0x2468ace0);put(0xc23810,0x1230000);put(0x8e44b0+input[0]*4,17);put(definition+0x6c,get(definition+0x6c)^0xfedcba98);}return args[0];}
            if(site==0x477860){++randoms;if(randoms==1&&(input[10]&2)){put(definition+0x84,get(definition+0x84)^0x55555555);put(definition+0x90,get(definition+0x90)^0xaaaaaaaa);put(frame+4,0x87654321);put(0xc23810,0x1234000);}if(randoms==2&&(input[10]&4)){put(definition+0x84,get(definition+0x84)^0xffffffff);put(definition+0x90,get(definition+0x90)^0xffffffff);put(frame+0x30,0x13579bdf);}return input[7+randoms];}
            if(site==0x45d970){if(input[10]&32)put(frame+0x24,get(frame+0x24)^0xffffffff);return 0x822e80u;}
            throw std::runtime_error("Unknown required body");}
    };}
    void initialize(){for(U i=0;i<0x114;++i)memory[frame+i]=0xa5;for(U i=0;i<4;++i)put(frame+0x120+4*i,input[i==0?0:i+1]);put(0x8e44b0+input[0]*4,input[1]);put(0xc23810,definition-input[1]*0x13c);put(definition+0x6c,input[5]);put(definition+0x84,input[6]);put(definition+0x90,input[7]);}
};
}
int main(){Fixture f;while(std::cin>>f.input[0]){for(std::size_t i=1;i<f.input.size();++i)std::cin>>f.input[i];f.memory.clear();f.trace.clear();f.reads=f.writes=f.calls=f.randoms=0;f.initialize();bool failed=false;try{dfre::mission_wet_effect(f.bindings(),frame);}catch(const std::runtime_error&){failed=true;}std::cout<<failed<<' '<<f.reads<<' '<<f.writes<<' '<<f.calls<<' ';for(U i=0;i<0x114;i+=4)std::cout<<f.get(frame+i)<<',';std::cout<<' '<<f.trace<<'\n';}}
