#!/usr/bin/env python3
"""Compare complete DF ready, resupply and reject bodies with original x86."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff
ROOT=[0x446a30,0x447e80,0x447fb0];RANGES=[(0x446a30,0x446d93),(0x447e80,0x447f60),(0x447fb0,0x44803f)]
IMPORTS={0x49a620:3,0x493a80:3,0x404d80:2,0x441750:4,0x447e80:1,0x447fb0:1,0x404cf0:2,0x479f00:2}
def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for lo,hi in RANGES for i in md.disasm(bytes(u.mem_read(lo,hi-lo)),lo)if i.mnemonic in['push','pop','call','ret']};frame=STACK;active=failed=False;x=[];nr=nw=nc=trace=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def client():return(0x6e9a90+x[1]*0x9b84)&MASK
 def packet():return client()+0x2c if x[18]==1 else frame if x[18]==2 else 0x1220000
 def norm(at):return 0x30000000+at-frame if frame<=at<frame+0x400 else at
 def ev(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def mapped(at,n):return any(lo<=at and at+n<=lo+size for lo,size in[(0x400000,0xa00000),(0x1200000,0x50000),(frame,0x400)])
 def foreign(s):
  nonlocal nc
  sp=u.reg_read(UC_X86_REG_ESP);count=4 if s==0x493a80 and x[0]==2 else IMPORTS[s];args=[get(sp+4+i*4)for i in range(count)];events.append(['C',s,[norm(v)for v in args]]);ev(3);ev(s);ev(len(args))
  for v in args:ev(norm(v))
  nc+=1
  if nc==x[17]:stop();return
  if s==0x49a620:
   u.mem_write(args[0],bytes([args[1]&255])*args[2])
   if x[14]&1:put(client()+8,0x7ccb94)
  elif s==0x493a80:put(args[0],ord('W'),1);put(args[0]+1,0,1)
  elif s==0x404d80:
   if x[14]&2:put(frame+0x124,0x1221000);put(frame+0x120,(x[1]+1)%17)
   if x[14]&16:put(frame+0x48,1,2)
  elif s==0x441750:
   if x[14]&4:
    for i in range(9):put(0xbd67d4+i*4,get(get(frame+0x124)+0xba+i*4))
  elif s==0x447e80:
   if x[14]&8:put(frame+0x124,0x1221000)
  r.finish(0x1230000 if s==0x404d80 else 0x1230100 if s==0x404cf0 else 0)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 # The resupply/reject entry is real when it is the selected root, foreign otherwise.
 def code(uc,pc,n,_):
  if active and not failed and pc in[0x447e80,0x447fb0]and ROOT[x[0]]==pc:r.stubs.pop(pc,None)
 u.hook_add(unicorn.UC_HOOK_CODE,code)
 def memory(uc,kind,at,n,v,_):
  nonlocal nr,nw,restore
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not any(lo<=pc<hi for lo,hi in RANGES):return
  if x[0]==1 and pc==0x447e84:return
  semantic=(frame<=at and at+n<=frame+(0x10c if x[0]==0 else 0x40 if x[0]==2 else 0))or(x[0]==0 and frame+0x120<=at and at+n<=frame+0x128)or(x[0]==2 and frame+0x48<=at and at+n<=frame+0x4c)
  if 0x2000000<=at<0x2010000 and(pc in controls or not semantic):return
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);ev(2);ev(norm(at));ev(n);ev(norm(v));nw+=1
   if nw==x[16]or not mapped(at,n):
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if x[14]&64 and client()+0xd4<=at<client()+0x114:
    for i in range(16):put(0x1200000+i*0xa8+0x74,(i+1)%16)
  else:
   events.append(['R',norm(at),n]);ev(1);ev(norm(at));ev(n);nr+=1
   if nr==x[15]or not mapped(at,n):stop();return
   if x[14]&32 and 0x1200000<=at<0x1201000 and(at-0x1200000)%0xa8==0x74:put(0xad3ee4,0x1210100)
   if x[14]&128 and 0xbd67d4<=at<=0xbd67f4:put(frame+0x124,0x1221000)
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def default(op):return[op,1,1,0,2,5,1,1,1,1,1,7,9,9,0,0,0,0,0,0]
 rows=[]
 for op in range(3):
  for field,values in[(1,[0,1,16]),(2,[0,1,2,3]),(3,[0,1,511,512]),(4,[0,1,8,9,0x7fff,0x8000,0xffff]),(5,[0,1,10,11,12,32]),(6,[0,1]),(7,[0,1]),(8,[0,1,2]),(9,[0,1,2,MASK]),(10,[0,1,2,255]),(11,[0,1,0x80000000,MASK]),(12,[0,2,0x80000000,MASK]),(13,list(range(10))),(14,[0,1,2,4,8,16,32,64,128,255]),(18,[0,1,2])]:
   for v in values:y=default(op);y[field]=v;rows.append(y)
  for field in[15,16,17]:
   for v in range(1,100 if field!=17 else 12):y=default(op);y[field]=v;rows.append(y)
 rng=random.Random(0x446a30)
 for _ in range(a.cases):
  y=default(rng.randrange(3));y[1]=rng.randrange(17);y[2]=rng.randrange(4);y[3]=rng.choice([0,1,511,512]);y[4]=rng.choice([0,1,2,8,9,0xffff]);y[5]=rng.randrange(33);y[8]=rng.randrange(3);y[11]=rng.getrandbits(32);y[12]=rng.getrandbits(32);y[13]=rng.randrange(10);y[14]=rng.choice([0,1,2,4,8,16,32,64,128,255]);rows.append(y)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,x in enumerate([]if a.replay_oracle else rows):
  frame=STACK-(0x11c if x[0]==0 else 0x44 if x[0]==2 else 0);nr=nw=nc=0;trace=1469598103934665603;failed=False;restore=None;events=[]
  for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
  r.stubs.pop(ROOT[x[0]],None)
  for at,n in[(0x7cca80,0x228),(0x1200000,0x50000),(frame,0x400),(0x67db40,4)]+[(0x6e9a90+i*0x9b84,0x200)for i in range(17)]:u.mem_write(at,b'\x5a'*n)
  for s in range(17):
   c=0x6e9a90+s*0x9b84;put(c+8,0x7cca80);put(c+0x18c,1,1)
   for i in range(10):put(c+0xac+i*4,i)
   put(c+0x168,x[8])
  for at in[0x7cca80,0x7ccb94]:
   put(at+0xc,0x1240000);put(at+0x6c,0x83,1);put(at+0x70,x[2],2)
   for i in range(4):put(at+0x24+i*4,((0xffff0000 if at==0x7cca80 else 0x76540000)+i*0x10000)&MASK)
  for i in range(512):put(0x78f280+i*0x88,0,1)
  if x[3]<512:
   at=0x78f280+x[3]*0x88;put(at,2,1);put(at+4,x[4],2)
   for i in range(8):put(at+8+i*4,77+i,2)
  for i in range(8):put(0x8cf9b4+i*4,100+i)
  put(0xad3ed0,0x1200000);put(0xad3ee4,0x1210000)
  for i in range(16):
   put(0x1200000+i*0xa8+0x74,i);put(0x1200000+i*0xa8+0x6c,x[9]);put(0x1200000+i*0xa8+0xa4,x[10],1);put(0x1210000+i*0x34+4,x[11]);put(0x1210000+i*0x34+8,x[12])
  for at in[packet(),0x1221000]:
   u.mem_write(at,bytes(0xe2));put(at,65,1);put(at+0x20,66,1)
   for i in range(9):put(at+0xba+i*4,50+i)
  for i in range(9):put(0xbd67d4+i*4,50+i+(x[13]==i))
  put(0x67f520,x[6]);put(0x67f528,x[7]);put(0x67db40,1234);put(frame+0x120,x[1]);put(frame+0x124,packet());put(frame+0x48,x[1])
  for i in range(x[5]+1):put(0x1230000+i,65+i%26 if i<x[5]else 0,1)
  args=[x[1],packet()]if x[0]==0 else[client()]if x[0]==1 else[x[1]];active=True
  try:r.call(ROOT[x[0]],*args,count=1000000)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,x,hex(u.reg_read(UC_X86_REG_EIP)),events[-20:]))
  finally:active=False
  if restore:put(*restore)
  state=1469598103934665603;ranges=[(0x67db40,4),(0x7cca80,0x228),(0x1200000,0x50000),(0xbd67d4,36)]+[(0x6e9a90+i*0x9b84,0x200)for i in range(17)]
  if x[0]==0:ranges.extend([(frame,0x10c),(frame+0x120,8)])
  if x[0]==2:ranges.extend([(frame,0x40),(frame+0x48,4)])
  for at,n in ranges:
   values=bytearray(u.mem_read(at,n))
   if x[0]==0 and at==frame+0x120:values[4:8]=norm(get(frame+0x124)).to_bytes(4,"little")
   for v in values:state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {nr} {nw} {nc} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,y))for y in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':v,'expected':e,'original_events':all_events[i]}for i,(v,e)in enumerate(zip(actual,expected))if v!=e];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
