#!/usr/bin/env python3
"""Compare live429BC0 with original code and synchronous foreign fixtures."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
A=0x1200000;P=0x1201000;TOKEN=0x76000000;RNG=0xbd0060
def run(args):
 rig=Rig(args.game);rng=random.Random(0x429bc0);rows=[];expected=[];trace=[];active=False;v=None;calls=0;markers=0;failed=False
 def put(at,n,width=4):rig.u.mem_write(at,(n&((1<<(width*8))-1)).to_bytes(width,'little'))
 def read(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def normalize(n):return TOKEN if n==A else n
 def foreign(site):
  nonlocal calls,markers,failed
  sp=rig.u.reg_read(UC_X86_REG_ESP);count={0x43f020:2,0x43ef80:4,0x45c6a0:5,0x42ee30:3,0x446fb0:3,0x447610:4}[site]
  args0=[read(sp+4+i*4) for i in range(count)];payload=''
  values=[0]*6
  if site==0x45c6a0:values[0]=normalize(args0[0])
  elif site==0x446fb0:values[:3]=[args0[0],0,args0[2]];payload=bytes(rig.u.mem_read(args0[1],24)).hex()
  else:values[:count]=[normalize(n) for n in args0]
  trace.append('C'+str(site)+''.join(','+str(n) for n in values)+','+payload+';')
  if calls==0:
   if v[10]&1:put(A+0xc,P+0xb4)
   if v[10]&2:put(P+0xa4,0)
   if v[10]&8:put(RNG,0xcafebabe)
   if v[10]&16:put(A+0x7c,1);put(A+0x1c,0)
  calls+=1
  if site==0x42ee30 and v[10]&4:put(P+0xac,2);put(P+0xb0,0xffffffff)
  if v[10]&32:failed=True;rig.u.emu_stop();return
  if site==0x45c6a0:
   markers+=1
   if markers>1 and v[10]&64:rig.finish(0);return
   for at,n in zip(args0[1:],[v[14],v[12],v[13],0]):put(at,n)
   rig.finish(v[11])
  else:rig.finish()
 for site in [0x43f020,0x43ef80,0x45c6a0,0x42ee30,0x446fb0,0x447610]:rig.stubs[site]=lambda site=site:foreign(site)
 def memory(u,kind,at,size,value,_):
  if not active:return
  if 0x78f280<=at<0x78f280+3*0x88 and (at-0x78f280)%0x88 in [0,4]:trace.append(f'G{at};')
  if at==0x7936bc:trace.append('T0,60;')
 rig.u.hook_add(unicorn.UC_HOOK_MEM_READ,memory)
 edges=[0,1,0x7fffffff,0x80000000,0xffffffff]
 for host in [0,1]:
  for flags in [0,4]:
   for health in [0,1,0x7fff,0x8000,0xffff]:
    for count in edges:
     for mutation in [0,1,2,4,8,16,32,5,9,33,64,65]:
      rows.append([host,flags,health,0,0,1,0,0,count,1,mutation,1,0,0,0x76543210,0x1111])
 for _ in range(args.cases):rows.append([1,rng.choice([0,0,4]),rng.choice([0,1,100,0xffff]),rng.getrandbits(32),rng.randrange(2),rng.randrange(2),rng.randrange(3),rng.choice(edges),rng.choice(edges),rng.randrange(2),rng.randrange(128),rng.randrange(2),rng.choice(edges),rng.choice(edges),rng.getrandbits(32),rng.getrandbits(32)])
 for v in rows:
  image=bytearray(0x114);struct.pack_into('<I',image,0,1);struct.pack_into('<I',image,4,1);struct.pack_into('<I',image,0xc,P);struct.pack_into('<I',image,0x1c,v[4]);struct.pack_into('<I',image,0x2c,0x7fff0000);image[0x6c]=v[1]&255;struct.pack_into('<H',image,0x74,v[2]&0xffff);struct.pack_into('<I',image,0x7c,v[3]);struct.pack_into('<I',image,0xf8,0x123)
  rig.u.mem_write(A,bytes(image));rig.u.mem_write(P,b'\0'*(2*0xb4));put(P+0xa4,v[5]);put(P+0xac,v[6]);put(P+0xb0,v[7]);put(P+0xb4+0xa4,3);put(P+0xb4+0xac,2);put(P+0xb4+0xb0,5)
  put(0x67f528,v[0]);put(RNG,0x12345678);put(0x7936bc,v[15])
  for index in range(3):put(0x78f280+index*0x88,v[9]);put(0x78f284+index*0x88,v[8])
  calls=0;markers=0;failed=False;trace=[];active=True
  try:rig.call(0x429bc0,A,0)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  image=bytearray(rig.u.mem_read(A,0x114));struct.pack_into('<I',image,0xc,(read(A+0xc)-P)//0xb4)
  people=[read(P+slot*0xb4+offset) for slot in range(2) for offset in [0xa4,0xac,0xb0]]
  expected.append(' '.join(map(str,[int(failed),read(RNG)]))+' '+image.hex()+' '+' '.join(map(str,people))+' '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr)
 actual=p.stdout.splitlines();assert len(actual)==len(expected)
 mismatches=[]
 for i,(a,b) in enumerate(zip(actual,expected)):
  if a!=b:
   mismatches.append({'case':i,'input':rows[i],'actual':a,'expected':b})
   if len(mismatches)<4:args.report.with_name(args.report.stem+f'.failure-{i}.json').write_text(json.dumps(mismatches[-1],indent=2))
 report={'source_sha256':SOURCE_SHA256,'body':'429BC0 live join','cases':len(rows),'differences':len(mismatches),'mismatches':mismatches[:5],'foreign_bodies':'scripted synchronous source-order fixtures; no body proof claimed','comparison':'whole current actor image,old/new personA4/AC/B0,RNG,ordered calls/payloads/global/type reads,foreign failure partial effects','audio_output':'none'}
 args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not mismatches
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
