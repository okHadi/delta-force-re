#!/usr/bin/env python3
"""Compare486370 current load-record metadata and failure prefixes."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectile_hits_original import Replay,HASH,STACK
G,L,M,E,F,H,N,Q,T=0x1300000,0x1301000,0x1302000,0x1303000,0x1304000,0x1305000,0x1306000,0x1307000,0xc54480
REGIONS=[(G,0x100),(L,2*0xb8),(M,4*0x4c),(E,12),(F,4),(H,0x7c),(N,32),(Q,4),(T,2)]
class FixtureFault(Exception):pass

def main():
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2048);p.add_argument('--report',type=Path);a=p.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x486370);rows=[];expected=[];faults=0;counts={};frame=STACK-0x14;arguments=STACK+4
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,n,w=4):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  mode=rng.randrange(8);mutation=rng.randrange(8192);regions=[(frame if b==F else arguments if b==E else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  r.u.mem_write(G,bytes(0x100));r.u.mem_write(H,b'3DI'+bytes([4 if case%13==0 else 6 if case%7==0 else 5]));put(H+0x14,[0,1,2,4,5,0xffffffff,0x80000000,0xfffffff0][case%8]);put(Q,[0,1,2,0xffffffff,0x80000000][case%5]);put(arguments,G);put(arguments+4,N);put(arguments+8,case%6);put(T,rng.choice([0,1,0x7fff,0x8000,0xffff]),2)
  if case%17==0:r.u.mem_write(H,b'BAD')
  for i in range(2):put(L+i*0xb8+0xa8,case%5);put(L+i*0xb8+0xac,M);put(L+i*0xb8+0x24,rng.getrandbits(32))
  for i in range(4):put(M+i*0x4c+0x1f,[0,1,7,8][(case+i)%4],1);put(M+i*0x4c+0x1e,rng.randrange(256),1);put(M+i*0x4c+0x1c,rng.getrandbits(16),2)
  rows.append(f'{mode} {mutation} '+b''.join(bytes(r.u.mem_read(b,n))for b,n in regions).hex());trace=[];pending=[];calls=stores=reads=textures=levels=0
  def norm(at):return F if at==frame else E+at-arguments if arguments<=at<arguments+12 else at
  def parent():return 0x486370<=r.u.reg_read(UC_X86_REG_EIP)<=0x4864e0
  def access(u,kind,at,width,value,user):
   nonlocal stores
   if not parent():return
   if kind==unicorn.UC_MEM_WRITE and (at<G or at+width>G+0x100) and 0x486451<=r.u.reg_read(UC_X86_REG_EIP)<=0x48645f:trace.append(f'W{at},{width},{value&0xffffffff};');raise FixtureFault()
   if not any(b<=at and at+width<=b+n for b,n in regions):return
   if kind==unicorn.UC_MEM_READ:
    trace.append(f'R{norm(at)},{width},{read(at,width)};')
    if mutation&32 and at==G+0x50:pending.append(lambda:put(G+0x50,L+0xb8))
    if mutation&64 and at==M+0x1f:pending.append(lambda:put(M+0x1c,0x7fff,2))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&8 and at==G+0x72:pending.append(lambda:put(G+0x14,0))
    if mutation&16 and at==G+0x74:pending.append(lambda:put(G+0x50,L+0xb8))
    if mutation&2048 and stores==((mutation>>7)%5+1):pending.append(lambda:(_ for _ in ()).throw(FixtureFault()))
  sites={0x499040:1,0x499120:1,0x499280:2,0x485ba0:1,0x485e30:3,0x499020:0}
  def code(u,at,size,user):
   nonlocal calls,reads,textures,levels
   while pending:pending.pop(0)()
   if at not in sites:return
   args=list(r.read(r.u.reg_read(UC_X86_REG_ESP)+4,sites[at]))if sites[at]else [];trace.append('C'+str(at)+''.join(','+str(norm(x))for x in args)+';');calls+=1;counts[hex(at)]=counts.get(hex(at),0)+1
   if mutation&4096 and calls==((mutation>>7)%7+1):raise FixtureFault()
   result=0
   if at==0x499040:
    if mutation&128:put(arguments,G+0x80);put(arguments+4,N+8);put(arguments+8,5)
    result=0 if mode&1 else 1
   if at==0x499280:
    reads+=1
    if reads==1:r.u.mem_write(args[0],bytes(r.u.mem_read(H,3 if mode&2 else 0x7c)))
    else:put(args[0],read(Q))
   if at==0x485ba0:
    textures+=1;put(T,read(T,2)+1,2)
    if mutation&1:put(frame,0)
   if at==0x485e30:
    levels+=1
    if mutation&2:put(G+0x14,0)
    if mutation&4:put(G+0x70,0x80018002)
    result=0 if mode&4 else L+((levels-1)%2)*0xb8
   if at==0x499020 and mutation&256:put(G+0x50,L+0xb8);put(L+0xb8+0xa8,1)
   r.u.reg_write(UC_X86_REG_EAX,result)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in sites:r.hook(site,lambda:None)
  failed=False;result=0
  try:
   r.call(0x486370,*r.read(arguments,3))
   while pending:pending.pop(0)()
   result=r.u.reg_read(UC_X86_REG_EAX)
  except (FixtureFault,unicorn.UcError):failed=True;faults+=1
  for h in hooks:r.u.hook_del(h)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected);diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y];report={'source_sha256':HASH,'cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'calls':counts,'real_body':'486370','foreign_fixtures':['499040/499120/499280/499020 currentfile effects','485BA0 texture/currentcounter','485E30 currentLOD'], 'comparison':'Full7Cheader/plussourceoverflowprefix, currentLOD/material/texturecount/frame/args/filefixtures andorderedparentreads/writes/calls. Versions4/5/6, nullLOD source-success, signedcounts/wordaliases/capturedargs/currentrereads/failures. ESP/savedregs excluded.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:print(json.dumps(diffs[0]));return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
