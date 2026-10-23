#!/usr/bin/env python3
"""Execute full original parameter wrappers and their direct callees."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
P=0x4b691c;R=0x4b69f0;T=0x1100100
def run(a):
 r=Rig(a.game);rng=random.Random(0x4b69d0);rows=[]
 for mode in [0,1]:
  for index in [0,1,2,3,0x7fffffff,0x80000000,0xffffffff]:
   for address in [T,P,R]:
    table=(address-index*4)&0xffffffff
    for mutation in range(16):rows.append([mode,index,rng.getrandbits(32),table,rng.getrandbits(32),rng.getrandbits(32),mutation,0,0])
  for mutation in range(16):rows.append([mode,0xffffffff,0x80000000,0,0x87654321,0,mutation,0,0])
 for _ in range(a.cases):
  index=rng.getrandbits(32);rows.append([rng.randrange(2),index,rng.getrandbits(32),(T-index*4)&0xffffffff,rng.getrandbits(32),rng.getrandbits(32),rng.randrange(16),rng.choice([0,1,2,3,4]),rng.choice([0,1])])
 active=False;pending=[];trace=[];words={};reads=writes=0;failed=False;rejected=None;v=None;address=0
 def put(at,n):r.u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(r.u.mem_read(at,4),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,failed,rejected
  if not active or at not in words:return
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[7]:trace.append(f'F{at},{width};');failed=True;u.emu_stop();return
   n=get(at);trace.append(f'R{at},{width},{n};')
   if at==P:
    if v[6]&1:pending.append(lambda:put(P,0))
    if v[6]&8 and v[3]:pending.append(lambda:put(address,get(address)^0xfedcba98))
   if v[6]&2 and at==address and at!=P:pending.append(lambda:put(R,0x12345678))
  else:
   writes+=1;n=value&0xffffffff
   if writes==v[8]:rejected=(at,bytes(u.mem_read(at,width)));trace.append(f'E{at},{width},{n};');failed=True;u.emu_stop();return
   trace.append(f'W{at},{width},{n};')
   if v[6]&4:pending.append(lambda:put(R,get(R)^0xabcdef01))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);expected=[]
 for v in rows:
  address=(v[3]+v[1]*4)&0xffffffff;words={P:v[3],R:v[4]}
  if v[3]:words[address]=v[5]
  words[P]=v[3]
  for at,n in words.items():put(at,n)
  trace=[];pending=[];reads=writes=0;failed=False;rejected=None;active=True
  try:r.call(0x4c4f60 if v[0] else 0x4c4f40,*([v[1]] if v[0] else [v[1],v[2]]));flush();result=r.u.reg_read(UC_X86_REG_EAX)
  except RuntimeError:
   if not failed:raise
   result=0
  finally:active=False
  if rejected:r.u.mem_write(*rejected)
  expected.append(f'{int(failed)} {result} '+''.join(f'{at}:{get(at)},' for at in sorted(words))+' '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',capture_output=True,text=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'bodies':['4C4F40','4C4F60','4B69D0','4B69F4'],'cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Exact currentDWORDreads/writes, wrappedindices, currentpointercapture, parameter/sharedreturn aliases, null retainedreturn, callbackmutations and read/writefailureprefix; no callee stubs'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=400);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
