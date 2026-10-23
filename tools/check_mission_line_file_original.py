#!/usr/bin/env python3
"""Compare 4568E0 control flow, caller cells, memory order and service calls."""
import argparse,json,struct,subprocess
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
FRAME=STACK-0x3d0

def run(a):
 r=Rig(a.game);trace=[];seed=0;reads=0;active=False
 def put(at,w,n):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def memory(u,kind,at,w,n,_):
  if not active or not(FRAME<=at<FRAME+0x3c0 or FRAME+0x3d4<=at<FRAME+0x3dc or at==0x1300100):return
  trace.append(f'R{at},{w},{get(at,w)};' if kind==uc.UC_MEM_READ else f'W{at},{w},{n&((1<<(8*w))-1)};')
 r.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,memory)
 def foreign(site,count):
  nonlocal reads
  args=list(struct.unpack('<'+'I'*count,r.u.mem_read(r.u.reg_read(UC_X86_REG_ESP)+4,count*4)))
  trace.append('C'+','.join(map(str,[site,*args]))+';')
  if site==0x49a41c:result=0x12340000 if seed%8 else 0
  elif site==0x49a550:
   reads+=1
   if reads>seed%5+1:result=0
   else:
    data=b'alpha beta\n\0' if reads%3==1 else b'\n\0' if reads%3==2 else b'// skipped\n\0'
    r.u.mem_write(FRAME,data);result=FRAME
  elif site==0x49a840:result=0
  else:
   put(FRAME+0x3bc,4,0x12340000+seed+args[1]);put(FRAME+0x3d8,4,site^4);result=(17 if seed%4==0 else 0) if args[0] else 19+seed
  r.finish(result)
 def tokenize():
  line=get(r.u.reg_read(UC_X86_REG_ESP)+4);out=r.u.reg_read(UC_X86_REG_ESI);trace.append(f'T{line},{out};')
  for i in range(31):put(out+i*4,4,(2 if get(line,1) else 0) if i==0 else 0x1300100 if i==1 else seed+i)
  r.finish(0)
 for site,n in [(0x49a41c,2),(0x49a550,3),(0x49a840,1),(0x1300200,2),(0x1300204,2)]:r.stubs[site]=lambda site=site,n=n:foreign(site,n)
 r.stubs[0x4565f0]=tokenize
 rows=subprocess.run([str(a.probe.resolve())],input=''.join(f'{s}\n'for s in range(a.cases)),text=True,capture_output=True,check=True).stdout.splitlines();assert len(rows)==a.cases
 diff=[]
 for seed,row in enumerate(rows):
  r.u.mem_write(FRAME,bytes((seed+i*13)&255 for i in range(0x400)));put(0x1300100,1,47 if seed&1 else 97);reads=0;trace=[];active=True
  r.call(0x4568e0,0x1300000,0x1300200);active=False
  expected=f'{r.u.reg_read(UC_X86_REG_EAX)} '+''.join(trace)
  if expected!=row:diff.append({'seed':seed,'expected':expected,'actual':row})
  if len(diff)>=2:break
 report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(diff),'details':diff,'scope':'4568E0 actual instructions; raw caller cells and ordered accesses, current FILE/callback changes, token record copy; file and tokenizer calls are synchronous fixtures'}
 a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items()if k!='details'}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
