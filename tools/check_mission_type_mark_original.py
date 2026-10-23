#!/usr/bin/env python3
"""Replay actual44A3E0 with source scalar indices and wrapping addresses."""
import argparse,json,random,subprocess
from pathlib import Path
import unicorn
from check_projectiles_original import Rig,SOURCE_SHA256

def run(a):
 r=Rig(a.game);u=r.u;writes=[];active=False
 def hook(u,kind,at,n,value,_):
  if active:writes.append(f'{at} {n} {value}')
 u.hook_add(unicorn.UC_HOOK_MEM_WRITE,hook);rng=random.Random(0x44a3e0);rows=list(range(64))+[64,0x3ffffff,0x4000000,0x400003f,0x7fffffff,0x80000000,0xffffffc0,0xffffffff]+[rng.getrandbits(32)for _ in range(a.cases)];expected=[]
 for index in rows:
  at=(0x7936a4+((index<<6)&0xffffffff))&0xffffffff
  try:u.mem_read(at,4)
  except unicorn.UcError:u.mem_map(at&~0xfff,0x1000)
  u.mem_write(at,b'\xa5'*4);writes=[];active=True;r.call(0x44a3e0,index);active=False;assert len(writes)==1;assert bytes(u.mem_read(at,4))==b'\x02\0\0\0';expected+=writes
 p=subprocess.run([str(a.probe)],input='\n'.join(map(str,rows))+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr;lines=p.stdout.splitlines();assert len(lines)==len(rows);bad=[{'input':rows[i],'actual':x,'expected':y}for i,(x,y)in enumerate(zip(lines,expected))if x!=y];a.report.write_text(json.dumps({'source_sha256':SOURCE_SHA256,'body':'44A3E0','cases':len(rows),'differences':len(bad),'mismatches':bad[:2],'comparison':'actual4instructions,rawscalarindex,wrapping32shift/address,oneDWORDstore2','foreign_bodies':[],'audio_output':0},indent=2)+'\n');print(json.dumps({'cases':len(rows),'differences':len(bad)}));assert not bad
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=256);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
