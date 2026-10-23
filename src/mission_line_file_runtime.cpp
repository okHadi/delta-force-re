#include "dfre/mission_line_file_runtime.hpp"
#include "dfre/mission_banned_record.hpp"
#include "dfre/mission_script_registry.hpp"
#include <fstream>
#include <map>
#include <utility>
namespace dfre {namespace {
using U=std::uint32_t;
template<class F>const F& need(const F& f,const char* message){if(!f)throw MissionSoundAssetError(message);return f;}
}
std::uint32_t mission_line_file_read(const MissionLineFileBindings& b,U target,U limit,U file){
    if(limit<2||limit>0x7fffffffu)throw MissionSoundAssetError("FILE line count needs at least two bytes");
    const auto read=[&](U at,U width=4){return need(b.read,"FILE reader is not bound")(at,width);};
    const auto write=[&](U at,U width,U value){need(b.write,"FILE writer is not bound")(at,width,value);};
    const auto call=[&](U site,std::initializer_list<U> args){return need(b.call,"FILE body is not bound")(site,{args});};
    call(0x501d64,{read(file+16)});const auto saved=read(file+12)&0x30;write(file+12,1,read(file+12,1)&0xcf);
    U cursor=target,c=0;
    while(--limit){c=call(0x4ba3c0,{file});if(c==0xffffffffu)break;write(cursor++,1,c&255u);if(std::uint8_t(c)==10)break;}
    U result=target;if(c==0xffffffffu&&(cursor==target||(read(file+12,1)&0x20)))result=0;else write(cursor,1,0);
    const auto flags=read(file+12),descriptor=read(file+16);write(file+12,4,flags|saved);call(0x501d68,{descriptor});return result;
}
struct MissionLineFileRuntime::Impl {
    const MissionSoundAssetBindings& memory;std::filesystem::path folder;U scratch,bytes,depth=0,next=0;
    MissionLineFileRuntimeBindings host;MissionLineFileBindings parser;
    struct File {std::ifstream stream;U node,buffer=0,descriptor;};std::map<U,File> files;
    struct Frame {Impl& owner;U at;explicit Frame(Impl& x):owner(x),at(x.scratch+x.depth*0x500){if(std::uint64_t(x.depth+1)*0x500>x.bytes)throw MissionSoundAssetError("Line parser scratch is full");++x.depth;}~Frame(){--owner.depth;}};
    Impl(const std::filesystem::path& root,const MissionSoundAssetBindings& b,U at,U n,MissionLineFileRuntimeBindings h):memory(b),folder(std::filesystem::canonical(root)),scratch(at),bytes(n),host(std::move(h)){
        if(!std::filesystem::is_directory(folder)||(at&3)||n<0x500||std::uint64_t(at)+n>0x100000000ull)throw MissionSoundAssetError("Line file runtime needs a folder and complete aligned scratch");
        parser.read=memory.read;parser.write=memory.write;parser.call=[this](U site,const std::vector<U>& args){return call(site,args);};
        parser.tokenize=[this](U line,U out){call(0x4565f0,{line,out});};
    }
    U read(U at,U n=4){return need(memory.read,"Line file memory reader is not bound")(at,n);}
    void write(U at,U value,U n=4){need(memory.write,"Line file memory writer is not bound")(at,n,value);}
    U call(U site,const std::vector<U>& args){if(const auto value=service(site,args))return *value;return need(memory.call,"Line file service is not bound")(site,args);}
    U allocate(U n){const auto at=need(memory.call,"Line file allocator is not bound")(0x497f60,{n,4,0,0});if(!at)throw MissionSoundAssetError("Line file allocation failed");for(U i=0;i<n;++i)write(at+i,0,1);return at;}
    void release(U at){if(at)need(memory.call,"Line file release is not bound")(0x4980d0,{at});}
    std::string string(U at){std::string s;for(U i=0;i<260;++i){const auto c=read(at+i,1);if(!c)return s;s+=char(c);}throw MissionSoundAssetError("Loose file name exceeds its supported span");}
    std::filesystem::path path(const std::string& name){
        const std::filesystem::path relative(name);if(relative.empty()||relative.is_absolute()||relative.has_root_name()||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos)throw MissionSoundAssetError("Loose file needs a relative game path");
        for(const auto& part:relative)if(part=="..")throw MissionSoundAssetError("Loose file path leaves the game folder");
        auto current=folder;std::error_code error;
        const auto lower=[](std::string text){for(auto& c:text)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return text;};
        for(const auto& part:relative){
            auto candidate=current/part;
            if(!std::filesystem::exists(candidate,error)){
                error.clear();U matches=0;
                for(std::filesystem::directory_iterator it(current,error),end;!error&&it!=end;it.increment(error))if(lower(it->path().filename().string())==lower(part.string())){candidate=it->path();++matches;}
                if(matches>1)throw MissionSoundAssetError("Loose file name has more than one case match");
            }
            const auto checked=std::filesystem::weakly_canonical(candidate,error);if(error)return {};
            const auto within=checked.lexically_relative(folder);if(within.empty()||*within.begin()=="..")throw MissionSoundAssetError("Loose file target leaves the game folder");
            current=checked;error.clear();
        }
        const auto result=std::filesystem::weakly_canonical(current,error);if(error)return {};
        const auto child=result.lexically_relative(folder);if(child.empty()||*child.begin()=="..")throw MissionSoundAssetError("Loose file target leaves the game folder");return result;
    }
    U open(U name,U mode){
        if(string(mode)!="r")throw MissionSoundAssetError("Loose file backend supports mode r");
        const auto target=path(string(name));if(target.empty())return 0;std::error_code error;if(!std::filesystem::is_regular_file(target,error)||error)return 0;
        std::ifstream stream(target,std::ios::binary);if(!stream)return 0;
        const auto file=allocate(0x1c);U node=0;try{node=allocate(0x20);write(file+8,node);write(file+12,1);write(file+16,++next);write(node+4,file);files.emplace(file,File{std::move(stream),node,0,next});}catch(...){release(node);release(file);throw;}return file;
    }
    File& known(U file){auto found=files.find(file);if(found==files.end())throw MissionSoundAssetError("FILE handle has no loose file owner");return found->second;}
    struct Lock {Impl& owner;U file;bool active=true;explicit Lock(Impl& x,U f):owner(x),file(f){need(x.host.unlock,"FILE unlock is not bound");need(x.host.lock,"FILE lock is not bound")(x.read(f+16));}void release(U descriptor){active=false;owner.host.unlock(descriptor);}~Lock(){if(active)owner.host.unlock(owner.read(file+16));}};
    U refill(U file){
        auto& owned=known(file);const auto node=read(file+8);if(node!=owned.node)throw MissionSoundAssetError("FILE node has no loose file owner");
        auto buffer=read(node+8);
        if(!buffer){
            if(read(file+13,1)&0x22)throw MissionSoundAssetError("Terminal FILE buffering is not bound");
            if(!read(file+20))write(file+20,(read(file+13,1)&4)?1:0x1000);
            buffer=allocate(read(file+20));owned.buffer=buffer;write(node+8,buffer);write(file+12,read(file+12,1)|8,1);write(file+4,0);write(file,buffer);
        }
        const auto high=read(file+13,1);if((high&0x20)&&(high&6))throw MissionSoundAssetError("Terminal FILE flush is not bound");
        write(file+12,read(file+12,1)&0xfb,1);write(file,read(node+8));
        if((read(file+12)&0x2400)==0x2400&&read(file+16)==0)throw MissionSoundAssetError("Console FILE input is not bound");
        const auto descriptor=read(file+16);if(descriptor!=owned.descriptor)throw MissionSoundAssetError("FILE descriptor has no loose file owner");
        const auto size=(read(file+13,1)&4)?1:read(file+20);if(size>0x1000000)throw MissionSoundAssetError("FILE read count exceeds its supported span");
        std::vector<char> data(size);owned.stream.read(data.data(),std::streamsize(size));const auto count=owned.stream.gcount();
        const U result=owned.stream.bad()?0xffffffffu:U(count);const auto target=read(file);
        for(U i=0;i<U(count);++i)write(target+i,std::uint8_t(data[i]),1);write(file+4,result);
        if(result>=0x80000000u){write(file+4,0);write(file+12,read(file+12,1)|0x20,1);return 0;}
        if(!result){write(file+12,read(file+12,1)|0x10,1);return 0;}return result;
    }
    U raw_character(U file){const auto prior=read(file+4),count=prior-1;write(file+4,count);if(count<0x80000000u){const auto at=read(file),value=read(at,1);write(file,at+1);return value;}if(!refill(file))return 0xffffffffu;const auto at=read(file);write(file+4,read(file+4)-1);write(file,at+1);return read(at,1);}
    U character(U file){
        Lock lock(*this,file);const auto node=read(file+8);const auto mode=read(node+12);if(mode!=1){if(mode)return 0xffffffffu;write(node+12,1);}
        U value;if(!(read(file+12,1)&1)){need(memory.call,"FILE error service is not bound")(0x4b9c10,{4});write(file+12,read(file+12,1)|0x20,1);value=0xffffffffu;}else value=raw_character(file);
        if(!(read(file+12,1)&0x40)){if(value==13)value=raw_character(file);if(value==26){write(file+12,read(file+12,1)|0x10,1);value=0xffffffffu;}}return value;
    }
    U gets(U target,U limit,U file){
        known(file);MissionLineFileBindings b=parser;
        b.call=[this](U site,const std::vector<U>& args)->U{
            if(site==0x4ba3c0)return character(args.at(0));
            if(site==0x501d64){need(host.lock,"FILE lock is not bound")(args.at(0));return 0;}
            if(site==0x501d68){need(host.unlock,"FILE unlock is not bound")(args.at(0));return 0;}
            throw MissionSoundAssetError("FILE read calls an unowned service");
        };
        return mission_line_file_read(b,target,limit,file);
    }
    U close(U file){auto it=files.find(file);if(it==files.end())return 0xffffffffu;auto item=std::move(it->second);files.erase(it);item.stream.clear();item.stream.close();const U result=item.stream.fail()?0xffffffffu:0;release(item.buffer);release(item.node);release(file);return result;}
    std::optional<U> service(U site,const std::vector<U>& a){switch(site){
        case 0x49a41c:return open(a.at(0),a.at(1));
        case 0x49a550:return gets(a.at(0),a.at(1),a.at(2));
        case 0x49a840:return close(a.at(0));
        case 0x4568e0:{Frame frame(*this);write(frame.at+0x3d4,a.at(0));write(frame.at+0x3d8,a.at(1));return mission_line_file(parser,frame.at);}
        case 0x448690:{Frame frame(*this);return mission_banned_record(parser,a.at(0),frame.at);}
        case 0x49afa0:{MissionSoundBankBindings b{memory.read,memory.write,memory.call};return mission_script_registry_id(b,a.at(0));}
        default:return std::nullopt;
    }}
};
MissionLineFileRuntime::MissionLineFileRuntime(const std::filesystem::path& path,const MissionSoundAssetBindings& memory,std::uint32_t scratch,std::uint32_t bytes,MissionLineFileRuntimeBindings host):impl_(std::make_unique<Impl>(path,memory,scratch,bytes,std::move(host))){}
MissionLineFileRuntime::~MissionLineFileRuntime()=default;
std::optional<std::uint32_t> MissionLineFileRuntime::service(std::uint32_t site,const std::vector<std::uint32_t>& args){return impl_->service(site,args);}
}
