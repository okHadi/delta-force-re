#!/usr/bin/env python3
"""Compare451CB0 plus complete451850 against the live searchlight join."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;D=0x1210000;P=0x1201000;RNG=0xbd0060
def run(args):
 rig=Rig(args.game);rng=random.Random(0x451cb0);active=False;trace=[];v=None;reads=0;failed=False;rows=[];expected=[]
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def read(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def memory(u,kind,at,size,value,_):
  nonlocal reads,failed
  if not active or at!=D+0xe0:return
  assert size==4;trace.append('D1,224;');n=reads;reads+=1
  if n==0 and v[12]&1:put(A+0xec,read(A+0xec)+0x1234)
  if n==1 and v[12]&2:put(A+0xec,read(A+0xec)^0x80000000)
  if n==0 and v[12]&4:
   for offset,value in [(0x30,0x98760000),(0x8c,0x76540000),(0x34,0x12340000),(0xf0,0xfedc0000)]:put(A+offset,value)
  if n==0 and v[12]&32:put(A+0xc,P+0xb4)
  if n==0 and v[12]&64:put(RNG,0xcafebabe)
  if v[12]&16:failed=True;rig.u.emu_stop()
 def code(u,at,size,_):
  if active and at in [0x451899,0x451972] and reads==1 and v[12]&8:put(D+0xe0,0x80000001)
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory);rig.u.hook_add(unicorn.UC_HOOK_CODE,code)
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for host in [0,1]:
  for flags in [0,4]:
   for target in [0,1]:
    for rate in edges:
     for speed in edges:
      for mutation in [0,1,2,4,8,16,32,64,7,72]:rows.append([host,flags,target,1,0x11110000,0x80000000,speed,0x12340000,0x43210000,0x80000000,0x7fff0000,rate,mutation,0x80,0xdeadbeef])
 for _ in range(args.cases):
  yaw=rng.getrandbits(32);pitch=rng.getrandbits(32)
  rows.append([rng.randrange(2),rng.choice([0,4]),rng.randrange(2),rng.choice(edges),rng.getrandbits(32),rng.getrandbits(32),rng.choice(edges),yaw,rng.choice([yaw,rng.getrandbits(32)]),pitch,rng.choice([pitch,rng.getrandbits(32)]),rng.getrandbits(32),rng.randrange(128),rng.randrange(256),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114);struct.pack_into('<I',image,0,1);struct.pack_into('<I',image,4,D);struct.pack_into('<I',image,0xc,P);image[0x6c]=v[1]&255;image[0x6d]=v[13]&255
  for offset,value in zip([0xe0,0xe4,0xe8,0xec,0x30,0x8c,0x34,0xf0],v[3:11]):struct.pack_into('<I',image,offset,value)
  struct.pack_into('<I',image,0x90,v[14]);rig.u.mem_write(A,bytes(image));rig.u.mem_write(P,b'\0'*(2*0xb4));put(P+0x44,v[2]);put(D+0xe0,v[11]);put(0x67f528,v[0]);put(RNG,0x12345678);trace=[];reads=0;failed=False;active=True
  try:rig.call(0x451cb0,A)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,4,1);struct.pack_into('<I',image,0xc,(read(A+0xc)-P)//0xb4)
  expected.append(str(int(failed))+' '+str(read(RNG))+' '+image.hex()+' '+''.join(trace))
 process=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if process.returncode:raise RuntimeError(process.stderr)
 actual=process.stdout.splitlines();assert len(actual)==len(expected);differences=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   detail={'case':i,'input':rows[i],'actual':a,'expected':b};differences.append(detail)
   if len(differences)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(detail,indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'451CB0 +451850 live join','cases':len(rows),'differences':len(differences),'mismatches':differences[:5],'comparison':'whole actor image,RNG,current definition reads,signed/wrapping edges,definition callback changes and partial failures','foreign_bodies':[],'audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not differences
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
