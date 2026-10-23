#!/usr/bin/env python3
"""Compare DF socket bodies, current globals, and actual OS call buffers."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256,STACK
BODIES=[0x454bb0,0x454ab0,0x454a10,0x454930,0x454890,0x454900,0x454850,0x4548c0,0x454df0]
CALLS={0x454ab0:0,0x454a10:1,0x454930:2,0x454890:1,0x454900:1,0x454850:0,0x4548c0:0,0x49dc80:1,0x4c55f8:2,0x4c55c8:3,0x4c55c2:3,0x4c55bc:1,0x4c55b6:3,0x4c55e0:0,0x4c55ce:5,0x4c55d4:5,0x4c55b0:1,0x4c55ec:1}
FRAMES={0x454a10:(-24,16),0x454930:(-32,20),0x454890:(4,4),0x454900:(4,4),0x454850:(-8,8),0x4548c0:(-8,8)}
FOREIGN=[site for site in CALLS if site>=0x4c0000 or site==0x49dc80]
def run(a):
 r=Rig(a.game);active=False;v=[];trace=[];frames=[];active_frames=[];current=0;attempts=0
 def put(at,w,n):r.u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(r.u.mem_read(at,w),'little')
 def norm(n):
  for depth,frame in enumerate(active_frames):
   if frame['address']<=n<frame['address']+frame['size']:return 0x30000000+depth*0x100+n-frame['address']
  return n
 def tracked(at):return 0x8e352c<=at<0x8e3550 or 0x1200000<=at<0x1200400
 def memory(u,kind,at,w,n,_):
  if not active or not tracked(at):return
  trace.append(f'R{at},{w},{get(at,w)};' if kind==unicorn.UC_MEM_READ else f'W{at},{w},{n&((1<<(8*w))-1)};')
 def record(site):
  sp=r.u.reg_read(UC_X86_REG_ESP);count=CALLS[site];args=list(struct.unpack('<'+'I'*count,r.u.mem_read(sp+4,4*count))) if count else []
  trace.append('C'+','.join(map(str,[site,*map(norm,args)]))+';')
  if site==v[16]:put(v[17],4,v[18])
  return args
 def code(u,at,size,_):
  if not active:return
  while active_frames and at==active_frames[-1]['return']:active_frames.pop()
  if at in CALLS and at not in FOREIGN and at!=current:record(at)
  if at in FRAMES:
   sp=u.reg_read(UC_X86_REG_ESP);delta,length=FRAMES[at];base=sp+delta;frames.append((at,length,list(u.mem_read(base,length))));active_frames.append({'address':base,'size':length,'return':get(sp)})
 def foreign(site):
  nonlocal attempts
  args=record(site);sp=r.u.reg_read(UC_X86_REG_ESP);width=address=0
  if site==0x4c55b6:address=args[1];width=16
  elif site==0x4c55c2:address=args[2];width=4
  elif site in [0x4c55ce,0x4c55d4]:address=args[3];width=4
  if width:
   values=list(r.u.mem_read(address,width));values+=([get(args[4])] if site==0x4c55d4 else []);trace.append('B'+','.join(map(str,[site,*values]))+';')
  if site==0x4c55f8:result=v[4]
  elif site==0x4c55c8:result=0xffffffff if v[5] else 77
  elif site==0x4c55c2:result=0xffffffff if v[6] else 0
  elif site==0x4c55bc:result=((args[0]&255)<<8)|((args[0]>>8)&255)
  elif site==0x4c55b6:result=0xffffffff if attempts<v[7] else 0;attempts+=1
  elif site==0x4c55e0:result=v[8]
  elif site==0x4c55ce:result=0xffffffff if v[9] else 0
  elif site==0x4c55d4:
   result=0xffffffff if v[10] else 0
   if not v[10]:put(args[3],4,v[11] if args[2]==0x1001 else v[12])
  elif site==0x49dc80:result=v[13]
  elif site==0x4c55b0:result=v[14]
  elif site==0x4c55ec:result=v[15]
  else:raise AssertionError(site)
  r.finish(result)
  if site>=0x4c0000:r.u.reg_write(UC_X86_REG_ESP,r.u.reg_read(UC_X86_REG_ESP)+CALLS[site]*4)
 for site in FOREIGN:r.stubs[site]=lambda site=site:foreign(site)
 r.u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,memory);r.u.hook_add(unicorn.UC_HOOK_CODE,code)
 def fixture():return {0x8e3530:(4,99),0x8e353c:(4,88),0x8e3548:(4,89),0x8e354e:(2,0),0x8e354c:(2,42),0x8e3544:(4,0),0x8e3540:(4,0),0x8e352c:(4,66),0x1200300:(2,0xf28),0x120000c:(4,0x1200100),0x1200100:(4,0x1200200),0x1200200:(4,0x0100007f)}
 def header(op):return [op,2,0,0xf28,0,0,0,0,0x2740,0,0,0x10000,0x8000,0x1400000,0x0100007f,0x1200000,0,0,0]
 rows=[];rng=random.Random(0x454bb0)
 for op in range(9):
  for failure in range(12):
   for mutation in range(4):
    h=header(op);d=fixture()
    if op==0:h[2]=0x1200300
    if op==2:h[1]=0xfffe if failure==10 else 0
    if op==3:h[2]=0x1200300;h[1]=1 if failure==11 else 2
    if op in [4,5]:h[1]=0x10000
    if op==8:h[1]=0x1200300;h[14]=0xffffffff if failure%2 else 0x0100007f;h[15]=0 if failure==1 else 0x1200000
    if failure==1:h[4]=1
    if failure==2:h[5]=1
    if failure==3:h[6]=1
    if failure==4:h[7]=3
    if failure==5:h[7]=3;h[8]=5
    if failure==6:h[9]=1
    if failure==7:h[10]=1
    if failure==8:h[13]=0
    if failure==9:h[11]=0xffffffff;h[12]=0x80000000
    if failure==10:h[7]=3
    if mutation==1:h[16:19]=[0x4c55bc,0x8e353c,123]
    if mutation==2:h[16:19]=[0x454850,0x8e354e,7]
    if mutation==3:h[16:19]=[0x4c55d4,0x8e3548,124]
    rows.append((h,d))
 for _ in range(a.cases):
  h=header(rng.randrange(9));d=fixture();h[1]=rng.choice([0,1,2,0xffff,0xfffe,0x10001,0x7fffffff,0xffffffff]);h[2]=rng.choice([0,0x1200300]);h[3]=rng.getrandbits(32);h[7]=rng.randrange(4);h[8]=rng.choice([5,0x2740]);h[11]=rng.getrandbits(32);h[12]=rng.getrandbits(32);d[0x8e354e]=(2,rng.choice([0,1,0xfffe,0xffff]));d[0x1200300]=(2,rng.choice([0,1,0xfffe,0xffff]));
  if h[0]==3:h[2]=0x1200300
  if h[0]==8:h[1]=0x1200300;h[14]=rng.choice([0,0x0100007f,0xffffffff]);h[15]=rng.choice([0,0x1200000])
  rows.append((h,d))
 for operation in [0,3]:
  for address in [0x8e3548,0x8e354e,0x8e3530,0x8e353c]:
   for mode in [1,2]:
    h=header(operation);h[1]=mode;h[2]=address;h[7]=2;rows.append((h,fixture()))
 expected=[];payload=[]
 for h,d in rows:
  r.u.mem_write(STACK-0x1000,b'\xa5'*0x2000);r.u.mem_write(0x8e352c,b'\0'*0x30);r.u.mem_write(0x1200000,b'\0'*0x400)
  for at,(width,n) in d.items():put(at,width,n)
  v=h;trace=[];frames=[];active_frames=[];attempts=0;current=BODIES[h[0]];active=True
  try:
   op=h[0];args=[h[1],h[2],h[3]] if op==0 else [h[1]] if op in [2,4,5,8] else [h[1],h[2]] if op==3 else []
   r.call(current,*args)
  finally:active=False
  result=r.u.reg_read(UC_X86_REG_EAX);expected.append(str(result)+' '+''.join(trace));payload.append(' '.join(map(str,[*h,len(d),len(frames),*[x for at,(width,n) in d.items() for x in [at,width,n]],*[x for site,size,data in frames for x in [site,size,*data]]])))
 p=subprocess.run([str(a.probe)],input='\n'.join(payload)+'\n',text=True,capture_output=True);assert p.returncode==0,p.stderr
 actual=p.stdout.splitlines();assert len(actual)==len(expected);diff=[{'case':i,'input':payload[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y];report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(diff),'mismatches':diff[:3],'frame_scope':'actual incoming original stack bytes per reached helper; mapped native caller-owned frames','OS_scope':'explicit mutable OS fixtures; actual source instructions and buffers','audio_output':'none'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:report[k] for k in ['cases','differences']}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
