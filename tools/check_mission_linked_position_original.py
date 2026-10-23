#!/usr/bin/env python3
"""Check the supplied game's linked-position epilogue instructions."""
import argparse
import hashlib
import json
import random
import subprocess
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ESI
from check_projectile_hits_original import Replay, HASH, pack, setword

BASES=(0x7dde80,0x867e80,0x8ace80,0x7cca80)
CAPACITIES=(1024,1024,512,256)
OWNER=BASES[3]
STRIDE=0x114

def run(args):
    source=(args.game/'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest()==HASH
    rig=Replay(source)
    rng=random.Random(0x487c24)
    count=0
    for index in range(args.cases):
        group=index%4
        slot=(index//4)%CAPACITIES[group]
        target=BASES[group]+slot*STRIDE
        alternate=BASES[0]+1023*STRIDE
        images={address:bytearray(rng.randbytes(STRIDE)) for address in {OWNER,target,alternate}}
        xyz=[rng.getrandbits(32) for _ in range(3)]
        images[OWNER][0x24:0x30]=pack(*xyz)
        setword(images[OWNER],0x90,target)
        for address,image in images.items():rig.u.mem_write(address,bytes(image))
        expected={address:bytearray(image) for address,image in images.items()}
        mutate=index%3==1 and target!=OWNER and alternate!=target
        expected[target][0x24:0x28]=pack(xyz[0])
        if mutate:setword(expected[OWNER],0x90,alternate)
        destination=alternate if mutate else target
        expected[destination][0x28:0x30]=pack(*xyz[1:])
        writes=[]
        def store(u,access,address,size,value,user):
            writes.append((address,size,value))
            if mutate and address==target+0x24:
                rig.write(OWNER+0x90,alternate)
        hook=rig.u.hook_add(uc.UC_HOOK_MEM_WRITE,store)
        rig.u.reg_write(UC_X86_REG_EBX,OWNER)
        rig.u.reg_write(UC_X86_REG_ESI,target)
        rig.u.emu_start(0x487c24,0x487c42,count=32)
        rig.u.hook_del(hook)
        assert writes==[(target+0x24,4,xyz[0]),(destination+0x28,4,xyz[1]),(destination+0x2c,4,xyz[2])],(index,writes)
        for address,image in expected.items():
            actual=bytes(rig.u.mem_read(address,STRIDE))
            assert actual==bytes(image),(index,hex(address))
        count+=1
    production_cases=0
    if args.probe:
        rows=[]
        for index in range(args.cases):
            group=index%4
            slot=(index//4)%CAPACITIES[group]
            rows.append([group,slot,index%2,*[rng.getrandbits(32) for _ in range(4)]])
        process=subprocess.run([str(args.probe)],input=''.join(' '.join(map(str,row))+'\n' for row in rows),text=True,capture_output=True)
        assert process.returncode==0,process.stderr
        output=process.stdout.splitlines()
        assert len(output)==len(rows)
        for index,(row,line) in enumerate(zip(rows,output)):
            before_owner,before_target,after_owner,after_target=map(bytes.fromhex,line.split())
            assert all(len(record)==STRIDE for record in (before_owner,before_target,after_owner,after_target))
            group,slot=row[:2];target=BASES[group]+slot*STRIDE
            rig.u.mem_write(OWNER,before_owner);rig.u.mem_write(target,before_target)
            rig.u.reg_write(UC_X86_REG_EBX,OWNER);rig.u.reg_write(UC_X86_REG_ESI,target)
            rig.u.emu_start(0x487c24,0x487c42,count=32)
            assert bytes(rig.u.mem_read(OWNER,STRIDE))==after_owner,('production owner',index)
            assert bytes(rig.u.mem_read(target,STRIDE))==after_target,('production target',index)
            production_cases+=1
    report={'source_sha256':HASH,'entry':'0x487C24','stop_before':'0x487C42 stack epilogue','cases':count,'production_cases':production_cases,'differences':0,'scope':'Original instructions; full owner and target records plus exact ordered stores. Production join requires a separate native check.','fixture':'A Unicorn memory-store hook changes owner+90 after X in selected cases to prove the original Y/Z current reads.','physical_bases':list(BASES),'physical_capacities':list(CAPACITIES)}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'cases':count,'differences':0}))

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('game',type=Path)
    parser.add_argument('--probe',type=Path)
    parser.add_argument('--cases',type=int,default=4096)
    parser.add_argument('--report',type=Path)
    run(parser.parse_args())
