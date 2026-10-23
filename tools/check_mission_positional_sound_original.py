#!/usr/bin/env python3
"""Replay positional sound, terrain occlusion and real bank resolution on Df.exe."""
import argparse,json,struct,subprocess
from pathlib import Path
from check_projectiles_original import Rig,STACK,SOURCE_SHA256
from unicorn import UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
GRAPH,PARAM,HEIGHT,SCRATCH=0x3100000,STACK-0x30,0x1800000,0xd099c0

def coord(seed,mul):return (((seed*mul)%17-8)*65536+(seed*33&0xffff))&0xffffffff

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=128);p.add_argument('--report',type=Path);a=p.parse_args();cases=[(mode,seed) for mode in ('o','p') for seed in range(a.cases)]
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{mode} {seed}\n' for mode,seed in cases),capture_output=True,text=True,check=True);got=[json.loads(line) for line in run.stdout.splitlines()]
 if len(got)!=len(cases):raise ValueError('Probe reply count differs')
 rig=Rig(a.game);rig.setup();rig.stubs.pop(0x43e7d0);rig.stubs.pop(0x49a620);rig.u.mem_map(GRAPH,0x10000);rig.u.mem_map(HEIGHT,0x100000);state={};maps={}
 def put(at,v,width=4):rig.u.mem_write(at,int(v&((1<<(width*8))-1)).to_bytes(width,'little'))
 def get(at,width=4):return int.from_bytes(rig.u.mem_read(at,width),'little')
 def sample(machine,access,at,width,value,user):
  if width!=1:raise ValueError('Height source width differs')
  state['samples'].append(at);seed=state['seed'];definition=state['definition']
  if len(state['samples'])==1 and seed&8:
   put(definition,GRAPH+0x400);put(definition+0x20,(seed&4)^4,1);put(0x6e06a8,get(0x6e06a8)+65536);put(0x6e06b4,get(0x6e06b4)+0x40000000)
 def voice(site,n):
  sp=rig.u.reg_read(UC_X86_REG_ESP);args=list(struct.unpack('<%dI'%n,rig.u.mem_read(sp+4,n*4)));state['calls'].append([site,*args]);seed=state['seed']
  if len(state['calls'])==1 and seed&16:
   put(state['definition'],0);put(PARAM+4,7);put(PARAM+0xc,3);put(0x6e06b4,0);at=SCRATCH+0x14
   for off,v in ((4,0x3200010),(8,0x10000),(0xc,128),(0x10,13)):put(at+off,v)
  rig.finish(0x90000000+len(state['calls']))
 rig.stubs[0x494dd0]=lambda:voice(0x494dd0,4);rig.stubs[0x494e80]=lambda:voice(0x494e80,5);hook=rig.u.hook_add(UC_HOOK_MEM_READ,sample,begin=HEIGHT,end=HEIGHT+0xfffff);diffs=[];played=0;sampled=0
 for (mode,seed),row in zip(cases,got):
  definition=0x4f2ea8+(seed%512)*0x28;state.update(seed=seed,definition=definition,samples=[],calls=[])
  source=[coord(seed,3),coord(seed,5),((seed%6-2)*65536)&0xffffffff];listener=[coord(seed+3,7),coord(seed+1,11),(seed%4)*65536]
  if seed%8==0:listener[:2]=source[:2]
  if seed%8==1:listener[:2]=[(source[0]+128)&0xffffffff,(source[1]+65535)&0xffffffff]
  parameter=PARAM if mode=='p' else 0x1302800
  rig.u.mem_write(GRAPH,bytes(0x1000));rig.u.mem_write(0x6e0518,bytes(0x190));rig.u.mem_write(definition,bytes(40));rig.u.mem_write(parameter,b'\xa5'*32);rig.u.mem_write(SCRATCH,b'\x5a'*0xa0)
  for at,v in ((definition,GRAPH if seed%7 else 0),(0x5014fc,seed+1),(0x501500,2),(0xbd5284,HEIGHT),(0x6e06a8,listener[0]),(0x6e06ac,listener[1]),(0x6e06b0,listener[2]),(0x6e06b4,seed*0x1234567)):put(at,v)
  put(definition+0x20,seed&4,1)
  for bank in (GRAPH,GRAPH+0x400):
   for off,v in ((0x48,0 if bank!=GRAPH and seed&32 else (0,1,2,8,32,0xffffffff)[seed%6]),(0x24,1),(0x28,GRAPH+0x100),(0x1c,0x10000 if bank==GRAPH else 0x20000)):put(bank+off,v)
  put(GRAPH+0x100,1,2)
  for at,v in ((GRAPH+0x108,3),(GRAPH+0x110,GRAPH+0x200),(GRAPH+0x200,GRAPH+0x300),(GRAPH+0x204,0x10000),(GRAPH+0x324,0x3200000)):put(at,v)
  put(GRAPH+0x323,1,1)
  key=seed%16
  if key not in maps:maps[key]=bytes((x*13+y*7+key)&255 for y in range(1024) for x in range(1024))
  rig.u.mem_write(HEIGHT,maps[key]);rig.call(0x43e940,*source,*listener) if mode=='o' else rig.call(0x43e7d0,seed%512,*source)
  result=rig.u.reg_read(UC_X86_REG_EAX) if mode=='o' else 0
  want=dict(result=result,params=bytes(rig.u.mem_read(parameter,32)).hex(),scratch=bytes(rig.u.mem_read(SCRATCH,0xa0)).hex(),globals=[get(0x5014fc),get(definition),get(definition+0x20,1),get(0x6e06a8),get(0x6e06ac),get(0x6e06b0),get(0x6e06b4)],samples=state['samples'],calls=state['calls'])
  played+=len(state['calls']);sampled+=len(state['samples'])
  if want!=row:diffs.append(dict(mode=mode,seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 rig.u.hook_del(hook);report=dict(source_sha256=SOURCE_SHA256,cases=len(cases),voice_calls=played,height_reads=sampled,differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
