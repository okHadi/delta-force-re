#!/usr/bin/env python3
"""Replay4477F0 and49BAE6 with mutable terrain/expiry fixtures."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UcError,UC_ERR_EXCEPTION
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
BASE=0x6e5778
CONTROLS=(0x007f,0x047f,0x087f,0x0c7f,0x027f,0x067f,0x0a7f,0x0e7f,0x037f,0x077f,0x0b7f,0x0f7f)
def original(raw,seed):
 h=Harness(raw);u=h.machine();cells=[];rng=seed+1
 for _ in range(67):rng=(rng*1664525+1013904223)&0xffffffff;cells.append(rng)
 for i in range(16):cells[i*4+3]=(0,1,2,0xffffffff,0x80000000,3)[(seed+i)%6]
 cells[66]=(0,1,0xffffffff,0x80000000)[seed%4];u.mem_write(BASE,struct.pack('<67I',*cells));u.reg_write(UC_X86_REG_FPCW,CONTROLS[seed%12]);calls=[];writes=[];height_calls=0
 def get(a):return int.from_bytes(u.mem_read(a,4),'little')
 def put(a,v):u.mem_write(a,struct.pack('<I',v));writes.append([a,v])
 def store(m,access,a,size,value,user):
  if size!=4:raise ValueError('Unexpected source cell width')
  writes.append([a,value&0xffffffff])
 def code(m,a,size,user):
  nonlocal height_calls
  sp=u.reg_read(UC_X86_REG_ESP)
  if a==0x47df40:
   calls.append([a,get(sp+4),get(sp+8)]);height_calls+=1
   if seed&1:put(BASE,123);put(0x6e5880,17);put(0x6e5874,1)
   value=(0,0xffffffff,0x7fffffff,0x80000000,0x007fffff,0xff800000,0x00ffffff,0xffffe666)[(seed+height_calls)%8];u.reg_write(UC_X86_REG_EAX,value)
  else:
   p=get(sp+8);calls.append([a,get(sp+4),*struct.unpack('<7I',u.mem_read(p,28))])
   if seed&2:put(0x6e5878,0);put(0x6e587c,0);put(0x6e5880,0xffffffff)
   u.mem_write(p,bytes(4))
  u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 hooks=[u.hook_add(UC_HOOK_CODE,code,begin=a,end=a) for a in (0x47df40,0x4470c0)];hooks.append(u.hook_add(UC_HOOK_MEM_WRITE,store,begin=BASE,end=0x6e5883));h.call(u,0x4477f0)
 result=dict(cells=list(struct.unpack('<67I',u.mem_read(BASE,268))),calls=calls,writes=writes)
 for x in hooks:u.hook_del(x)
 return result
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=240);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{s}\n' for s in range(a.cases)),text=True,capture_output=True,check=True);got=[json.loads(x) for x in run.stdout.splitlines()]
 if len(got)!=a.cases:raise ValueError('Probe reply count differs')
 # A separate actual FILD/FADD/49BAE6/FISTP fixture covers precision boundaries.
 heights=[0,1,0xffffffff,0x7fffffff,0x80000000]
 for edge in (0,1<<22,1<<23,1<<24,1<<30,0x7fffffff):
  for delta in range(-8,9):heights.extend(((edge+delta-6553)&0xffffffff,(-edge+delta-6553)&0xffffffff))
 heights=sorted(set(heights));fp_cases=[(v,c) for c in CONTROLS for v in heights]
 fp_run=subprocess.run([str(a.probe.resolve())],input=''.join(f'h {v} {c}\n' for v,c in fp_cases),text=True,capture_output=True,check=True)
 fp_rows=[json.loads(x) for x in fp_run.stdout.splitlines()]
 if len(fp_rows)!=len(fp_cases):raise ValueError('Height reply count differs')
 h=Harness(raw);u=h.machine();at=0x480000
 code=bytes.fromhex('db05')+struct.pack('<I',0x1500000)+bytes.fromhex('dc05')+struct.pack('<I',0x4cc6b4)+bytes.fromhex('e8')+struct.pack('<i',0x49bae6-(at+17))+bytes.fromhex('db1d')+struct.pack('<I',0x1500004)+bytes.fromhex('c3')
 u.mem_write(at,code);fp_diffs=[]
 for (v,c),row in zip(fp_cases,fp_rows):
  u.mem_write(0x1500000,struct.pack('<I',v));u.reg_write(UC_X86_REG_FPCW,c);h.call(u,at);want=int.from_bytes(u.mem_read(0x1500004,4),'little')
  if row!={'height':want}:fp_diffs.append(dict(height=v,control=c,want=want,got=row))
 diffs=[]
 for s,row in enumerate(got):
  want=original(raw,s)
  if want!=row:diffs.append(dict(seed=s,want=want,got=row))
  if len(diffs)>=3:break
 # The exact IDIV overflow retains only the earlier timer decrement.
 fault_h=Harness(raw);fault_u=fault_h.machine();fault_u.mem_write(BASE,bytes(268))
 for address,value in ((0x6e5784,2),(0x6e5878,0x80000000),(0x6e587c,0x40000001),(0x6e5880,1)):fault_u.mem_write(address,struct.pack('<I',value))
 try:fault_h.call(fault_u,0x4477f0);raise ValueError('Original IDIV did not fault')
 except UcError as error:
  if error.errno!=UC_ERR_EXCEPTION:raise
 if struct.unpack('<4I',fault_u.mem_read(BASE,16))!=(0,0,0,1) or int.from_bytes(fault_u.mem_read(0x6e5880,4),'little')!=1:raise ValueError('Original fault prefix differs')
 report=dict(idiv_fault_cases=1,source_sha256=SOURCE_HASH,cases=a.cases,height_cases=len(fp_cases),differences=len(diffs)+len(fp_diffs),details=diffs,height_details=fp_diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k not in ('details','height_details')}));return bool(diffs or fp_diffs)
if __name__=='__main__':raise SystemExit(main())
