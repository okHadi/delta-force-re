#!/usr/bin/env python3
"""Compare the two exact profile-start instruction fragments."""
import argparse,json,random,subprocess
import unicorn
from unicorn.x86_const import UC_X86_REG_EDX,UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
from pathlib import Path

def run(a):
 r=Rig(a.game);rng=random.Random(0x407c66);rows=[]
 for mode in [0,1]:
  for campaign in [0,1,3,5,0x40000000,0x80000000,0xffffffff]:
   for mutation in range(8):
    for fail in range(8):rows.append([mode,0x1100000,campaign,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),mutation,fail])
 for _ in range(a.cases):rows.append([rng.randrange(2),0x1100000,rng.choice([0,1,3,5,0x40000000]),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(8),rng.randrange(8)])
 active=False;pending=[];trace=[];words={};count=0;failed=False;rejected=None;v=None
 def put(at,n):r.u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(r.u.mem_read(at,4),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def step():
  nonlocal count,failed
  count+=1
  if count==v[7]:failed=True;r.u.emu_stop();return False
  return True
 def memory(u,kind,at,width,value,_):
  nonlocal rejected
  if not active or at not in words:return
  if not step():
   if kind==unicorn.UC_MEM_WRITE:rejected=(at,bytes(u.mem_read(at,width)))
   return
  if kind==unicorn.UC_MEM_READ:
   n=get(at);trace.append(f'R{at},{width},{n};')
   if v[6]&1 and at==0x4cf2e4:pending.append(lambda:put(at,v[1]+0x400))
   if v[6]&2 and at==0x67f548:pending.append(lambda:put(0x503328,get(0x503328)^0x87654321))
  else:trace.append(f'W{at},{width},{value&0xffffffff};')
 def clear():
  if not step():return
  sp=r.u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4)for i in range(3)]
  assert args==[0x67f714,0,0x514],args
  trace.append('C4826656;') #49A620
  r.u.mem_write(0x67f714,bytes(0x514))
  if v[6]&4:put(0x67f548,get(0x67f548)^0xfedcba98);put(0x67f70c,get(0x67f70c)^0xabcdef01)
  r.finish(0)
 r.stubs[0x49a620]=clear;r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);expected=[]
 for v in rows:
  mode,profile,campaign,mask,rules,branch,mutation,fail=v;target=(profile+0x70+campaign*0x44)&0xffffffff
  words={0x4cee90:0x12345678,0x4cf2e4:profile,profile+0x28:campaign,target:mask,0x67f54c:0,0x67f70c:mask,0x67dbec:0,0x67f548:rules,0x503328:branch,0x67dbe4:0}
  for at,n in words.items():put(at,n)
  trace=[];pending=[];count=0;failed=False;rejected=None;r.u.reg_write(UC_X86_REG_ESP,0x2008000)
  #403AA1 starts after source pushes size and zero.
  if mode:r.u.mem_write(0x2008000,(0).to_bytes(4,'little')+(0x514).to_bytes(4,'little'))
  active=True
  r.u.emu_start(0x403aa1 if mode else 0x407c61,0x403acd if mode else 0x407c8b,count=10000)
  flush();active=False
  if rejected:r.u.mem_write(*rejected)
  result=0 if failed or not mode else r.u.reg_read(UC_X86_REG_EDX)
  expected.append(f'{int(failed)} {result} '+''.join(f'{at}:{get(at)},'for at in sorted(words))+' '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v))for v in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(actual,expected))if x!=y]
 a.report.write_text(json.dumps({'sha256':SOURCE_SHA256,'fragments':['407C61..407C86','403AA1..403AC8'],'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'clear':'Required declared 49A620 provider; output bytes cleared; source args checked','coverage':'Current DWORD traces, captured selected pointer, wrapped campaign addresses, clear callback rules mutation, current branch mutation, read/write/call failure prefixes'},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=200);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
