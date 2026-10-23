#!/usr/bin/env python3
"""Compare raw model codec consumers with original item and helicopter callers."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
A,D,F,O,S,G,L,HP=0x7dde80,0x1300000,0x1b000000,0x1b000100,0x1b000200,0x20000000,0x20100000,0x20200000
class FixtureFault(Exception):pass

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);r.call(0x477510);r.u.mem_map(F,0x1000);r.u.mem_map(G,0x800000);rng=random.Random(0x20000000);rows=[];expected=[];faults=0;counts={};seed=[0x12345678,0x23456789,0x3456789a,0x456789ab,0x56789abc]
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def packed(b,at,n):struct.pack_into('<I',b,at,n&0xffffffff)
 for case in range(a.cases):
  mode=case%4;phase=(case//4)%2;first=int(case%3!=0);mutation=(case*37)&31;frame=STACK-(0x48 if mode==1 else 0x8c)if mode in [1,2] else F;framebytes=0x40 if mode==1 else 0x80;flag=STACK-0x14 if mode==0 else S
  source=bytearray(128+184+9*12);source[:4]=b'3DI\x05';source[4:12]=b'fixture\0';packed(source,20,1)
  for i in range(4):packed(source,40+i*4,[0x68656c6f,0x676e7263,0x65666374,0x7fffffff][(case+i)%4]);packed(source,0x3c+i*4,rng.getrandbits(32))
  packed(source,0x4c,rng.getrandbits(32));packed(source,128+20,9*12);packed(source,128+0xa0,9);packed(source,128+0x40,rng.getrandbits(32));packed(source,128+0x58,rng.getrandbits(32))
  for i in range(27):packed(source,128+184+i*4,[0,0x80000000,0x7fffffff,0xffffffff,rng.getrandbits(32)][(case+i)%5])
  actor=bytearray(rng.randbytes(0x114));item=bytearray(rng.randbytes(0x13c));frame_image=rng.randbytes(0x80);out=rng.randbytes(0x18);packed(actor,0x10,0x71000000);packed(item,0x28,8);packed(item,0x6c,0x71000000 if case%7==0 else 0);packed(item,0x70,0);item[0x2c:0x2c+8]=b'fixture\0';item[0x3c]=0
  rows.append(f'{mode} {phase} {first} {mutation} {source.hex()} {actor.hex()} {frame_image.hex()} {item.hex()} {out.hex()}')
  r.u.mem_write(A,bytes(actor));r.u.mem_write(D,bytes(item));r.u.mem_write(frame,frame_image);r.u.mem_write(O,out);put(flag,0);put(A+0x10,G);put(D+0x6c,G if case%7==0 else 0);put(0xc23810,D);put(0xc23814,1);put(0x503138,1);put(0x503228,0x4960552)
  r.u.mem_write(G,bytes(source[:0x7c]));r.u.mem_write(L,bytes(source[128:312]));r.u.mem_write(HP,bytes(source[312:]));put(G+0x50,L)
  for i in range(1,4):put(G+0x50+i*4,0)
  put(L+0xa4,HP)
  if phase:
   for i,n in enumerate(seed):put(G+0x3c+i*4,n)
  trace=[];pending=[];calls=stores=0
  def norm(at):
   if frame<=at<frame+framebytes:return F+at-frame
   if at==flag:return S
   return at
  regions=[(A,0x114),(D,0x13c),(frame,framebytes),(O,0x18),(flag,4),(G+0x28,16),(G+0x3c,36),(L+0x40,4),(L+0x58,4),(L+0xa4,4),(HP,9*12),(0xc23810,8),(0x503138,4),(0x503228,4)]
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x45d6f0<=pc<=0x45d965 if not mode else 0x4505d0<=pc<0x45062c if mode==1 else 0x450630<=pc<0x450707
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if mode==3 or not parent() or not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:trace.append(f'R{norm(at)},{width},{read(at,width)};')
   else:
    trace.append(f'W{norm(at)},{width},{value&((1<<(width*8))-1)};');stores+=1
    if mutation&8 and stores==3:pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  sites={0x486530:3,0x4818f0:1,0x481a00:1,0x45d1e0:2,0x45d400:2,0x45cb40:3,0x45cda0:3}
  def code(u,at,size,user):
   nonlocal calls
   while pending:pending.pop(0)()
   if at==0x4505e4 or at==0x450644:
    pass
   if at not in sites:return
   caller=read(r.u.reg_read(UC_X86_REG_ESP))
   if not (0x45d6f0<=caller<=0x45d965 or 0x4505d0<=caller<0x45062c or 0x450630<=caller<0x450707):return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,sites[at]));trace.append('C'+str(at)+''.join(','+str(norm(x))for x in args)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mutation&16 and calls==2:raise FixtureFault()
   if at==0x486530:put(flag,first);r.u.reg_write(UC_X86_REG_EAX,G)
   if at==0x45d1e0 and mutation&1:
    # Change current aim and hardpoint after the real matrix body returns.
    ret=read(r.u.reg_read(UC_X86_REG_ESP));after[ret]=True
   if at in after:put(A+0xc4,0x40000000);put(HP+0x34,0x80000000);after.pop(at)
  after={}
  def mutate_return(u,at,size,user):
   if at in after:put(A+0xc4,0x40000000);put(HP+0x34,0x80000000);after.pop(at)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code),r.u.hook_add(unicorn.UC_HOOK_CODE,mutate_return)];r.hook(0x486530,lambda:None);failed=False
  try:
   if mode==0:r.call(0x45d6f0,0)
   elif mode in [1,2]:r.call(0x4505d0 if mode==1 else 0x450630,A,O)
   else:
    for offset in range(0x3c,0x50):
     trace.append(f'W{G+offset},1,{mutation+offset};');put(G+offset,mutation+offset,1);stores+=1
     if mutation&8 and stores==3:raise FixtureFault()
    for width in [1,2,4]:
     for offset in range(0x3c,0x51-width):trace.append(f'R{G+offset},{width},{read(G+offset,width)};')
   while pending:pending.pop(0)()
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in [(A,0x114),(D,0x13c),(frame,framebytes),(O,0x18),(flag,4),(G+0x3c,20),(HP,9*12)]:
   data=bytearray(r.u.mem_read(b,n))
   if b==A:packed(data,0x10,0x71000000)
   if b==D:
    for offset in [0x6c,0x70]:
     if int.from_bytes(data[offset:offset+4],'little')==G:packed(data,offset,0x71000000)
   for byte in data:digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_bodies':['45D6F0','4505D0','450630','4818F0','481A00','45D1E0','45D400','45CB40','45CDA0','477510'],'foreign_fixtures':['486530 cache return and firstloadflag','runtimegraphic/LODpointerinitialization from suppliedfixture3DI sourcecontract'],'comparison':'Newcodec typedhandles to separatedrawmodel/LOD/HP spans; pre/post seeds; actualsourceconsumers; ordered access and state including callbackmutation/failure. Directbytealias mode compares source-memory bytes; no original loader claim.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
