#!/usr/bin/env python3
"""Check raw direct/extended/compression-select sends with current memory and required imports."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10005020,0x10005120,0x10005860];SIZES={ROOT[0]:0x30,ROOT[1]:0x40,ROOT[2]:0x10};LOCAL={ROOT[0]:0x20,ROOT[1]:0x2c,ROOT[2]:0}
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image);crc=image[0x7234:0x7634]
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);ranges=[(ROOT[0],0x1000511c),(ROOT[1],0x1000523c),(ROOT[2],0x100058ce)];controls=set();returns=set()
 for start,end in ranges:
  for i in md.disasm(image[start-0x10000000:end-0x10000000],start):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns.add(i.address)
 row=None;reads=writes=calls=serial=0;trace=0;active=failed=crc_changed=False;restore=None;frames=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def source():return 0x1200000+(10 if row[0]==1 else 4)if row[15]==1 else 0x10009854 if row[15]==2 else 0x10019854 if row[15]==3 else 0 if row[15]==4 else 0x1210000
 def arg():return 0x24 if frames[-1][0]==ROOT[0]else 0x30
 def norm(at):
  for site,base,size,index in reversed(frames):
   if base<=at<base+size:return 0x30000000+index*0x100+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  events.append(('C',site,[norm(n)for n in args]));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[13]:stop();return False
  if row[10]&2048 and site==0x10005952:put(args[1],0xee,1);put(args[4]+4,row[18])
  return True
 def foreign(site):
  sp=u.reg_read(UC_X86_REG_ESP);argc={0x1000592e:1,0x10005916:0,0x10005952:6,0x1100100:4}[site];args=[get(sp+4+i*4)for i in range(argc)]
  if not call_event(site,args):return
  result=0
  if site==0x1000592e:
   if row[10]&8:put(0x10019854,0x1220000);put(0x100096b4,row[19]);put(frames[-1][1]+arg()+4,row[16]+3)
   result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif site==0x10005916:result=row[9]
  elif site==0x10005952:
   if args[2]>0x1000:stop();return
   u.mem_write(0x1219000,bytes(u.mem_read(args[1],args[2])))
   for at,value in[(0x1218000,args[0]),(0x1218004,args[1]),(0x1218008,args[2]),(0x121800c,args[3]),(0x1218010,get(args[4],2)),(0x1218014,get(args[4]+2,2)),(0x1218018,get(args[4]+4)),(0x121801c,args[5])]:put(at,value)
   result=row[8]
  else:
   u.mem_write(args[2],bytes((i*7+row[14])&255 for i in range(row[17])))
   if row[10]&16:put(frames[-1][1]+12,row[18]);put(frames[-1][1]+4,0x1220000);put(frames[-1][1]+8,row[16])
   if row[10]&1024:put(0x10019860,0)
   result=row[7]
  r.finish(result)
  if site!=0x1100100:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+argc*4)
 for site in[0x1000592e,0x10005916,0x10005952,0x1100100]:r.stubs[site]=lambda site=site:foreign(site)
 def code(uc,pc,size,_):
  nonlocal serial
  if not active:return
  if pc in ROOT[:2]and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(4 if pc==ROOT[1]else 3)]
   if not call_event(pc,args):return
   base=sp-LOCAL[pc];u.mem_write(base,b'\x5a'*LOCAL[pc]);frames.append((pc,base,SIZES[pc],serial));serial+=1
  if pc in returns and len(frames)>1:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore,crc_changed
  if not active:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(start<=pc<end for start,end in ranges):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   valid=False
   for site,base,size,index in frames:
    spans=[(0x10,0x20),(0x24,0x30)]if site==ROOT[0]else[(0x18,0x2c),(0x30,0x40)]if site==ROOT[1]else[(4,16)]
    if any(base+lo<=at and at+width<=base+hi for lo,hi in spans):valid=True
   if not valid:return
  mapped=any(base<=at and at+width<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x30000)]+[(base,size)for site,base,size,index in frames])
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(8*width))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[12]or not mapped:
    if at>=0x1000:restore=(at,get(at,width),width)
    stop();return
   site,base,_,_=frames[-1];a=arg();header=10 if site==ROOT[1]else 4
   if row[10]&1 and at==0x1200000+header:put(0x10019854,0x1220000)
   if row[10]&4 and at in[0x1200000,0x1220000]:put(base+a+8,row[18])
   if row[10]&32 and at==0x1200000+header:put(base+a+4,row[16])
   if row[10]&128 and site==ROOT[1]and at==base+0x19:put(base+0x3c,0)
   if row[10]&256 and site==ROOT[1]and at==0x1200000:put(0x10019854,0x1220000)
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[11]or not mapped:stop();return
   if row[10]&2 and not crc_changed and 0x10007234<=at<0x10007634:put(at,get(at)^1);crc_changed=True
   if row[10]&64 and at==source()and frames[-1][0]in ROOT[:2]:put(frames[-1][1]+arg(),0x1220000)
   if row[10]&512 and at==0x1001985c:put(0x10019854,0x1220000)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,width,value,user):
  if failed:return False
  memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,width,value,user);stop();return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op=0):return[op,40,0x2000,0x04030201,0x1234,1,1,24,0,0x2733,0,0,0,0,13,0,7,64,0x12345678,88]
 rng=random.Random(0x10005020);rows=[]
 for op in range(3):
  for length in[0,1,3,4,7,31,32,33,64]:
   for mutation in[0,1,2,4,8,16,32,64,128,256,512,1024,2048,4095]:x=base(op);x[1]=length;x[10]=mutation;rows.append(x)
 for op in range(3):
  for result in[0,1,MASK]:
   for error in[0,0x2733,0x2716]:x=base(op);x[8]=result;x[9]=error;rows.append(x)
 for bits in[0,1,7,8,9,23,24,25,240,0x7fffffff,0x80000000,0xfffffff8,MASK]:x=base(2);x[7]=bits;x[11]=80 if bits>=0x7fffffff else 0;rows.append(x)
 for op in range(3):
  for capacity in[0,1,3,4,7,0x7fffffff,0x80000000,MASK]:x=base(op);x[2]=capacity;rows.append(x)
  for alias in[1,2,3,4]:x=base(op);x[15]=alias;rows.append(x)
  for field,positions in[(11,list(range(1,25))+[31,40,64,80,100,128,160,200]),(12,list(range(1,18))+[31,32,33,40,64,65]),(13,[1,2,3,4,5])]:
   for n in positions:x=base(op);x[field]=n;rows.append(x)
 for op in range(3):
  for length in[0x7fffffff,0x80000000,0xfffffffe,MASK]:
   for capacity in[0,4,0x2000,0x80000000,MASK]:x=base(op);x[1]=length;x[2]=capacity;x[11]=40;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(3));x[1]=rng.choice([0,1,3,7,31,32,33,64,127,255]);x[2]=rng.choice([0,4,64,0x2000,MASK]);x[3]=rng.getrandbits(32);x[4]=rng.getrandbits(32);x[5]=rng.getrandbits(32);x[6]=rng.randrange(2);x[7]=rng.randrange(512);x[8]=rng.choice([0,1,MASK]);x[9]=rng.getrandbits(32);x[10]=rng.randrange(4096);x[14]=rng.randrange(256);x[15]=rng.randrange(4);x[16]=rng.choice([0,3,7,32,33,64]);x[18]=rng.getrandbits(32);x[19]=rng.getrandbits(32);rows.append(x)
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 for row in rows:
  reads=writes=calls=0;serial=1;trace=1469598103934665603;failed=crc_changed=False;restore=None;events=[];u.mem_write(0x10007234,crc);u.mem_write(0x1200000,bytes((i*3+row[14])&255 for i in range(0x30000)));u.mem_write(0x10009854,b'\xdd'*0x10000);u.mem_write(0x1218000,bytes(0x2000))
  for at,value in[(0x10019854,0x1200000),(0x1001985c,row[2]),(0x10019860,row[6]),(0x10019868,0xa5a50000|(row[4]&65535)),(0x100096b4,77),(0x100096a8,0x1100100)]:put(at,value)
  site=ROOT[row[0]];frame=STACK-LOCAL[site];frames=[(site,frame,SIZES[site],0)];u.mem_write(frame,b'\x5a'*SIZES[site]);args=[source(),row[1],row[3]]+([row[5]]if row[0]==1 else[]);active=True;result=0
  try:r.call(site,*args);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x1200000,0x30000),(0x10007234,1024),(0x10009854,0x10000),(0x10019854,24),(0x100096a8,4),(0x100096b4,4)]:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:100]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':[hex(x)for x in ROOT],'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
