#!/usr/bin/env python3
"""Compare 460E40 with the exact local executable and mutable shared cells."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from check_game_messages_original import Harness,SOURCE_HASH

def main():
 p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--cases',type=int,default=1024);p.add_argument('--report');a=p.parse_args()
 raw=(Path(a.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
 h=Harness(raw);u=h.machine();seeds=list(range(a.cases))
 output=subprocess.run([a.probe],input=''.join(f'{s}\n' for s in seeds),text=True,capture_output=True,check=True)
 ports=[json.loads(s) for s in output.stdout.splitlines()];assert len(ports)==len(seeds)
 def read(at):return struct.unpack('<I',u.mem_read(at,4))[0]
 def put(at,v):u.mem_write(at,struct.pack('<I',v&0xffffffff))
 state={};targets={0x460e6a:0x8e61b8,0x460e71:0x8e61bc,0x460e77:0x8e61b4,0x460e8c:0x8e61b4,0x460e95:0x8e61bc,0x460e9a:0x8e61b8,0x460ea9:0x8e61bc,0x460eb3:0x8e61b4,0x460ebd:0x8e61b8}
 def hook(m,at,size,user):
  if at==0x497790:
   state['calls'].append([at,read(0x8ee1cc),read(0x8e61b4)])
   if state['flags']&16:
    for addr,value in [(0x501344,0xe0010000+state['seed']),(0x50135c,0x10002000+state['seed']),(0x501360,200+state['seed']),(0x8ee1cc,55)]:put(addr,value)
   sp=m.reg_read(UC_X86_REG_ESP);ret=read(sp);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EAX,0x12345678 if state['mode']==3 else 0);m.reg_write(UC_X86_REG_EIP,ret)
  elif at in targets:
   addr=targets[at];state['writes'].append([addr,read(addr)]);state['step']+=1;step=state['step'];flags=state['flags']
   if flags&1:
    for addr,value in [(0x50133c,0xc0000000+step),(0x501344,0xc1000000+step),(0x50135c,0x10001000+step),(0x501360,100+step)]:put(addr,value)
   if flags&2:put(0x8e61b4,0xd0000000+step)
   if flags&4:put(0x8ee1cc,70+step)
   if flags&8:u.mem_write(0x8ec1cc,bytes([step]))
 u.hook_add(UC_HOOK_CODE,hook);errors=[];branches=[0,0,0,0]
 for seed,port in zip(seeds,ports):
  mode=seed%4;flags=seed//4;branches[mode]+=1;state.update(seed=seed,flags=flags,mode=mode,step=0,writes=[],calls=[])
  cells={0x67f520:int(mode>=2),0x50133c:0xa0010000+seed,0x501344:0xb0020000+seed,0x50135c:0x10000000+seed,0x501360:seed%8193,0x8e61b4:0x11111111,0x8e61b8:0x22222222,0x8e61bc:0x33333333}
  for addr,value in cells.items():put(addr,value)
  put(0x8ee1cc,[1,8191,8192,0x7fffffff,0x80000000,0xffffffff,16,1024][flags%8] if mode==0 else 0);u.mem_write(0x8ec1cc,bytes((seed+i*7)&255 for i in range(8192)))
  h.call(u,0x460e40)
  expected={'result':u.reg_read(UC_X86_REG_EAX),'count':read(0x8ee1cc),'first':u.mem_read(0x8ec1cc,1)[0],'bytes':bytes(u.mem_read(0x8ec1cc,8192)).hex(),'cells':[[addr,read(addr)] for addr in sorted(cells)],'writes':state['writes'],'calls':state['calls']}
  if port!=expected:errors.append({'seed':seed,'original':expected,'port':port})
 report={'source_sha256':SOURCE_HASH,'cases':len(seeds),'branches':branches,'mismatches':errors,'fixture':'497790 return/current-cell mutation; post-store shared-cell and producer-buffer mutation at actual instruction boundaries'}
 if a.report:Path(a.report).write_text(json.dumps(report,indent=2)+'\n')
 if errors:print(json.dumps(errors[:3],indent=2));raise SystemExit(f'{len(errors)} mismatches')
 print(f'all {len(seeds)} peer receive cases match')
if __name__=='__main__':main()
