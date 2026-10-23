#!/usr/bin/env python3
"""Compare the real45D970 reads, stores, alias copies and required person call."""
import argparse,json,random,subprocess
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from pathlib import Path
from check_projectiles_original import Rig,SOURCE_SHA256

def run(args):
 rig=Rig(args.game);rig.stubs.pop(0x45d970,None);u=rig.u;active=False;trace=[];mutation=source=count_at=base=occupied=writes=person=0;failed=False;memory={}
 def put(at,n,value):
  data=(value&((1<<(n*8))-1)).to_bytes(n,'little');u.mem_write(at,data)
  for i,v in enumerate(data):memory[at+i]=v
 def get(at,n=4):return int.from_bytes(u.mem_read(at,n),'little')
 def relevant(at):return base<=at<base+0x45000 or source<=at<source+0x114 or count_at<=at<count_at+4 or 0x1210024<=at<0x121002c
 def read_hook(u,kind,at,n,value,_):
  if not active or not relevant(at):return
  trace.append(f'R{at},{n};')
  if mutation&2 and at==source+0x10:put(source+0x30,4,0xfedcba98)
  if mutation&4 and at==count_at:put(count_at,4,0xffffffff)
 def write_hook(u,kind,at,n,value,_):
  nonlocal writes,failed
  if not active or not relevant(at):return
  trace.append(f'W{at},{n},{value};');writes+=1
  if mutation&16 and writes==14:failed=True;u.emu_stop();return
  for i in range(n):memory[at+i]=(value>>(i*8))&255
  if mutation&8 and at==base+occupied*0x114+0x20:put(source+0x40,4,0xabcdef01)
 u.hook_add(unicorn.UC_HOOK_MEM_READ,read_hook);u.hook_add(unicorn.UC_HOOK_MEM_WRITE,write_hook)
 def allocate():
  nonlocal failed
  actor=get(u.reg_read(UC_X86_REG_ESP)+4);trace.append(f'P{actor};');put(actor+0xc,4,0xf00df00d);put(count_at,4,0x12345678)
  if mutation&32:failed=True;u.emu_stop();return
  rig.finish(person)
 rig.stubs[0x44a0c0]=allocate
 rows=[];rng=random.Random(0x45d970)
 for mode in [0,1,0xffffffff]:
  for occupied in [0,2,1023,1024]:
   for count in [0,1024,0x80000000,0xffffffff]:
    for flags in [0,4]:rows.append([mode,occupied,count,6,flags,0 if flags==0 else 0x794680,0,0])
 for alias in [0,1,2,3]:
  for mutation in [0,1,2,4,8,16,32,63]:
   for mode in [0,1]:rows.append([mode,2,0,6,4,0x794680,alias,mutation])
 rows += [[0,0,0,k,4,0,0,0] for k in [0,5,7,0xffffffff]]
 for _ in range(args.cases):rows.append([rng.randrange(2),rng.randrange(5),rng.getrandbits(32),rng.choice([5,6,7]),rng.choice([0,4,255]),rng.choice([0,0x794680]),rng.randrange(4),rng.randrange(64)])
 expected=[]
 for row in rows:
  mode,occupied,count,kind,flags,person,alias,mutation=row;base=0x822e80 if mode else 0x7dde80;count_at=0x8cfa14 if mode else 0x8cfa1c;source={1:base+0x114*5,2:base+0x114*2-4,3:base+0x114*2+4}.get(alias,0x1200000);trace=[];writes=0;failed=False;memory={}
  u.mem_write(base,b'\0'*0x45000)
  memory.update((base+i,0)for i in range(0x45000))
  for i in range(occupied):put(base+i*0x114,4,i+1)
  for off in range(0,0x114,4):put(source+off,4,0x10203040^off)
  put(source,4,0 if mutation&1 else 3);put(source+4,4,0 if alias==2 and mode else 0x1210000);put(0x1210028,4,kind);put(0x1210024,1,flags);put(count_at,4,count)
  active=True
  try:
   rig.call(0x45d970,source,mode);result=u.reg_read(UC_X86_REG_EAX)
  except RuntimeError:
   if not failed:raise
   result=0
  finally:active=False
  h=2166136261
  for _,v in sorted(memory.items()):h=((h^v)*16777619)&0xffffffff
  expected.append(f'{int(failed)} {result} {get(count_at)} {h} '+''.join(trace))
 p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r))for r in rows)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 lines=p.stdout.splitlines();assert len(lines)==len(expected),(len(lines),len(expected));bad=[{'case':i,'input':rows[i],'actual':a,'expected':b}for i,(a,b)in enumerate(zip(lines,expected))if a!=b]
 args.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'body':'45D970','cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'exact ordered reads/stores, full physical pool/source/count bytes, 69dword forward REP aliases, signed highwater, current source mutations, person result/store/failure prefix','foreign_body':'44A0C0 typed required provider','audio_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
