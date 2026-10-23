#include "dfre/crt_random.hpp"
#include <stdexcept>
namespace dfre {namespace {
std::uint32_t* cell(const CrtRandomBindings& b){if(!b.current_seed)throw std::runtime_error("CRT current thread seed is not bound");return b.current_seed();}
}
std::uint32_t crt_random_next(const CrtRandomBindings& b){auto* seed=cell(b);if(!seed)return 0;const auto next=*seed*0x41c64e6du+0x3039u;*seed=next;return(next>>16)&0x7fff;}
void crt_random_seed(const CrtRandomBindings& b,std::uint32_t value){if(auto* seed=cell(b))*seed=value;}
} // namespace dfre
