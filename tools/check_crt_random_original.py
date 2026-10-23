#!/usr/bin/env python3
"""Compare49AF6C and49AF90 using the current seed provider."""
import argparse,json,random,subprocess
from pathlib import Path
from check_projectiles_original import Rig,SOURCE_SHA256
from unicorn.x86_const import UC_X86_REG_EAX

def run(args):
 rig=Rig(args.game);u=rig.u;slots=[0x1200000,0x1200004];reads=0;mode=0
 def put(at,n):u.mem_write(at,(n&0xffffffff).to_bytes(4,'little'))
 def get(at):return int.from_bytes(u.mem_read(at,4),'little')
 def current():
  nonlocal reads
  n=reads;reads+=1
  if mode&4:rig.finish(0);return
  at=slots[1 if mode&1 and n%2 else 0]
  if mode&2:put(at,get(at)^0x89abcdef)
  rig.finish(at)
 rig.stubs[0x49af60]=current;rng=random.Random(0x49af6c);rows=[]
 for seed in [0,1,0x7fffffff,0x80000000,0xffffffff]:
  for mode in range(8):rows.append([seed,rng.getrandbits(32),mode,rng.getrandbits(32)])
 for _ in range(args.cases):rows.append([rng.getrandbits(32),rng.getrandbits(32),rng.randrange(8),rng.getrandbits(32)])
 expected=[]
 for a,b,mode,value in rows:
  put(slots[0],a);put(slots[1],b);reads=0;rig.call(0x49af6c);first=u.reg_read(UC_X86_REG_EAX);rig.call(0x49af90,value);rig.call(0x49af6c);second=u.reg_read(UC_X86_REG_EAX);expected.append(f'{get(slots[0])} {get(slots[1])} {first} {second} {reads}')
 actual=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,r)) for r in rows)+'\n',text=True,capture_output=True);assert actual.returncode==0,actual.stderr
 lines=actual.stdout.splitlines();assert len(lines)==len(expected);bad=[{'case':i,'input':rows[i],'actual':a,'expected':b} for i,(a,b) in enumerate(zip(lines,expected)) if a!=b]
 # Check the actual default-seed store before the initializer's required callbacks.
 init=[];block=0x1201000;u.mem_write(block,bytes([0x99])*0x100)
 def locale():
  init.append(['locale',get(block+12)]);rig.finish()
 rig.stubs[0x4bf7f0]=locale;rig.stubs[0x1101000]=lambda:rig.finish(0x1234);put(0x4c6740,0x1101000)
 rig.call(0x4c19f0,block);assert get(block+12)==1 and get(block+0xda)==0x1234 and init==[['locale',1]]
 rig.call(0x4c19f0,0);assert init==[['locale',1]]
 report={'source_sha256':SOURCE_SHA256,'bodies':['49AF6C','49AF90'],'initializer_default':1,'initializer_locale_observed_seed':1,'initializer_null_returns':True,'cases':len(rows),'differences':len(bad),'mismatches':bad[:3],'comparison':'return values,bothcurrentseedcells,providerreadcount,switchingcurrentseed,providermutations,nullseed,wrappingedges'};args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'initializer_default':1,'initializer_locale_observed_seed':1,'initializer_null_returns':True,'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
