#!/usr/bin/env python3
"""Compare404150 ordered memory/call effects with the supplied executable."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
P=0x1300000;B=0x1320000
GLOBALS=[0x67f41c,0xbfc104,0x67f400,0x67f404,0x67dbe4,0x67f548]
CALLS={0x404540:1,0x404610:0,0x47a640:0,0x47a660:0,0x403a50:0,0x43e6d0:0,0x401e70:0,0x4401e0:0,0x402d40:0,0x47a690:0,0x486660:0,0x47ec50:0,0x42cd30:1,0x42c950:0,0x449620:0,0x482910:0}
def run(a):
 r=Rig(a.game);rng=random.Random(0x404150);active=False;trace=[];pending=[];calls=reads=writes=0;failed=False;owned={};v=None;write_before=b''
 def put(at,value,width=4):r.u.mem_write(at,(value&((1<<(8*width))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(r.u.mem_read(at,width),'little')
 def flush(*_):
  while pending:pending.pop(0)()
 def memory(u,kind,at,width,value,_):
  nonlocal reads,writes,failed,write_before
  if not active:return
  if not(at in owned or P<=at<P+0x100000):return
  valid=all(at+i in owned for i in range(width))
  if kind==unicorn.UC_MEM_READ:
   reads+=1
   if reads==v[8] or not valid:trace.append(f'F{at},{width};');failed=True;u.emu_stop();return
   n=get(at,width);trace.append(f'R{at},{width},{n};')
   if v[6]&16 and at==0x67dbfc:pending.append(lambda:put(0x67dc00,P+256))
  else:
   writes+=1;n=value&((1<<(width*8))-1)
   if writes==v[9] or not valid:write_before=bytes(u.mem_read(at,width));trace.append(f'E{at},{width},{n};');failed=True;u.emu_stop();return
   trace.append(f'W{at},{width},{n};')
   if v[6]&2 and P<=at<P+512:pending.append(lambda:(put(0x67f400,2),put(0x67f404,1)))
   if v[6]&4 and at==0x67f548:pending.append(lambda:put(0x7cca98+255*0x114,0x7fffffff))
 r.u.hook_add(unicorn.UC_HOOK_CODE,flush);r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 def callback(site,count):
  def body():
   nonlocal calls,failed
   sp=r.u.reg_read(UC_X86_REG_ESP);args=[get(sp+4+i*4) for i in range(count)];trace.append('C'+str(site)+''.join(','+str(n) for n in args)+';');calls+=1
   if calls==v[7]:failed=True;r.u.emu_stop();return
   if site==0x404540:
    if v[6]&1:put(0x67f41c,32);put(0x67f400,4);put(0x67f404,2)
    r.finish(v[1]);return
   if site==0x42cd30 and v[6]&8:put(0x7cca98+17*0x114,0)
   r.finish()
  return body
 for site,count in CALLS.items():r.stubs[site]=callback(site,count)
 rows=[]
 for capture in [0,1]:
  for mode in [0,8,32,0xffffffff]:
   for width in [0,1,2,4,8,0xffffffff,0x80000000]:
    for height in [0,1,2,0xffffffff]:
     rows.append([rng.getrandbits(32),capture,mode,width,height,1,rng.randrange(32),0,0,0])
 for _ in range(a.cases):rows.append([rng.getrandbits(32),rng.randrange(2),rng.choice([8,32]),rng.choice([0,1,2,4,8]),rng.choice([0,1,2,3]),rng.randrange(2),rng.randrange(32),rng.choice([0,0,1,4,7,12,18,30]),rng.choice([0,0,1,4,10,30,200]),rng.choice([0,0,1,4,10])])
 expected=[]
 for v in rows:
  owned={};spans=[]
  def reserve(at,n):
   block=bytes(((i*37)^(v[0]>>(i%4*8)))&255 for i in range(n));r.u.mem_write(at,block);spans.append((at,n))
   for i in range(n):owned[at+i]=block[i]
  reserve(P,512);reserve(B,4096);reserve(0x7cca80,0x11400)
  for at in GLOBALS:reserve(at,4)
  reserve(0x67dbfc,32)
  for at,n in [(0x67f41c,v[2]),(0xbfc104,B if v[5] else 0),(0x67f400,v[3]),(0x67f404,v[4]),(0x67dbe4,v[0]),(0x67f548,0xabcdef01)]:put(at,n)
  for i in range(8):put(0x67dbfc+i*4,P+i*64)
  for i in range(256):put(0x7cca98+i*0x114,0x71000000+i if i%17==0 else 0)
  trace=[];pending=[];calls=reads=writes=0;failed=False;active=True
  try:r.call(0x404150);flush()
  except RuntimeError:
   if not failed:raise
  finally:active=False
  # A stopped write hook can still commit its instruction. Restore that one
  # rejected store to model the provider's pre-mutation failure boundary.
  # The saved fixture bytes are updated below through successful writes only.
  digest=1469598103934665603
  if trace and trace[-1].startswith('E'):
   at,w,_=map(int,trace[-1][1:-1].split(','))
   # Reconstruct the exact state before the rejected store by a saved snapshot.
   for i in range(w):put(at+i,write_before[i],1)
  for at,n in sorted(spans):
   for value in bytes(r.u.mem_read(at,n)):digest=((digest^value)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 p=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,v)) for v in rows)+'\n',text=True,capture_output=True);assert not p.returncode,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 diffs=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'body':'404150','cases':len(rows),'differences':len(diffs),'mismatches':diffs[:3],'coverage':'Full current memory; ordered1/4/8-byte accesses; palette and qword dim; current dimensions/pointers; partial fault prefixes; required call order; full256CHRscan; mutations','foreign_calls':'Required typed fixture boundaries; real401E70/EventStage join is native'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(diffs)}));assert not diffs
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=300);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
