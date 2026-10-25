#!/usr/bin/env python3
"""Replay the complete46BFB0 producer with ordered memory and call failures."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import capstone,unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP
from check_projectiles_original import Rig,STACK
SHA='abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff';MASK=0xffffffff;LO=0x46bfb0;HI=0x46c175
IMPORTS={0x493a80:3,0x404cf0:2,0x4828c0:1}
def run(a):
 assert hashlib.sha256((a.game/'Df.exe').read_bytes()).hexdigest()==SHA;r=Rig(a.game);u=r.u;md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);controls={i.address for i in md.disasm(bytes(u.mem_read(LO,HI-LO)),LO)if i.mnemonic in['push','pop','call','ret']};frame=STACK-0x30;active=failed=False;x=[];nr=nw=nc=trace=formats=lookups=graphics=fills=0;restore=None;events=[]
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def put(at,v,n=4):u.mem_write(at,(v&((1<<(8*n))-1)).to_bytes(n,'little'))
 def norm(at):return 0x30000000+at-frame if frame<=at<frame+0x100 else at
 def ev(v):
  nonlocal trace
  for i in range(4):trace=((trace^((v>>(8*i))&255))*1099511628211)&0xffffffffffffffff
 def stop():
  nonlocal failed
  failed=True;u.emu_stop()
 def toggle():put(0xad3ed0,0x1200000 if get(0xad3ed0)==0x4f9934 else 0x4f9934)
 def mapped(at,n):return any(lo<=at and at+n<=lo+size for lo,size in[(0x400000,0xa00000),(0x1200000,0x50000),(frame,0x100)])
 def foreign(s):
  nonlocal nc,formats,lookups,graphics
  sp=u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(IMPORTS[s])];events.append(['C',s,[norm(v)for v in args]]);ev(3);ev(s);ev(len(args))
  for v in args:ev(norm(v))
  nc+=1
  if nc==x[7]:stop();return
  result=0
  if s==0x493a80:
   formats+=1
   if formats==1:put(0xad3edc,x[0])
   put(args[0],ord('K'),1);put(args[0]+1,0,1)
  elif s==0x404cf0:
   if x[4]&1:toggle()
   if x[11]:result=frame
   else:lookups+=1;result=0x1240000 if lookups%2 else 0x1240100
  elif s==0x4828c0:
   graphics+=1
   if x[4]&2:toggle()
   if x[4]&8 and graphics%2==0:put(0xad3edc,x[8])
   result=(args[0]+x[3])&MASK
  r.finish(result)
 for s in IMPORTS:r.stubs[s]=lambda s=s:foreign(s)
 def memory(uc,kind,at,n,v,_):
  nonlocal nr,nw,restore,fills
  if not active or failed:return
  pc=u.reg_read(UC_X86_REG_EIP)
  if not LO<=pc<HI:return
  if 0x2000000<=at<0x2010000 and(pc in controls or not(frame<=at and at+n<=frame+0x20)):return
  if kind==unicorn.UC_MEM_WRITE:
   v&=(1<<(8*n))-1;events.append(['W',norm(at),n,norm(v)]);ev(2);ev(norm(at));ev(n);ev(norm(v));nw+=1
   if nw==x[6]or not mapped(at,n):
    try:restore=(at,get(at,n),n)
    except unicorn.UcError:pass
    stop();return
   if x[4]&4 and((0x4f9934<=at<0x4fb2cc and(at-0x4f9934)%0xa8==0x94)or(0x1200000<=at<0x1201998 and(at-0x1200000)%0xa8==0x94)):toggle()
   if 0xad3dd0<=at<=0xad3ec8:
    if fills<63:
     fills+=1
     if fills==63 and x[4]&16:put(0xad3edc,x[8])
    elif x[4]&32:put(0xad3edc,0)
  else:
   events.append(['R',norm(at),n]);ev(1);ev(norm(at));ev(n);nr+=1
   if nr==x[5]or not mapped(at,n):stop();return
 u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def invalid(uc,kind,at,n,v,user):
  if not failed:memory(uc,unicorn.UC_MEM_WRITE if kind==unicorn.UC_MEM_WRITE_UNMAPPED else unicorn.UC_MEM_READ,at,n,v,user);stop()
  return False
 u.hook_add(unicorn.UC_HOOK_MEM_READ_UNMAPPED|unicorn.UC_HOOK_MEM_WRITE_UNMAPPED,invalid)
 def default():return[2,5,7,1000,0,0,0,0,2,17,MASK,0]
 rows=[]
 for field,values in[(0,[0,1,2,39,MASK,0x80000000]),(1,[0,1,2,31,32,63]),(2,[0,1,2,31,32,63]),(3,[0,1,MASK]),(4,[0,1,2,4,8,16,32,63]),(8,[0,1,2,39,MASK,0x10000000,0x40000000,0x7fffffff]),(9,[0,599,600,601,MASK,0x80000000]),(10,[0,5,6,8,9,27,63,MASK]),(11,[0,1])]:
  for value in values:y=default();y[field]=value;rows.append(y)
 for field in[5,6,7]:
  for value in range(1,130 if field!=7 else 15):y=default();y[field]=value;rows.append(y)
 for value in[0,1,39,0x10000000,0x40000000,0x7fffffff,MASK]:y=default();y[4]=16;y[8]=value;rows.append(y)
 for field,values in[(5,[500,1000,1800]),(6,[300,700,1500]),(7,[100,200,234])]:
  for value in values:y=default();y[0]=39;y[field]=value;rows.append(y)
 rng=random.Random(0x46bfb0)
 for _ in range(a.cases):
  y=default();y[0]=rng.choice([0,1,2,3,39,MASK]);y[1]=rng.randrange(33);y[2]=rng.randrange(33);y[3]=rng.getrandbits(32);y[4]=rng.choice([0,1,2,4,8,16,32,63]);y[8]=rng.choice([0,1,2,39,MASK,0x10000000,0x40000000,0x7fffffff]);y[9]=rng.getrandbits(32);y[10]=rng.choice([0,5,6,8,9,27,63,MASK]);y[11]=rng.randrange(2)
  if y[4]&8 and y[8]not in[0,1,2,39,MASK]:y[8]=2
  rows.append(y)
 if a.case_file:rows=json.loads(a.case_file.read_text())
 if a.limit:rows=rows[:a.limit]
 expected=[];all_events=[]
 if a.replay_oracle:
  saved=json.loads(a.replay_oracle.read_text());assert saved['source_sha256']==SHA and saved['checker_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest();rows=saved['inputs'];expected=saved['outputs'];all_events=[[]for _ in rows]
 for index,x in enumerate([]if a.replay_oracle else rows):
  nr=nw=nc=formats=lookups=graphics=fills=0;trace=1469598103934665603;failed=False;restore=None;events=[]
  for at,n in[(0x4f9154,0x2178),(0x1200000,0x50000),(0xad3dcc,0x124),(frame,0x100)]:u.mem_write(at,b'\x5a'*n)
  for base in[0x4f9934,0x1200000]:
   for i in range(39):put(base+i*0xa8+0x64,100+i);put(base+i*0xa8+0x68,200+i);put(base+i*0xa8+0x74,i%28 if x[10]==MASK else x[10]);put(base+i*0xa8+0x9c,(x[9]+i)&MASK)
  for i in range(x[1]+1):put(0x1240000+i,65+i%26 if i<x[1]else 0,1)
  for i in range(x[2]+1):put(0x1240100+i,97+i%26 if i<x[2]else 0,1)
  active=True
  try:r.call(LO,count=1000000)
  except(unicorn.UcError,RuntimeError):
   if not failed:raise RuntimeError((index,x,hex(u.reg_read(UC_X86_REG_EIP)),events[-20:]))
  finally:active=False
  if restore:put(*restore)
  state=1469598103934665603
  for at,n in[(0x4f9154,0x2178),(0x1200000,0x3000),(0x1240000,0x200),(0xad3dcc,0x124),(frame,0x20)]:
   for v in u.mem_read(at,n):state=((state^v)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {nr} {nw} {nc} {trace} {state}');all_events.append(events)
 if not a.replay_oracle:a.report.with_suffix('.oracle.json').write_text(json.dumps({'source_sha256':SHA,'checker_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'inputs':rows,'outputs':expected})+'\n')
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,y))for y in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,(p.returncode,p.stderr);actual=p.stdout.splitlines();assert len(actual)==len(rows);bad=[{'case':i,'input':rows[i],'actual':v,'expected':e,'original_events':all_events[i]}for i,(v,e)in enumerate(zip(actual,expected))if v!=e];a.report.write_text(json.dumps({'source_sha256':SHA,'cases':len(rows),'differences':len(bad),'mismatches':bad[:8],'mismatch_summary':[{k:v for k,v in m.items()if k!='original_events'}for m in bad],'network_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--cases',type=int,default=128);p.add_argument('--limit',type=int,default=0);p.add_argument('--case-file',type=Path);p.add_argument('--replay-oracle',type=Path);run(p.parse_args())
