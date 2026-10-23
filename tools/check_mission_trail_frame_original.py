#!/usr/bin/env python3
"""Replay trail update, clipping and actual 32-bit line pixels on original code."""
import argparse,json,random,struct,subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
from check_projectiles_original import Rig,SOURCE_SHA256
CAM=0x1200000;POINT=0x1300000;OTHER=0x1300100;BASE=0x9e3098;MASK=(1<<64)-1
def digest(data):
 h=1469598103934665603
 for byte in data:h=((h^byte)*1099511628211)&MASK
 return h
def run(a):
 r=Rig(a.game);u=r.u;inputs=[];expected=[];groups={};rng=random.Random(0x4634d0)
 def put(at,n,w=4):u.mem_write(at,(n&((1<<(8*w))-1)).to_bytes(w,'little'))
 def get(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def call(fn,*args):
  try:r.call(fn,*args);return False
  except (unicorn.UcError,RuntimeError):return True
 def row(mode,values):return mode+' '+' '.join(str(n&0xffffffff) for n in values)
 # 44AEE0: exact signed division, wrapping differences and stores.
 endpoints=[]
 for d0,d1 in [(0,0),(-16,16),(1,-1),(-1,1),(-0x80000000,0),(-0x80000000,1),(0x7fffffff,1),(-1,0x7fffffff),(1,0x7fffffff)]:endpoints.append(([0x7fffffff,-10,3],[-0x80000000,100,23],d0,d1))
 for _ in range(a.cases):endpoints.append(([rng.getrandbits(32) for _ in range(3)],[rng.getrandbits(32) for _ in range(3)],rng.getrandbits(32),rng.getrandbits(32)))
 for p,q,d0,d1 in endpoints:
  for i,n in enumerate(p):put(POINT+i*4,n)
  for i,n in enumerate(q):put(OTHER+i*4,n)
  failed=call(0x44aee0,POINT,OTHER,d0,d1);expected.append(' '.join(map(str,[int(failed),*[get(POINT+i*4) for i in range(3)],*[get(OTHER+i*4) for i in range(3)]])));inputs.append(row('E',p+q+[d0,d1]))
 groups['endpoint']=len(endpoints)
 # 44AF80: full original clipping with current camera-read mutations.
 active=False;mutation=0;used=False;other=0;h=0
 def trace(kind,at,value=0):
  nonlocal h
  for n in [kind,at,value]:
   for byte in (n&0xffffffff).to_bytes(4,'little'):h=((h^byte)*1099511628211)&MASK
 def clip_hook(uc,kind,at,width,value,_):
  nonlocal used,other
  if not active or kind!=unicorn.UC_MEM_READ or not CAM+0x1880<=at<CAM+0x18d0:return
  trace(1,at)
  if not used and mutation==1 and at==CAM+0x1880:put(POINT,get(POINT)+65536);used=True
  if not used and mutation==2 and at==CAM+0x1894:put(POINT+4,get(POINT+4)+131072);used=True
  if mutation==3 and at==CAM+0x1890:
   other+=1
   if other==2:put(OTHER,get(OTHER)+262144)
  if not used and mutation==4 and at==CAM+0x188c:put(POINT,get(POINT)+32768);used=True
  if not used and mutation==5 and at==CAM+0x1880:put(at,0x200000);used=True
 clip_id=u.hook_add(unicorn.UC_HOOK_MEM_READ,clip_hook)
 planes=[0x400000,0,0,4096,0x300000,-0x300000,0,0,0x300000,0x300000,0,0,0x300000,0,-0x300000,0,0x300000,0,0x300000,0]
 clips=[]
 for p,q in [([0,0,0],[65536,0,0]),([4111,0,0],[4112,0,0]),([65536,131072,0],[131072,0,0]),([65536,0,-131072],[65536,0,131072]),([-0x80000000,0,0],[0x7fffffff,0,0])]:
  for m in range(6):clips.append((planes,p,q,m))
 for _ in range(a.cases):
  ps=planes.copy()
  if rng.randrange(3)==0:ps=[rng.randrange(-0x400000,0x400001) for _ in range(20)]
  clips.append((ps,[rng.randrange(-0x8000000,0x8000000) for _ in range(3)],[rng.randrange(-0x8000000,0x8000000) for _ in range(3)],rng.randrange(6)))
 for ps,p,q,mutation in clips:
  for i,n in enumerate(ps):put(CAM+0x1880+i*4,n)
  for i,n in enumerate(p):put(POINT+i*4,n)
  for i,n in enumerate(q):put(OTHER+i*4,n)
  h=1469598103934665603;used=False;other=0;active=True;failed=call(0x44af80,CAM,POINT,OTHER);active=False;result=int(not failed and u.reg_read(UC_X86_REG_EAX)!=0);expected.append(' '.join(map(str,[int(failed),result,*[get(POINT+i*4) for i in range(3)],*[get(OTHER+i*4) for i in range(3)],h])));inputs.append(row('C',ps+p+q+[mutation]))
 u.hook_del(clip_id);groups['clip']=len(clips)
 # 462520: actual MMX line colors, current depth tests, all octants and clips.
 pixels=[]
 for x0,y0,x1,y1 in [(2,3,29,3),(29,3,2,3),(2,3,2,20),(2,20,2,3),(1,1,31,22),(31,22,1,1),(31,1,1,22),(-20,10,50,10),(10,-20,10,50),(-20,-20,-10,-10),(5,5,5,5),(2,3,29,20),(29,20,2,3)]:
  for alpha in [0,1,56,127,128,200,255]:pixels.append([x0<<16,y0<<16,0x1000000,x1<<16,y1<<16,0x2000000,0xb9ffa0a0,alpha,54719])
 for _ in range(a.cases):pixels.append([rng.randrange(-40*65536,64*65536),rng.randrange(-30*65536,50*65536),rng.getrandbits(32),rng.randrange(-40*65536,64*65536),rng.randrange(-30*65536,50*65536),rng.getrandbits(32),rng.getrandbits(32),rng.randrange(256),rng.getrandbits(32)])
 color_base=0x1300000;depth_base=0x1310000;vertices=0x1320000;material=0x1330000;writes=0;pixel_active=False
 def pixel_hook(uc,kind,at,width,value,_):
  nonlocal writes
  if pixel_active and color_base<=at<color_base+32*24*4:writes+=1
 pixel_id=u.hook_add(unicorn.UC_HOOK_MEM_WRITE,pixel_hook);put(0x8ee1e0,CAM)
 for at,n in [(0x34,2),(0x38,3),(0x3c,29),(0x40,20),(0x50,32)]:put(CAM+at,n)
 for y in range(24):put(CAM+0x68+y*4,color_base+y*32*4);put(CAM+0xc68+y*4,depth_base+y*32*2)
 for v in pixels:
  seed=v[8];u.mem_write(color_base,b''.join(((0x12345678^((seed*65537+i*71)&0xffffffff))&0xffffffff).to_bytes(4,'little') for i in range(32*24)));u.mem_write(depth_base,b''.join(((seed+i*113)&65535).to_bytes(2,'little') for i in range(32*24)))
  for endpoint in range(2):
   for axis in range(3):put(vertices+endpoint*32+axis*4,v[endpoint*3+axis])
  put(material+0x14,v[6]);put(material+0x41,v[7],1);writes=0;pixel_active=True;failed=call(0x462520,0xa00,vertices,material);pixel_active=False;expected.append(' '.join(map(str,[int(failed),0 if failed else writes,digest(u.mem_read(color_base,32*24*4)),digest(u.mem_read(depth_base,32*24*2))])));inputs.append(row('P',v))
 u.hook_del(pixel_id);groups['pixels']=len(pixels)
 # 4634D0 outer body with real45CDA0+44AF80 and an explicit draw boundary.
 state={};frame_active=False
 def frame_hook(uc,at,size,_):
  if not frame_active:return
  mutation=state['v'][8]
  if at==0x45cda0:
   if mutation==10:state['failed']=True;uc.emu_stop();return
   sp=uc.reg_read(UC_X86_REG_ESP);matrix=get(sp+4);point=get(sp+8);trace(3,matrix,point);state['transforms']+=1
   if mutation==1 and state['transforms']==1:put(BASE+12,state['v'][6]+32768);put(BASE+16,0x20000);put(BASE+24,12345);put(BASE+28,0x13579bdf)
   if mutation==2 and state['transforms']==2:put(CAM+0x54,48);put(0x6e4724,get(0x6e4724)+100);put(0x6e4718,1)
  if at==0x462df0:
   if mutation==11:state['failed']=True;uc.emu_stop();return
   sp=uc.reg_read(UC_X86_REG_ESP);assert get(sp+4)==0xa00;vertex=get(sp+8);mat=get(sp+12);line=[*[get(vertex+i*4) for i in range(3)],*[get(vertex+32+i*4) for i in range(3)],get(mat+0x14),get(mat+0x41,1)];trace(4,0xa00,line[6]);state['draws'].append(line)
   if mutation==3 and len(state['draws'])==1:put(BASE+2047*32+24,30);put(BASE+2047*32,0x40000);put(0x6e4724,get(0x6e4724)+7)
   if mutation==9:state['failed']=True;uc.emu_stop();return
   r.finish(0)
 def frame_memory(uc,kind,at,width,n,_):
  if not frame_active:return
  pc=uc.reg_read(UC_X86_REG_EIP)
  if not (0x4634d0<=pc<=0x463783 or 0x44af80<=pc<=0x44b2b9):return
  if not (at in [0x6e4724,0xa158a0,0x6e4718] or BASE<=at<0x9f3098 or CAM+0x54<=at<CAM+0x60 or CAM+0x1880<=at<CAM+0x18d0):return
  mutation=state['v'][8]
  if kind==unicorn.UC_MEM_READ:
   trace(1,at)
   if mutation==4 and at==0x6e4724:put(0xa158a0,get(at)+1)
   if not state['used'] and mutation==7 and at==CAM+0x54:put(at,48);put(BASE+28,0xaabbccdd);state['used']=True
   if mutation==8 and at==BASE+28:put(BASE+24,0);put(CAM+0x54,96)
  else:
   trace(2,at,n)
   if mutation==5 and at==0xa158a0 or mutation==6 and at==BASE+24 and n==0:state['failed']=True;state['restore']=(at,get(at));uc.emu_stop()
 frame_id=u.hook_add(unicorn.UC_HOOK_CODE,frame_hook);frame_mem=u.hook_add(unicorn.UC_HOOK_MEM_READ|unicorn.UC_HOOK_MEM_WRITE,frame_memory)
 frames=[]
 for now,previous,demo in [(0,0,0),(10,9,0),(40,10,0),(0x80000000,0x7fffffff,0),(0x7fffffff,0x80000000,0),(0,0xffffffff,0),(0xffffffff,0,0),(100,0,1)]:
  for life in [0,1,3,4,30,64,0x7fffffff,0x80000000]:
   for m in range(12):frames.append([now,previous,demo,life,30,5,65536,0xb9ffa0a0,m])
 for _ in range(a.cases):frames.append([rng.getrandbits(32),rng.getrandbits(32),rng.randrange(2),rng.getrandbits(32),rng.randrange(40),rng.randrange(40),rng.choice([0,4095,4096,4112,65536,0x3e80000,0x3e80001]),rng.getrandbits(32),rng.randrange(12)])
 for v in frames:
  u.mem_write(BASE,b'\0'*0x10000);put(0x6e4724,v[0]);put(0xa158a0,v[1]);put(0x6e4718,v[2]);put(CAM+0x54,32);put(CAM+0x58,16);put(CAM+0x5c,12)
  for i in range(20):put(CAM+0x1880+i*4,0)
  put(CAM+0x1880,0x400000);put(CAM+0x188c,4096)
  for i in range(12):put(CAM+0x1920+i*4,0x400000 if i in [0,5,10] else 0)
  for j,slot in enumerate([0,1,2047]):
   for i,n in enumerate([v[6],0x8000+j*1024,0x4000,v[6]+16384,0x10000+j*2048,0x8000,v[3+j],v[7]+j*0x11223344]):put(BASE+slot*32+i*4,n)
  state={'v':v,'transforms':0,'draws':[],'used':False,'failed':False,'restore':None};h=1469598103934665603;frame_active=True;failed=call(0x4634d0,CAM);frame_active=False;failed=failed or state['failed']
  if state['restore']:put(*state['restore'])
  result=[int(failed),get(0xa158a0),get(0x6e4724),get(0x6e4718),state['transforms'],len(state['draws']),h]
  for slot in [0,1,2047]:result.extend(get(BASE+slot*32+i*4) for i in range(8))
  for line in state['draws']:result.extend(line)
  expected.append(' '.join(map(str,result)));inputs.append(row('F',v))
 u.hook_del(frame_id);u.hook_del(frame_mem);groups['frame']=len(frames)
 # Full native binding join against actual frame -> transform -> clip -> raster.
 integrated=[]
 for now,previous,demo in [(1,0,0),(1,1,0),(30,0,0),(100,0,1),(0,0xffffffff,0)]:
  for life in [3,4,30,64,128]:integrated.append([now,previous,demo,life,30,655360,1310720,0xb9ffa0a0,61440])
 for _ in range(a.cases//10):integrated.append([rng.randrange(50),rng.randrange(50),rng.randrange(2),rng.randrange(256),rng.randrange(40),rng.randrange(4112,0x4000000),rng.randrange(4112,0x4000000),rng.getrandbits(32),rng.getrandbits(32)])
 pixel_id=u.hook_add(unicorn.UC_HOOK_MEM_WRITE,pixel_hook)
 for v in integrated:
  u.mem_write(BASE,b'\0'*0x10000);put(0x6e4724,v[0]);put(0xa158a0,v[1]);put(0x6e4718,v[2]);put(CAM+0x54,32);put(CAM+0x58,16);put(CAM+0x5c,12)
  for i in range(20):put(CAM+0x1880+i*4,0)
  put(CAM+0x1880,0x400000);put(CAM+0x188c,4096)
  for i in range(12):put(CAM+0x1920+i*4,0x400000 if i in [0,5,10] else 0)
  for j,slot in enumerate([0,2047]):
   x=v[5+j];values=[x,-32768 if j else 32768,16384,x+4096,-98304 if j else 98304,32768,v[3+j],v[7]+j*0x11223344]
   for i,n in enumerate(values):put(BASE+slot*32+i*4,n)
  seed=v[8];u.mem_write(color_base,b''.join(((0x12345678^((seed*65537+i*71)&0xffffffff))&0xffffffff).to_bytes(4,'little') for i in range(32*24)));u.mem_write(depth_base,b''.join(((seed+i*113)&65535).to_bytes(2,'little') for i in range(32*24)))
  writes=0;pixel_active=True;failed=call(0x4634d0,CAM);pixel_active=False;expected.append(' '.join(map(str,[int(failed),get(0xa158a0),get(BASE+24),get(BASE+2047*32+24),writes,digest(u.mem_read(color_base,32*24*4)),digest(u.mem_read(depth_base,32*24*2))])));inputs.append(row('I',v))
 u.hook_del(pixel_id);groups['integrated_pixels']=len(integrated)
 result=subprocess.run([str(a.probe)],input='\n'.join(inputs)+'\n',text=True,capture_output=True);assert result.returncode==0,result.stderr;actual=result.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected));diff=[{'case':i,'input':inputs[i],'actual':x,'expected':y} for i,(x,y) in enumerate(zip(actual,expected)) if x!=y]
 report={'source_sha256':SOURCE_SHA256,'cases':groups,'differences':len(diff),'mismatches':diff[:5],'scope':'44AEE0/44AF80/4634D0 full original; actual46252032-bit pixels, unchanged depth; frame462DF0 draw boundary fixture plus full frame-to-pixels integrated cases'};a.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'cases':groups,'differences':len(diff)}));assert not diff
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=100);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
