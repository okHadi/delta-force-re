#!/usr/bin/env python3
"""Compare4533F0 with current global and foreign-call effects."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;TOKEN=0x76000000;G=0xa158c0;RNG=0xbd0060
def run(args):
 rig=Rig(args.game);rng=random.Random(0x4533f0);trace=[];calls=0;failed=False;active=False;v=None
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def foreign(site):
  nonlocal calls,failed
  sp=rig.u.reg_read(UC_X86_REG_ESP);values=[TOKEN,read(sp+8) if site==0x447980 else 0,0,0,0,0];trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';');call=calls;calls+=1
  if call==0:
   if v[5]&1:put(A+0x6c,read(A+0x6c,1)^0x80,1)
   if v[5]&4:put(G,0x55555555)
   if v[5]&128:put(RNG,0xcafebabe)
  if call==1 and v[5]&2:put(A+0x6c,read(A+0x6c,1)^0x40,1)
  if (call==0 and v[5]&8) or (call==1 and v[5]&32):failed=True;rig.u.emu_stop();return
  rig.finish()
 for site in [0x453270,0x447980]:rig.stubs[site]=lambda site=site:foreign(site)
 def memory(u,kind,at,size,value,_):
  nonlocal failed
  if not active or at!=G:return
  assert size==4;trace.append('G'+str(G)+';')
  if v[5]&64:put(A+0x6c,read(A+0x6c,1)^0x20,1)
  if v[5]&16:failed=True;rig.u.emu_stop()
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory)
 rows=[];expected=[]
 for mode in [0,1,0x80000000]:
  for param in [0,4,0xffffffff]:
   for flags in [0,4,8,0xc,0x80,0xf4,0xf8,0xff]:
    for health in [0,1,0x7fff,0x8000,0xffff]:
     for mutation in [0,1,2,4,8,16,32,64,128,255]:rows.append([mode,param,flags,health,0x12345678,mutation,0xabcdef])
 for _ in range(args.cases):rows.append([rng.getrandbits(32),rng.choice([0,4,rng.getrandbits(32)]),rng.randrange(256),rng.randrange(65536),rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114);struct.pack_into('<I',image,0,1);struct.pack_into('<I',image,4,1);image[0x6c]=v[2]&255;struct.pack_into('<H',image,0x74,v[3]&0xffff);struct.pack_into('<I',image,0xfc,v[6]);rig.u.mem_write(A,bytes(image));put(0x67f528,v[0]);put(G,v[4]);put(RNG,123);trace=[];calls=0;failed=False;active=True
  try:rig.call(0x4533f0,A,v[1])
  except RuntimeError:
   if not failed:raise
  finally:active=False
  expected.append(f'{int(failed)} {read(G)} {read(RNG)} '+bytes(rig.u.mem_read(A,0x114)).hex()+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected);differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'4533F0','cases':len(rows),'differences':len(differences),'mismatches':differences[:5],'comparison':'whole current actor image,RNG,source-order global and foreign calls,callback changes,partial failures','foreign_bodies':['453270','447980'],'audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
