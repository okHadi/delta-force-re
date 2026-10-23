#!/usr/bin/env python3
"""Compare complete485E30 raw parent with current mutable memory."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EDX
from check_projectile_hits_original import Replay,HASH,STACK
C,L,P,H,Q,E,F,T,V=0xc65dcc,0xc65dd0,0x1300000,0x1302000,0x1303000,0x1304000,0x1305000,0xc54488,0xc80d10
REGIONS=[(C,4),(L,0x170),(P,4096),(H,0xb8),(Q,4096),(E,12),(F,40),(T,72),(V,12)]
class Fault(Exception):pass

def main():
 parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path);parser.add_argument('--cases',type=int,default=2048);parser.add_argument('--report',type=Path);a=parser.parse_args();raw=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==HASH;r=Replay(raw);rng=random.Random(0x485e30);rows=[];expected=[];faults=0;fcases=0;frame=STACK-0x38;args=STACK+4
 def read(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def put(at,value,w=4):r.u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 for case in range(a.cases):
  mode=int(case%23==0);mutation=0 if case%3 else rng.randrange(65536)&127 | (rng.randrange(20)<<8)
  regions=[(frame if b==F else args if b==E else b,n)for b,n in REGIONS]
  for b,n in regions:r.u.mem_write(b,rng.randbytes(n))
  put(C,601 if case%37==0 else 0);r.u.mem_write(H,bytes(0xb8));put(H+0x10,case%2,1)
  vertices=case%5;normals=case%3;faces=case%4;meshes=1+case%3;hp=case%3;collisions=case%3;materials=1+case%3
  size=vertices*8+normals*8+faces*72+meshes*112+hp*12+collisions*72+materials*76
  for off,value in [(0x14,size),(0x80,vertices),(0x88,normals),(0x90,faces),(0x98,meshes),(0xa0,hp),(0xb0,collisions),(0xa8,materials)]:put(H+off,value)
  meshbase=vertices*8+normals*8+faces*72;matbase=meshbase+meshes*112+hp*12+collisions*72
  for i in range(vertices):put(Q+i*8+6,i%meshes,2)
  for i in range(meshes):
   m=Q+meshbase+i*112
   for off,value in [(4,vertices if i==0 else 0),(12,faces if i==0 else 0),(20,normals if i==0 else 0),(28,collisions if i==0 else 0),(36,i),(52,rng.getrandbits(32)),(56,rng.getrandbits(32)),(60,rng.getrandbits(32))]:put(m+off,value)
  for i in range(faces):put(Q+vertices*8+normals*8+i*72+68,i%materials)
  for i in range(materials):
   m=Q+matbase+i*76;put(m+0x10,(case+i)%2,1);put(m+0x11,0x40 if (case+i)%3==0 else 0,1);put(m+0x1c,rng.choice([0,1,32767,32768,65535,50,100]),2);put(m+0x34,i%3,1)
  put(args,0);put(args+4,2 if case%7 else 0);put(args+8,rng.getrandbits(32));put(V,0);put(V+8,0 if case%3 else 1)
  for i in range(2):
   t=T+i*36;put(t+0x14,i,2);put(t+0x16,0x20 if i else 0,1);put(t+0x18,[1,8,32,32768][case%4],2);put(t+0x1a,[2,16,64,65535][case%4],2)
  rows.append(f'{mode} {mutation} '+b''.join(bytes(r.u.mem_read(b,n))for b,n in regions).hex());trace=[];pending=[];stores=calls=reads=0
  def norm(at):return F+at-frame if frame<=at<frame+40 else E+at-args if args<=at<args+12 else at
  def access(u,kind,at,width,value,user):
   nonlocal stores
   pc=u.reg_read(UC_X86_REG_EIP)
   if not 0x485e37<=pc<=0x486362 or 0x4860be<=pc<=0x4860d2:return
   if not any(b<=at and at+width<=b+n for b,n in regions):
    if STACK-0x100<=at<=STACK+0x20:return
    if kind==unicorn.UC_MEM_WRITE:trace.append(f'W{norm(at)},{width},{value&((1<<(width*8))-1)};')
    raise Fault()
   if kind==unicorn.UC_MEM_READ:
    value=read(at,width);trace.append(f'R{norm(at)},{width},{value};')
    if mutation&1 and at==L+0x98:pending.append(lambda:put(L+0x98,0))
    if mutation&2 and at==args+8:pending.append(lambda:put(args+8,value+1))
   else:
    value&=(1<<(width*8))-1;trace.append(f'W{norm(at)},{width},{value};');stores+=1
    if mutation&4 and at==L+0x84:pending.append(lambda:put(L+0x18,P+16))
    if mutation&8 and at==frame+0x18:pending.append(lambda:put(args+8,read(args+8)+1))
    if mutation&16 and stores==((mutation>>8)%20+1):pending.append(lambda:(_ for _ in ()).throw(Fault()))
  def code(u,at,size,user):
   nonlocal calls,reads,fcases
   while pending:pending.pop(0)()
   if at==0x4860be:trace.append(f'F{u.reg_read(UC_X86_REG_EDX)},{F+0x24};');fcases+=1
   if at not in (0x499280,0x497f60):return
   argc=2 if at==0x499280 else 4;aa=list(r.read(u.reg_read(UC_X86_REG_ESP)+4,argc));trace.append('C'+str(at)+''.join(','+str(norm(v))for v in aa)+';');calls+=1
   if mutation&32 and calls==((mutation>>8)%3+1):raise Fault()
   result=0
   if at==0x497f60:result=0 if mode&1 else P
   else:
    reads+=1;src=H if reads==1 else Q;r.u.mem_write(aa[0],bytes(r.u.mem_read(src,aa[1])))
    if mutation&64 and reads==2:put(L+0x18,P+8)
   u.reg_write(UC_X86_REG_EAX,result)
  hooks=[r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,access),r.u.hook_add(unicorn.UC_HOOK_CODE,code)]
  for site in (0x499280,0x497f60):r.hook(site,lambda:None)
  failed=False;result=0
  try:
   r.call(0x485e30,*r.read(args,3))
   while pending:pending.pop(0)()
   result=r.u.reg_read(UC_X86_REG_EAX)
  except(Fault,unicorn.UcError):failed=True;faults+=1
  for hook in hooks:r.u.hook_del(hook)
  r.clear_hooks();digest=1469598103934665603
  for b,n in regions:
   for byte in bytes(r.u.mem_read(b,n)):digest=((digest^byte)*1099511628211)&0xffffffffffffffff
  expected.append(f'{int(failed)} {result} {digest} '+''.join(trace))
 got=subprocess.run([str(a.probe.resolve())],input='\n'.join(rows)+'\n',capture_output=True,text=True,check=True).stdout.splitlines();assert len(got)==len(expected)
 diffs=[{'case':i,'input':rows[i],'original':x,'port':y}for i,(x,y)in enumerate(zip(expected,got))if x!=y]
 report={'source_sha256':HASH,'body':'485E30..486366','cases':a.cases,'differences':len(diffs),'fault_prefixes':faults,'actual_x87_blocks':fcases,'comparison':'Complete LOD/payload/args/frame/texture/currentcells bytes and ordered parent reads/writes/calls; x87 block actual source executes, normalized as one required F boundary. File/allocator effects are explicit fixtures. CPU status is outside numeric provider scope.','details':diffs[:1]}
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps({k:v for k,v in report.items()if k!='details'}))
 if diffs:
  d=diffs[0];x=d['original'].split(' ',3);y=d['port'].split(' ',3);print('first',d['case'],x[:3],y[:3]);xt=x[-1].split(';');yt=y[-1].split(';')
  for i,(v,z)in enumerate(zip(xt,yt)):
   if v!=z:print('trace',i,xt[max(0,i-3):i+4],yt[max(0,i-3):i+4]);break
  return 1
 return 0
if __name__=='__main__':raise SystemExit(main())
