#!/usr/bin/env python3
"""Check raw receiver reads, stores, calls and bytes against shipped DLL instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
from check_network_receive_original import Scanner,ENCODE_SIMPLE,ENCODE_EXTENDED,build_retry_packet
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x100052e0,0x10005280,0x10005830]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for start,end in[(0x5280,0x565e),(0x5830,0x5854)]for i in md.disasm(image[start:end],0x10000000+start)if i.mnemonic in['push','pop','call','ret']}
 crc=image[0x7234:0x7634];row=None;wire=b'';reads=writes=calls=0;trace=0;failed=active=expanded=crc_changed=False;restore=None;frames=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):
  for i,(base,size)in enumerate(frames):
   if base<=at<base+size:return 0x30000000+i*0x100+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  events.append(('C',site,args));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[14]:stop();return False
  return True
 def foreign(site):
  nonlocal expanded
  sp=u.reg_read(UC_X86_REG_ESP);argc={0x1000595e:6,0x10005916:0,0x10005958:1,0x1000592e:1,0x10005934:3,0x1100100:3}[site];args=[get(sp+4+i*4)for i in range(argc)]
  if not call_event(site,args):return
  result=0
  if site==0x1000595e:
   if row[1]!=MASK:
    u.mem_write(args[1],wire);put(args[4],2,2);put(args[4]+2,row[5],2);put(args[4]+4,row[4]);put(args[5],16)
   if row[11]&1:put(0x10019854,0x1202000)
   result=row[1]
  elif site==0x10005916:result=row[2]
  elif site==0x10005958:
   if row[11]&256:put(0x100096b8,1);put(0x100076a8,0);put(0x100076ac,MASK)
   result=int.from_bytes(args[0].to_bytes(4,'little'),'big')
  elif site==0x1000592e:
   if row[11]&16:put(frames[0][0]+0x40,0x1220014);put(frames[0][0]+0x44,0x1220018)
   result=int.from_bytes((args[0]&65535).to_bytes(2,'little'),'big')
  elif site==0x10005934:put(args[2],row[16]);result=row[15]
  else:
   expanded=True;u.mem_write(args[1],bytes((i*7+row[17])&255 for i in range(row[10])));result=row[10]
   if row[11]&32:put(frames[0][0]+0x24,0x12345678);put(frames[0][0]+0x22,0x4321,2)
  r.finish(result)
  if site!=0x1100100:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+argc*4)
 for site in[0x1000595e,0x10005916,0x10005958,0x1000592e,0x10005934,0x1100100]:r.stubs[site]=lambda site=site:foreign(site)
 def code(uc,pc,size,_):
  if active and pc==0x10005280 and row[0]==0:
   sp=u.reg_read(UC_X86_REG_ESP)
   # Log the direct foreign boundary before its mapped nested frame exists.
   if not call_event(pc,[get(sp+4)]):return
   frames.append((sp,8))
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore,crc_changed
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not(0x10005280<=pc<0x1000565e or 0x10005830<=pc<0x10005854):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   valid=False
   if row[0]==0:valid=frames[0][0]+0x10<=at<frames[0][0]+0x30 or frames[0][0]+0x34<=at<frames[0][0]+0x48
   elif row[0]==1:valid=STACK+4<=at<STACK+8
   else:valid=STACK-4<=at<STACK
   if len(frames)>1 and frames[1][0]+4<=at<frames[1][0]+8:valid=True
   if not valid:return
  mapped=any(base<=at and at+width<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x4000),(0x1210000,0x4000),(0x1220000,0x100)]+frames)
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(width*8))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[13]or not mapped:restore=(at,get(at,width),width);stop();return
   if row[11]&4 and at==0x1210000:put(0x10019854,0x1202000)
   if row[11]&8 and at==0x1220000:put(frames[0][0]+0x40,0x1220014)
   if row[11]&64 and expanded and at==0x1210000:put(frames[0][0]+0x24,0xabcdef01);put(frames[0][0]+0x22,0x9876,2)
   if row[11]&128 and not expanded and at==0x1210000:put(frames[0][0]+0x34,0x1212000)
   if row[11]&512 and at==frames[0][0]+0x10:put(frames[0][0]+0x38,0)
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[12]or not mapped:stop();return
   if row[11]&2 and not crc_changed and 0x10007234<=at<0x10007634:put(at,get(at)^1);crc_changed=True
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,width,value,user):
  memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,width,value,user);stop();return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 rng=random.Random(0x100052e0);scanner=Scanner(raw);cases=[]
 def base(op=0):return[op,0,0,4096,0x04030201,0x3412,0,0,0,1,7,0,0,0,0,0,63,13]
 packets=[]
 for length in[1,3,4,7,15,31,32,33,40,64,127,255]:
  payload=bytes(rng.randrange(256)for _ in range(length));packets.append(scanner.encode(ENCODE_SIMPLE,payload))
  for flag in[0,1]:packets.append(scanner.encode(ENCODE_EXTENDED,payload,flag=flag))
 packets+=[build_retry_packet(5)[0],bytes(4),bytes(10),bytes(range(40)),b'']
 for packet in packets:
  for mutation in[0,1,2,4,8,16,32,64,128,512,1023]:
   x=base();x[1]=len(packet);x[11]=mutation;cases.append((x,packet))
 for error in[0,1,0x2733,0x2736,MASK]:x=base();x[1]=MASK;x[2]=error;cases.append((x,b''))
 for count in[0,1,3,0x80000000,MASK]:
  for ip in[0,0x01000000,0x04030201,MASK]:
   for mutation in[0,256]:x=base(1);x[4]=ip;x[6]=count;x[7]=0x01020304;x[8]=0x100;x[11]=mutation;cases.append((x,b''))
 for result in[0,1,MASK,0xfffffffe]:
  for count in[0,1,64,MASK]:x=base(2);x[15]=result;x[16]=count;cases.append((x,b''))
 for field,limit in[(12,70),(13,42),(14,6)]:
  for n in range(1,limit):x=base();x[1]=len(packets[23]);x[field]=n;cases.append((x,packets[23]))
 for packet in[packets[0],packets[23],packets[-5],packets[-4]]:
  for field,positions in[(12,[80,99,100,101,127,128,129,160,200,250,300,400,512,700]),(13,[43,44,45,63,64,65,80,99,100,101,128]),(14,[1,2,3,4,5])]:
   for n in positions:x=base();x[1]=len(packet);x[field]=n;cases.append((x,packet))
 for length in[0,1,2,3,4,5,7,8,9,16,17,31,32,33,127]:x=base();x[1]=len(packets[23]);x[10]=length;cases.append((x,packets[23]))
 for capacity in[0,1,3,7,0x7fffffff,0x80000000,MASK]:
  for received in[0,1,4,9,10,0x80000000,0xfffffffe]:x=base();x[1]=received;x[3]=capacity;cases.append((x,packets[1]))
 for _ in range(a.cases):
  x=base();packet=rng.choice(packets);x[1]=len(packet);x[3]=rng.choice([0,1,7,4096,0x80000000,MASK]);x[6]=rng.randrange(4);x[7]=rng.choice([0,0x01020304,MASK]);x[8]=rng.choice([0,20,MASK]);x[9]=rng.randrange(2);x[10]=rng.randrange(20);x[11]=rng.randrange(1024);x[17]=rng.randrange(256);cases.append((x,packet))
 if a.limit:cases=cases[:a.limit]
 expected=[];all_events=[]
 for row,wire in cases:
  reads=writes=calls=0;trace=1469598103934665603;failed=expanded=crc_changed=False;restore=None;events=[]
  u.mem_write(0x10007234,crc);u.mem_write(0x1200000,bytes((i+row[17])&255 for i in range(0x4000)));u.mem_write(0x1202000,wire);u.mem_write(0x1210000,b'\xcc'*0x4000);u.mem_write(0x1220000,b'\xa5'*0x100);u.mem_write(0x10009854,b'\xdd'*0x10000)
  for at,value in[(0x100096b4,77),(0x10019854,0x1200000),(0x1001985c,0x2000),(0x100096b8,row[6]),(0x10019860,row[9]),(0x100096ac,0x1100100)]:put(at,value)
  for i in range(3):put(0x100076a8+i*8,row[7]+i*0x100);put(0x100076ac+i*8,row[8])
  frame=STACK-0x30 if row[0]==0 else STACK if row[0]==1 else STACK-4;frames=[(frame,0x48 if row[0]==0 else 8 if row[0]==1 else 4)];u.mem_write(frame,b'\x5a'*frames[0][1]);args=[0x1210000,row[3],0x1220000,0x1220004,0x1220008]if row[0]==0 else[row[4]]if row[0]==1 else[]
  active=True;result=0
  try:r.call(ROOT[row[0]],*args);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print("ORIGINAL_FAILURE",row,"pc",hex(u.reg_read(UC_X86_REG_EIP)),"wire",list(wire));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x1200000,0x4000),(0x1210000,0x4000),(0x1220000,0x40),(0x10019854,16),(0x10007234,1024),(0x100076a8,24),(0x10009854,0x100)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x+[len(wire)]+list(wire)))for x,wire in cases)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(expected),(len(lines),len(expected),p.stderr)
 bad=[{'case':i,'input':cases[i][0],'wire':list(cases[i][1]),'actual':x,'expected':y,'original_events':all_events[i][:50]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':['100052E0','10005280','10005830'],'cases':len(cases),'differences':len(bad),'mismatches':bad[:5],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(cases),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
