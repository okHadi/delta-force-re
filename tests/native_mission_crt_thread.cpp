#include "dfre/mission_crt_thread.hpp"
#include <stdexcept>
namespace {void check(bool ok){if(!ok)throw std::runtime_error("CRT thread check failed");}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}}
int main(){using U=std::uint32_t;dfre::MissionCrtThreadCells cells;const U source=0x19000000;dfre::MissionSoundRuntimeBindings base;
    unsigned delegated=0;base.record_base=[&](U at,U width)->std::optional<U>{check(at==7&&width==1);++delegated;return 3;};
    base.record_read=[&](U at,U offset,U width){check(at==3&&offset==4&&width==1);++delegated;return 9u;};base.record_store=[&](U at,U offset,U width,U n){check(at==3&&offset==4&&width==1&&n==6);++delegated;};
    auto b=dfre::mission_crt_thread_bindings(base,cells,source);check(b.record_base(7,1)==3);check(b.record_read(3,4,1)==9);b.record_store(3,4,1,6);check(delegated==3);
    check(cells.seed==1&&cells.tokenizer_cursor==0);check(b.record_base(source+12,4)==source);check(b.record_read(source,12,4)==1);
    dfre::CrtRandomBindings random;random.current_seed=[&]{return &cells.seed;};check(dfre::crt_random_next(random)==16838);check(b.record_read(source,12,4)==1103527590u);
    b.record_store(source,12,4,0x12345678);b.record_store(source,16,4,0x90abcdef);check(cells.seed==0x12345678&&cells.tokenizer_cursor==0x90abcdef);
    check(b.record_read(source,15,2)==0xef12);b.record_store(source,15,2,0xa1b2);check(cells.seed==0xb2345678&&cells.tokenizer_cursor==0x90abcda1);
    for(U offset=12;offset<20;++offset)check(b.record_base(source+offset,1)==source);
    for(auto n:{0u,3u,5u,0xffffffffu}){rejects([&]{b.record_base(source+12,n);});rejects([&]{b.record_read(source,12,n);});rejects([&]{b.record_store(source,12,n,0);});}
    for(auto offset:{0u,11u,18u,19u,20u,0xffffffffu}){const auto before=cells;rejects([&]{b.record_read(source,offset,4);});rejects([&]{b.record_store(source,offset,4,0);});check(cells.seed==before.seed&&cells.tokenizer_cursor==before.tokenizer_cursor);}
    rejects([&]{b.record_base(source+11,2);});rejects([&]{b.record_base(source+19,2);});rejects([&]{b.record_base(0xffffffff,4);});rejects([&]{dfre::mission_crt_thread_bindings({},cells,0xfffffff0);});
}
