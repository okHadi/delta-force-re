#!/usr/bin/env python3
"""Compare44A200 current memory accesses against original instructions."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
BASES=[0x867e80,0x7cca80,0x7dde80];COUNTS=[0x8cf904,0x8cfa18,0x8cfa1c]
def run(a):
 r=Rig(a.game);active=False;trace=[];seen=[];v=[];failed=False;failed_old=0
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def hook(u,kind,at,w,n,_):
  nonlocal failed,failed_old
  if not active:return
  tracked=0x593000<=at<0x993800 and (at in COUNTS or 0x593680<=at<0x993680 and ((at-0x793680)%64 in [44,56]) or any(b<=at<b+3*0x114 for b in BASES))
  if not tracked:return
  if kind==unicorn.UC_MEM_READ:
   trace.append(f'R{at},{w};')
   for g,c in enumerate(COUNTS):
    if at==c:
     if seen[g]==1 and v[7]&(1<<g):put(at,v[8])
     seen[g]+=1
   if at==BASES[1]+0x6c and v[7]&8:put(at+4,63<<16)
  else:
   trace.append(f'W{at},{w},{n&0xffffffff};')
   if at==0x7936b8 and v[7]&16:failed_old=get(at);failed=True;r.u.emu_stop()
   if at==0x7936ac and n==0 and v[7]&32:put(0x7936b8,v[9])
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,hook)
 rng=random.Random(0x44a200);rows=[];expected=[]
 for side in [0,1,63,64,0x7fff,0x8000,0xffff]:
  for mutation in range(64):rows.append([1,0xffffffff,3,3,3,0,side,mutation,1,777])
 for _ in range(a.cases):rows.append([rng.getrandbits(32),rng.getrandbits(32),*[rng.choice([0,1,3,0x80000000,0xffffffff]) for _ in range(3)],rng.randrange(512),rng.randrange(65536),rng.randrange(64),rng.choice([0,1,2,3,0x80000000,0xffffffff]),rng.getrandbits(32)])
 for v in rows:
  # Clear all possible signed-side destinations; input side plus row/group offsets wrap16.
  for delta in range(5):
   side=(v[6]+delta)&65535;side=side-65536 if side&32768 else side;put(0x7936ac+side*64,0)
  for i in range(64):put(0x7936ac+i*64,v[0]+i);put(0x7936b8+i*64,v[1]+i)
  for g,b in enumerate(BASES):
   put(COUNTS[g],v[2+g])
   for i in range(3):r.u.mem_write(b+i*0x114,b'\0'*0x114);put(b+i*0x114+0x6c,4 if v[5]&(1<<(g*3+i)) else 0,1);put(b+i*0x114+0x70,((v[6]+i+g)&65535)<<16)
  trace=[];seen=[0,0,0];failed=False;active=True
  try:r.call(0x44a200)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  if failed:put(0x7936b8,failed_old)
  values=[int(failed)]
  for i in range(64):values.extend([get(0x7936ac+i*64),get(0x7936b8+i*64)])
  expected.append(' '.join(map(str,values))+' '+''.join(trace))
 result=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert result.returncode==0,result.stderr
 actual=result.stdout.splitlines();assert len(actual)==len(expected);diff=[{'case':i,'input':rows[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'body':'44A200','cases':len(rows),'differences':len(diff),'mismatches':diff[:2],'audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
