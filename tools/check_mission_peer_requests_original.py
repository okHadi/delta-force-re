#!/usr/bin/env python3
"""Replay 447150,447760,445B80 and real441750 with mutable canonical cells."""
import argparse,hashlib,json,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from check_game_messages_original import Harness,SOURCE_HASH
ACTORS,OWNERS,CLIENTS,STRIDE,DEFS,PACKET,CB1,CB2=0x7dde80,0x7cca80,0x6e9a90,0x9b84,0x1600000,0x10000000,0x410500,0x410600
REGIONS=[(ACTORS,8*0x114),(OWNERS,4*0x114),(CLIENTS,17*STRIDE),(DEFS,8*0x13c),(PACKET,15),(0x8cfa1c,4),(0x6e4760,3604),(0x6e5778,0x110),(0x50321c,4)]
ENTRIES={'trigger':0x447150,'strike':0x447760,'ledger':0x445b80}
class FixtureFault(Exception):pass

def fixture(op,seed):
 v=seed//(5 if op=='ledger' else 6);slot=seed%17;client=CLIENTS+slot*STRIDE;r={a:bytearray(n) for a,n in REGIONS}
 def put(a,value,w=4):
  for base,data in r.items():
   if base<=a and a+w<=base+len(data):data[a-base:a-base+w]=(value&((1<<(w*8))-1)).to_bytes(w,'little');return
  raise ValueError(hex(a))
 for i in range(17):put(CLIENTS+i*STRIDE,10+i)
 for i in range(8):
  a=ACTORS+i*0x114;d=DEFS+i*0x13c
  for at,value,w in [(0,0 if i==4 else 1,4),(4,d,4),(0x6c,2 if i==2 else 4 if i==3 else 0,1),(0x74,0xabcd1234,4),(0xf8,OWNERS+0x114 if i==1 else OWNERS,4),(0x110,CB2 if i==5 else CB1,4)]:put(a+at,value,w)
  put(d+0x20,999 if i==6 else 0xfb4 if i==5 else 0xfc8)
 put(0x8cfa1c,[0,1,6,8,0xffffffff,0x80000000][seed%6])
 for at,value in [(0x24,0x11110000),(0x28,0x22220000),(0x78,0x11111111),(0x114+0x24,0x33330000),(0x114+0x28,0x44440000),(0x114+0x78,0x22222222)]:put(OWNERS+at,value)
 put(client+8,OWNERS);put(client+0x1b8,32000);put(0x50321c,3)
 put(0x6e5884,[0,1,2,0xffffffff,0x80000000,0x7fffffff][seed%6]);free_slot=v%17;free_at=0x6e5778+free_slot*16
 for i in range(16):put(0x6e5784+i*16,1)
 if free_slot<16:put(free_at+12,[0,0xffffffff,0x80000000][(v//17)%3])
 if v&128:
  for i in range(64):put(client+0x204+i*0x264+9,1,1)
 count=[0,1,99,100,0x7fffffff][seed%5];record=(0x6e4760+count*36)&0xffffffff;put(0x6e5570,count)
 put(PACKET,0x12345680);put(PACKET+4,0x876543ff);put(PACKET+8,[0,1,0xffff,0x8000,0x7fff][v%5],2);put(PACKET+10,0x1234,2);put(PACKET+12,0xabcd,2);put(PACKET+14,4,1)
 put(client+0x1a4,0xffffffff);second=CLIENTS+((slot+1)%17)*STRIDE;put(second+8,OWNERS+0x114);put(second+0x1a4,10)
 if op=='ledger':put(client+8,OWNERS)
 return r,dict(op=op,seed=seed,v=v,slot=slot,client=client,free_at=free_at,record=record)

def main():
 p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--cases',type=int,default=192);p.add_argument('--report');a=p.parse_args()
 raw=(Path(a.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
 h=Harness(raw);u=h.machine();cases=[]
 for op in ENTRIES:
  n=5 if op=='ledger' else 6;forced=[(v*n+i) for v in [0,1,2,3,4,7,8,15,16,31,32,63,64,65,127,128,129,255,256,257,511] for i in range(n)]
  cases.extend((op,s) for s in dict.fromkeys(list(range(a.cases))+forced))
 native=subprocess.run([a.probe],input=''.join(f'{op} {s}\n' for op,s in cases),text=True,capture_output=True,check=True);ports=[json.loads(line) for line in native.stdout.splitlines()];assert len(ports)==len(cases)
 def read(at,w=4):return int.from_bytes(u.mem_read(at,w),'little')
 def put(at,value,w=4):u.mem_write(at,(value&((1<<(w*8))-1)).to_bytes(w,'little'))
 state={};pending=[]
 def after_write(at):
  op,v,client,record,free_at=state['op'],state['v'],state['client'],state['record'],state['free_at']
  if op=='trigger' and ACTORS<=at<ACTORS+8*0x114:
   if v&2:put(at-0x74+0x110,CB2)
   if v&32:raise FixtureFault()
  if op=='strike':
   if at==free_at+12 and v&1:put(client+8,OWNERS+0x114)
   if at==free_at and v&2:put(client+8,OWNERS)
   if at==free_at+4 and v&4:put(client+8,OWNERS+0x114)
   if at==free_at+8 and v&8:put(0x6e5884,50)
   if at==0x6e5884 and v&16:put(0x6e5884,99)
   if at==free_at+12 and v&32:raise FixtureFault()
  if op=='ledger':
   if at==record and v&1:put(PACKET,0x99aa0011);put(PACKET+4,0xbbcc0022)
   if at==record+4 and v&2:put(record+4,0xdead0000);put(PACKET+4,0xcafe0099)
   if at==record+8 and v&4:put(record+4,0xbeef0000);put(PACKET+10,0x7777,2);put(PACKET+12,0x8888,2)
   if at==record+0x20 and v&16:put(0x6e5570,88)
   if at==0x6e5570 and v&32:put(PACKET+8,1,2)
   if CLIENTS<=at<CLIENTS+17*STRIDE and v&64:put(at-0x1a4+8,OWNERS+0x114)
   if at==record+0x10 and v&256:raise FixtureFault()
 def writes(m,kind,at,size,value,user):
  ip=m.reg_read(UC_X86_REG_EIP);owned=(0x447150<=ip<0x4471c1) or (0x447760<=ip<0x4477e7) or (0x445b80<=ip<0x445c76)
  if owned and any(base<=at and at+size<=base+n for base,n in REGIONS):state['writes'].append([at,size,value&((1<<(size*8))-1)]);pending.append(at)
 def hook(m,at,size,user):
  while pending:after_write(pending.pop(0))
  if at==0x4417da and state['op']=='strike' and state['v']&64:put(0x6e5884,77)
  if at not in [CB1,CB2,0x47df40]:return
  sp=m.reg_read(UC_X86_REG_ESP);args=[read(sp+4),read(sp+8)];state['calls'].append([at,*args]);v=state['v']
  if state['op']=='trigger':
   if v&1:put(0x8cfa1c,8);put(ACTORS+5*0x114+0x6c,2,1)
   if v&4:put(ACTORS+0x114+0xf8,OWNERS);put(DEFS+6*0x13c+0x20,0xfb4)
   if v&16:put(0x8cfa1c,0xffffffff)
   if v&8:raise FixtureFault()
  if state['op']=='ledger':
   if v&8:put(0x6e5570,50);put(PACKET+8,1,2);put(PACKET+14,9,1)
   if v&128:raise FixtureFault()
  result=0x40000000+state['seed'] if at==0x47df40 else 0;m.reg_write(UC_X86_REG_EAX,result);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,read(sp))
 u.hook_add(UC_HOOK_MEM_WRITE,writes);u.hook_add(UC_HOOK_CODE,hook);errors=[];counts={};faults=0
 for (op,seed),port in zip(cases,ports):
  regions,case=fixture(op,seed);state.clear();state.update(case,writes=[],calls=[]);pending.clear()
  for base,data in regions.items():u.mem_write(base,bytes(data))
  failed=False
  try:h.call(u,ENTRIES[op],[case['client'] if op=='strike' else PACKET if op=='ledger' else OWNERS])
  except FixtureFault:failed=True
  changes=[]
  for base,data in sorted(regions.items()):
   actual=bytes(u.mem_read(base,len(data)))
   if actual==data:continue
   for start in range(0,len(data),4096):
    before=data[start:start+4096];after=actual[start:start+4096]
    if before!=after:changes.extend([base+start+i,value] for i,value in enumerate(after) if value!=before[i])
  expected={'failed':failed,'changes':changes,'writes':state['writes'],'calls':state['calls']};counts[op]=counts.get(op,0)+1;faults+=int(failed)
  if port!=expected:errors.append({'op':op,'seed':seed,'fields':[k for k in expected if port[k]!=expected[k]],'original':expected,'port':port})
 report={'source_sha256':SOURCE_HASH,'cases':counts,'total':sum(counts.values()),'fault_prefix_cases':faults,'mismatches':errors,'fixtures':'Actual parent bodies and real441750; required terrain/actor behavior providers; post-write mutations applied at next actual instruction; current packet/client/count/definition/provider target changes'}
 if a.report:Path(a.report).write_text(json.dumps(report,indent=2)+'\n')
 if errors:
  for e in errors[:4]:print(e['op'],e['seed'],e['fields']);print('original writes',e['original']['writes'],'port writes',e['port']['writes']);print('original calls',e['original']['calls'],'port calls',e['port']['calls'])
  raise SystemExit(f'{len(errors)} mismatches')
 print(f'all {sum(counts.values())} peer request cases match')
if __name__=='__main__':main()
