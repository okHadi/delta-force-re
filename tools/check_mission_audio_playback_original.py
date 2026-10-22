#!/usr/bin/env python3
"""Compare owned voice records, code operands, calls and PCM reads with Df.exe."""
import argparse, hashlib, json, struct, subprocess
from pathlib import Path
from check_game_messages_original import Harness, SOURCE_HASH
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
BASE=0x4a16d2
OPS=('init','play','fixed','ambient','update','playing','stop','register','publish','frames','dirty','stream','disabled')
def original(raw,op,s):
 h=Harness(raw);u=h.machine()
 def put(a,v,n=4):u.mem_write(a,int(v&((1<<(n*8))-1)).to_bytes(n,'little'))
 def get(a,n=4):return int.from_bytes(u.mem_read(a,n),'little')
 def call(a,*args):h.call(u,a,args);return u.reg_read(UC_X86_REG_EAX)
 calls=[];reads=[]
 def hook(m,a,size,user):
  if a not in (0x480123,0x4b4bd4):return
  if a==0x480123:
   calls.append([a,u.reg_read(UC_X86_REG_EAX),u.reg_read(UC_X86_REG_ECX)])
   if s&1:put(0x4a1d52+3*0x30+0x10,99)
  sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EIP,get(sp));u.reg_write(UC_X86_REG_ESP,sp+4)
 def read(m,access,a,size,value,user):
  if size==1 and 0x1500010<=a<0x1500080:reads.append(a)
 hooks=[u.hook_add(UC_HOOK_CODE,hook,begin=0x480123,end=0x480123),u.hook_add(UC_HOOK_CODE,hook,begin=0x4b4bd4,end=0x4b4bd4),u.hook_add(UC_HOOK_MEM_READ,read,begin=0x1500010,end=0x150007f)]
 put(0xd08940,(s//4)&1);put(0x4a2ae0,[1,0xffffffff,0,123][s%4]);put(0x4a2716,[255,0,128,0xffffffff][s%4]);put(0x4a2ae8,0x4a2aec)
 for i in range(16):
  for table,target in ((0x4a2720,0x4a3651),(0x4a2760,0x4a3667)):put(table+i*4,target+i*0x40)
  for table,right,left in ((0x4a27a0,0x4a3671,0x4a367e),(0x4a2820,0x4a3679,0x4a3686)):
   put(table+i*8,right+i*0x40);put(table+i*8+4,left+i*0x40)
 sample=bytearray(128);struct.pack_into('<II',sample,4,[0,1,2,32,100,0x80000000,0xffffffff][s%7],0x10000000);sample[12]=s%33
 for i in range(16,128):sample[i]=((0x1500000+i)*17)&255
 u.mem_write(0x1500000,bytes(sample));u.mem_write(BASE,b'\x5a'*0x1038);call(0x4a2aed)
 for i in range(16):
  u.mem_write(0x4a1a12+i*0x34+0x31,bytes.fromhex('a55ac3'));u.mem_write(0x4a2052+i*0x34+0x31,bytes.fromhex('b66bd4'))
 pan=(s*73)&0xffffffff;vol=[0,255,0xffffffff,0x7fffffff,0x80000000,128][s%6];rate=[0,0x10000,0x8000,0xffffffff,0x7fffffff][s%5];result=0
 if op=='init':call(0x4a2aed)
 elif op=='play':
  for i in range(16):
   a=BASE+i*0x34;put(a+0x30,int(s%4==1),1);put(a+0x31,int(s%4==3),1);put(a+0x1c,0 if i==2 else 500)
   if s%4==2:put(0x4a1a12+i*0x34+0x30,1,1)
  result=call(0x494dd0,0x1500000,pan,vol,rate)
 elif op=='fixed':
  if s%3==0:put(0x4a1a12+(s%16)*0x34+0x30,1,1)
  result=call(0x494e80,s%16,0x1500000,pan,vol,rate)
 elif op=='ambient':result=call(0x494f40,0x1500000,pan,vol,rate,s%16)
 elif op in ('update','playing','stop'):
  for a in (BASE+3*0x34,0x4a1d52+11*0x30):
   for off,v in ((0x10,7),(0x18,64),(0x1c,100),(0x14,0x10000000)):put(a+off,v)
  if s%3==1:put(BASE+3*0x34+0x10,0)
  put(0x4a1a12+5*0x34+0x10,7);put(0x4a1a12+5*0x34+0x30,s&1,1);handle=[0,7,99][s%3]
  if op=='update':result=call(0x494fa0,handle,0xffffffff if s&1 else pan,0xffffffff if s&2 else vol,rate)
  elif op=='playing':result=call(0x494ff0,handle)
  else:call(0x495070,handle)
 elif op=='register':call(0x495160,0x480123)
 elif op in ('publish','frames','dirty','stream','disabled'):
  put(0x4a2716,255);put(0x150000c,0,1);put(0x1500004,16+s%32)
  call(0x494e80,3,0x1500000,pan,128,0x10000);call(0x494f40,0x1500000,(pan+64)&0xffffffff,100,0x10000,5)
  if s&1:put(0x4a1a12+3*0x34+0x2c,77)
  call(0x495160,0x480123)
  if op=='dirty':
   call(0x4a48fc);call(0x494fa0,(get(0x4a2ae0)-2)&0xffffffff,64,150,0x8000);call(0x4a48fc)
  elif op=='publish':call(0x4a48fc)
  else:
   if op=='stream':
    u.mem_write(0x4a23c2,bytes(u.mem_read(0x4a1a12+3*0x34,0x30)));put(0x4a23c2,0x1500030);put(0x4a23c2+4,32);put(0x4a23c2+12,0xffff8400);put(0x4a23c2+8,0x1000);put(0x4a23c2+0x30,1,1);put(0x4a2392+12,0xffffffff)
   put(0x4a270a,0x1600000);put(0x4a270e,8 if op=='stream' else [0,1,2,8,32,128,441,1024][s%8]);put(0x4a2712,int(op=='disabled'));put(0x4a35b4,0);put(0x4a35b8,0);call(0x4a35bc)
 compiled=[]
 for i in range(16):
  a=i*0x40;compiled.append([get(0x4a3651+a),get(0x4a3667+a),bytes(u.mem_read(0x4a3671+a,8)).hex(),bytes(u.mem_read(0x4a367e+a,8)).hex(),get(0x4a3679+a),get(0x4a3686+a)])
 result_data=dict(records=bytes(u.mem_read(BASE,0x1038)).hex(),scalars=[get(a) for a in (0x4a2ae0,0x4b4f5a,0x4a35b0,0x4a2ae8)],result=result,compiled=compiled,calls=calls,reads=reads)
 for hook_id in hooks:u.hook_del(hook_id)
 return result_data
def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--seeds',type=int,default=64);p.add_argument('--ops',default=','.join(OPS));p.add_argument('--report',type=Path);a=p.parse_args();raw=a.game.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=SOURCE_HASH:raise ValueError('Source executable hash differs')
 ops=a.ops.split(',');
 if any(op not in OPS for op in ops):raise ValueError('Unknown operation')
 cases=[(op,s) for op in ops for s in range(a.seeds)];run=subprocess.run([str(a.probe.resolve())],input=''.join(f'{op} {s}\n' for op,s in cases),text=True,capture_output=True,check=True);actual=[json.loads(x) for x in run.stdout.splitlines()]
 if len(actual)!=len(cases):raise ValueError('Probe reply count differs')
 diffs=[]
 for (op,s),got in zip(cases,actual):
  want=original(raw,op,s)
  if want!=got:
   fields=[k for k in want if want[k]!=got[k]];diffs.append(dict(op=op,seed=s,fields=fields,want={k:want[k] for k in fields},got={k:got[k] for k in fields}))
   if len(diffs)>=3:break
 report=dict(source_sha256=SOURCE_HASH,cases=len(cases),differences=len(diffs),details=diffs)
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items() if k!='details'}));return bool(diffs)
if __name__=='__main__':raise SystemExit(main())
