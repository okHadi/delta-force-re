#!/usr/bin/env python3
"""Compare the eleven startup producers with the supplied Netsock instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,pefile,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,STACK
SHA='fe36bd0181d27ab48b39d55fe0e1899a63ef5cfdc07a903acce0d0cf2aafcfb2';MASK=0xffffffff
ROOT=[0x10001000,0x10001180,0x10004d90,0x10004c80,0x10004c00,0x10004af0,0x10004b30,0x10004b90,0x10004bd0,0x10004fa0,0x10005660]
SIZE=dict(zip(ROOT,[4,8,16,0x28,4,8,12,8,8,8,0x22c]));LOCAL=dict(zip(ROOT,[0,0,0,0x1c,0,8,4,8,0,0,0x224]));ARGC=dict(zip(ROOT,[0,1,3,2,0,0,1,0,1,1,1]))
SPANS={ROOT[0]:[],ROOT[1]:[(4,8)],ROOT[2]:[(4,16)],ROOT[3]:[(8,0x1c),(0x20,0x28)],ROOT[4]:[],ROOT[5]:[(0,8)],ROOT[6]:[(0,4),(8,12)],ROOT[7]:[(0,8)],ROOT[8]:[(4,8)],ROOT[9]:[(4,8)],ROOT[10]:[(8,0x224),(0x228,0x22c)]}
RANGES=list(zip(ROOT,[0x10001075,0x10001249,0x10004e19,0x10004d8a,0x10004c77,0x10004b2a,0x10004b8b,0x10004bca,0x10004bfa,0x10005008,0x1000577c]))
HASH_RANGES=[(0x10019880,0x426),(0x10019854,24),(0x100096a8,32),(0x100096c0,16),(0x100076a0,8),(0x10006000,12),(0x1000602c,8),(0x1000605c,4),(0x1200000,64),(0x120ffc0,64),(0x1210000,64),(0x121ffc0,64),(0x1220000,0x21c0),(0x1230000,0x1000)]
def run(a):
 raw=(a.game/'Netsocknl.dll').read_bytes();assert hashlib.sha256(raw).hexdigest()==SHA
 r=Rig(a.game);u=r.u;image=pefile.PE(data=raw).get_memory_mapped_image();u.mem_map(0x10000000,0x1b000);u.mem_write(0x10000000,image)
 md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls=set();returns={}
 for start,end in RANGES:
  for i in md.disasm(image[start-0x10000000:end-0x10000000],start):
   if i.mnemonic in['push','pop','call','ret']:controls.add(i.address)
   if i.mnemonic=='ret':returns[i.address]=start
 row=None;reads=writes=calls=serial=allocations=binds=sets=exports=0;trace=0;active=failed=False;restore=None;frames=[];events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,value,n=4):u.mem_write(at,(value&((1<<(8*n))-1)).to_bytes(n,'little'))
 def string(at,s):u.mem_write(at,s.encode()+b'\0')
 def frame(site):return next((base for target,base,size,index in reversed(frames)if target==site),0)
 def norm(at):
  for site,base,size,index in reversed(frames):
   if base<=at<base+size:return 0x30000000+index*0x1000+at-base
  return at
 def event(n):
  nonlocal trace
  for i in range(4):trace=((trace^((n>>(i*8))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def call_event(site,args):
  nonlocal calls
  # Only these two source boundaries consume WORD arguments. All real DWORDs remain intact.
  if site==0x10004d90:args[2]&=0xffff
  if site==0x1000592e and frame(0x10004c80):args[0]&=0xffff
  events.append(('C',site,[norm(n)for n in args]));event(3);event(site);event(len(args))
  for n in args:event(norm(n))
  calls+=1
  if calls==row[26]:stop();return False
  return True
 imports={0x1100100:(1,0),0x1100b00:(1,0),0x10001990:(0,0),0x10005922:(2,1),0x1000593a:(3,1),0x10005934:(3,1),0x1000592e:(1,1),0x10005928:(3,1),0x10005916:(0,1),0x1000591c:(5,1),0x10005910:(5,1),0x10005946:(1,1),0x10005940:(1,1),0x1100200:(2,0),0x1100300:(1,0),0x1100400:(1,1),0x1100500:(2,1),0x1100a00:(2,1),0x1100600:(1,1),0x1100700:(0,0)}
 def foreign(site):
  nonlocal allocations,binds,sets,exports
  sp=u.reg_read(UC_X86_REG_ESP);argc,stdcall=imports[site];args=[get(sp+4+i*4)for i in range(argc)]
  if not call_event(site,args):return
  result=0;m=row[23]
  if site in[0x1100100,0x1100b00]:
   allocations+=1
   if m&1:put(0x1000605c,0x1100b00)
   if m&2:put(0x100096b4,99)
   if allocations==row[14]:result=0
   elif args[0]>0x10000 or allocations>4:stop();return
   else:result=0x1200000+(allocations-1)*0x10000
  elif site==0x10001990:put(0x100199b8,0)
  elif site==0x10005922:
   if m&4 and frame(0x10004d90):put(frame(0x10004d90)+8,0x1230204)
   put(args[1],0x101,2);result=row[9]
  elif site==0x1000593a:result=row[10]
  elif site==0x10005934:
   if m&16:put(0x100096b4,99)
   result=row[11]
  elif site==0x1000592e:result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif site==0x10005928:
   binds+=1
   if m&32:
    put(0x10019858,0xffff,2)
    if frame(0x10004d90):put(frame(0x10004d90)+8,0x1230204);put(frame(0x10004d90)+12,row[3]+1)
   result=MASK if binds<=row[12]else 0
  elif site==0x10005916:result=row[13]if binds and binds<=row[12]else row[20]
  elif site==0x1000591c:
   sets+=1
   if m&128:put(0x100096b4,99)
   result=MASK if sets==row[15]else 0
  elif site==0x10005910:
   if row[16]!=MASK:put(args[3],row[17]if args[2]==0x1001 else row[18])
   if m&64 and args[2]==0x1001:put(0x10019858,0,2)
   result=row[16]
  elif site==0x10005946:
   if m&16384:string(args[0],'255.255.255.255')
   result=row[7]
  elif site==0x10005940:result=0 if row[8]else 0x1230300
  elif site==0x1100200:string(args[1]+0x14,'Cmprssh0.dll');result=MASK if row[19]==1 else 7
  elif site==0x1100300:
   if m&512 and frame(0x10005660):string(frame(0x10005660)+0x120,'OTHER.DLL')
  elif site==0x1100400:result=0 if row[22]==1 else 0x4455000
  elif site in[0x1100500,0x1100a00]:
   exports+=1
   if m&1024 and exports==1:put(0x100096b0,0x4455111);put(0x10006004,0x1100a00)
   if m&2048 and exports==3:put(0x100096a8,0)
   failure=(row[22]==2 and args[1]==0x10007674)or(row[22]==3 and args[1]==0x10007664)or(row[22]==4 and args[1]==0x10007654)
   result=0 if failure else 0x1100700 if args[1]==0x10007674 else 0x1100800 if args[1]==0x10007664 else 0x1100900
  elif site==0x1100600:result=1
  elif site==0x1100700:
   if m&4096:put(0x10019884,0);put(0x10019860,9)
  r.finish(result)
  if stdcall:u.reg_write(UC_X86_REG_ESP,u.reg_read(UC_X86_REG_ESP)+argc*4)
 def safe_foreign(site):
  try:foreign(site)
  except unicorn.UcError:stop()
 for site in imports:r.stubs[site]=lambda site=site:safe_foreign(site)
 def code(uc,pc,size,_):
  nonlocal serial
  if not active or failed:return
  if pc in ROOT and not(pc==ROOT[row[0]]and u.reg_read(UC_X86_REG_ESP)==STACK):
   sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(ARGC[pc])]
   if not call_event(pc,args):return
   if pc!=0x10004c00:
    base=sp-LOCAL[pc]
    if LOCAL[pc]:u.mem_write(base,b'\x5a'*LOCAL[pc])
    frames.append((pc,base,SIZE[pc],serial));serial+=1
  if pc in returns and len(frames)>1 and frames[-1][0]==returns[pc]:frames.pop()
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,width,value,_):
  nonlocal reads,writes,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(start<=pc<end for start,end in RANGES):return
  if 0x2000000<=at<0x2010000:
   if pc in controls:return
   if not any(any(base+lo<=at and at+width<=base+hi for lo,hi in SPANS[site])for site,base,size,index in frames):return
  mapped=any(base<=at and at+width<=base+size for base,size in[(0x10000000,0x1b000),(0x1200000,0x50000)]+[(base,size)for site,base,size,index in frames])
  m=row[23]
  if kind==unicorn.UC_MEM_WRITE:
   value&=(1<<(8*width))-1;events.append(('W',norm(at),width,norm(value)));event(2);event(norm(at));event(width);event(norm(value));writes+=1
   if writes==row[25]or not mapped:
    try:restore=(at,get(at,width),width)
    except unicorn.UcError:pass
    stop();return
   if m&8 and at==0x100076a4 and frame(0x10004d90):put(frame(0x10004d90)+4,2 if row[1]==1 else 1);put(frame(0x10004d90)+8,0x1230204)
   if m&256 and at==0x1001985c:put(0x1000605c,0x1100b00)
   if m&2048 and at==0x100096a8:put(0x100096b0,0x4455222)
   if m&8192 and at==0x10019888 and frame(0x10001180):put(frame(0x10001180)+4,0x1230100)
  else:
   events.append(('R',norm(at),width));event(1);event(norm(at));event(width);reads+=1
   if reads==row[24]or not mapped:stop();return
   if m&32768 and at==0x1230400:put(0x123030c,0x1230420)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,width,value,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,width,value,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def base(op=0):return[op,1,3874,3874,1,0,0,0x0100007f,0,0,55,0,0,0x2740,0,0,0,0x2000,0x4000,0,0x2733,1,0,0,0,0,0,13,7,0,0x10000,0x04030201]
 rows=[];rng=random.Random(0x10001000)
 for op in range(11):
  for mutation in[0]+[1<<i for i in range(16)]+[65535]:x=base(op);x[23]=mutation;rows.append(x)
 for op in range(11):
  for field,values in[(4,[0]),(5,[1]),(6,[1]),(9,[1,MASK]),(10,[MASK]),(11,[MASK]),(12,[1,2,3]),(13,[0,0x2740,0x2733]),(14,[1,2,3]),(15,[1,2,3]),(16,[MASK]),(19,[1,2]),(21,[0,2,3]),(22,[1,2,3,4]),(29,[0xa5a50000]),(2,[0,0xffff]),(17,[0x7fffffff,0x80000000,MASK]),(18,[0x7fffffff,0x80000000,MASK])]:
   for value in values:x=base(op);x[field]=value;x[1]=2 if field in[12,13,29]else x[1];x[7]=MASK if field==21 and value in[2,3]else x[7];rows.append(x)
  for field,positions in[(24,list(range(1,25))+[31,40,60,100]),(25,list(range(1,20))+[265,266,267,268,269,1000,2430]),(26,list(range(1,15)))]:
   for n in positions:x=base(op);x[field]=n;rows.append(x)
 for _ in range(a.cases):
  x=base(rng.randrange(11));x[1]=rng.choice([1,2]);x[2]=rng.randrange(65536);x[3]=rng.getrandbits(32);x[6]=rng.randrange(2);x[7]=rng.choice([MASK,rng.getrandbits(32)]);x[8]=rng.randrange(2);x[12]=rng.randrange(4);x[14]=rng.randrange(5);x[15]=rng.randrange(5);x[16]=rng.choice([0,MASK]);x[17]=rng.choice([0,1,0x2000,0x80000000,MASK]);x[18]=rng.choice([0,1,0x4000,0x7fffffff,MASK]);x[19]=rng.randrange(3);x[21]=rng.randrange(4);x[22]=rng.randrange(5);x[23]=rng.randrange(65536);x[27]=rng.randrange(256);x[28]=rng.getrandbits(32);x[29]=rng.getrandbits(32)&0xffff0000;x[31]=rng.getrandbits(32);rows.append(x)
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 for row in rows:
  reads=writes=calls=allocations=binds=sets=exports=0;serial=1;trace=1469598103934665603;failed=False;restore=None;events=[]
  u.mem_write(0x1200000,bytes([row[27]])*0x50000);u.mem_write(0x10019880,bytes(0x426))
  for at,value in[(0x1000605c,0x1100100),(0x1000602c,0x1100200),(0x10006030,0x1100300),(0x10006008,0x1100400),(0x10006004,0x1100500),(0x10006000,0x1100600),(0x10019880,row[4]),(0x10019884,row[5]),(0x100199b8,row[6]),(0x10019c74,1 if row[1]==2 else 0),(0x10019854,0x1240000),(0x1001985c,0x55aa55aa),(0x10019860,row[28]),(0x100096a8,0x11111111),(0x100096ac,0x22222222),(0x100096b0,0x33333333),(0x100096b4,55),(0x100096b8,9),(0x100076a4,(row[29]&0xffff0000)|0x1000),(0x1230200,0xa5a50000|(row[2]&0xffff)),(0x1230204,0x5a5a0000|((row[2]+1)&0xffff)),(0x123030c,0x1230400),(0x1230400,0x1230500),(0x1230420,0x1230510),(0x1230500,row[31]),(0x1230510,row[31]^MASK)]:put(at,value)
  for at,value in[(0x10019c72,row[2]),(0x10019c70,row[3]),(0x10019858,row[2]),(0x10019868,row[3])]:put(at,value,2)
  string(0x1230000,'255.255.255.255'if row[21]==3 else'peer.local'if row[21]==2 else'127.0.0.1');string(0x1230100,'Cmprssh0.dll')
  op=row[0];site=ROOT[op];base_at=STACK-LOCAL[site];frames=[(site,base_at,SIZE[site],0)];u.mem_write(base_at,b'\x5a'*SIZE[site]);args=[]
  if op in[1,9]:args=[0x1230000 if row[21]else 0]
  if op==2:args=[row[1],0x1230200,row[3]]
  if op==3:args=[row[1],0x1230200]
  if op in[6,8]:args=[row[30]]
  if op==10:args=[0x1230100 if row[19]==2 else 0]
  active=True;result=0
  try:r.call(site,*args,count=4000000);result=u.reg_read(UC_X86_REG_EAX)
  except(unicorn.UcError,RuntimeError):
   if not failed:print('FAIL',row,hex(u.reg_read(UC_X86_REG_EIP)));raise
  finally:active=False
  if failed:result=0
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in HASH_RANGES:
   for value in u.mem_read(at,n):state=((state^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {reads} {writes} {calls} {trace} {state}');all_events.append(events)
 p=subprocess.run([str(a.probe),str(a.game/'Netsocknl.dll')],input='\n'.join(' '.join(map(str,x))for x in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,(p.returncode,p.stderr);lines=p.stdout.splitlines();assert len(lines)==len(rows),(len(lines),len(rows));bad=[{'case':i,'input':rows[i],'actual':x,'expected':y,'original_events':all_events[i][:100]}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SHA,'bodies':[hex(x)for x in ROOT],'cases':len(rows),'differences':len(bad),'mismatches':bad[:5],'word_boundaries':{'10001180->10004D90':'argument3 WORD','10004C80->1000592E':'argument1 WORD'},'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);p.add_argument('--limit',type=int,default=0);run(p.parse_args())
