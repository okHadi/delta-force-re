#include "dfre/mission_terrain_types.hpp"
#include <cstring>
namespace dfre {namespace {
using U=std::uint32_t;
std::int32_t si(U value){std::int32_t out;std::memcpy(&out,&value,4);return out;}
std::string lower(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return value;}
U decimal(const std::string& text){std::size_t i=0;while(i<text.size()&&(text[i]==' '||(text[i]>='\t'&&text[i]<='\r')))++i;const bool negative=i<text.size()&&text[i]=='-';if(i<text.size()&&(text[i]=='+'||text[i]=='-'))++i;U value=0;while(i<text.size()&&text[i]>='0'&&text[i]<='9'){value=value*10+U(text[i]-'0');++i;}return negative?0-value:value;}
}
void mission_terrain_classify(const MissionTerrainTypeBindings& b,U palette){
 if(!b.read||!b.write)throw AssetError("Terrain classification needs current memory providers");
 for(U color=0;color<256;++color,palette+=3){
  const auto red=b.read(palette,1),green=b.read(palette+1,1);
  const auto count=b.read(0xbd527c,4),blue=b.read(palette+2,1);
  U best=0x7fffffff,index=0,row=0,at=0xbd4f98;
  if(si(count)>0)for(;;){
   const auto dr=red-b.read(at,1),dg=green-b.read(at+1,1),db=blue-b.read(at+2,1);
   const auto distance=dr*dr+dg*dg+db*db;
   if(si(distance)<si(best)){best=distance;index=row;}
   const auto current=b.read(0xbd527c,4);++row;at+=4;
   if(si(row)>=si(current))break;
  }
  const auto current=b.read(0xbd527c,4);
  const auto type=current?b.read(0xbd4f9b+index*4,1):1;
  b.write(0xbd5334+color,1,type);
 }
}
std::array<std::uint8_t,256> mission_terrain_types(const PcxImage& image,const TerrainConfig& config){
 if(image.palette.size()!=768)throw AssetError("Terrain types need a 256-color RGB palette");
 std::array<std::uint8_t,404> rows{};U count=0;
 const char* names[]={"dirt","grass","snow","cement","sand","packed_dirt"};
 for(const auto& field:config.fields)if(lower(field.name)=="type"&&field.values.size()==4){
  for(U i=0;i<3;++i)rows[count*4+i]=std::uint8_t(decimal(field.values[i]));
  const auto name=lower(field.values[3]);for(U i=0;i<6;++i)if(name==names[i]){rows[count*4+3]=std::uint8_t(i+1);break;}
  if(count<100)++count;
 }
 std::array<std::uint8_t,256> output{};MissionTerrainTypeBindings b;
 b.read=[&](U at,U width)->U{if(at==0xbd527c&&width==4)return count;if(width==1&&at>=0x1200000&&at<0x1200300)return image.palette[at-0x1200000];if(width==1&&at>=0xbd4f98&&at<0xbd4f98+rows.size())return rows[at-0xbd4f98];throw AssetError("Terrain type read is outside its current rows");};
 b.write=[&](U at,U width,U value){if(width!=1||at<0xbd5334||at>=0xbd5434)throw AssetError("Terrain type write is outside its table");output[at-0xbd5334]=std::uint8_t(value);};
 mission_terrain_classify(b,0x1200000);return output;
}
}
