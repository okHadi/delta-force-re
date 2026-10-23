#!/usr/bin/env python3
"""Compare real 442AE0 and reached 441750 ACK instructions with shared records."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from check_game_messages_original import Harness,SOURCE_HASH,STACK
IDS=[0,1,2,4,9,10,11,0x12,0x13,0x14,0x17,0x21,0x22,0x28,0x2a,0x2c,0x2f,0x32,0x40,0x43,0x45,0x46,0x4c,0x4e,0x50,0x31]
BASE,STRIDE,PACKET=0x6e9a90,0x9b84,0x10000000
REGIONS=[(BASE,17*STRIDE),(PACKET,4096),(0x1500000,0x400),(0x1600000,0x100),(0x1700000,0x100),(0x7cca80,0x501*0x114),(0x1800000,39*0xa8),(0x1810000,12*0x34),(0xad3dd0,48),(0x8e61b4,12),(0x6e5878,12),(0x67f520,4),(0x67f538,4),(0x67dbc0,4),(0x6e5588,4),(0x50321c,4),(0xad3ed0,4),(0xad3ee4,4)]
ARGUMENTS={0x493a80:(3,0),0x4564b0:(1,0),0x47df40:(2,-1),0x445b80:(1,0),0x462f20:(3,0),0x448c00:(1,-1),0x446a30:(2,1),0x4488e0:(2,1),0x441820:(1,-1),0x4423a0:(4,2),0x4657d0:(5,-1),0x445f80:(2,1),0x447150:(1,-1),0x4473f0:(2,1),0x447760:(1,-1),0x447dd0:(3,2),0x447de0:(2,1),0x448040:(2,1),0x4482c0:(2,1),0x447fb0:(1,-1),0x46c180:(0,-1)}
class FixtureFault(Exception):pass

def fixture(seed,image):
 id=IDS[seed%len(IDS)];v=seed//len(IDS);slot=v%17;client=BASE+slot*STRIDE;regions={a:bytearray(n) for a,n in REGIONS}
 def put(a,value,w=4):
  for base,data in regions.items():
   if base<=a and a+w<=base+len(data):data[a-base:a-base+w]=(value&((1<<(w*8))-1)).to_bytes(w,'little');return
  raise ValueError(hex(a))
 for s in range(17):put(BASE+s*STRIDE,10+s)
 for at,value in [(8,0 if v&8 else 0x1500000),(0x70,0xfffffff0),(0x80,0xfffffffe),(0x98,99),(0x154,0xfffffff0 if v&4 else 40),(0x158,500),(0x15c,10),(0x16c,0),(0x170,1),(0x1b8,32000),(0x198,5 if v&16 else 0)]:put(client+at,value)
 put(client+0x1c4,5 if v%3==0 else 1 if v%3==1 else 3,2)
 for i in range(12):put(client+0xd4+i*4,4);put(0xad3dd0+i*4,7+i)
 for i in range(64):
  put(client+0x9b04+i*2,0xffff,2)
  if v&32:put(client+0x204+i*0x264+9,1,1)
 put(client+0x204+2*0x264,0x1234,2);put(client+0x204+2*0x264+9,1,1)
 seq=0 if v%4==3 else 0x4321
 if v%4==1:put(client+0x9b04,seq,2)
 if v%4==2:put(client+0x9b04+126,seq,2)
 put(0x1500000+0x6c,4 if v&2 else 0,1);put(0x1500200+0x6c,0,1)
 for a,value in [(0xad3ed0,0x1800000),(0xad3ee4,0x1810000),(0x67f538,100),(0x67dbc0,1 if v&1 else 0),(0x67f520,0),(0x6e5588,42),(0x50321c,3)]:put(a,value)
 for i in range(4096):put(PACKET+i,(i*7+seed)&255,1)
 flags=image[0x4f8d48-0x400000+id*4+1];length=struct.unpack_from('<H',image,0x4f8d48-0x400000+id*4+2)[0];payload=PACKET+1+(2 if flags&6 else 0);n=8 if flags&1 else length
 put(PACKET,id,1)
 if flags&6:put(PACKET+1,seq,2)
 if flags&1:put(payload,8,2)
 if id==9:
  for at,value,w in [(0,0x10002311,4),(4,0x20004522,4),(10,0x1234,2),(12,0xabcd,2),(14,4,1)]:put(payload+at,value,w)
 if id==10:put(payload,0x10002311);put(payload+4,0x20004522);put(payload+12,0 if v&4 else 10)
 if id==0x14:put(payload,0x1234,2)
 if id==0x17:
  actor_slot=0xffff if v%4==3 else 0x500 if v%4==2 else v%2;put(payload,actor_slot,2)
  if actor_slot!=0xffff:
   actor=0x7cca80+actor_slot*0x114
   for at,value,w in [(4,0x1600000,4),(0xc,0x1700000,4),(8,0x12345678,4),(0x24,0x11111111,4),(0x28,0x22222222,4),(0x2c,0x33333333,4),(0x30,0x44445555,4),(0x34,0x66667777,4),(0x6c,0x1234,2),(0x70,0x56,1),(0x98,0x78,1)]:put(actor+at,value,w)
   put(0x1600000+0x20,0x1234);put(0x1600000+0x24,4);put(0x1600000+0x28,0x89,1)
   for i,b in enumerate(b'SOLDIER\0'):put(0x1700004+i,b,1)
 if id==0x4c:
  for i in range(7):put(payload+i,0,1)
  if v&1:put(payload+6,1,1)
 if id==0x4e:put(payload,1 if v&1 else 0,2);put(payload+2,0,1)
 if id==0x50:put(payload,0x12345678);put(payload+4,18 if v&1 else 17)
 put(payload+n,0x32,1);put(0x8e61b4,0 if v%29==28 else PACKET);put(0x8e61b8,payload+n+1-PACKET);put(0x8e61bc,99 if v%11==10 else 10+slot)
 return regions,dict(seed=seed,id=id,v=v,slot=slot,client=client,payload=payload,seq=seq)

def main():
 p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--cases',type=int,default=832);p.add_argument('--report');a=p.parse_args()
 raw=(Path(a.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
 h=Harness(raw);u=h.machine()
 for base,length in REGIONS:
  start=base&~4095;end=(base+length+4095)&~4095
  for page in range(start,end,4096):
   if not any(lo<=page<=hi for lo,hi,perm in u.mem_regions()):u.mem_map(page,4096)
 seeds=list(dict.fromkeys(list(range(a.cases))+[v*len(IDS)+i for v in [64,65,66,128,129,130,256,257,512,513,1024,1025] for i in range(len(IDS))]))
 native=subprocess.run([a.probe],input=''.join(f'{seed}\n' for seed in seeds),text=True,capture_output=True,check=True)
 ports=[json.loads(s) for s in native.stdout.splitlines()];assert len(ports)==len(seeds)
 def read(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,value,w=4):u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 state={}
 def hook(m,at,size,user):
  if at==0x4417da and state['v']&128:put(state['client']+0x9b04,state['seq'],2);put(state['payload'],0x1234,2)
  if at==0x49a620:
   sp=m.reg_read(UC_X86_REG_ESP);dst,val,n=struct.unpack('<3I',m.mem_read(sp+4,12));m.mem_write(dst,bytes([val&255])*n);ret=read(sp);m.reg_write(UC_X86_REG_EAX,dst);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,ret);return
  if at not in ARGUMENTS:return
  sp=m.reg_read(UC_X86_REG_ESP);argc,index=ARGUMENTS[at];args=list(struct.unpack('<'+'I'*argc,m.mem_read(sp+4,argc*4))) if argc else []
  normalized=[0 if STACK<=x<STACK+0x100000 else x for x in args];state['calls'].append([at,*normalized]);pointer=args[index] if index>=0 else 0
  if at==0x493a80:m.mem_write(pointer,bytes((i+state['seed'])&255 for i in range(128)));blob=''
  elif pointer:
   if at==0x4564b0:n=128
   elif at==0x462f20:n=20
   elif at==0x4423a0:n=45
   elif at==0x447dd0:n=max(0,read(pointer-3,2)-3)
   else:
    flags=read(0x4f8d49+state['id']*4,1);n=read(pointer,2) if flags&1 else read(0x4f8d4a+state['id']*4,2)
   blob=bytes(m.mem_read(pointer,n)).hex()
  else:blob=''
  state['payloads'].append(blob);v=state['v'];client=state['client'];payload=state['payload']
  if v&64 and at==0x47df40:put(client+8,0x1500200);put(0x67f538,200);put(payload+14,8,1);put(0xad3dd0+4*4,33)
  if v&256 and at==0x445b80:put(client+8,0x1500200);put(payload+14,9,1);put(client+0x154,0x12345678)
  if v&512 and at==0x462f20:put(client+0x154,0x87654321);put(client+0xd4+4*4,77);put(0xad3dd0+4*4,44)
  if v&1024 and at not in [0x493a80,0x4564b0]:raise FixtureFault()
  m.reg_write(UC_X86_REG_EAX,0x30000000 if at==0x47df40 else 0x12345678 if at==0x46c180 else 0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,read(sp))
 u.hook_add(UC_HOOK_CODE,hook);errors=[];counts={}
 for seed,port in zip(seeds,ports):
  regions,case=fixture(seed,h.image);state.clear();state.update(case,calls=[],payloads=[])
  for base,data in regions.items():u.mem_write(base,bytes(data))
  failed=False
  try:h.call(u,0x442ae0)
  except FixtureFault:failed=True
  changes=[]
  for base,data in sorted(regions.items()):
   actual=bytes(u.mem_read(base,len(data)))
   if actual!=data:
    for start in range(0,len(data),4096):
     before=data[start:start+4096];after=actual[start:start+4096]
     if before!=after:changes.extend([base+start+i,value] for i,value in enumerate(after) if value!=before[i])
  expected={'failed':failed,'changes':changes,'calls':state['calls'],'payloads':state['payloads']};counts[str(case['id'])]=counts.get(str(case['id']),0)+1
  if port!=expected:
   errors.append({'seed':seed,'id':case['id'],'v':case['v'],'fields':[k for k in expected if port[k]!=expected[k]],'original':expected,'port':port})
 report={'source_sha256':SOURCE_HASH,'cases':len(seeds),'ids':counts,'mismatches':errors,'fixtures':'Required foreign handlers and terrain; real 441750 ACK with mutation before actual 4417DA delay read; callback faults retain prefix writes; normalized stack pointer views'}
 if a.report:Path(a.report).write_text(json.dumps(report,indent=2)+'\n')
 if errors:
  for e in errors[:5]:print(json.dumps({k:v for k,v in e.items() if k not in ['original','port']}));print('original calls',e['original']['calls'],'port calls',e['port']['calls']);print('original payloads',e['original']['payloads'],'port payloads',e['port']['payloads'])
  raise SystemExit(f'{len(errors)} mismatches')
 print(f'all {len(seeds)} peer parser cases match')
if __name__=='__main__':main()
