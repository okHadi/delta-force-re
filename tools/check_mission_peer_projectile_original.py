#!/usr/bin/env python3
"""Replay real producer/receive/parser/ledger/spawn and raw-pose alias cases."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig,SCRATCH,PAYLOAD,ENTITY,ENTITY_INDEX,SOURCE_SHA256
class FixtureFault(Exception):pass
OWNER,CLIENT,LOW,HIGH=0x7cca80,0x6e9a90,0x3000000,0x3100000

def main():
 p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--cases',type=int,default=192);p.add_argument('--report');a=p.parse_args()
 rig=Rig(a.game);rig.setup();u=rig.u;raw=(Path(a.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_SHA256
 for base in [LOW,HIGH]:u.mem_map(base,1024*1024)
 u.mem_write(LOW,bytes([5])*1024*1024);u.mem_write(HIGH,bytes([100])*1024*1024)
 rig.stubs.pop(0x47df40);rig.stubs.pop(0x46f3e0)
 rig.call(0x460de0)
 cases=[(mode,s) for mode in ['view','host'] for s in range(a.cases)]
 cases=list(dict.fromkeys(cases+[('host',v*8+g) for v in [4,5,6,12,20,36,68,132,148,196,228] for g in range(8)]))
 native=subprocess.run([a.probe],input=''.join(f'{mode} {s}\n' for mode,s in cases),text=True,capture_output=True,check=True)
 ports=[json.loads(line) for line in native.stdout.splitlines()];assert len(ports)==len(cases)
 def read(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,value,w=4):u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 def args(n):return [read(u.reg_read(UC_X86_REG_ESP)+4+i*4) for i in range(n)]
 state={};pending=[]
 def effect(site):
  values=args(4 if site in [0x43ef80,0x43e7d0] else 6 if site==0x45fcf0 else 1)
  if not state['phase']:
   rig.finish(0);return
  v,view,pose=state['v'],state['view'],state['pose']
  if site==0x43ef80:
   values[0]=(values[0]-ENTITY)//0x114;state['calls'].append([site,*values])
   if (view and v&1) or (not view and v&32):put(pose,read(pose)+777)
   if v&8:raise FixtureFault()
  elif site==0x43e7d0:
   state['calls'].append([site,*values])
   if (view and v&2) or (not view and v&64):put(pose+4,read(pose+4)^555)
  elif site==0x43f4d0:
   state['calls'].append([site,*values])
   if (view and v&4) or (not view and v&128):put(pose+8,read(pose+8)+333);put(OWNER+0x70,0xffff,2);put(OWNER+0x114+0x70,0xffff,2)
   if v&16:put(ENTITY_INDEX,127)
  else:state['calls'].append([site,*values])
  rig.finish(9 if site==0x43f4d0 and state['fx'] else 0)
 for site in [0x43ef80,0x43e7d0,0x43f4d0,0x45fcf0]:rig.stubs[site]=lambda site=site:effect(site)
 def send():
  pointer,n=args(2);state['wire']=bytes(u.mem_read(pointer,n));rig.finish(0)
 rig.stubs[0x4977a0]=send;rig.stubs[0x497790]=lambda:rig.finish(1)
 def mutate(at):
  if state['view'] or not state['phase']:return
  if at==state['row']+4 and state['v']&1:put(0xbd5284,HIGH);put(PAYLOAD+15,8,1);put(CLIENT+8,OWNER+0x114)
  if at==0x6e5570 and state['v']&2:put(CLIENT+0xe4,9);put(0xad3dd0+4*4,77)
 def writes(m,kind,at,size,value,user):
  if state.get('phase') and not state['view'] and (at==state['row']+4 or at==0x6e5570):pending.append(at)
 def hook(m,at,size,user):
  while pending:mutate(pending.pop(0))
  if not state.get('phase'):return
  if at==0x47df40:state['calls'].append([at,*args(2)])
  elif at==0x445b80 and not state['view']:state['calls'].append([at])
  elif at==0x462f20:
   values=args(3);state['pose']=values[0]
   if not state['view']:state['calls'].append([at,*values[1:]])
 u.hook_add(UC_HOOK_MEM_WRITE,writes);u.hook_add(UC_HOOK_CODE,hook)
 items=[SCRATCH+0x8000+i*0x13c for i in range(4)]
 def item_setup():
  for i,(index_cell,pointer_cell) in enumerate([(0xa158b4,0xa158ac),(0xa158b8,0xa158a8),(0xa158b0,0xa1589c)],1):
   put(index_cell,i);put(pointer_cell,items[i]);put(items[i]+0x6c,0x7000+i);put(items[i]+0x118,11 if i==3 else 0);put(items[i]+0x128,12)
 def cls(kind,flags,ttl,sound):
  r=rig.class_record(kind,1,300 if kind==3 else 500 if kind==4 else 800,kind+1,kind+2,ttl,flags);put(r+0x20,sound)
 def reset_owners():
  u.mem_write(OWNER,bytes(2*0x114))
  for i in range(2):put(OWNER+i*0x114,1);put(OWNER+i*0x114+0x70,i+1,2);put(OWNER+i*0x114+0x78,99-i*11)
 def record(index):
  data=bytearray(u.mem_read(ENTITY+index*0x114,0x114));ptr=int.from_bytes(data[4:8],'little')
  if ptr:
   assert ptr in items;data[4:8]=items.index(ptr).to_bytes(4,'little')
  return data.hex()
 errors=[];counts={};faults=0
 for (mode,seed),port in zip(cases,ports):
  OWNER=0x7cca80+(0,256,259,1280,2304,3328)[seed%6]*0x114
  view=mode=='view';v=seed if view else seed//8;g=seed%8;ring=511 if seed%5==0 else seed%512;fx=seed%11!=10 if view else bool(v&4);count=0 if view else 100 if g==4 else 99 if g==5 else 0;row=0x6e4760+min(count,99)*36
  state.clear();state.update(view=view,v=v,phase=False,pose=SCRATCH+0x2000,wire=b'',calls=[],fx=fx,row=row);pending.clear();item_setup();reset_owners();put(0xbd5284,LOW);put(0x8cf982,0,1);put(0x67dbc9,0,1)
  u.mem_write(ENTITY,bytes(0x22800));put(ENTITY_INDEX,511);cls(4,0x80,44,0);predicted=bytes(0x114).hex();pose=bytes((0,))*20
  if not view:
   u.mem_write(0xbc5d24,bytes(64*0x264));put(0xbc5d1c,1);put(0x50321c,0);put(0x8ea1c4,0);put(0x67f520,1);put(0x67f528,0)
   vector=SCRATCH+0x2000;values=[0x12345670+seed*17,0x23456780+seed*19,0x34567890+seed*23,0x12345678+seed,0xabcdef12-seed]
   u.mem_write(vector,struct.pack('<5I',*[x&0xffffffff for x in values]));rig.call(0x46fb40,vector,OWNER,4);rig.call(0x46f4b0);predicted=record(0);u.mem_write(PAYLOAD,state['wire'])
  u.mem_write(ENTITY,bytes(0x22800));put(ENTITY_INDEX,ring);reset_owners();put(0x67f520,1);put(0x67f528,1);put(0x8ee1cc,0);put(0xbd5284,LOW)
  cls(3,0x40 if fx else 0x80,90,5 if fx else 0);cls(4,0x40 if fx else 0x80,44,5 if fx else 0);cls(8,0x40 if fx else 0x80,88,5 if fx else 0)
  u.mem_write(0x6e4760,bytes(3604));put(0x6e5570,count);u.mem_write(CLIENT,bytes(17*0x9b84))
  for i in range(17):put(CLIENT+i*0x9b84,7+i)
  for at,value in [(8,OWNER+0x114 if g==7 else OWNER),(0x70,10),(0x80,20),(0x98,30),(0xe4,0 if g==1 else 4),(0x154,0xffffffff if g==3 else 100),(0x15c,4),(0x198,5 if g==2 else 0)]:put(CLIENT+at,value)
  put(CLIENT+0x1c4,5,2);put(0x67f538,0xffffffff if g==3 else 100);put(0xad3dd0+4*4,11);state['phase']=True;failed=False
  try:
   if view:
    pose=b''.join(((0x12345670+seed*17+i*0x11111111)&0xffffffff).to_bytes(4,'little') for i in range(5));u.mem_write(SCRATCH+0x2000,pose);rig.call(0x462f20,SCRATCH+0x2000,OWNER if fx else 0,3);pose=bytes(u.mem_read(SCRATCH+0x2000,20))
   else:
    put(0x501344,7);put(0x50135c,PAYLOAD);put(0x501360,len(state['wire']));rig.call(0x460e40);rig.call(0x442ae0)
  except FixtureFault:failed=True
  if view:pose=bytes(u.mem_read(SCRATCH+0x2000,20))
  expected={'failed':failed,'ring':read(ENTITY_INDEX),'entity':record((ring+1)&511),'predicted':predicted,'wire':bytes(u.mem_read(PAYLOAD,len(state['wire']))).hex() if not view else '', 'pose':pose.hex() if view else '', 'ledger':bytes(u.mem_read(row,36)).hex(),'count':read(0x6e5570),'client':[read(CLIENT+at) for at in [8,0x70,0x80,0x98,0xe4,0x154,0x15c,0x198,0x1a4]],'owners':[read(OWNER+0x78),read(OWNER+0x114+0x78),read(OWNER+0x6d,1),read(OWNER+0x114+0x6d,1)],'calls':state['calls']}
  counts[mode]=counts.get(mode,0)+1;faults+=int(failed)
  if port!=expected:errors.append({'mode':mode,'seed':seed,'fields':[k for k in expected if port[k]!=expected[k]],'original':expected,'port':port})
 report={'source_sha256':SOURCE_SHA256,'cases':counts,'total':sum(counts.values()),'fault_prefix_cases':faults,'mismatches':errors,'real_bodies':['46FB40','46F3E0','46F4B0','4611C0','460E40','442AE0','445B80','462F20','47DF40'],'fixtures':['4977A0 send and497790 receive backend','43EF80/43E7D0/43F4D0/45FCF0 effects; live pose aliases and faults','post-ledger-store terrain/packet/client/ammo mutations'],'comparison':'Serialized09 bytes, client predicted record, full host projectile114 record, real ledger row, shared counter/ring, current client ammo/cooldown/owner, exact effect call order and raw pose changes'}
 if a.report:Path(a.report).write_text(json.dumps(report,indent=2)+'\n')
 if errors:
  for e in errors[:4]:print(e['mode'],e['seed'],e['fields']);print('original calls',e['original']['calls'],'port calls',e['port']['calls'])
  raise SystemExit(f'{len(errors)} mismatches')
 print(f'all {sum(counts.values())} peer projectile cases match')
if __name__=='__main__':main()
