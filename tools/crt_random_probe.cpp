#include "dfre/crt_random.hpp"
#include <iostream>
int main(){std::uint32_t a,b,mode,value;while(std::cin>>a>>b>>mode>>value){unsigned reads=0;dfre::CrtRandomBindings binding;binding.current_seed=[&]()->std::uint32_t*{const auto n=reads++;if(mode&4)return nullptr;auto* p=((mode&1)&&n%2)?&b:&a;if(mode&2)*p^=0x89abcdef;return p;};const auto first=dfre::crt_random_next(binding);dfre::crt_random_seed(binding,value);const auto second=dfre::crt_random_next(binding);std::cout<<a<<' '<<b<<' '<<first<<' '<<second<<' '<<reads<<'\n';}}
