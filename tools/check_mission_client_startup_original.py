#!/usr/bin/env python3
"""Compare client startup and its owned helpers with exact original instructions."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SOURCE_SHA256
BODIES=[0x43ff30,0x4406e0,0x444c50,0x4715b0,0x4974c0,0x4487e0]
CALLS={0x49a620:3,0x404cf0:2,0x454df0:1,0x4406e0:2,0x4974c0:1,0x497500:1,0x4974d0:1,0x4974e0:1,0x4715b0:1,0x454bb0:3,0x4487e0:0,0x4568e0:2}
REGIONS=[(0x4cc178,0x100),(0x4f81b0,0x60),(0x6e5500,0x400),(0x6e9a90,17*0x9b84),(0xbc5c70,160),(0x1200000,0x500),(0x1300000,0x500)]
CELLS=[0x67f520,0x67f53c,0x67f548,0x67db68,0x50133c,0x501340,0x501374,0x501378,0x50137c,0x501380]
def run(a):
 r=Rig(a.game);active=False;v=[];seen=0;trace=[];current=0
 def put(at,w,n):r.u.mem_write(at,(n&((1<<(w*8))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 constants={at:(1,get(at,1)) for base,size in REGIONS[:2] for at in range(base,base+size)}
 def tracked(at):return at in CELLS or any(base<=at<base+size for base,size in REGIONS)
 def memory(u,kind,at,w,n,_):
  nonlocal seen
  if not active or not tracked(at):return
  if kind==unicorn.UC_MEM_READ:
   if at==v[4]:
    if seen==v[5]:put(at,w,v[6])
    seen+=1
   trace.append(f'R{at},{w},{get(at,w)};')
  else:trace.append(f'W{at},{w},{n&((1<<(8*w))-1)};')
 def code(u,at,size,_):
  if not active or at==current or at not in CALLS:return
  if at in [0x49a620,0x404cf0,0x454df0,0x454bb0,0x4568e0] and _!="foreign":return
  sp=u.reg_read(UC_X86_REG_ESP);n=CALLS[at];args=list(struct.unpack('<'+'I'*n,u.mem_read(sp+4,4*n))) if n else []
  trace.append('C'+','.join(map(str,[at,*args]))+';')
  if at==v[7]:put(v[8],4,v[9])
 def foreign(site):
  if site==0x404cf0:r.finish(0x1300100)
  elif site==0x454df0:r.finish(0x11223344)
  else:r.finish(0)
 for site in [0x404cf0,0x454df0,0x454bb0,0x4568e0]:r.stubs[site]=lambda site=site:foreign(site)
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory)
 r.u.hook_add(unicorn.UC_HOOK_CODE,code)
 # Rig's existing code hook runs first. Wrap stubs to record their entry there.
 for site in [0x49a620,0x404cf0,0x454df0,0x454bb0,0x4568e0]:
  original=r.stubs[site]
  r.stubs[site]=lambda site=site,original=original:(code(r.u,site,0,"foreign"),original())[-1]
 def fixture():
  d=dict(constants)
  def p(at,n,w=4):d[at]=(w,n&((1<<(w*8))-1))
  for at in CELLS:p(at,0)
  p(0x50133c,77)
  for i in range(17):
   c=0x6e9a90+i*0x9b84;p(c+8,0x1200000+(i%3)*0x114);p(c+0x9c,1+i%2);p(c+0x18c,1,1);p(c+0x1c4,5,2);p(c+0x9b80,0xabc00000+i)
  for i in range(3):p(0x1200000+i*0x114+0x70,0xaaaa5555)
  for base,text in [(0x1300000,b'Hadi\0'),(0x1300100,b'Server fixture\0')]:
   for i,n in enumerate(text):p(base+i,n,1)
  for i in range(16):
   p(0xbc5c70+i*10,i+1,1);p(0xbc5c71+i*10,0,1)
  for i,n in enumerate([1,1,3,4,5,6,7,8,9,10]):p(0x1300200+i,n,1)
  return d
 rows=[];rng=random.Random(0x43ff30)
 for op in range(6):
  for mutation in range(16):
   d=fixture();h=[op,0x6e9a90,0,0,0,0,0,0,0,0]
   if op==2:h[1:4]=[0x1300000,4,5]
   if op==3:h[1]=0x1300200
   if op==4:h[1:3]=[0x4974c0,0x43f830]
   if mutation&1:h[4:7]=[0x6e9a98,1,0x1200114]
   if mutation&2:h[7:10]=[0x4974c0,0x67f548,0x20]
   if mutation&4:h[4:7]=[0x6e9b2c,1,2]
   if mutation&8:h[7:10]=[0x454df0,0x6e9a98,0x1200228]
   rows.append((h,d))
 edges=[0,1,2,3,4,5,0x7fffffff,0x80000000,0xffffffff]
 for _ in range(a.cases):
  op=rng.randrange(6);d=fixture();slot=rng.randrange(17);h=[op,0x6e9a90+slot*0x9b84,rng.choice(edges),rng.getrandbits(32),0,0,0,0,0,0]
  d[0x67f520]=(4,rng.randrange(2));d[0x67f53c]=(4,rng.randrange(7));d[0x67f548]=(1,rng.choice([0,0x20]));
  for i in range(17):d[0x6e9c1c+i*0x9b84]=(1,rng.randrange(2));d[0x6e9b2c+i*0x9b84]=(4,rng.choice(edges))
  if op==2:h[1]=0x1300000
  if op==3:
   h[1]=rng.choice([0x1300200,0xbc5c70,0xbc5c74,0xbc5c7a]);
   for i in range(16):d[0xbc5c71+i*10]=(1,rng.randrange(2));d[0xbc5c70+i*10]=(1,rng.randrange(5))
  if op==4:h[1]=rng.choice([0x4974c0,0x4974d0,0x4974e0,0x497500])
  if rng.randrange(2):h[4:7]=[0x6e9a98+slot*0x9b84,1,rng.choice([0x1200000,0x1200114,0x1200228])]
  rows.append((h,d))
 expected=[];payload=[]
 for h,d in rows:
  for base,size in REGIONS[2:]:r.u.mem_write(base,b'\0'*size)
  for at in CELLS:put(at,4,0)
  for at,(w,n) in d.items():put(at,w,n)
  v=h;seen=0;trace=[];current=h[1] if h[0]==4 else BODIES[h[0]];active=True
  try:
   if h[0]==0 or h[0]==5:r.call(current)
   elif h[0]==1:r.call(current,h[1],h[2])
   elif h[0]==2:r.call(current,h[1],h[2],h[3])
   elif h[0]==3:r.call(current,h[1])
   else:r.call(current,h[2])
  finally:active=False
  for at,(w,n) in d.items():trace.append(f'V{at},{w},{get(at,w)};')
  expected.append(''.join(trace));payload.append(' '.join(map(str,[*h,len(d),*[x for at,(w,n) in d.items() for x in [at,w,n]]])))
 p=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',capture_output=True,text=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected);diff=[{'case':i,'input':payload[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(diff),'mismatches':diff[:2],'audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
