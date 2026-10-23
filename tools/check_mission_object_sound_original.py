#!/usr/bin/env python3
"""Compare the three sound-plant bodies and their actual477860 RNG."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;D=0x1210000;RNG=0xbd0060
BODIES=[0x45c340,0x45c3a0,0x45c400]
def run(args):
 rig=Rig(args.game);rng=random.Random(0x45c3a0);active=False;v=None;trace=[];failed=False
 def put(at,n):rig.u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def read(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def memory(u,kind,at,size,value,_):
  nonlocal failed
  if not active or at!=D+0x138:return
  assert size==4;trace.append('D1,312;')
  if v[7]&1:put(A+0x24,read(A+0x24)^0xffffffff)
  if v[7]&2:put(RNG,0x12345678)
  if v[7]&4:failed=True;rig.u.emu_stop()
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory)
 def sound():
  nonlocal failed
  sp=rig.u.reg_read(UC_X86_REG_ESP);values=[read(sp+4+4*i) for i in range(4)]+[0,0];trace.append('C'+str(0x43e7d0)+''.join(','+str(n) for n in values)+';')
  if v[7]&8:put(RNG,0xcafebabe)
  if v[7]&16:put(A+0x28,read(A+0x28)^0xffffffff)
  if v[7]&32:failed=True;rig.u.emu_stop();return
  rig.finish()
 rig.stubs[0x43e7d0]=sound;rows=[];expected=[];edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for body in BODIES:
  for param in [0,1,0xffffffff]:
   for seed in edges:
    for mutation in [0,1,2,4,8,16,32,63]:rows.append([body,param,0x12345678,1,2,3,seed,mutation,0xabcdef])
 for _ in range(args.cases):rows.append([rng.choice(BODIES),rng.choice([0,0,0,1,rng.getrandbits(32)]),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(64),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114);struct.pack_into('<I',image,0,1);struct.pack_into('<I',image,4,D)
  for at,n in zip([0x24,0x28,0x2c,0xfc],v[3:6]+[v[8]]):struct.pack_into('<I',image,at,n)
  rig.u.mem_write(A,bytes(image));put(D+0x138,v[2]);put(RNG,v[6]);trace=[];failed=False;active=True
  try:rig.call(v[0],A,v[1])
  except RuntimeError:
   if not failed:raise
  finally:active=False
  image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,4,1)
  expected.append(f'{int(failed)} {read(RNG)} '+image.hex()+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected);differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'bodies':[hex(n) for n in BODIES],'cases':len(rows),'differences':len(differences),'mismatches':differences[:5],'comparison':'actual477860 instructions,currentRNG after sound,cachedposition before definitionread,ordered calls,partial failures,whole actor image','foreign_body':'43E7D0 fixture; actualruntime join separate','audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
