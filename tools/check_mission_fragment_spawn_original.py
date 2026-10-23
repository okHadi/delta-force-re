#!/usr/bin/env python3
"""Replay463200, real4776C0 and full114-byte pool records with callback changes."""
import argparse,json,struct,subprocess
from pathlib import Path
from check_projectiles_original import Rig,SCRATCH,ENTITY,ENTITY_INDEX,SOURCE_SHA256
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=160);p.add_argument('--report',type=Path);a=p.parse_args();game=a.game.parent if a.game.is_file() else a.game
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{seed}\n' for seed in range(a.cases)),text=True,capture_output=True,check=True);got=[json.loads(line) for line in run.stdout.splitlines()]
 if len(got)!=a.cases:raise ValueError('Probe reply count differs')
 rig=Rig(game);rig.setup();rig.stubs.pop(0x49a620);vector=SCRATCH+0x2400;current={}
 def get(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
 def put(at,value):rig.u.mem_write(at,struct.pack('<I',value&0xffffffff))
 def sine(machine,address,size,user):
  seed,selected=current['seed'],current['selected'];sp=rig.u.reg_read(UC_X86_REG_ESP);angle=get(sp+4);record=ENTITY+selected*0x114
  current['calls'].append([angle,get(ENTITY_INDEX),bytes(rig.u.mem_read(record,0x114)).hex()]);current['call']+=1;call=current['call']
  if seed&1:
   put(vector,get(vector)+call*11);put(vector+4,get(vector+4)^(call*22));put(vector+8,get(vector+8)-call*33);put(vector+12,get(vector+12)+call*0x12345);put(vector+16,get(vector+16)^(call*0x67890));put(ENTITY_INDEX,77 if call==1 else 0xffffffff)
   for at,value in ((8,0xa0+call),(0x20,0xb0+call),(0x70,0xc0+call),(0x10c,0xd0+call)):rig.u.mem_write(record+at,bytes([value]))
   put(record+0x10,0x12345678)
 hook=rig.u.hook_add(UC_HOOK_CODE,sine,begin=0x4776c0,end=0x4776c0);diffs=[]
 for seed,row in enumerate(got):
  ring=(0,511,0xffffffff,1000)[seed%4];selected=(ring+1)&511;current.update(seed=seed,selected=selected,calls=[],call=0)
  pool=bytearray()
  for index in range(512):
   image=bytearray((seed+index+at)&255 for at in range(0x114));struct.pack_into('<I',image,0xf8,0);pool.extend(image)
  rig.u.mem_write(ENTITY,bytes(pool));put(ENTITY_INDEX,ring);rng=seed+1;position=[]
  for _ in range(6):rng=(rng*1664525+1013904223)&0xffffffff;position.append(rng)
  rig.u.mem_write(vector,struct.pack('<6I',*position));owner=0x7cca80+(seed%32)*0x114 if seed%5 else 0;speed=(0,1,63,64,0x7fffffff,0x80000000,0xffffffff,0x100000)[seed%8]
  rig.call(0x463200,vector,owner,speed);result=rig.u.reg_read(UC_X86_REG_EAX);checksum=2166136261
  for byte in rig.u.mem_read(ENTITY,512*0x114):checksum=((checksum^byte)*16777619)&0xffffffff
  want=dict(result=result,ring=get(ENTITY_INDEX),record=bytes(rig.u.mem_read(ENTITY+selected*0x114,0x114)).hex(),checksum=checksum,input=list(struct.unpack('<6I',rig.u.mem_read(vector,24))),calls=current['calls'])
  if want!=row:diffs.append(dict(seed=seed,want=want,got=row))
  if len(diffs)>=3:break
 rig.u.hook_del(hook);report=dict(source_sha256=SOURCE_SHA256,cases=a.cases,pool_records=512,record_bytes=0x114,real_bodies=['463200','4776C0','49A620'],differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
