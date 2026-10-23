#!/usr/bin/env python3
"""Replay4470C0 with mutable outbox/effect fixtures and actual x87 conversion."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from check_game_messages_original import Harness,SOURCE_HASH,STACK
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE,UcError,UC_ERR_EXCEPTION,UC_ERR_READ_UNMAPPED
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
BASE=0x6e5778
CONTROLS=(0x007f,0x047f,0x087f,0x0c7f,0x027f,0x067f,0x0a7f,0x0e7f,0x037f,0x077f,0x0b7f,0x0f7f)
def original(raw,seed):
 h=Harness(raw);u=h.machine();words=[];rng=seed+1
 for _ in range(7):rng=(rng*1664525+1013904223)&0xffffffff;words.append(rng)
 words[2]=(0,1,0xffffffff,0x7fffffff,0x80000000,0x007fffff,0xff800000,0xffffcccd)[seed%8]
 actor=(0x7cca80,0x7cca80+0x114*7,0,0xffffffff,0x7cca7f,0x7cca80+0x114*0x500)[seed%6];u.mem_write(0x1500000,struct.pack('<7I',*words));u.mem_write(0x67f528,struct.pack('<I',seed%3));u.reg_write(UC_X86_REG_FPCW,CONTROLS[seed%12]);packet=[];effect=[];calls=[]
 def get(a):return int.from_bytes(u.mem_read(a,4),'little')
 def code(m,a,size,user):
  sp=u.reg_read(UC_X86_REG_ESP);calls.append(a)
  if a==0x442630:
   if get(sp+4)!=0x27 or get(sp+12)!=18:raise ValueError('Original packet arguments differ')
   p=get(sp+8);packet.extend(u.mem_read(p,18))
   if seed&1:
    u.mem_write(0x1500000,struct.pack('<I',get(0x1500000)^0x12345678));u.mem_write(0x1500008,struct.pack('<I',0x80000000));u.mem_write(0x67f528,bytes(4))
   u.mem_write(p,bytes(1))
  else:
   p=get(sp+8);effect.extend([get(sp+4),get(sp+12),*struct.unpack('<7I',u.mem_read(p,28))]);u.mem_write(p+4,struct.pack('<I',99))
  u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 hooks=[u.hook_add(UC_HOOK_CODE,code,begin=a,end=a) for a in (0x442630,0x460950)];h.call(u,0x4470c0,(actor,0x1500000));result=dict(words=list(struct.unpack('<7I',u.mem_read(0x1500000,28))),packet=packet,effect=effect,calls=calls,gate=get(0x67f528))
 for x in hooks:u.hook_del(x)
 return result
def original_effect(raw,seed):
 h=Harness(raw);u=h.machine();words=[];rng=seed+1
 for _ in range(7):rng=(rng*1664525+1013904223)&0xffffffff;words.append(rng)
 u.mem_write(0x1500000,struct.pack('<7I',*words));u.mem_write(STACK+0x80000-20,struct.pack('<I',0xa5a50000+seed));calls=[]
 actor=(0x7cca80+seed*0x114)&0xffffffff;count=(0,1,3,0xffffffff,0x80000000,7)[seed%6]
 arity={0x477850:1,0x477860:0,0x45dc70:4,0x4657d0:5,0x43e7d0:4,0x45fcf0:6}
 def get(a):return int.from_bytes(u.mem_read(a,4),'little')
 def put(a,v):u.mem_write(a,struct.pack('<I',v&0xffffffff))
 def hook(m,site,size,user):
  nonlocal rng
  sp=u.reg_read(UC_X86_REG_ESP);result=0
  if site==0x463200:
   p=get(sp+4);current=get(sp+8);speed=get(sp+12);calls.append([site,current,speed,*struct.unpack('<6I',u.mem_read(p,24))])
   if seed&4:
    put(p,get(p)+1);put(p+4,get(p+4)^2);put(p+8,get(p+8)-3);put(p+20,get(p+20)^4);put(0x150000c,get(0x150000c)+5)
  else:
   args=[get(sp+4+i*4) for i in range(arity[site])];calls.append([site,*args])
   if site==0x477850:
    rng=args[0]^seed
    if seed&1:put(0x1500000,get(0x1500000)+11)
   elif site==0x477860:
    rng=(rng*1664525+1013904223)&0xffffffff;result=rng
    if seed&2:put(0x150000c,get(0x150000c)+rng)
   elif seed&1:
    if site==0x45dc70:put(0x1500000,get(0x1500000)^11)
    if site==0x4657d0:put(0x1500004,get(0x1500004)+22)
    if site==0x43e7d0:put(0x1500008,get(0x1500008)-33)
    if site==0x45fcf0:put(0x1500000,get(0x1500000)+44)
  u.reg_write(UC_X86_REG_EAX,result);u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 hooks=[u.hook_add(UC_HOOK_CODE,hook,begin=site,end=site) for site in (*arity,0x463200)];h.call(u,0x460950,(actor,0x1500000,count));result=dict(words=list(struct.unpack('<7I',u.mem_read(0x1500000,28))),calls=calls)
 for hook_id in hooks:u.hook_del(hook_id)
 return result
def check_record_extent(raw):
 for count in (0,1):
  h=Harness(raw);u=h.machine();p=0x1550000-12;u.mem_write(p,struct.pack('<3I',1,2,3));sites=[]
  def code(m,site,size,user):
   sites.append(site);sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_EIP,int.from_bytes(u.mem_read(sp,4),'little'));u.reg_write(UC_X86_REG_ESP,sp+4)
  hooks=[u.hook_add(UC_HOOK_CODE,code,begin=site,end=site) for site in (0x477850,0x477860,0x45dc70,0x4657d0,0x43e7d0,0x45fcf0)]
  try:
   h.call(u,0x460950,(77,p,count))
   if count:raise ValueError('Original fourth-word read did not fault')
  except UcError as error:
   if not count or error.errno!=UC_ERR_READ_UNMAPPED:raise
  want=[0x477850,0x45dc70,0x4657d0,0x43e7d0,0x45fcf0]+([0x477860,0x477860] if count else [])
  if sites!=want:raise ValueError('Original record extent prefix differs')
  for hook_id in hooks:u.hook_del(hook_id)
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=240);p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source hash differs')
 run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{s}\n' for s in range(a.cases)),text=True,capture_output=True,check=True);got=[json.loads(x) for x in run.stdout.splitlines()]
 if len(got)!=a.cases:raise ValueError('Probe reply count differs')
 # A separate actual FILD/FADD/49BAE6/FISTP fixture covers precision boundaries.
 heights=[0,1,0xffffffff,0x7fffffff,0x80000000]
 for edge in (0,1<<22,1<<23,1<<24,1<<30,0x7fffffff):
  for delta in range(-8,9):heights.extend(((edge+delta-13107)&0xffffffff,(-edge+delta-13107)&0xffffffff))
 heights=sorted(set(heights));fp_cases=[(v,c) for c in CONTROLS for v in heights]
 fp_run=subprocess.run([str(a.probe.resolve())],input=''.join(f'h {v} {c}\n' for v,c in fp_cases),text=True,capture_output=True,check=True)
 fp_rows=[json.loads(x) for x in fp_run.stdout.splitlines()]
 if len(fp_rows)!=len(fp_cases):raise ValueError('Height reply count differs')
 h=Harness(raw);u=h.machine();at=0x480000
 code=bytes.fromhex('db05')+struct.pack('<I',0x1500000)+bytes.fromhex('dc05')+struct.pack('<I',0x4cc6ac)+bytes.fromhex('e8')+struct.pack('<i',0x49bae6-(at+17))+bytes.fromhex('db1d')+struct.pack('<I',0x1500004)+bytes.fromhex('c3')
 u.mem_write(at,code);fp_diffs=[]
 for (v,c),row in zip(fp_cases,fp_rows):
  u.mem_write(0x1500000,struct.pack('<I',v));u.reg_write(UC_X86_REG_FPCW,c);h.call(u,at);want=int.from_bytes(u.mem_read(0x1500004,4),'little')
  if row!={'height':want}:fp_diffs.append(dict(height=v,control=c,want=want,got=row))
 effect_run=subprocess.run([str(a.probe.resolve())],input=''.join(f'e {seed}\n' for seed in range(a.cases)),text=True,capture_output=True,check=True)
 effect_rows=[json.loads(line) for line in effect_run.stdout.splitlines()]
 if len(effect_rows)!=a.cases:raise ValueError('Effect reply count differs')
 effect_diffs=[]
 for seed,row in enumerate(effect_rows):
  want=original_effect(raw,seed)
  if row!=want:effect_diffs.append(dict(seed=seed,want=want,got=row))
  if len(effect_diffs)>=3:break
 diffs=[]
 for s,row in enumerate(got):
  want=original(raw,s)
  if want!=row:diffs.append(dict(seed=s,want=want,got=row))
  if len(diffs)>=3:break
 check_record_extent(raw)
 report=dict(record_extent_cases=2,source_sha256=SOURCE_HASH,cases=a.cases,height_cases=len(fp_cases),effect_cases=len(effect_rows),differences=len(diffs)+len(fp_diffs)+len(effect_diffs),details=diffs,height_details=fp_diffs,effect_details=effect_diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k not in ('details','height_details','effect_details')}));return bool(diffs or fp_diffs or effect_diffs)
if __name__=='__main__':raise SystemExit(main())
