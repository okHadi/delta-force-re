#!/usr/bin/env python3
"""Compare485BA0/485B10 with current raw texture descriptor effects."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
C,T,G,V,E,H,Q,P=0xc54480,0xc54488,0xc65dc8,0xc80d10,0x1300000,0x1301000,0x1302000,0x1304000
REGIONS=[(C,8),(T,144),(G,4),(V,4),(E,4),(H,36),(Q,2048),(P,4096)]
class Fault(Exception):pass

def main():
 parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path);parser.add_argument('--cases',type=int,default=1024);parser.add_argument('--report',type=Path);a=parser.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x485ba0);rows=[];expected=[];faults=0;sites_count={};args=STACK+4
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,value,w=4):r.u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  mode=(1 if case%19==0 else 0)|(2 if case%31==0 else 0)|(4 if case%3 else 0);mutation=0 if case%3 else rng.randrange(512)|(rng.randrange(800)<<8)
  regions=[(args if b==E else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  put(C,2000 if case%43==0 else 0xffffffff if case%47==0 else 1);put(C+4,case%2);put(G,1 if case%17==0 else 0);level=[0,1,2,3,7,8,31,32,255][case%9];put(V,level);put(args,[0,1,2,255,256,0xffffffff][case%6]);r.u.mem_write(H,b'Texture\0'+bytes(28));put(H+0x10,16);put(H+0x16,[0,1,3,7,255][case%5],1);put(T+0x10,16 if case%7 else 8);put(T+0x17,read(args)&255,1);put(T+0x1c,P+1000);put(T+0x20,P+1016)
  if case%8==1:
   mode=0;mutation=(128|256) if case%16==1 else 0;put(C,1);put(C+4,1);put(G,0);put(V,0);put(args,2);put(T+0x17,2,1);put(T+0x10,16);put(H+0x16,1,1)
  rows.append(f'{mode} {mutation} '+b''.join(bytes(r.u.mem_read(b,n))for b,n in regions).hex());trace=[];pending=[];calls=reads=stores=0
  def norm(at):return E+at-args if args<=at<args+4 else at
  def parent():
   pc=r.u.reg_read(UC_X86_REG_EIP);return 0x485ba4<=pc<=0x485cfb or 0x485b10<=pc<=0x485b2c
  def owned(at,width):return any(b<=at and at+width<=b+n for b,n in regions)
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent():return
   if not owned(at,width):
    if STACK-0x100<=at<=STACK+0x20:return
    if kind==unicorn.UC_MEM_WRITE:trace.append(f'W{norm(at)},{width},{value&((1<<(width*8))-1)};')
    raise Fault()
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==args:pending.append(lambda:put(args,value+1))
    if mutation&2 and at==G:pending.append(lambda:put(G,1))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&4 and at==T+36+0x1c:pending.append(lambda:put(T+36+0x10,8))
    if mutation&8 and at==P+16:pending.append(lambda:put(T+36+0x20,P+24))
    if mutation&16 and stores==((mutation>>8)%800+1):pending.append(lambda:(_ for _ in ()).throw(Fault()))
  sites={0x499280:2,0x497f60:4,0x4992d0:2,0x499e5e:2,0x4980d0:1,0x49a620:3}
  def code(u,at,size,user):
   nonlocal calls,reads
   while pending:pending.pop(0)()
   if at not in sites:return
   aa=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,sites[at]));trace.append('C'+str(at)+''.join(','+str(norm(v))for v in aa)+';');calls+=1;sites_count[hex(at)]=sites_count.get(hex(at),0)+1
   if mutation&32 and calls==((mutation>>8)%5+1):raise Fault()
   result=0
   if at==0x497f60:result=0 if mode&1 else P
   if at==0x499280:
    reads+=1;src=H if reads==1 else Q
    # Explicit same bytewise fixture boundary as native.
    for i in range(aa[1]):
     if not owned(src+i,1) or not owned(aa[0]+i,1):raise Fault()
     put(aa[0]+i,read(src+i,1),1)
    if mutation&64 and reads==2:put(T+36+0x20,P+32)
   if at==0x499e5e:
    if mutation&128:put(args,2);put(C,2);put(T+0x1c,P+2000)
    result=1 if mode&4 else 0
   if at==0x4980d0 and mutation&256:put(T+0x20,P+2010)
   if at==0x49a620:r.u.mem_write(T,bytes(144))
   u.reg_write(UC_X86_REG_EAX,result)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in sites:r.hook(site,lambda:None)
  failed=False
  try:
   r.call(0x485b10 if mode&2 else 0x485ba0,*(()if mode&2 else(read(args),)))
   while pending:pending.pop(0)()
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'real_bodies':['485BA0..485CFF','485B10..485B2F'],'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':sites_count,'comparison':'Complete declared descriptor/count/gates/argument/payload state and ordered parent accesses/calls. File/alloc/seek/compare/release/memset child effects explicit fixtures. Init memset compares declared144-byte subset plus actual72000-byte call. CPU registers/stack excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:
  d=diffs[0];x=d['original'].split(' ',2);y=d['port'].split(' ',2);print('first',d['case'],x[:2],y[:2]);xx=x[-1].split(';');yy=y[-1].split(';')
  for i,(v,z)in enumerate(zip(xx,yy)):
   if v!=z:print('trace',i,xx[max(0,i-3):i+4],yy[max(0,i-3):i+4]);break
  return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
