#include "dfre/mission_client_live.hpp"
#include "dfre/mission_client_startup.hpp"
#include "dfre/mission_df_socket_runtime.hpp"
#include "dfre/mission_line_file_runtime.hpp"
#include "dfre/mission_localization.hpp"
#include "dfre/mission_sound_runtime.hpp"
#include <fstream>
#include <iostream>
#include <mutex>
using namespace dfre;
using U=std::uint32_t;
void check(bool value){if(!value)throw std::runtime_error("Client startup join check failed");}
void client_checks(){
 MissionClientLive clients;
 const U base=mission_client_source_base,end=base+U(mission_client_source_bytes);
 auto fails=[](auto call){bool failed=false;try{call();}catch(const MissionClientLiveError&){failed=true;}check(failed);};
 for(std::size_t slot=0;slot<mission_client_count;++slot){const auto view=clients.client(slot);check(view.length==mission_client_record_size);for(std::size_t i=0;i<view.length;++i)check(view.bytes[i]==0);check(MissionClientLive::source_address(slot)==base+slot*mission_client_record_size);}
 check(!clients.claims(base-4,4)&&!clients.claims(end,4));
 fails([&]{clients.claims(base-1,2);});fails([&]{clients.claims(end-1,2);});fails([&]{clients.claims(0xffffffff,2);});fails([&]{clients.claims(base,3);});fails([&]{clients.read(end,1);});fails([&]{clients.write(base-4,4,0);});fails([&]{clients.client(17);});fails([&]{MissionClientLive::source_address(0,mission_client_record_size);});
 clients.write(base+3,4,0x87654321);check(clients.read(base+3,4)==0x87654321&&clients.client(0).bytes[4]==0x43);
 clients.write(base+U(mission_client_record_size)-1,4,0x12345678);check(clients.client(0).bytes[mission_client_record_size-1]==0x78&&clients.client(1).bytes[0]==0x56&&clients.read(base+U(mission_client_record_size),2)==0x3456);
 auto final=clients.client(16);final.bytes[mission_client_record_size-1]=0xa7;check(clients.read(end-1,1)==0xa7);
 GameMessageStage messages;MissionServerMessageBuffer server;MissionClientOutboxBindings out;
 out.client=[&](auto slot){return clients.client(slot);};out.cell=[](auto){return 0u;};
 MissionClientOutbox outbox(messages,server,out);const std::array<std::uint8_t,16> payload{};
 clients.write(base+0x1b8,4,9);check(outbox.enqueue(0,0x0a,payload.data(),payload.size()));
 check(clients.read(base+0x1b8,4)==10&&clients.read(base+0x204,2)==9&&clients.read(base+0x20d,1)==1);
 clients.write(base+0x20d,1,0);check(clients.client(0).bytes[0x20d]==0);

}
int main(int argc,char** argv){try{
 client_checks();if(argc==1){std::cout<<"Shared client byte/view checks passed\n";return 0;}if(argc!=2)return 2;
 const std::filesystem::path folder=argv[1];AssetStore assets(folder);
 std::ifstream file(folder/"Df.exe",std::ios::binary);check(bool(file));
 const AssetBytes executable((std::istreambuf_iterator<char>(file)),{});
 Fixed3 listener{};U yaw=0;MissionAudioFrameCells cells{0,listener,yaw};
 const TerrainGeometry* terrain=nullptr;MissionDfSocketRuntime* socket=nullptr;
 MissionLineFileRuntime* lines=nullptr;MissionSoundRuntime* owner=nullptr;
 MissionClientLive clients;MissionSoundRuntimeBindings host;host.script_key=[](){return 0x04960552u;};
 host.record_base=[&](U at,U width)->std::optional<U>{if(clients.claims(at,width))return at;if(socket&&socket->claims(at,width))return at;return {};};
 host.record_read=[&](U base,U offset,U width){if(clients.claims(base+offset,width))return clients.read(base+offset,width);return socket->read(base+offset,width);};
 host.record_store=[&](U base,U offset,U width,U value){if(clients.claims(base+offset,width)){clients.write(base+offset,width,value);return;}socket->write(base+offset,width,value);};
 host.service=[&](U site,const std::vector<U>& args)->U{
  if(lines)if(const auto result=lines->service(site,args))return *result;
  throw std::runtime_error("Unbound startup service "+std::to_string(site));
 };
 MissionSoundRuntime sound(assets,executable,cells,terrain,host);owner=&sound;
 const auto& memory=owner->asset_bindings();
 MissionDfSocketRuntimeMemory shared{memory.read,memory.write};
 MissionDfSocketRuntime sockets(shared);socket=&sockets;
 std::recursive_mutex mutex;MissionLineFileRuntimeBindings locks;
 locks.lock=[&](U){mutex.lock();};locks.unlock=[&](U){mutex.unlock();};
 const auto scratch=memory.call(0x497f60,{0x2000,4,0,0});
 MissionLineFileRuntime line_owner(folder,memory,scratch,0x2000,locks);lines=&line_owner;
 MissionClientStartupBindings startup;startup.read=memory.read;startup.write=memory.write;
 U lookups=0;startup.call=[&](U site,const std::vector<U>& args)->U{
  if(site==0x49a620){const auto count=args.at(2);U i=0;const U byte=args.at(1)&255,word=byte*0x01010101u;
   for(;count-i>=4;i+=4)memory.write(args.at(0)+i,4,word);
   for(;i<count;++i)memory.write(args.at(0)+i,1,byte);return args.at(0);}
  if(site==0x404cf0){MissionLocalizationBindings binding;binding.read=memory.read;
   binding.lookup_49a092=[](U,U,U)->U{throw std::runtime_error("Cold startup must use the unloaded text table");};
   return mission_localized_text(binding,args.at(0),args.at(1));}
  if(site==0x454df0){++lookups;const auto value=mission_df_socket_resolve(sockets.bindings(),args.at(0));check(value==0&&sockets.bindings().call(0x4c55e0,{})==10093);return value;}
  if(site==0x454bb0)return mission_df_socket_start(sockets.bindings(),args.at(0),args.at(1),args.at(2));
  if(const auto result=lines->service(site,args))return *result;
  throw std::runtime_error("Unbound client body "+std::to_string(site));};
 check(memory.read(0x503230,4)==0);
 clients.client(16).bytes[mission_client_record_size-1]=255;
 mission_client_startup(startup);
 check(clients.client(16).bytes[mission_client_record_size-1]==0);
 check(clients.client(0).bytes[0x18c]==1&&clients.client(0).bytes[0x18d]==1);
 check(clients.read(MissionClientLive::source_address(0,0x9c),4)==1);
 for(std::size_t slot=1;slot<mission_client_count;++slot){const auto view=clients.client(slot);for(std::size_t i=0;i<view.length;++i)check(view.bytes[i]==0);}
 // The existing local setup producer writes the same physical record view.
 const auto name=memory.call(0x497f60,{16,4,0,0});memory.write(name,4,0x00656f4a);memory.write(0x50133c,4,77);
 mission_client_setup_local(startup,name,8,2);
 check(clients.read(mission_client_source_base,4)==77&&clients.client(0).bytes[12]=='J');
 check(clients.read(MissionClientLive::source_address(0,0xa0),4)==8&&clients.read(MissionClientLive::source_address(0,0xa4),4)==2);
 check(lookups==1&&memory.read(0x6e575c,4)==0);
 check(memory.read(0x6e9c1c,1)==1&&memory.read(0x6e9c1d,1)==1&&memory.read(0x6e9b2c,4)==1);
 check(memory.read(0x501374,4)==0x43f830&&memory.read(0x501378,4)==0x43fd60);
 check(memory.read(0x50137c,4)==0x43fef0&&memory.read(0x501380,4)==0x43ff20);
 check(memory.read(0x6e5888,4)==0&&memory.read(0x6e588c,4)==0);
 check(sockets.socket_count()==1&&sockets.frame_depth()==0&&memory.read(0x8e3540,4)!=0);
 std::cout<<"Cold43FF30 startup: shared client/input cells, Winsock10093 lookup, real UDP startup, banned-file parser; device output0\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
