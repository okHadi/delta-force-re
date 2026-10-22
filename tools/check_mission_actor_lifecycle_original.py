#!/usr/bin/env python3
"""Compare actual45DA30/4604C0/42CD30/439C40/43F070 instructions."""
import argparse,hashlib,json,random,struct,subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX
from check_game_messages_original import Harness,SOURCE_HASH
SELF,TARGET,PERSON,CHR,USED,REGS=0x7cca80,0x7dde80,0x794680,0x42fc60,0x439860,0x6e0700
ACTOR_HANDLE=0x76000000
PERSON_HANDLE=0x75000000
CHAR_HANDLE=0x74000000

def put(b,at,value):struct.pack_into('<I',b,at,value&0xffffffff)
def fixture(kind,flags,link,person,seed):
    a=bytearray(0x114);put(a,0,1);put(a,4,1);put(a,0x24,seed);put(a,0x28,seed^0x98765432);put(a,0x2c,seed+10);a[0x6c]=flags
    t=bytearray(a);put(t,0x6c,0x1234ffff)
    put(a,0x30,seed);put(a,0x18,CHAR_HANDLE);put(a,0x90,ACTOR_HANDLE if link else 0);put(a,0xc,PERSON_HANDLE if person else 0)
    people=[]
    for value in [99,88]:
        p=bytearray(0xb4);put(p,0,11);p[4:9]=b'name\0';put(p,0x70,12);put(p,0x6c,value);people.append(p)
    return a,t,people

def main():
    p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('probe');p.add_argument('--report');args=p.parse_args()
    raw=(Path(args.game)/'Df.exe').read_bytes();assert hashlib.sha256(raw).hexdigest()==SOURCE_HASH
    rng=random.Random(0x45da30);cases=[]
    for op in ['remove','drop','null']:
        for kind in [0,1,3]:
            for link in [0,1]:
                for person in [0,1]:
                    for mutation in [0,1]:
                        for flags in [0,4,255]:
                            for _ in range(2):cases.append((op,kind,flags,link,person,mutation,rng.getrandbits(32)))
    commands=[' '.join(map(str,c)) for c in cases]
    native=subprocess.run([args.probe],input='\n'.join(commands)+'\n',text=True,capture_output=True,check=True)
    outputs=[json.loads(line) for line in native.stdout.splitlines()];assert len(outputs)==len(cases)
    mismatches=[]
    for case,port in zip(cases,outputs):
        op,kind,flags,link,person,mutation,seed=case;h=Harness(raw);u=h.machine();a,t,people=fixture(kind,flags,link,person,seed)
        for image in [a,t]:put(image,4,0x1500000)
        put(a,0x18,CHR);put(a,0x90,TARGET if link else 0);put(a,0xc,PERSON if person else 0)
        u.mem_write(SELF,bytes(a));u.mem_write(TARGET,bytes(t));u.mem_write(PERSON,b''.join(people));u.mem_write(0x1500000+0x28,struct.pack('<I',kind))
        for index in [0,1]:u.mem_write(CHR+index*0x9c+4,struct.pack('<I',0x12340000+index));u.mem_write(USED+index,b'\1')
        u.mem_write(REGS,bytes(0x4000))
        for slot,values in [(0,[1,SELF,3,4]),(1,[5,TARGET,7,8]),(1023,[9,SELF,11,12])]:u.mem_write(REGS+slot*16,struct.pack('<4I',*values))
        u.mem_write(0xc91e90,struct.pack('<3i',111,222,333))
        calls=[]
        def hook(m,address,size,user):
            if address not in [0x4776c0,0x47df40,0x4980d0]:return
            sp=m.reg_read(UC_X86_REG_ESP);words=struct.unpack('<4I',m.mem_read(sp,16))
            value=0
            if address==0x4776c0:
                calls.append([address,words[1]]);m.mem_write(words[2],struct.pack('<i',-96));m.mem_write(words[3],struct.pack('<i',160))
                if mutation:
                    for offset,v in [(0x24,0xfffffff0),(0x28,32),(0x18,CHR+0x9c),(0xc,PERSON+0xb4)]:m.mem_write(SELF+offset,struct.pack('<I',v))
            if address==0x47df40:calls.append([address,words[1],words[2]]);value=(words[1]*7+words[2]*13)&0xffffffff
            m.reg_write(UC_X86_REG_EAX,value);m.reg_write(UC_X86_REG_ESP,sp+4);m.reg_write(UC_X86_REG_EIP,words[0])
        u.hook_add(UC_HOOK_CODE,hook)
        h.call(u,0x4604c0 if op=='drop' else 0x45da30,[0 if op=='null' else SELF])
        def actor(base):
            image=bytearray(u.mem_read(base,0x114));definition=struct.unpack_from('<I',image,4)[0]
            if definition:put(image,4,1)
            if struct.unpack_from('<I',image,0xc)[0]:put(image,0xc,PERSON_HANDLE+(struct.unpack_from('<I',image,0xc)[0]-PERSON))
            if struct.unpack_from('<I',image,0x18)[0]:put(image,0x18,CHAR_HANDLE+(struct.unpack_from('<I',image,0x18)[0]-CHR)//0x9c)
            if struct.unpack_from('<I',image,0x90)[0]:put(image,0x90,ACTOR_HANDLE)
            return image.hex()
        registrations=[]
        for slot in [0,1,1023]:
            values=list(struct.unpack('<4I',u.mem_read(REGS+slot*16,16)))
            if values[1]:values[1]=ACTOR_HANDLE+(3*0x10000 if values[1]==SELF else 0)
            registrations.append(values)
        expected={'cache':list(struct.unpack('<3i',u.mem_read(0xc91e90,12))),'self':actor(SELF),'linked':actor(TARGET),'people':[bytes(u.mem_read(PERSON+i*0xb4,0xb4)).hex() for i in range(2)],'instances':list(u.mem_read(USED,2)),'registrations':registrations,'calls':calls}
        for key,value in expected.items():
            if port[key]!=value:mismatches.append({'case':case,'field':key,'original':value,'port':port[key]})
    report={'source_sha256':SOURCE_HASH,'cases':len(cases),'mismatches':mismatches}
    if args.report:Path(args.report).write_text(json.dumps(report,indent=2)+'\n')
    if mismatches:print(json.dumps(mismatches[:3],indent=2));raise SystemExit(f'{len(mismatches)} mismatches')
    print(f'all {len(cases)} lifecycle cases match')
if __name__=='__main__':main()
