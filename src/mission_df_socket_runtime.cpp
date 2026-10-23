#include "dfre/mission_df_socket_runtime.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace dfre {namespace {
using U=std::uint32_t;
#ifdef _WIN32
using Socket=SOCKET;constexpr Socket invalid_socket=INVALID_SOCKET;
void close_socket(Socket s)noexcept{closesocket(s);}
#else
using Socket=int;constexpr Socket invalid_socket=-1;
void close_socket(Socket s)noexcept{close(s);}
U windows_error(int error){switch(error){
 case EACCES:return 10013;case EADDRINUSE:return 10048;case EADDRNOTAVAIL:return 10049;
 case EAFNOSUPPORT:return 10047;case EBADF:return 10038;case ENOTSOCK:return 10038;
 case EFAULT:return 10014;case EINTR:return 10004;case EINVAL:return 10022;
 case EMFILE:return 10024;case ENOBUFS:return 10055;case ENOMEM:return 10055;
 case ENOPROTOOPT:return 10042;case EPROTONOSUPPORT:return 10043;case EPROTOTYPE:return 10041;
 case EOPNOTSUPP:return 10045;case EDESTADDRREQ:return 10039;case EMSGSIZE:return 10040;
 case ENETDOWN:return 10050;case ENETUNREACH:return 10051;case ENETRESET:return 10052;
 case ECONNABORTED:return 10053;case ECONNRESET:return 10054;case ENOTCONN:return 10057;
 case EISCONN:return 10056;case ETIMEDOUT:return 10060;case ECONNREFUSED:return 10061;
 case EHOSTUNREACH:return 10065;case EINPROGRESS:return 10036;case EALREADY:return 10037;
 case EWOULDBLOCK:return 10035;
#if EAGAIN != EWOULDBLOCK
 case EAGAIN:return 10035;
#endif
 default:throw MissionClientStartupError("POSIX socket error has no Winsock mapping "+std::to_string(error));}}
