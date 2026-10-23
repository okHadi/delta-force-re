#!/usr/bin/env python3
"""Compare assembled effect metadata with the original45D6F0 item loader."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH

def main():
 q=argparse.ArgumentParser();q.add_argument('game',type=Path);q.add_argument('probe',type=Path);q.add_argument('--cases',type=int,default=1024);q.add_argument('--report',type=Path);a=q.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x45d6f0);rows=[];expected=[];D,G,L=0x1300000,0x1301000,0x1302000
 edges=[0,1,0xffff,0x10000,0xffffffff,0x80000000,0x7fffffff,0x7fff0000,0xffff0000]
 for case in range(a.cases):
  width=edges[case%len(edges)] if case<3*len(edges)**2 else rng.getrandbits(32);height=edges[(case//len(edges))%len(edges)] if case<3*len(edges)**2 else rng.getrandbits(32);kind=6 if case%7==0 else 8;loaded=int(case%5!=0);lod=int(case%3!=0);rows.append(f'{width} {height} {kind} {loaded} {lod}')
  r.u.mem_write(D,bytes(0x13c));r.u.mem_write(G,bytes(0x60));r.u.mem_write(L,bytes(184));r.write(0xc23810,D);r.write(0xc23814,1);r.write(0x503138,1);r.write(D+0x28,kind);r.u.mem_write(D+0x2c,b'sheet\0');r.write(G+0x50,L if lod else 0);r.write(L+0x40,width);r.write(L+0x58,height)
  def model():
   sp=r.u.reg_read(UC_X86_REG_ESP);out=r.read(sp+12)[0];r.write(out,0);r.u.reg_write(UC_X86_REG_EAX,G if loaded else 0)
  r.hook(0x486530,model);r.call(0x45d6f0,0);r.clear_hooks();expected.append(f'{r.read(D+0x9c)[0]} {int(kind==8 and loaded)}')
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',text=True,capture_output=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y} for i,(x,y) in enumerate(zip(expected,got)) if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'real_body':'45D6F0','foreign_fixture':'486530 returns the supplied model pointer and firstload0; no husk or character files','comparison':'Assembledtype8 count from firstLOD40/58 signedhighwords, nullmodel/LOD and nontype8 gates','details':diffs[:3]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
