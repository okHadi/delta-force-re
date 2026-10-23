#include "dfre/mission_actor_message.hpp"
#include <iostream>
#include <sstream>
int main(){using namespace dfre;using U=std::uint32_t;U operation,actor,message,phrase,x,y,z,mutation;while(std::cin>>operation>>actor>>message>>phrase>>x>>y>>z>>mutation){MissionActorMessageBindings b;std::ostringstream trace;U reads=0;bool failed=false;
 b.broadcast=[&](U type,auto& data){trace<<'B'<<type<<','<<unsigned(data[0])<<','<<unsigned(data[1])<<';';if(mutation&64)throw std::runtime_error("producer failure");data[0]^=0x42;};
 b.read=[&](U at,U width){trace<<'R'<<at<<','<<width<<';';++reads;if((mutation&32)&&reads==3)throw std::runtime_error("reader failure");if(at==actor+0x2c)return z;if(at==actor+0x28){if(mutation&1)z^=0xffffffff;return y;}if(at==actor+4){if(mutation&2)x^=0xabcdef01;return 0x1210000u;}if(at==0x12100d4){if(mutation&4)phrase^=0xffffffff;return phrase;}if(at==actor+0x24){if(mutation&8)message^=0x13579bdf;return x;}const auto id=at^0x55667788;if(mutation&16){x^=1;y^=2;z^=3;message^=0xffffffff;}return id;};
 b.positional=[&](U sound,Fixed3 p){trace<<'S'<<sound<<','<<U(p.x)<<','<<U(p.y)<<','<<U(p.z)<<';';if(mutation&128)throw std::runtime_error("sound failure");};
 try{if(operation==0)mission_actor_message_send(b,actor,message);else if(operation==1)mission_actor_message_receive(b,actor,message);else mission_actor_message_send_index(b,actor,message);}catch(const std::exception&){failed=true;}
 std::cout<<failed<<' '<<message<<' '<<trace.str()<<'\n';}}
