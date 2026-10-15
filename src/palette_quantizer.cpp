#include "dfre/palette_quantizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#if defined(_MSC_VER) && defined(_M_X64)
extern "C" double dfre_palette_variance_x87(const double* moments);
extern "C" std::int32_t dfre_palette_frequency_x87(double count,double reciprocal);
#endif

namespace dfre {
std::int32_t palette_frequency_weight(std::uint32_t count,std::uint32_t total) {
    if(!total || count>total || total>0x7fffffffu)throw std::invalid_argument("invalid palette frequency");
    const double reciprocal=1.0/double(total);
#if defined(_MSC_VER) && defined(_M_X64)
    return dfre_palette_frequency_x87(double(count),reciprocal);
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    const double samples=count,scale=65536.0;
    unsigned short saved,control;std::int32_t result;
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control=static_cast<unsigned short>((saved & 0xf0ffu)|0x0f00u);
    __asm__ volatile("fldcw %[control]\n\t" "fldl %[samples]\n\t"
        "fmull %[reciprocal]\n\t" "fmull %[scale]\n\t"
        "fistpl %[result]\n\t" "fldcw %[saved]"
        : [result] "=m"(result)
        : [samples] "m"(samples),[reciprocal] "m"(reciprocal),[scale] "m"(scale),
          [control] "m"(control),[saved] "m"(saved) : "st");
    return result;
#else
    return std::int32_t(std::trunc(static_cast<long double>(count)*reciprocal*65536));
#endif
}
namespace {
std::size_t bin(int r,int g,int b){return std::size_t(r)*4096+std::size_t(g)*64+std::size_t(b);}
double variance(const std::array<double,5>& value) {
    if(value[0]==0)return std::numeric_limits<double>::quiet_NaN();
#if defined(_MSC_VER) && defined(_M_X64)
    return dfre_palette_variance_x87(value.data());
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    double result;
    unsigned short saved,control;
    __asm__ volatile("fnstcw %0" : "=m"(saved));
    control=static_cast<unsigned short>((saved & 0xf0ffu)|0x0300u);
    __asm__ volatile(
        "fldcw %[control]\n\t"
        "fldl 16(%[values])\n\t" "fmull 16(%[values])\n\t"
        "fldl 24(%[values])\n\t" "fmull 24(%[values])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fldl 32(%[values])\n\t" "fmull 32(%[values])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fdivl 0(%[values])\n\t" "fsubrl 8(%[values])\n\t"
        "fstpl %[result]\n\t" "fldcw %[saved]"
        : [result] "=m"(result)
        : [values] "r"(value.data()),[control] "m"(control),[saved] "m"(saved)
        : "st");
    return result;
#else
    long double squared=static_cast<long double>(value[2])*value[2];
    squared+=static_cast<long double>(value[3])*value[3];
    squared+=static_cast<long double>(value[4])*value[4];
    return static_cast<double>(static_cast<long double>(value[1])-squared/value[0]);
#endif
}
bool splittable(const PaletteBox6& box){return box.minimum!=box.maximum;}
}
PaletteQuantizer6::PaletteQuantizer6():histogram_(64*64*64){}
void PaletteQuantizer6::clear() {
    std::fill(histogram_.begin(),histogram_.end(),0);active_=0;occupied_=maximum_=0;built_=false;
}
void PaletteQuantizer6::add_color(const std::array<std::uint8_t,3>& rgb,std::int32_t weight) {
    if(built_)throw std::invalid_argument("clear the quantizer before adding another bank");
    auto increment=std::int32_t(weight>=0?std::int64_t(weight)/4096:-((-std::int64_t(weight)+4095)/4096));
    if(!increment)increment=1;
    auto& value=histogram_[bin(rgb[0]>>2,rgb[1]>>2,rgb[2]>>2)];
    if(!value)++occupied_;
    value+=std::uint32_t(increment);maximum_=std::max(maximum_,value);
}
void PaletteQuantizer6::trim(PaletteBox6& box) const {
    const auto low=box.minimum,high=box.maximum;box.minimum=high;box.maximum=low;
    for(int r=low[0];r<=high[0];++r)for(int g=low[1];g<=high[1];++g)for(int b=low[2];b<=high[2];++b) {
        if(!histogram_[bin(r,g,b)])continue;
        const std::array<std::int32_t,3> point{r,g,b};
        for(std::size_t k=0;k<3;++k){box.minimum[k]=std::min(box.minimum[k],point[k]);box.maximum[k]=std::max(box.maximum[k],point[k]);}
    }
}
std::array<double,5> PaletteQuantizer6::moments(const PaletteBox6& box) const {
    std::array<double,5> result{};
    for(int r=box.minimum[0];r<=box.maximum[0];++r)for(int g=box.minimum[1];g<=box.maximum[1];++g)for(int b=box.minimum[2];b<=box.maximum[2];++b) {
        const auto weight=histogram_[bin(r,g,b)];if(!weight)continue;
        result[0]+=double(weight);
        result[2]+=double(std::uint32_t(r)*weight);result[3]+=double(std::uint32_t(g)*weight);result[4]+=double(std::uint32_t(b)*weight);
        result[1]+=(double(b*b)+double(g*g)+double(r*r))*double(weight);
    }
    return result;
}
void PaletteQuantizer6::split(std::size_t index) {
    const auto source=boxes_[index];
    std::array<double,3> scores{1e25,1e25,1e25};std::array<std::int32_t,3> cuts{1024,1024,1024};
    for(std::size_t axis=0;axis<3;++axis) {
        std::array<double,5> left{},right=source.moments;
        for(int value=source.minimum[axis];value<source.maximum[axis];++value) {
            auto slice=source;slice.minimum[axis]=slice.maximum[axis]=value;const auto part=moments(slice);
            for(std::size_t k=0;k<5;++k){left[k]+=part[k];right[k]-=part[k];}
            const double score=variance(left)+variance(right);
            if(score<scores[axis]){scores[axis]=score;cuts[axis]=value;}
        }
    }
    std::size_t axis=scores[1]<scores[0]?1:0;if(scores[2]<scores[axis])axis=2;
    if(cuts[axis]<source.minimum[axis] || cuts[axis]>=source.maximum[axis])throw std::runtime_error("quantizer could not split a color box");
    auto& left=boxes_[index];auto& right=boxes_[active_++];left=right=source;
    left.maximum[axis]=cuts[axis];right.minimum[axis]=cuts[axis]+1;trim(left);trim(right);
    left.moments=moments(left);
    for(std::size_t k=0;k<5;++k)right.moments[k]=source.moments[k]-left.moments[k];
    right.variance=variance(right.moments);left.variance=variance(left.moments);
    left.splittable=splittable(left);right.splittable=splittable(right);
}
std::vector<std::uint8_t> PaletteQuantizer6::build(std::size_t colors) {
    if(built_ || !colors || colors>256)throw std::invalid_argument("quantizer requires one bank of 1 through 256 colors");
    for(unsigned value=0;value<255;++value)add_color({std::uint8_t(value),std::uint8_t(value),std::uint8_t(value)});
    auto& first=boxes_[0];first.minimum={0,0,0};first.maximum={63,63,63};trim(first);first.moments=moments(first);first.variance=variance(first.moments);active_=1;
    std::size_t selected=0;
    while(active_<colors && selected<active_) {
        split(selected);
        for(std::size_t i=0;i+1<active_;++i)for(std::size_t j=i+1;j<active_;++j)if(boxes_[i].variance<boxes_[j].variance)std::swap(boxes_[i],boxes_[j]);
        selected=0;while(selected<active_ && !boxes_[selected].splittable)++selected;
    }
    std::vector<std::uint8_t> result;result.reserve(colors*3);
    for(std::size_t i=0;i<colors;++i)for(std::size_t k=0;k<3;++k)result.push_back(std::uint8_t(((boxes_[i].minimum[k]+boxes_[i].maximum[k])>>1)<<2));
    built_=true;return result;
}
}
