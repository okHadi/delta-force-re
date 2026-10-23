#include "dfre/mission_leave.hpp"
#include <cstring>
#include <stdexcept>
namespace dfre {namespace {
template<class T> const T& need(const T& f){if(!f)throw std::runtime_error("Mission leave reached a missing provider");return f;}
std::int32_t signed_word(std::uint32_t n){std::int32_t result;std::memcpy(&result,&n,4);return result;}
std::uint32_t half_signed(std::uint32_t n){return (n>>1)|(n&0x80000000u);}
}
void mission_leave(const MissionLeaveBindings& b){
    const auto read=[&](std::uint32_t at,std::uint32_t width){return need(b.read)(at,width);};
    const auto word=[&](std::uint32_t at){return std::uint32_t(read(at,4));};
    const auto write=[&](std::uint32_t at,std::uint32_t width,std::uint64_t n){need(b.write)(at,width,n);};
    const auto call=[&](std::uint32_t site,const std::vector<std::uint32_t>& args=std::vector<std::uint32_t>{}){return need(b.call)(site,args);};
    if(call(0x404540,{0x67dbf8})){
        const auto mode=word(0x67f41c);
        if(mode==8){
            const auto palette=word(0xbfc104);
            const auto table=palette+0xa00;
            if(palette){
                std::uint32_t row=0;
                const auto height=word(0x67f404);
                if(signed_word(height)>0){
                    auto row_pointer=std::uint32_t(0x67dbf8);
                    do {
                        auto pixel=word(row_pointer+4);
                        std::uint32_t column=0;
                        auto width=word(0x67f400);
                        if(signed_word(width)>0){
                            do {
                                const auto index=std::uint8_t(read(pixel,1));
                                const auto colour=std::uint8_t(read(table+index,1));
                                write(pixel,1,colour);
                                ++column;
                                width=word(0x67f400);
                                ++pixel;
                            }while(signed_word(column)<signed_word(width));
                        }
                        row_pointer+=4;
                        ++row;
                        const auto current_height=word(0x67f404);
                        if(signed_word(row)>=signed_word(current_height))break;
                    }while(true);
                }
            }
        }else if(mode==0x20){
            std::uint32_t row=0;
            const auto height=word(0x67f404);
            if(signed_word(height)>0){
                auto row_pointer=std::uint32_t(0x67dbf8);
                do {
                    auto pixel=word(row_pointer+4);
                    auto remaining=half_signed(word(0x67f400));
                    do {
                        const auto before=read(pixel,8);
                        write(pixel,8,(before&0xfefefefefefefefeull)>>1);
                        pixel+=8;
                        --remaining;
                    }while(remaining);
                    row_pointer+=4;
                    ++row;
                    const auto current_height=word(0x67f404);
                    if(signed_word(row)>=signed_word(current_height))break;
                }while(true);
            }
        }
        call(0x404610);
    }
    call(0x47a640);call(0x47a660);call(0x403a50);call(0x43e6d0);
    const auto rules=word(0x67dbe4);write(0x67f548,4,rules);
    call(0x401e70);call(0x4401e0);call(0x402d40);call(0x47a690);call(0x486660);call(0x47ec50);
    for(std::uint32_t offset=0;offset!=0x11400;offset+=0x114){
        const auto instance=word(0x7cca98+offset);
        if(instance)call(0x42cd30,{instance});
    }
    call(0x42c950);call(0x449620);call(0x482910);
}
}
