#!/usr/bin/env python3
"""Prove fixed-pool enumeration and current record images before visibility work."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_EFLAGS
from check_projectiles_original import Rig,SOURCE_SHA256
BASE=0x822e80;STRIDE=0x114;DEFINITIONS=0x1210000
def run(a):
 r=Rig(a.game);u=r.u;active_case=None;seen=0;records=[]
 def put(at,n):u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def hook(uc,at,size,_):
  nonlocal seen
  if active_case is None:return
  actor=uc.reg_read(UC_X86_REG_EBX)
  if at==0x485070 and BASE<=actor<BASE+1024*STRIDE:
   slot=(actor-BASE)//STRIDE;seen+=1
   if slot==0 and active_case[4]:put(BASE+1023*STRIDE,7);put(BASE+1023*STRIDE+0x30,0xdeadbeef);put(BASE+1023*STRIDE+4,DEFINITIONS+2*0x13c)
  if at==0x4850d6 and BASE<=actor<BASE+1024*STRIDE:
   # Original has read actor+4 and compared definition+28 to3. The boundary
   # fixture skips projection and queue writes after that source comparison.
   image=bytearray(uc.mem_read(actor,STRIDE));definition=(get(actor+4)-DEFINITIONS)//0x13c;image[4:8]=definition.to_bytes(4,'little');h=1469598103934665603
   for byte in image:h=((h^byte)*1099511628211)&((1<<64)-1)
   records.extend([(actor-BASE)//STRIDE,int(bool(uc.reg_read(UC_X86_REG_EFLAGS)&0x40)),h]);uc.reg_write(UC_X86_REG_EIP,0x485075)
 u.hook_add(unicorn.UC_HOOK_CODE,hook,begin=0x485070,end=0x4850d6)
 for at in [0x8cfa18,0x8cfa1c,0x8cf904]:put(at,0)
 put(DEFINITIONS+0x13c+0x28,6);put(DEFINITIONS+2*0x13c+0x28,3)
 rng=random.Random(0x822e80);cases=[(seed,mask,char,count,mutation) for seed in [0,255] for mask in range(16) for char in [0,5,15] for count in [0,0xffffffff] for mutation in [0,1]]
 cases.extend((rng.getrandbits(32),rng.randrange(16),rng.randrange(16),rng.getrandbits(32),rng.randrange(2)) for _ in range(a.cases));expected=[]
 for case in cases:
  seed,mask,char,count,mutation=case;u.mem_write(BASE,b'\0'*(STRIDE*1024));put(0x8cfa14,count)
  for i,slot in enumerate([0,1,511,1023]):
   actor=BASE+slot*STRIDE;u.mem_write(actor,bytes((seed+i*31+j*17)&255 for j in range(STRIDE)));put(actor,7 if mask&(1<<i) else 0);put(actor+4,DEFINITIONS+(2 if char&(1<<i) else 1)*0x13c);put(actor+0x10,0x71000000)
   for axis in range(3):put(actor+0x24+axis*4,0)
   u.mem_write(actor+0x6c,b'\0')
  seen=0;records=[];active_case=case;r.call(0x485290);active_case=None;expected.append(' '.join(map(str,[seen,len(records)//3,*records])))
 result=subprocess.run([str(a.probe)],input='\n'.join(' '.join(map(str,c)) for c in cases)+'\n',capture_output=True,text=True);assert result.returncode==0,result.stderr;actual=result.stdout.splitlines();assert len(actual)==len(expected)
 diff=[{'case':i,'input':cases[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(cases),'differences':len(diff),'mismatches':diff[:3],'executed':'485290 + 485040 through actual definition type comparison; projection/queue boundary skipped'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(cases),'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=100);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
