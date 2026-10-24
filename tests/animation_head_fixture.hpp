#pragma once
#include "dfre/animation.hpp"
#include <cstdint>
#include <exception>
#include <sstream>
namespace animation_head_fixture {
using U=std::uint32_t;
inline void word(std::array<std::uint8_t,52>& b,U at,U n){for(U i=0;i<4;++i)b.at(at+i)=std::uint8_t(n>>(8*i));}
inline dfre::CharacterModel model(U duration,U last_y){dfre::CharacterModel m;m.runtime_prepared=true;m.raw.resize(0x200);m.bone_map={0,1,2};for(U slot=0;slot<2;++slot){dfre::CharacterAnimation a;a.slot=slot;a.offset=slot?0x20:0;a.raw_header[8]=1;dfre::CharacterFrame f;f.offset=slot?0x80:0x40;word(f.raw_header,8,slot?duration:8);word(f.raw_header,0x20,0x100);if(slot)word(f.raw_header,4,0x80);for(U i=0;i<3;++i)f.packed_angles.push_back({std::uint8_t(slot?i+1:0),std::uint8_t(slot?(i==2?last_y:i+2):0),std::uint8_t(slot?i+3:0),0});a.frames.push_back(f);m.animations.push_back(a);}return m;}
inline std::string run(U duration,U pitch,U last_y){auto m=model(duration,last_y);dfre::AnimationController c(m);dfre::AnimationInputs input;input.pitch=std::uint8_t(pitch);c.tick(input);bool failed=false;try{c.select_action(1,0);}catch(const std::exception&){failed=true;}std::ostringstream out;out<<failed<<' '<<c.frame_ticks();for(const auto& p:c.pose_delta()){for(auto v:p.angles)out<<' '<<U(v);for(auto v:p.scale)out<<' '<<U(v);}for(auto v:c.root_delta().front())out<<' '<<U(v);for(auto v:c.motion())out<<' '<<U(v);return out.str();}
}
