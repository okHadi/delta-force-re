#!/usr/bin/env python3
"""Compare45DAB0 current reads, calls, shared RNG and full actor image."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
RNG=0xbd0060;ITEMS=[0x8e44c4,0x8e44c8,0x8e44cc,0x8e44d0,0x8e44d4,0x8e44e4]
def run(args):
 rig=Rig(args.game);rig.stubs.pop(0x45dab0,None);u=rig.u;v=None;trace=[];index=3;table=0x1210000;reads=calls=0;failed=False;active=False;grid_at=0
 def put(at,n,width=4):u.mem_write(at,(n&((1<<(8*width))-1)).to_bytes(width,'little'))
 def read(at,width=4):return int.from_bytes(u.mem_read(at,width),'little')
 def source(u,kind,at,size,value,_):
  nonlocal reads,index,failed
  if not active:return
  relevant=at in [0xbd52ac,grid_at,0xbd533b,0xc23810]+ITEMS or (at>=table and at<table+0x1000 and (at-table)%4==0)
  if not relevant:return
  trace.append(f'R{at},{size};');reads+=1
  if v[5]&64 and reads==1:failed=True;u.emu_stop();return
  if at==0xbd533b and v[5]&1:
   index=5
   for p in ITEMS:put(p,index)
  if at>=table and at<table+0x1000 and at%4==0 and v[5]&8:put(RNG,read(RNG)^0xabcdef01)
 u.hook_add(unicorn.UC_HOOK_MEM_READ,source)
 def foreign(site):
  nonlocal calls,index,table,failed
  sp=u.reg_read(UC_X86_REG_ESP);values=[0]*6;data=b''
  if site==0x43e7d0:values[:4]=[read(sp+4+i*4) for i in range(4)]
  elif site==0x45fcf0:values=[read(sp+4+i*4) for i in range(6)]
  else:values[1]=read(sp+8);data=bytes(u.mem_read(read(sp+4),0x114))
  trace.append('C'+str(site)+''.join(','+str(n) for n in values)+':'+data.hex()+';');calls+=1
  if site==0x43e7d0:
   if v[5]&2:
    index=9
    for at in ITEMS:put(at,index)
   if v[5]&4:table=0x1211000;put(0xc23810,table)
   if v[5]&16:put(RNG,0xfedcba98)
  if(v[5]&32 and calls==2)or(v[5]&128 and site==0x45d970):failed=True;u.emu_stop();return
  rig.finish()
 for site in [0x43e7d0,0x45fcf0,0x45d970]:rig.stubs[site]=lambda site=site:foreign(site)
 rng=random.Random(0x45dab0);rows=[]
 for t in [0,1,2,3,4,5,6,7,255,256,257]:
  for mutation in [0,1,2,4,8,16,32,64,128,255]:rows.append([t,rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),mutation])
 for _ in range(args.cases):rows.append([rng.randrange(9),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256)])
 expected=[]
 for v in rows:
  t,x,y,z,seed,mutation=v;index=3;table=0x1210000;trace=[];reads=calls=0;failed=False;grid_at=0x1220000+((x&0x3ff0000)>>16)+((y&0x3ff0000)>>6);put(0xbd52ac,0x1220000);put(grid_at,7,1);put(0xbd533b,t,1);put(0xc23810,table);put(RNG,seed)
  for at in ITEMS:put(at,3)
  for base in [0x1210000,0x1211000]:
   for i in [3,5,9]:
    for off in [0x6c,0x84,0x90]:put(base+i*0x13c+off,(base+i*0x13c+off)^0x55667788)
  active=True
  try:rig.call(0x45dab0,x,y,z)
  except RuntimeError:
   if not failed:raise
  finally:active=False
  expected.append(f'{int(failed)} {read(RNG)} {index} {table} '+''.join(trace))
 result=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True);assert result.returncode==0,result.stderr;lines=result.stdout.splitlines();assert len(lines)==len(expected);bad=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(lines,expected)) if a!=b]
 report={'source_sha256':SOURCE_SHA256,'body':'45DAB0','cases':len(rows),'differences':len(bad),'mismatches':bad[:3],'comparison':'sourceorderedgrid/type/selectedindex/itemreads,currenttableandRNGaftercallbacks,whole0x114image,sound/zone/insertionarguments,unknownterraintypes,wrappingcoordinates,errorprefix','foreign_bodies':['43E7D0','45FCF0','45D970'],'audio_output':0};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
