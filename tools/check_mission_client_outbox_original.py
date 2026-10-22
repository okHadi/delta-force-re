#!/usr/bin/env python3
"""Replay actual client enqueue, worker, serializer and local echo bodies."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_game_messages_original import Harness,SOURCE_HASH,IMAGE
BASE,STRIDE,BUFFER,COUNT,ECHO,ECHO_COUNT=0x6e9a90,0x9b84,0x8e61c0,0x8e81c0,0x8ea1c8,0x8ec1c8
SEQUENCES=[0,32000,32001,0xffffffff,0x7fffffff,0x80000000]
DELAYS=[0,1,0x7fff,0x8000,0xffff]
REPEATS=[0,1,2,3,0x7fff,0x8000,0x8001,0xffff]
def put(b,at,value,width=4):b[at:at+width]=(value&((1<<(width*8))-1)).to_bytes(width,'little')
def main():
 p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--report');args=p.parse_args()
 raw=(Path(args.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
 cases=[(op,seed) for op,n in [('enqueue',12),('broadcast',12),('tick',80),('append',30),('flush',20)] for seed in range(n)]
 native=subprocess.run([args.probe],input='\n'.join(f'{op} {seed}' for op,seed in cases)+'\n',text=True,capture_output=True,check=True)
 values=[json.loads(line) for line in native.stdout.splitlines()];assert len(values)==len(cases)
 errors=[]
 for (op,seed),port in zip(cases,values):
  h=Harness(raw);u=h.machine();clients=bytearray(17*STRIDE)
  for slot in range(2):
   c=bytearray([0x5a])*STRIDE
   for index in range(64):c[0x204+index*0x264+9]=0
   c[0x18c]=1;c[0x18d]=int(bool(slot and seed&1));put(c,0,7 if slot else 8);put(c,0x1c4,5,2);put(c,0x1b8,SEQUENCES[seed%6]);put(c,0x6c,10+seed);put(c,0x7c,20+seed)
   if op=='tick':
    for i,index in enumerate([0,2,5]):
     at=0x204+index*0x264;msg=[0xa,0x1a,0x30][i];length=struct.unpack_from('<H',h.image,0x4f8d48-IMAGE+msg*4+2)[0]
     put(c,at,7+i,2);put(c,at+2,REPEATS[(seed+i)%8],2);put(c,at+4,length,2);put(c,at+6,DELAYS[(seed+i)%5],2);c[at+8]=msg;c[at+9]=1
   clients[slot*STRIDE:(slot+1)*STRIDE]=c
  u.mem_write(BASE,bytes(clients));u.mem_write(BUFFER,bytes([0xcc])*8192);u.mem_write(COUNT,bytes(4))
  echo_len=[0,8187,8190,8191][seed%4] if op in ['flush','tick'] else 0
  u.mem_write(ECHO,bytes([0xee])*echo_len);u.mem_write(ECHO_COUNT,struct.pack('<I',echo_len));u.mem_write(0x501340,struct.pack('<I',7));u.mem_write(0x50321c,struct.pack('<I',DELAYS[seed%5]))
  calls=[];packets=[]
  def hook(m,address,size,user):
   if address not in [0x4977e0,0x493a80,0x4564b0]:return
   sp=m.reg_read(UC_X86_REG_ESP);words=struct.unpack('<4I',m.mem_read(sp,16))
   if address==0x4977e0:
    calls.append([address,words[2],words[3]]);packets.append(bytes(m.mem_read(words[1],words[2])).hex())
    if seed&1:
     for at,val in [(COUNT,33),(BASE+0x6c,100),(BASE+0x7c,200)]:m.mem_write(at,struct.pack('<I',val))
     m.mem_write(BASE+STRIDE+0x18c,b'\0');m.mem_write(BASE+0x204+5*0x264+9,b'\0')
   if address==0x4564b0:
    calls.append([address,0xa])
    if seed&1:
     patch=bytearray(128);patch[0xa*4+1]=2;m.mem_write(0x4f8d48,bytes(patch));m.mem_write(COUNT,struct.pack('<I',2))
   m.reg_write(UC_X86_REG_EAX,0);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,words[0])
  u.hook_add(UC_HOOK_CODE,hook)
  payload=bytes((seed+i*7)&255 for i in range(16));u.mem_write(0x10000000,payload)
  if op=='enqueue':h.call(u,0x441750,[BASE,0xa,0x10000000,16])
  elif op=='broadcast':h.call(u,0x442630,[0xa,0x10000000,16])
  elif op=='tick':h.call(u,0x4421d0)
  elif op=='append':
   u.mem_write(COUNT,struct.pack('<I',[0,4,8175][seed%3]));h.call(u,0x460ed0,[0xa,0x1234,0 if seed%5==4 else 0x10000000,[0,1,16,-1,16][seed%5]])
  elif op=='flush':u.mem_write(COUNT,struct.pack('<I',1+seed%9));h.call(u,0x460fb0,[7 if seed&2 else 8])
  size=struct.unpack('<I',u.mem_read(ECHO_COUNT,4))[0]
  expected={'clients':[bytes(u.mem_read(BASE+slot*STRIDE,STRIDE)).hex() for slot in range(2)],'buffer':bytes(u.mem_read(BUFFER,8192)).hex(),'count':struct.unpack('<I',u.mem_read(COUNT,4))[0],'echo':bytes(u.mem_read(ECHO,size)).hex(),'calls':calls,'packets':packets}
  if op=='flush':expected['result']=u.reg_read(UC_X86_REG_EAX)
  for field,value in expected.items():
   if port[field]!=value:
    detail={'case':[op,seed],'field':field}
    if field in ['count','calls','packets','result']:detail.update(original=value,port=port[field])
    else:detail.update(original_sha=hashlib.sha256(str(value).encode()).hexdigest(),port_sha=hashlib.sha256(str(port[field]).encode()).hexdigest())
    errors.append(detail)
 report={'source_sha256':SOURCE_HASH,'cases':len(cases),'mismatches':errors}
 if args.report:Path(args.report).write_text(json.dumps(report,indent=2)+'\n')
 if errors:print(json.dumps(errors[:10],indent=2));raise SystemExit(f'{len(errors)} mismatches')
 print(f'all {len(cases)} client outbox cases match')
if __name__=='__main__':main()
