#!/usr/bin/env python3
"""Compare4519A0 with current actor/person callback changes."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;P=0x1201000;B=0x7cca80;RNG=0xbd0060;TOKEN=0x76000000
def run(args):
 rig=Rig(args.game);rng=random.Random(0x4519a0);rows=[];expected=[];trace=[];calls=0;angles=0;failed=False;v=None
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def foreign(site):
  nonlocal calls,angles,failed
  sp=rig.u.reg_read(UC_X86_REG_ESP);count={0x44f540:2,0x477770:2,0x42edd0:2,0x446fb0:3}[site];aa=[read(sp+4+4*i) for i in range(count)];values=[0]*6;payload=''
  if site==0x44f540:values[0]=TOKEN
  elif site==0x446fb0:values[:3]=[aa[0],0,aa[2]];payload=bytes(rig.u.mem_read(aa[1],24)).hex()
  else:values[:count]=aa
  trace.append('C'+str(site)+''.join(','+str(n) for n in values)+','+payload+';')
  if calls==0:
   if v[17]&1:put(A+0xc,P+0xb4)
   if v[17]&2:put(P+0x38,99);put(P+0x44,0);put(P+0x24,read(P+0x24,1)|0x40,1)
   if v[17]&4:put(B+0x24,read(B+0x24)^0xffffffff);put(B+0x72,3,2)
   if v[17]&8:put(RNG,0xcafebabe)
   if v[17]&16:put(A+0xe0,0x55555555);put(A+0xe4,0x66666666)
   if v[17]&128:put(A+0xc,0)
  if calls==1 and v[17]&32:put(A+0xe0,0x11111111);put(A+0xe4,0x22222222)
  calls+=1
  if v[17]&64:failed=True;rig.u.emu_stop();return
  if site==0x44f540:put(aa[1],v[12]);rig.finish(B if v[11] else 0)
  elif site==0x477770:result=v[18+(1 if angles else 0)];angles+=1;rig.finish(result)
  else:rig.finish()
 for site in [0x44f540,0x477770,0x42edd0,0x446fb0]:rig.stubs[site]=lambda site=site:foreign(site)
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for host in [0,1]:
  for health in [0,1,0x8000,0xffff]:
   for target in [0,1]:
    for timer in [0,1,2,0x80000000]:
     for mutation in [0,1,2,4,8,16,32,64,128,129,255]:rows.append([host,0,health,0,0,0,0,0,target,timer,0,1,0x7fffffff,0,0x20000,0x10000,0x7fff0000,mutation,0x12345678,0x98765432,0xdeadbeef])
 for _ in range(args.cases):rows.append([rng.randrange(2),rng.choice([0,0,4]),rng.choice([0,1,100,0xffff]),rng.choice(edges),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(2),rng.choice(edges+[2,9]),rng.choice([0,0x40]),rng.randrange(2),rng.choice(edges),rng.choice([0,4]),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114)
  for at,n in [(0,1),(4,1),(0xc,P),(0x70,0x10000),(0xf8,0x123),(0x2c,0x7fff0000)]+list(zip([0xe0,0xe4,0xe8,0x30,0x34],v[3:8])):struct.pack_into('<I',image,at,n)
  image[0x6c]=v[1]&255;image[0x6d]=0x80;struct.pack_into('<H',image,0x74,v[2]&0xffff);rig.u.mem_write(A,bytes(image))
  other=bytearray(0x114)
  for at,n in [(0,1),(4,1),(0x70,0x20000)]+list(zip([0x24,0x28,0x2c],v[14:17])):struct.pack_into('<I',other,at,n)
  other[0x6c]=v[13]&255;rig.u.mem_write(B,bytes(other));rig.u.mem_write(P,b'\0'*(2*0xb4));put(P+0x24,v[10],1);put(P+0x38,v[9]);put(P+0x44,B if v[8] else 0)
  for at,n in [(0x24,0xa0),(0x38,123456),(0x44,0x123),(0x48,11),(0x4c,22),(0x50,33)]:put(P+0xb4+at,n,1 if at==0x24 else 4)
  put(0x67f528,v[0]);put(RNG,v[20]);calls=angles=0;failed=False;trace=[]
  try:rig.call(0x4519a0,A,0)
  except RuntimeError:
   if not failed:raise
  link=(read(A+0xc)-P)//0xb4 if read(A+0xc) else -1;image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,0xc,link if link>=0 else 0)
  people=[read(P+slot*0xb4+offset,1 if offset==0x24 else 4) for slot in range(2) for offset in [0x24,0x38,0x44,0x48,0x4c,0x50]]
  expected.append(' '.join(map(str,[int(failed),read(RNG),link]))+' '+image.hex()+' '+' '.join(map(str,people))+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected);differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'4519A0 live join','cases':len(rows),'differences':len(differences),'mismatches':differences[:5],'comparison':'whole actor image,old/current person fields,RNG,synchronous callback order and mutations,partial failures','foreign_bodies':'scripted fixtures; nested body proof separate','audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
