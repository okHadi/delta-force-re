#!/usr/bin/env python3
"""Check the current dispatch/gateway/lookups/queue wrapper against the original DLL."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x100046c0,0x10004420,0x10003fe0,0x100041d0,0x10004220];SIZES={ROOT[0]:12,ROOT[1]:16,ROOT[2]:12,ROOT[3]:12,ROOT[4]:20,0x10003b20:28};ARGC={site:size//4-1 for site,size in SIZES.items()}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);tables=image[0x477c:0x4811]
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);ranges=[(0x100046c0,0x1000477c),(0x10004420,0x1000448a),(0x10003fe0,0x10004017),(0x100041d0,0x10004220),(0x10004220,0x10004244),(0x10003b20,0x10003c00)];controls=set();returns=set()
 for start,end in ranges:
  for i in md.disasm(image[start-0x10000000:end-0x10000000],start):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns.add(i.address)
 row=None;reads=writes=calls=serial=0;trace=0;failed=active=False;restore=None;frames=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):
  for base,size,index in reversed(frames):
   if base<=at<base+size:return 0x30000000+index*0x100+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  events.append(('C',site,[norm(n)for n in args]));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[17]:stop();return False
  if row[14]&8 and site==0x10003fe0:
   for base,size,index in frames:
    if size==16:put(base+4,0x1212000);put(base+8,row[3]^255);put(base+12,row[4]^255)
  if row[14]&1024 and site==0x100041d0:put(0x10019c80,1)
  if row[14]&2048 and site==0x10004220:put(args[0],0xee,1)
  return True
 def foreign(site):
  sp=u.reg_read(UC_X86_REG_ESP);n=1 if site in[0x1100100,0x1100200,0x1100300,0x10002180]else 2 if site==0x10003aa0 else 3;args=[get(sp+4+i*4)for i in range(n)]
  if not call_event(site,args):return
  result=0x1234
  if site==0x1100100:
   if row[14]&512:put(frames[-1][0]+4,0x1212008)
   if row[18]:result=0
   elif args[0]>0x2000:stop();return
   else:result=0x1230000
  if site==0x1100300:put(0x10019b1c,0)
  r.finish(result)
  if site==0x1100300:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+4)
 for site in[0x1100100,0x1100200,0x1100300,0x10003aa0,0x10003f00,0x100040c0,0x10004340,0x10004550,0x10004660,0x100029d0,0x10002180]:r.stubs[site]=lambda site=site:foreign(site)
 def code(uc,pc,size,_):
  nonlocal serial
  if not active:return
  if pc==0x1100900:stop();return
  if pc in SIZES and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not call_event(pc,args):return
   frames.append((sp,SIZES[pc],serial));serial+=1
  if pc in returns and len(frames)>1:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(start<=pc<end for start,end in ranges):return
  if 0x2000000<=at<0x2010000 and(pc in controls or not any(base+4<=at and at+width<=base+size for base,size,index in frames)):return
  mapped=any(base<=at and at+width<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x50000)]+[(base,size)for base,size,index in frames])
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(width*8))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[16]or not mapped:
    if at>=0x1000:restore=(at,get(at,width),width)
    stop();return
   if row[14]&16 and 0x1220000<=at<0x1220048 and(at-0x1220000)%24==8:put(0x10019afc,0x1223000)
   if row[14]&32 and 0x1222000<=at<0x1224000 and(at-0x1222000)%0xbc==0xb0:put(0x10019c80,row[8]^1);put(at-16,row[10]^255)
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[15]or not mapped:stop();return
   if row[14]&1 and at==0x1210000:put(0x10019c5c,0x1212000)
   if row[14]&2 and at==0x100198b8:put(0x10019af8,0x1220018)
   if row[14]&4 and 0x1220000<=at<0x1220048 and(at-0x1220000)%24==0:put(0x100198b8,0)
   if row[14]&64 and 0x1222000<=at<0x1224000 and(at-0x1222000)%0xbc==0xa0:
    for base,size,index in frames:
     if size==16:put(base+4,0x1212000)
   if row[14]&128 and 0x1224000<=at<0x1224048 and(at-0x1224000)%24==4:
    for i in range(3):put(0x1222000+i*0xbc+0xa4,0x1224080)
   if row[14]&256 and 0x1000477c<=at<0x100047a0:put(STACK+4,row[3]^0x11);put(STACK+8,row[4]^0x22)
   if row[14]&4096 and at==0x1210002:
    for base,size,index in frames:
     if size==16:put(base+4,0x1212000)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,width,value,user):
  if failed:return False
  if kind==unicorn.UC_MEM_FETCH_UNMAPPED:stop();return False
  memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,width,value,user);stop();return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED|unicorn.UC_HOOK_MEM_FETCH_UNMAPPED,invalid)
 def base(op=0):return[op,0x1080,19,0x04030201,0x1234,3,1,2,0,7,0x10002,1,1,7,0,0,0,0,0,MASK,0,13,0,0]
 rng=random.Random(0x100046c0);rows=[]
 for opcode in list(range(0x100f,0x1082))+[0,0xffff]:x=base();x[1]=opcode;rows.append(x)
 for op in range(5):
  for mutation in[0,1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8191]:x=base(op);x[14]=mutation;rows.append(x)
 for op in[0,1,2,3]:
  for count in[0,1,2,3]:
   for match in[0,1,2,MASK]:x=base(op);x[5]=count;x[6]=x[7]=match;rows.append(x)
 for length in[0,1,7,8,11,12,13,19,40,0xffff]:
  for gate in[0,1]:x=base();x[2]=length;x[8]=gate;rows.append(x)
 for choice in range(9):x=base();x[19]=choice;rows.append(x)
 for branch in[0x1000477b,0x10004774,0x100046e8,0x1000474c,0x1100900]:x=base();x[20]=branch;rows.append(x)
 for field,limit in[(15,90),(16,24),(17,9)]:
  for n in range(1,limit):x=base();x[field]=n;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(5));x[2]=rng.choice([0,12,13,19,40,0xffff]);x[5]=rng.randrange(4);x[6]=rng.choice([0,1,2,MASK]);x[7]=rng.choice([0,1,2,MASK]);x[8]=rng.randrange(2);x[9]=rng.choice([0,7,0x7fffffff,0x80000000,MASK]);x[11]=rng.choice([0,1,0x800,0xff6,MASK]);x[12]=rng.randrange(3);x[13]=rng.choice([0,1,7,19]);x[14]=rng.randrange(8192);x[18]=int(rng.randrange(9)==0);x[21]=rng.randrange(256);x[22]=rng.randrange(2);x[23]=int(rng.randrange(9)==0);rows.append(x)
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 for row in rows:
  reads=writes=calls=0;serial=1;trace=1469598103934665603;failed=False;restore=None;events=[];u.mem_write(0x1200000,bytes(0x50000));u.mem_write(0x1000477c,tables)
  u.mem_write(0x1210000,bytes((i+row[21])&255 for i in range(0x4000)))
  for at in[0x1210000,0x1212000]:put(at,row[1],2);put(at+2,row[2]+int(at==0x1212000),2)
  for at,value in[(0x10019c5c,0x1210000),(0x100198b8,row[5]),(0x10019af8,0x1220000),(0x10019afc,0x1222000),(0x10019c80,row[8]),(0x1000605c,0x1100100),(0x10006068,0x1100200),(0x10006010,0x1100300),(0x10019b0c,0x1200000),(0x10019b10,0x1200000),(0x10019b14,0x1200000),(0x10019b18,row[11]),(0x10019b1c,row[22])]:put(at,value)
  for i in range(3):
   c=0x1220000+i*24;put(c,1);put(c+4,row[3]^(0 if i==row[6]else 1));put(c+8,0xfeed);put(c+20,row[4]^(0 if i==row[6]else 1));other=0x1224000+i*24;put(other+4,row[3]^(0 if i==row[7]else 1));put(other+20,row[4]^(0 if i==row[7]else 1))
   for p in[0x1222000+i*0xbc,0x1223000+i*0xbc]:
    for off,value in[(0x98,1),(0x9c,1),(0xa0,row[10]),(0xa4,0 if row[23]else other),(0xb0,row[9])]:put(p+off,value)
  index=(row[1]-0x1010)&MASK
  if index<=0x70:
   if row[19]!=MASK:put(0x100047a0+index,row[19],1)
   which=get(0x100047a0+index,1)
   if row[20]:put(0x1000477c+which*4,row[20])
  size=16 if row[0]==1 else 20 if row[0]==4 else 12;frames=[(STACK,size,0)];u.mem_write(STACK,b'\x5a'*size);args=[row[3],row[4]]if row[0]in[0,2,3]else[0x1210000,row[3],row[4]]if row[0]==1 else[0x1210008,row[13],row[10],row[12]];active=True;result=0
  try:r.call(ROOT[row[0]],*args);result=u.reg_read(UC_X86_REG_EAX)if row[0]in[2,3]else 0
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x1200000,32),(0x1210000,0x4000),(0x1220000,0x5000),(0x1230000,0x2000),(0x100198b8,4),(0x10019af8,8),(0x10019b0c,20),(0x10019c5c,4),(0x10019c80,4),(0x1000477c,0x95)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:100]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':[hex(x)for x in ROOT],'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
