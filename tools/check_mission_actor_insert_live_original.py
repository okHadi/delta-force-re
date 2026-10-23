#!/usr/bin/env python3
"""Compare the real488118 extra actor lifetime and full-clear block."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256

def run(args):
 r=Rig(args.game);u=r.u;actor=0x822e80;clears=actions=mutation=0
 def put(at,n):u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def clear():
  nonlocal clears
  sp=u.reg_read(UC_X86_REG_ESP);assert[get(sp+4),get(sp+8),get(sp+12)]==[actor,0,0x114];clears+=1;u.mem_write(actor,b'\0'*0x114);r.finish(actor)
 def action():
  nonlocal actions
  assert get(u.reg_read(UC_X86_REG_ESP)+4)==actor;actions+=1
  if mutation:put(actor+0xfc,get(actor+0xfc)^0xabcdef01);u.mem_write(actor+0x113,bytes([u.mem_read(actor+0x113,1)[0]^0x42]));u.mem_write(actor+0xe3,bytes([u.mem_read(actor+0xe3,1)[0]^0x19]))
  r.finish()
 r.stubs[0x49a620]=clear;r.stubs[0x45c340]=action
 rows=[];rng=random.Random(0x488118)
 for fc in [0,1,0x7ffffffe,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]:
  for limit in [0,2,0x7fffffff,0x80000000,0xffffffff]:
   for cb in [0,0x4563a0,0x45c340]:rows.append([fc,limit,rng.randrange(256),cb,rng.randrange(2)])
 for _ in range(args.cases):rows.append([rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256),rng.choice([0,0x4563a0,0x45c340]),rng.randrange(2)])
 expected=[]
 for fc,limit,seed,callback,mutation in rows:
  image=bytearray([seed]*0x114)
  for off,value in [(0,1),(4,0x1210000),(0xc,0),(0x18,0),(0xfc,fc),(0x10c,callback)]:image[off:off+4]=value.to_bytes(4,'little')
  u.mem_write(actor,bytes(image));put(0x121009c,limit);clears=actions=0;u.reg_write(UC_X86_REG_EBX,actor);u.reg_write(UC_X86_REG_ESI,0);u.reg_write(UC_X86_REG_ESP,0x2008000);u.emu_start(0x488118,0x487f94,count=10000)
  image=bytearray(u.mem_read(actor,0x114))
  if int.from_bytes(image[:4],'little'):image[4:8]=(1).to_bytes(4,'little')
  expected.append(f'{clears} {actions} '+image.hex())
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row))for row in rows)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows)
 bad=[{'input':rows[i],'actual':a,'expected':b}for i,(a,b)in enumerate(zip(lines,expected))if a!=b];args.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'body':'488118..488163 extra lifetime block','cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'wrappingFCincrement,signedlimit,full114clearbeforesecondary,currentsecondarymutation,normalizedfullcurrentimage','foreign_body':'secondary callback declaredfixture','audio_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
