#include "dfre/crt_random.hpp"
#include <cassert>
#include <stdexcept>
int main(){std::uint32_t a=dfre::crt_random_initial_seed,b=7;dfre::CrtRandomBindings source;source.current_seed=[&]{return &a;};assert(dfre::crt_random_next(source)==16838&&a==0x41c67ea6);dfre::crt_random_seed(source,0xffffffff);assert(a==0xffffffff);source.current_seed=[&]{return &b;};const auto old=a;const auto draw=dfre::crt_random_next(source);assert(draw==((b>>16)&0x7fff)&&a==old);unsigned calls=0;source.current_seed=[&]()->std::uint32_t*{++calls;return nullptr;};assert(dfre::crt_random_next(source)==0);dfre::crt_random_seed(source,99);assert(calls==2&&a==old);source.current_seed={};bool failed=false;try{dfre::crt_random_next(source);}catch(const std::runtime_error&){failed=true;}assert(failed);}