#endif
void require(bool value,const char* text){if(!value)throw MissionClientStartupError(text);}
std::int32_t si(U n){std::int32_t value;std::memcpy(&value,&n,4);return value;}
U little_ip(const void* data){const auto* p=static_cast<const std::uint8_t*>(data);return U(p[0])|(U(p[1])<<8)|(U(p[2])<<16)|(U(p[3])<<24);}
std::mutex& resolver_mutex(){static std::mutex value;return value;}
constexpr U scratch_base=0x23000000,scratch_bytes=0x10000,lookup_base=0x23100000,lookup_bytes=0x10000,heap_base=0x24000000,wsadata_base=0x8e1390,wsadata_bytes=400;
}
struct MissionDfSocketRuntime::Impl {
 MissionDfSocketRuntimeMemory& memory;MissionDfSocketRuntimeOptions options;std::thread::id thread=std::this_thread::get_id();
 struct Block {U base;std::vector<std::uint8_t> bytes;};std::map<U,Block> blocks;
 std::map<U,Socket> sockets;U next_socket=0x71000000,next_heap=heap_base,used_heap=0,last_error=0,startups=0;
 struct ActiveFrame {U site,address,size;};std::vector<ActiveFrame> frames;
 Impl(MissionDfSocketRuntimeMemory& owner,MissionDfSocketRuntimeOptions settings):memory(owner),options(settings){require(bool(memory.read)&&bool(memory.write),"DF socket runtime needs canonical memory providers");require(options.allocation_limit<=0xffffffffu-heap_base,"DF socket memory limit exceeds its source address range");add(wsadata_base,wsadata_bytes,0);add(lookup_base,lookup_bytes,options.new_memory_byte);}
 ~Impl(){for(const auto& pair:sockets)close_socket(pair.second);
#ifdef _WIN32
  while(startups){WSACleanup();--startups;}
#endif
 }
 void check()const{require(thread==std::this_thread::get_id(),"DF socket runtime callback is on another thread");}
 void add(U base,U size,std::uint8_t fill){blocks.emplace(base,Block{base,std::vector<std::uint8_t>(size,fill)});}
 bool reserved(U at)const{return (at>=scratch_base&&at<scratch_base+scratch_bytes)||(at>=lookup_base&&at<lookup_base+lookup_bytes)||(at>=heap_base&&std::uint64_t(at)<std::uint64_t(heap_base)+options.allocation_limit)||(at>=wsadata_base&&at<wsadata_base+wsadata_bytes);}
 const Block* block(U at)const{auto p=blocks.upper_bound(at);if(p==blocks.begin())return nullptr;--p;return std::uint64_t(at-p->first)<p->second.bytes.size()?&p->second:nullptr;}
 Block* block(U at){return const_cast<Block*>(static_cast<const Impl*>(this)->block(at));}
 static void width(U n){require(n==1||n==2||n==4,"DF socket memory width is invalid");}
 bool claims(U at,U n)const{check();width(n);for(U i=0;i<n;++i){const auto a=at+i;if(reserved(a)||block(a))return true;}return false;}
 U read(U at,U n)const{check();width(n);if(!claims(at,n))return memory.read(at,n);U result=0;for(U i=0;i<n;++i){const auto a=at+i;if(const auto* b=block(a))result|=U(b->bytes[a-b->base])<<(8*i);else{require(!reserved(a),"DF socket read is outside a current owned block");result|=(memory.read(a,1)&255)<<(8*i);}}return result;}
 void write(U at,U n,U value){check();width(n);if(!claims(at,n)){memory.write(at,n,value);return;}for(U i=0;i<n;++i){const auto a=at+i;if(auto* b=block(a))b->bytes[a-b->base]=std::uint8_t(value>>(8*i));else{require(!reserved(a),"DF socket write is outside a current owned block");memory.write(a,1,(value>>(8*i))&255);}}}
 std::string text(U at)const{std::string result;for(U i=0;i<4096;++i){const auto c=read(at+i,1);if(!c)return result;result.push_back(char(c));}throw MissionClientStartupError("DF socket string exceeds its mapped window");}
 void text(U at,const std::string& s){for(U i=0;i<=s.size();++i)write(at+i,1,i<s.size()?std::uint8_t(s[i]):0);}
 U failure(){
#ifdef _WIN32
  last_error=U(WSAGetLastError());
#else
  last_error=windows_error(errno);
#endif
  return 0xffffffffu;
 }
 Socket socket(U token){const auto p=sockets.find(token);if(p==sockets.end()){last_error=10038;
#ifdef _WIN32
  WSASetLastError(int(last_error));
#endif
  return invalid_socket;}return p->second;}
 U frame(U site,U size,const std::vector<U>& args){check();const bool parameter=site==0x454890||site==0x454900;const U expected=parameter?4:site==0x454850||site==0x4548c0?8:site==0x454a10?16:site==0x454930?20:0;require(expected&&size==expected&&args.size()==(parameter?1u:0u),"DF socket frame layout is unsupported");require(size<=20&&frames.size()<scratch_bytes/0x100,"DF socket frame exceeds its owned window");const auto at=scratch_base+U(frames.size())*0x100;if(!block(at))add(at,20,options.new_memory_byte);frames.push_back({site,at,size});if(!args.empty()){require(size==4&&args.size()==1,"DF socket frame argument layout is unsupported");write(at,4,args[0]);}return at;}
 void release(U site,U at,U size){check();require(!frames.empty()&&frames.back().site==site&&frames.back().address==at&&frames.back().size==size,"DF socket frame release order is invalid");frames.pop_back();}
 U allocate(U requested){if(!requested||requested>0xffffffd4u)return 0;U size=(requested+11)&0xfffffff8u;if(size<16)size=16;if(size>options.allocation_limit-used_heap)return 0;try{const auto pointer=next_heap;add(pointer,size,options.new_memory_byte);next_heap+=size;used_heap+=size;return pointer;}catch(const std::bad_alloc&){return 0;}}
 U startup(U version,U target){require(version==0x101,"DF socket runtime supports source Winsock version1.1");
#ifdef _WIN32
  WSADATA data{};const auto result=WSAStartup(WORD(version),&data);if(result)return U(result);++startups;
  require(data.lpVendorInfo==nullptr,"DF socket vendor data layout is unsupported");write(target,2,data.wVersion);write(target+2,2,data.wHighVersion);for(U i=0;i<257;++i)write(target+4+i,1,std::uint8_t(data.szDescription[i]));for(U i=0;i<129;++i)write(target+261+i,1,std::uint8_t(data.szSystemStatus[i]));write(target+390,2,data.iMaxSockets);write(target+392,2,data.iMaxUdpDg);write(target+396,4,0);return 0;
#else
  ++startups;for(U i=0;i<wsadata_bytes;++i)write(target+i,1,0);write(target,2,0x101);write(target+2,2,0x101);text(target+4,"POSIX");text(target+261,"Running");write(target+390,2,0xffff);write(target+392,2,65507);return 0;
#endif
 }
 U lookup(U name){const auto query=text(name);in_addr numeric{};const bool numeric_name=inet_pton(AF_INET,query.c_str(),&numeric)==1;require(options.allow_external_dns||numeric_name,"DF socket external DNS is disabled");std::lock_guard<std::mutex> lock(resolver_mutex());hostent numeric_result{};char* numeric_aliases[]={nullptr};char* numeric_addresses[]={reinterpret_cast<char*>(&numeric),nullptr};if(numeric_name){numeric_result.h_name=const_cast<char*>(query.c_str());numeric_result.h_aliases=numeric_aliases;numeric_result.h_addrtype=AF_INET;numeric_result.h_length=4;numeric_result.h_addr_list=numeric_addresses;}auto* result=numeric_name?&numeric_result: ::gethostbyname(query.c_str());if(!result){
#ifdef _WIN32
  failure();return 0;
#else
  switch(h_errno){case HOST_NOT_FOUND:last_error=11001;break;case TRY_AGAIN:last_error=11002;break;case NO_RECOVERY:last_error=11003;break;case NO_DATA:last_error=11004;break;default:throw MissionClientStartupError("Resolver error has no Winsock mapping");}return 0;
#endif
  }
  require(result->h_addrtype==AF_INET&&result->h_length==4,"DF socket resolver returned an unsupported address type");
  U cursor=lookup_base+16;const auto alloc=[&](U count){require(std::uint64_t(cursor-lookup_base)+count<=lookup_bytes,"DF socket lookup result exceeds its owned window");const auto at=cursor;cursor+=count;return at;};
  const std::string hostname=result->h_name?result->h_name:query;const auto current_name=alloc(U(hostname.size()+1));text(current_name,hostname);
  std::vector<std::string> aliases;for(U i=0;result->h_aliases&&result->h_aliases[i];++i){require(i<256,"DF socket alias list exceeds its source window");aliases.emplace_back(result->h_aliases[i]);}
  const auto alias_list=alloc(U(aliases.size()+1)*4);for(U i=0;i<aliases.size();++i){const auto at=alloc(U(aliases[i].size()+1));text(at,aliases[i]);write(alias_list+i*4,4,at);}write(alias_list+U(aliases.size())*4,4,0);
  std::vector<std::array<std::uint8_t,4>> addresses;for(U i=0;result->h_addr_list&&result->h_addr_list[i];++i){require(i<256,"DF socket address list exceeds its source window");std::array<std::uint8_t,4> ip{};std::memcpy(ip.data(),result->h_addr_list[i],4);addresses.push_back(ip);}require(!addresses.empty(),"DF socket resolver returned an empty address list");
  const auto address_list=alloc(U(addresses.size()+1)*4);for(U i=0;i<addresses.size();++i){const auto at=alloc(4);for(U j=0;j<4;++j)write(at+j,1,addresses[i][j]);write(address_list+i*4,4,at);}write(address_list+U(addresses.size())*4,4,0);
  write(lookup_base,4,current_name);write(lookup_base+4,4,alias_list);write(lookup_base+8,2,2);write(lookup_base+10,2,4);write(lookup_base+12,4,address_list);return lookup_base;
 }
 U call(U site,const std::vector<U>& a){check();const auto count=[&](std::size_t n){require(a.size()==n,"DF socket import argument count is invalid");};
  if(site==0x49dc80){count(1);return allocate(a[0]);}
  if(site==0x4c55f8){count(2);return startup(a[0],a[1]);}
  if(site==0x4c55e0){count(0);
#ifdef _WIN32
   last_error=U(WSAGetLastError());
#endif
   return last_error;}
  if(site==0x4c55bc){count(1);return ((a[0]&255)<<8)|((a[0]>>8)&255);}
  if(site==0x4c55b0){count(1);const auto value=::inet_addr(text(a[0]).c_str());return little_ip(&value);}
  if(site==0x4c55ec){count(1);if(!startups){last_error=10093;
#ifdef _WIN32
   WSASetLastError(int(last_error));
#endif
   return 0;}return lookup(a[0]);}
  if(site==0x4c55c8){count(3);require(startups>0,"DF socket creation needs its source WSAStartup");require(a[0]==2&&a[1]==2&&a[2]==0,"DF socket creation parameters are unsupported");const auto native=::socket(AF_INET,SOCK_DGRAM,0);if(native==invalid_socket)return failure();const auto token=next_socket++;try{sockets.emplace(token,native);}catch(...){close_socket(native);throw;}return token;}
  if(site==0x4c55c2){count(3);require(a[1]==0x8004667e,"DF socket ioctl operation is unsupported");const auto native=socket(a[0]);if(native==invalid_socket)return 0xffffffffu;const auto prior=read(a[2],4);
#ifdef _WIN32
   u_long value=prior;const auto result=ioctlsocket(native,FIONBIO,&value);
#else
   int value=si(prior);const auto result=ioctl(native,FIONBIO,&value);
#endif
   if(U(value)!=prior)write(a[2],4,U(value));return result<0?failure():0;}
  if(site==0x4c55b6){count(3);require(a[2]==16&&read(a[1],2)==2,"DF socket bind address layout is unsupported");const auto native=socket(a[0]);if(native==invalid_socket)return 0xffffffffu;sockaddr_in address{};
#ifdef __APPLE__
   address.sin_len=sizeof(address);
#endif
   address.sin_family=AF_INET;const auto network=read(a[1]+2,2);const auto port=((network&255)<<8)|(network>>8);address.sin_port=htons(std::uint16_t(port));std::uint8_t ip[4];for(U i=0;i<4;++i)ip[i]=std::uint8_t(read(a[1]+4+i,1));std::memcpy(&address.sin_addr.s_addr,ip,4);for(U i=0;i<8;++i)address.sin_zero[i]=char(read(a[1]+8+i,1));return ::bind(native,reinterpret_cast<const sockaddr*>(&address),sizeof(address))<0?failure():0;}
  if(site==0x4c55ce||site==0x4c55d4){count(5);require(a[1]==0xffff&&(a[2]==0x1001||a[2]==0x1002),"DF socket option is unsupported");const auto native=socket(a[0]);if(native==invalid_socket)return 0xffffffffu;const int option=a[2]==0x1001?SO_SNDBUF:SO_RCVBUF;const auto prior=read(a[3],4);int value=si(prior);
   if(site==0x4c55ce){require(a[4]==4&&value>0,"DF socket buffer size is unsupported");
#ifdef _WIN32
    const auto result=setsockopt(native,SOL_SOCKET,option,reinterpret_cast<const char*>(&value),sizeof(value));
#else
    const auto result=setsockopt(native,SOL_SOCKET,option,&value,sizeof(value));
#endif
    return result<0?failure():0;
   }
   require(read(a[4],4)==4,"DF socket output option length is unsupported");
#ifdef _WIN32
   int length=4;const auto result=getsockopt(native,SOL_SOCKET,option,reinterpret_cast<char*>(&value),&length);
#else
   socklen_t length=4;const auto result=getsockopt(native,SOL_SOCKET,option,&value,&length);
#endif
   if(result>=0||U(value)!=prior)write(a[3],4,U(value));if(result>=0||U(length)!=4)write(a[4],4,U(length));return result<0?failure():0;
  }
  throw MissionClientStartupError("DF socket runtime does not own import "+std::to_string(site));
 }
};
MissionDfSocketRuntime::MissionDfSocketRuntime(MissionDfSocketRuntimeMemory& memory,MissionDfSocketRuntimeOptions options):impl_(std::make_unique<Impl>(memory,options)){}
MissionDfSocketRuntime::~MissionDfSocketRuntime()=default;
MissionDfSocketBindings MissionDfSocketRuntime::bindings(){MissionDfSocketBindings b;b.read=[this](auto a,auto w){return read(a,w);};b.write=[this](auto a,auto w,auto n){write(a,w,n);};b.call=[this](auto site,const auto& args){return impl_->call(site,args);};b.frame=[this](auto site,auto size,const auto& args){return impl_->frame(site,size,args);};b.release=[this](auto site,auto at,auto size){impl_->release(site,at,size);};return b;}
bool MissionDfSocketRuntime::claims(U at,U n)const{return impl_->claims(at,n);}
U MissionDfSocketRuntime::read(U at,U n)const{return impl_->read(at,n);}
void MissionDfSocketRuntime::write(U at,U n,U value){impl_->write(at,n,value);}
std::uint16_t MissionDfSocketRuntime::local_port(U token)const{impl_->check();const auto p=impl_->sockets.find(token);require(p!=impl_->sockets.end(),"DF socket port needs a current owned socket");sockaddr_in address{};
#ifdef _WIN32
 int size=sizeof(address);
#else
 socklen_t size=sizeof(address);
#endif
 require(getsockname(p->second,reinterpret_cast<sockaddr*>(&address),&size)==0,"DF socket local port read failed");return ntohs(address.sin_port);}
std::size_t MissionDfSocketRuntime::socket_count()const{impl_->check();return impl_->sockets.size();}
std::size_t MissionDfSocketRuntime::frame_depth()const{impl_->check();return impl_->frames.size();}
}
