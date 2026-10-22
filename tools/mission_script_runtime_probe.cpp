#include "dfre/mission_script_runtime.hpp"
#include <iostream>
#include <map>
#include <sstream>
using namespace dfre;
namespace {
constexpr std::uint32_t resource=0x1100000,names=0x1110000;
const std::array<std::string,8> text_names{"radio_123","root","number_+42","radio_-7","radio_1x","","_","long_last_42"};
void run(const std::string& mode,std::istream& in){
    MissionScriptQueueCells queue{};std::uint32_t host=0;MissionScriptRuntimeCells cells{queue,host};MissionScriptRuntimeBindings b;
    std::map<std::uint32_t,std::uint8_t> memory;std::ostringstream trace;
    const auto put=[&](std::uint32_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)memory[at+i]=std::uint8_t(value>>(i*8));};
    const auto string=[&](std::uint32_t at,const std::string& value){for(auto byte:value)memory[at++]=std::uint8_t(byte);memory[at]=0;};
    b.byte=[&](auto at){return memory[at];};b.word=[&](auto at){std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(memory[at+i])<<(i*8);return value;};
    b.string=[&](auto at){std::string value;while(memory[at])value+=char(memory[at++]);return value;};
    MissionScriptRuntime runtime(cells,b);
    if(mode=="step"){
        std::uint32_t index,sample,name,dictionary,found,accept,mutation,mode_byte,text_value;std::array<std::uint32_t,3> position,args;
        in>>index>>sample>>name>>dictionary>>found>>accept>>mutation>>mode_byte>>text_value;
        for(auto& n:position)in>>n;for(auto& n:args)in>>n;
        const auto base=resource+0x34+index*0x44;string(base+4,"voice.wav");string(base+0x1c,text_names.at(name));memory[base+0x34]=std::uint8_t(mode_byte);
        for(unsigned i=0;i<3;++i)put(base+0x38+i*4,position[i]);cells.dictionary_8e3f3c=dictionary;
        b.load_audio=[&](auto at){trace<<"L,"<<at<<';';return sample;};
        b.missing_audio=[&](auto at){trace<<"M,"<<at<<';';};
        b.parse_id=[&](auto at){trace<<"I,"<<at<<';';return std::uint32_t(std::stoll(b.string(at)));};
        b.text=[&](auto table,auto id)->std::optional<std::uint32_t>{trace<<"T,"<<table<<','<<id<<';';if(found)return text_value;return std::nullopt;};
        b.step=[&](auto& value){trace<<"C,"<<value.text<<','<<value.sample<<','<<unsigned(value.mode);for(auto n:value.position)trace<<','<<n;trace<<';';
            if(mutation&1)value.sample+=3;if(mutation&2)++value.text;value.playback=args;return accept!=0;};
        b.play=[&](auto value,const auto& parameters){trace<<"P,"<<value;for(auto n:parameters)trace<<','<<n;trace<<';';if(mutation&4)cells.voice_count_8e3d50=2;return 91u;};
        b.show_text=[&](auto value){trace<<"S,"<<value<<','<<cells.voice_count_8e3d50<<';';};runtime.step(resource,index);
        std::cout<<cells.voice_count_8e3d50;for(unsigned i=0;i<3;++i)std::cout<<' '<<cells.voices[i].handle<<' '<<cells.voices[i].name;
    }else if(mode=="announce"){
        std::uint32_t index,name;in>>host>>index>>name;string(names,text_names.at(name));
        b.message=[&](auto id,const auto& payload){trace<<id;for(auto byte:payload)trace<<','<<unsigned(byte);trace<<';';};runtime.announce(names,std::uint16_t(index));std::cout<<0;
    }else if(mode=="remove"){
        std::uint32_t count,wanted;in>>count>>wanted;
        for(std::uint32_t i=0;i<17;++i)string(names+i*32,"name"+std::to_string(i%5));
        for(std::uint32_t i=0;i<count;++i){MissionScriptQueueEntry entry;in>>entry.resource>>entry.index>>entry.delay>>entry.armed;entry.resource=names+entry.resource*32-4;queue.entries_8e3d74.push_back(entry);}
        std::cout<<runtime.remove(names+wanted*32)<<' '<<queue.entries_8e3d74.size();for(const auto& entry:queue.entries_8e3d74)std::cout<<' '<<entry.resource<<' '<<entry.index<<' '<<entry.delay<<' '<<entry.armed;
    }else if(mode=="cleanup"){
        std::uint32_t mutation;in>>cells.voice_count_8e3d50>>mutation;std::array<MissionScriptAudioCache,4> cache;
        for(std::uint32_t i=0;i<16;++i)string(names+i*32,"name"+std::to_string(i%4));
        for(std::uint32_t i=0;i<cells.voice_count_8e3d50;++i){std::uint32_t name;in>>cells.voices[i].handle>>name;cells.voices[i].name=names+name*32;}
        for(auto& value:cache){unsigned loaded;in>>loaded>>value.allocation;value.loaded=std::uint8_t(loaded);}
        bool changed=false;
        b.playing=[&](auto handle){trace<<"Q,"<<handle<<';';if((mutation&1)&&!changed){cells.voices[0].name=names+32;changed=true;}return (handle&1)!=0;};
        b.cache=[&](auto at){trace<<"F,"<<at<<';';const auto key=((at-names)/32)%4;return &cache[key];};
        b.free_audio=[&](auto allocation){trace<<"R,"<<allocation<<';';if(mutation&2)for(auto& value:cache)if(!value.loaded)value.allocation=99;};
        runtime.cleanup();std::cout<<cells.voice_count_8e3d50;for(unsigned i=0;i<8;++i)std::cout<<' '<<cells.voices[i].handle<<' '<<cells.voices[i].name;
        for(const auto& value:cache)std::cout<<' '<<unsigned(value.loaded)<<' '<<value.allocation;
    }else throw std::runtime_error("Unknown script probe mode");
    std::cout<<' '<<trace.str()<<'\n';
}
}
int main(int argc,char** argv){try{if(argc!=2)return 2;std::string line;while(std::getline(std::cin,line)){std::istringstream input(line);run(argv[1],input);}}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
