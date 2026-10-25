#include "dfre/mission_event_markers.hpp"
#include <stdexcept>
namespace dfre {namespace {
using U=std::uint32_t;
std::int64_t si(U value){return value<0x80000000u?std::int64_t(value):std::int64_t(value)-0x100000000ll;}
U read(const MissionEventResourceBindings& b,U at,unsigned width=4){if(!b.read)throw std::runtime_error("Event marker read is not bound");const U value=b.read(at,width);return width==1?value&255u:value;}
void write(const MissionEventResourceBindings& b,U at,U value){if(!b.write)throw std::runtime_error("Event marker write is not bound");b.write(at,4,value);}
//Each destination selection reads the current run pointer at its source site.
void first(const MissionEventResourceBindings& b,U event,U run_field,U offset,U marker,U x_field){
 U destination=read(b,event+run_field)+offset;U value=read(b,marker+0x24);write(b,destination+x_field,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x28);write(b,destination+0x14,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x24);write(b,destination+0x18,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x28);write(b,destination+0x1c,value);
}
void bounds(const MissionEventResourceBindings& b,U event,U run_field,U offset,U marker,U x_field){
 U destination=read(b,event+run_field)+offset;U value=read(b,marker+0x24);if(si(value)<si(read(b,destination+x_field)))write(b,destination+x_field,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x28);if(si(value)<si(read(b,destination+0x14)))write(b,destination+0x14,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x24);if(si(value)>si(read(b,destination+0x18)))write(b,destination+0x18,value);
 destination=read(b,event+run_field)+offset;value=read(b,marker+0x28);if(si(value)>si(read(b,destination+0x1c)))write(b,destination+0x1c,value);
}
void markers(const MissionEventResourceBindings& b,U event,U run_field,U offset,U id,U count,U x_field){
 U index=0,marker=0x8ace80;bool found=false;
 if(si(count)>0)for(;;){const U definition=read(b,marker+4);if(definition&&read(b,definition+0x20)==0x177d&&read(b,marker+0x7c)==id){if(!found){first(b,event,run_field,offset,marker,x_field);found=true;}else bounds(b,event,run_field,offset,marker,x_field);}
  const U current_count=read(b,0x8cf900);++index;marker+=0x114;if(si(index)>=si(current_count))break;
 }
 if(!found){write(b,read(b,event+run_field)+offset+4,0);if(run_field==4)write(b,read(b,event+run_field)+offset+8,0);}
}
}
void mission_event_markers_triggers(const MissionEventResourceBindings& b){
 U event=read(b,0x4f2bcc),count=read(b,0x4f2bc8),index=0;if(si(count)<=0)return;
 for(;;){U slot=0,offset=0;for(;;){const U n=read(b,event+0x15,1);if(si(slot)>=n)break;const U entry=read(b,event+4)+offset;if(read(b,entry+4)==1&&read(b,entry+8)==10){const U marker_count=read(b,0x8cf900),id=read(b,entry+0x10);markers(b,event,4,offset,id,marker_count,0x10);}offset+=32;++slot;}
  count=read(b,0x4f2bc8);++index;event+=24;if(si(index)>=si(count))break;
 }
}
void mission_event_markers_actions(const MissionEventResourceBindings& b){
 U event=read(b,0x4f2bcc),count=read(b,0x4f2bc8),index=0;if(si(count)<=0)return;
 for(;;){U slot=0,offset=0;for(;;){const U n=read(b,event+0x16,1);if(si(slot)>=n)break;const U entry=read(b,event+8)+offset,kind=read(b,entry+4);if(kind==12||kind==13){const U marker_count=read(b,0x8cf900);const U id=read(b,read(b,event+8)+offset+0xc);markers(b,event,8,offset,id,marker_count,0xc);}offset+=32;++slot;}
  count=read(b,0x4f2bc8);++index;event+=24;if(si(index)>=si(count))break;
 }
}
}
