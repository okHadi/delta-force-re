#!/usr/bin/env python3
"""Compare45F670+45C5E0 with current shared memory reads and writes."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
from unicorn.x86_const import UC_X86_REG_EAX
P=0x794680;L=0x78f280;A=0x7cca80;M=0x8ace80
ACTORS=[A+i*0x114 for i in range(3)]+[0x7dde80+i*0x114 for i in range(2)]
def run(args):
 rig=Rig(args.game);rng=random.Random(0x45f670);active=False;trace=[];v=None;counts=pointers=listcounts=xreads=yreads=0;failed=False
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def memory(u,kind,at,size,value,_):
  nonlocal pointers,counts,listcounts,xreads,yreads,failed
  if not active:return
  tracked=at in [0x8cfa18,0x8cfa1c] or L<=at<L+2*0x88 or any(a<=at<a+0x114 for a in ACTORS+[M+i*0x114 for i in range(3)])
  if kind==unicorn.UC_MEM_READ:
   if not tracked:return
   trace.append(f'R{at},{size};')
   if at==A+0xc:
    occurrence=pointers;pointers+=1
    if not v[0]:
     if occurrence==0 and v[20]&1:put(at,P+5*0xb4)
     if occurrence==1 and v[20]&2:put(at,P+4*0xb4)
     if occurrence==2 and v[20]&4:put(at,0)
     if occurrence==3 and v[20]&8:put(at,P+3*0xb4)
     if occurrence==3 and v[20]&1024:failed=True;rig.u.emu_stop()
   if at==0x8cfa18:
    if counts==1 and v[20]&128:put(at,1)
    counts+=1
   if at==L+4:
    if listcounts==0 and v[20]&32:put(at,0)
    listcounts+=1
   if at==A+0x24:
    if xreads==0 and v[20]&64:put(M+0x24,read(M+0x24)^0xffffffff)
    xreads+=1
   if at==M+2*0x114+0x28:
    if yreads==0 and v[20]&256:put(A+0x28,read(A+0x28)^0xffffffff)
    yreads+=1
  elif kind==unicorn.UC_MEM_WRITE and (P<=at<P+6*0xb4 or at==0xb0):
   trace.append(f'W{at},{size},{value&0xffffffff};')
   if at==P+0xa4 and v[20]&16:put(A+0xc,P+4*0xb4)
   if at==P+0xac and v[20]&512:put(A+0xc,0)
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 rows=[];expected=[];faults=0;edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for op in [0,1]:
  for side in [0,2,3,0x8000,0xffffffff]:
   for count in [0,1,3,0x80000000]:
    for mutation in [0,1,2,4,8,16,32,64,128,256,512,1024,255]:rows.append([op,side,0,3,2,count,31,31,0,0,0,0,0,0,0x10000,0,0,0x10000,0,0,mutation])
 for _ in range(args.cases):
  side=rng.choice([3,4,0x7fff,0x8000,0xffffffff]);coords=[rng.choice(edges+[rng.getrandbits(32)]) for _ in range(12)]
  rows.append([rng.randrange(2),side,rng.choice([0,1,0x20000000,0x40000000]),rng.choice([0,1,3,0xffffffff]),rng.choice([0,1,2,0x80000000]),rng.randrange(4),rng.randrange(32),rng.randrange(32),*coords,rng.randrange(2048)])
 for v in rows:
  for i,a in enumerate(ACTORS):
   rig.u.mem_write(a,b'\0'*0x114);put(a,1);put(a+4,1);put(a+0xc,P+i*0xb4 if v[6]&(1<<i) else 0);put(a+0x70,((((v[1] if v[7]&(1<<i) else v[1]+1)&0xffff)<<16)|1))
   for axis in range(3):put(a+0x24+axis*4,v[8+axis]+i*0x10000)
  for i in range(3):
   rig.u.mem_write(M+i*0x114,b'\0'*0x114)
   for axis in range(3):put(M+i*0x114+0x24+axis*4,v[11+i*3+axis])
  rig.u.mem_write(L,b'\0'*(2*0x88));put(L+4,v[5]);put(L+8,2);put(L+12,0);put(L+16,1)
  rig.u.mem_write(P,b'\0'*(6*0xb4))
  for i in range(6):
   for off,n in [(0xa4,10+i),(0xac,20+i),(0xb0,30+i)]:put(P+i*0xb4+off,n)
  put(0x8cfa18,v[3]);put(0x8cfa1c,v[4]);trace=[];pointers=counts=listcounts=xreads=yreads=0;failed=False;active=True;result=0
  try:
   result=rig.call(0x45c5e0,A,v[2]) if v[0] else rig.call(0x45f670,v[1],v[2]);result=rig.u.reg_read(UC_X86_REG_EAX) if v[0] else 0
  except unicorn.UcError:
   if not(v[20]&(4|512)) or read(A+0xc):raise
   failed=True;faults+=1
  except RuntimeError:
   if not failed:raise
  finally:active=False
  values=[int(failed),result,read(0x8cfa18),read(0x8cfa1c),read(L+4)]
  for a in ACTORS:
   pointer=read(a+0xc);values.append((pointer-P)//0xb4 if pointer else -1);values.extend(read(a+0x24+axis*4) for axis in range(3))
  for i in range(3):values.extend(read(M+i*0x114+0x24+axis*4) for axis in range(3))
  for i in range(6):values.extend(read(P+i*0xb4+off) for off in [0xa4,0xac,0xb0])
  expected.append(' '.join(map(str,values))+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected);differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'bodies':['45F670','45C5E0'],'cases':len(rows),'differences':len(differences),'original_null_destination_faults':faults,'mismatches':differences[:5],'comparison':'actual nested instructions,ordered per-cell reads/writes,current person/count/list/position changes,signed/wrapped distances,ties,null-destination partial writes','audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences','original_null_destination_faults']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
