#!/usr/bin/env python3
"""Compare41EF80/41EF40/41F320 using declared format/RTXT/audio fixtures."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
P=0x1100000;F=0x1300000;T=0x1320000;H=0x1330000;A=0x1340000;S=STACK-0x138
CELLS=[0x4cf2e4,0x67f718,0x68ba34,0x4e6cfc,0x503238,0x503234,0x68bef0,0x4e7550,0x4e756c]
CALLS={0x4c4f40:2,0x497f60:4,0x4564b0:1,0x4980d0:1,0x49a620:3,0x49a092:3}
BODIES=[0x41ef80,0x41ef40,0x41f320]
def run(a):
 r=Rig(a.game);rng=random.Random(0x41ef80);v=None;active=False;trace=[];pending=[];owned=set();calls=reads=writes=lookups=0;failed=False;rejected=None;frame=F
 def norm(n):return F+n-S if v[0]==0 and S<=n<S+0x200 else n
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def text(at,s):r.u.mem_write(at,s.encode('ascii')+b'\0')
 def string(at):
  out=[]
  while True:
   if at not in owned:raise ValueError('Unowned fixture string')
   c=get(at,1);at+=1
   if not c:return bytes(out).decode('ascii')
   out.append(c)
 def number(n):return str(n if n<0x80000000 else n-0x100000000)
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,failed,rejected
  if not active:return
  watched=at in owned or 0x1100000<=at<0x1400000 or (kind==unicorn.UC_MEM_WRITE and v[0]==0 and S<=at<STACK and u.reg_read(UC_X86_REG_EIP) in [0x41f0d5,0x41f0e1])
  if not watched:return
  valid=all(at+i in owned for i in range(width));n=value&((1<<(width*8))-1)
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[5] or not valid:trace.append(f'F{norm(at)},{width};');failed=True;u.emu_stop();return
   trace.append(f'R{norm(at)},{width},{get(at,width)};')
   if v[3]&1 and at==0x4d0d0c:pending.append(lambda:put(0x4cf2e4,P+0x200))
   if v[3]&32 and at==A+4:pending.append(lambda:put(A+4,A+0x1300))
  else:
   writes+=1
   if writes==v[6] or not valid:rejected=(at,bytes(u.mem_read(at,width)));trace.append(f'E{norm(at)},{width},{n};');failed=True;u.emu_stop();return
   trace.append(f'W{norm(at)},{width},{n};')
   if v[3]&256 and at==frame+0x124:pending.append(lambda:put(at,H+32))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def callback(site,count):
  def body():
   nonlocal calls,lookups,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4) for i in range(count)]
   if site==0x493a80:
    nargs=6 if args[1]==0x4cadbc else 4 if args[1]==0x4cae08 else 3;args=[get(sp+4+i*4) for i in range(nargs)]
   trace.append('C'+str(site)+''.join(','+str(norm(n)) for n in args)+';');calls+=1
   if calls==v[4]:failed=True;r.u.emu_stop();return
   result=0
   if site==0x4c4f40:
    if v[3]&2:put(0x4cf2e4,P+0x200);put(0x68ba34,1)
   elif site==0x497f60:result=0 if v[2]==15 else H
   elif site==0x49a620:
    for i in range(args[2]):
     if args[0]+i not in owned:failed=True;r.u.emu_stop();return
     put(args[0]+i,args[1],1)
    result=args[0]
   elif site==0x49a092:
    lookups+=1
    if lookups==1:
     if v[3]&8:put(0x68bef0,T+32)
     result=0 if v[2]==14 else A
    elif args[2]==0x4cada8:result=A+12
    elif args[2]==0x4cad8c:result=A+16
    else:
     key=string(args[2])
     if v[3]&128:put(frame+0x124,H+64)
     result={'A':A+4,'B':A+8,'N':A+20}.get(key,0)
   elif site==0x493a80:
    dest,fmt=args[:2]
    if fmt==0x4cad48:value='Win'+number(args[2])
    elif fmt==0x4cadd0:value='Lose'+number(args[2])
    elif fmt==0x4cad50:value='Briefing'+number(args[2])
    elif fmt==0x4cad5c:value='Unable to find mission '+number(args[2])+' debriefing won text'
    elif fmt==0x4cadd8:value='Unable to find mission '+number(args[2])+' debriefing lose text'
    elif fmt==0x4cae08:value=string(args[2])+' ~ ~ '+string(args[3])
    elif fmt==0x4cadbc:value=string(args[2])+' ~ '+string(args[3])+' : '+number(args[4])+' ~ ~ '+string(args[5])
    elif fmt==0x4cae14:value="Unable to find '"+string(args[2])+"'"
    else:raise ValueError('Unknown format fixture')
    text(dest,value);result=len(value)
    if v[3]&4 and dest==frame:put(0x68ba34,2)
    if v[3]&16 and fmt in [0x4cae08,0x4cadbc]:put(frame+0x124,H+32)
    if v[3]&64 and fmt in [0x4cae08,0x4cadbc]:put(dest+len(value)+1,ord('Z'),1);put(dest+len(value)+2,0,1)
   r.finish(result)
  return body
 for site,count in CALLS.items():r.stubs[site]=callback(site,count)
 r.stubs[0x493a80]=callback(0x493a80,3)
 rows=[]
 for mode in range(3):
  for scenario in range(16):
   for mutation in [0,1,2,4,8,16,32,64,128,256,511]:rows.append([mode,rng.getrandbits(32),scenario,mutation,0,0,0])
 for _ in range(a.cases):rows.append([rng.randrange(3),rng.getrandbits(32),rng.randrange(16),rng.randrange(512),rng.choice([0,1,2,4,8,12,18]),rng.choice([0,1,4,12,50,200]),rng.choice([0,1,4,20,100])])
 expected=[]
 for v in rows:
  owned=set();spans=[];frame=S if v[0]==0 else F
  def reserve(at,n):r.u.mem_write(at,bytes(n));owned.update(range(at,at+n));spans.append((at,n))
  for at,n in [(P,0x500),(frame,0x128),(T,0x2710),(H,0x1000),(A,0x3000),(0x4d0d08,24)]:reserve(at,n)
  for at in CELLS:reserve(at,4)
  put(0x4cf2e4,P);put(P+0x28,1);put(P+0x228,2)
  for c in range(2):
   for i in range(4):put(P+c*0x200+(c+1)*0x44+0x30+i*4,1 if v[1]&(1<<i) else 0)
  for at,n in [(0x4d0d0c,[0,1,3,4,0xffffffff,0x80000000][v[2]%6]),(0x4d0d10,2),(0x67f718,v[1]&1),(0x68ba34,0),(0x4e6cfc,v[1]&1),(0x503238,0x77770000),(0x503234,0x77780000),(0x68bef0,T),(0x4e7550,0),(0x4e756c,v[1])]:put(at,n)
  payloads=['plain','a{ADD}A','{ADD}A tail','{ADD}M','{ADD}A {ADD}B ','{ADD} A','{AD}x','{ADD}'+'A'*70,'x'*4200]
  for at,value in [(A+0x100,payloads[v[2]%9]),(A+0x1200,'ALPHA'),(A+0x1300,'BETA'),(A+0x1400,'STATS'),(A+0x1500,'KILLS'),(A+0x1600,'{ADD}A')]:text(at,value)
  for i,at in enumerate([A+0x100,A+0x1200,A+0x1300,A+0x1400,A+0x1500,A+0x1600]):put(A+i*4,at)
  trace=[];pending=[];calls=reads=writes=lookups=0;failed=False;rejected=None;active=True
  try:r.call(BODIES[v[0]]);flush()
  except (RuntimeError,unicorn.UcError):
   if not failed:raise
  finally:active=False
  if rejected:r.u.mem_write(*rejected)
  digest=1469598103934665603
  for at,n in sorted(spans,key=lambda span:norm(span[0])):
   for value in bytes(r.u.mem_read(at,n)):digest=((digest^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr;actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'bodies':[hex(x) for x in BODIES],'cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Full current frame/profile/text/scratch/entry bytes; sourceordered1/4byteaccesses; allcalls; terminalADD+nextbyte; lateentrypointer; currentclear/free; longkey/scratchfaultprefix; audio/profilemutations','required_foreign_fixtures':'493A80 closedformatset;49A092 controlledentries;4C4F40 audio;heapclear/free. ActualRTXT/80texts native separate'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=250);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
