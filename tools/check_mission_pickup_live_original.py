#!/usr/bin/env python3
"""Check live pickup state and group-zero wire indexes against Df.exe."""
import argparse
import hashlib
import json
import random
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
from check_projectile_hits_original import Replay, HASH, match, setword


def run(args):
    source=(args.game/'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest()==HASH
    replay=Replay(source)
    rng=random.Random(0x447610)
    counts={}
    for site in (0x446f60,0x460470,0x4605d0,0x447610):
        rows,expected=[],[]
        for i in range(args.cases):
            pose=[rng.getrandbits(32) for _ in range(4)]
            home=pose[:] if i%3==0 else [rng.getrandbits(32) for _ in range(4)]
            hp,rules,flags=rng.randrange(65536),rng.randrange(256),rng.getrandbits(32)
            host,owner,slot,mutation,item=rng.randrange(2),rng.randrange(2),rng.randrange(256),rng.randrange(8),rng.getrandbits(32)
            if site==0x460470:item=rng.choice([0xfc3,0xfc4,0,0xfc2])
            rows.append([*pose,*home,hp,rules,flags,host,owner,slot,mutation,item])
            actor=0x7cca80+slot*0x114
            created=0x7dde80+slot*0x114
            target_group=rules&3;target=[0x7dde80,0x867e80,0x8ace80,0x7cca80][target_group]+slot*0x114
            image=bytearray(0x114)
            setword(image,0,0x2240000);setword(image,4,0x2240000);setword(image,0x6c,flags)
            for at,n in zip((0x24,0x28,0x2c,0x30),pose):setword(image,at,n)
            for base in (0x7dde80,0x867e80,0x8ace80):replay.u.mem_write(base+slot*0x114,bytes(image))
            setword(image,0xc,0x2204000);setword(image,0x74,0x1234)
            replay.u.mem_write(actor,bytes(image))
            replay.write(0x2204054,*home);replay.write(0x2240094,hp);replay.write(0x2240020,item)
            replay.u.mem_write(0x67f549,bytes([rules]));replay.write(0x67f528,host);replay.write(0xc23820,actor)
            sounds=[];packets=[];local_flags=[]
            def sound():
                assert replay.read(replay.u.reg_read(UC_X86_REG_ESP)+4)[0]==0x165
                sounds.append(1)
                if mutation&1:
                    replay.write(actor+0x74,replay.read(actor+0x74)[0]+1)
                    replay.write(actor+0x24,replay.read(actor+0x24)[0]+7)
            def allocate():
                assert replay.read(replay.u.reg_read(UC_X86_REG_ESP)+4,4)==(actor if owner else 0,1,2,3)
                if mutation&4 and not mutation&2:replay.write(created+0x6c,0x87654321)
                replay.u.reg_write(UC_X86_REG_EAX,0 if mutation&2 else created)
            def packet():
                message,data,length=replay.read(replay.u.reg_read(UC_X86_REG_ESP)+4,3)
                if site==0x460470:
                    assert message==0x20 and replay.u.mem_read(data,length)==bytes([0x14,slot,0])
                    assert replay.read(actor+0x90)[0]==target
                    if mutation&1:replay.write(actor+0x24,replay.read(actor+0x24)[0]+7)
                    if mutation&2:replay.write(0xc23820,0)
                else:assert message==0x2b
                packets.append(replay.u.mem_read(data,length).hex())
            def local_flag():
                local_flags.append(1)
                if mutation&4:replay.write(actor+0x90,0)
            replay.hook(0x4423f0,packet);replay.hook(0x484ed0,local_flag)
            replay.hook(0x43e720,sound);replay.hook(0x460d20,allocate);replay.hook(0x442630,packet)
            args_call=(actor if owner else 0,1,2,3) if site==0x447610 else (actor,target) if site==0x460470 else (actor,)
            result=replay.call(site,*args_call)
            values=[result if site==0x4605d0 else 0]
            values.extend(replay.read(actor+at)[0] for at in (0x24,0x28,0x2c,0x30,0x6c))
            values.extend((replay.read(actor+0x74)[0]&0xffff,replay.read(created+0x6c)[0],len(sounds),replay.read(actor+0x90)[0],replay.read(target+0x6c)[0],len(local_flags)))
            expected.append(' '.join(map(str,values))+' '+(packets[0] if packets else '-'))
            replay.clear_hooks()
        match(args.probe,f'{site:x}',rows,expected)
        counts[f'{site:X}']=len(rows)
    report={'source_sha256':HASH,'cases':counts,'identity_contract':'Stored+90 is compared as the physical source pointer, without normalization; typed API arguments remain actor handles',
            'comparison':'Raw pickup links across all four groups, live canonical actor writes, slot-zero person home, synchronous sound mutation, group-zero allocation and full 43-byte packet','foreign_fixtures':['43E720 sound','460D20 allocator','442630 reliable transport','4423F0 broadcast flag packet','484ED0 local flag effect']}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path)
    parser.add_argument('--cases',type=int,default=500);parser.add_argument('--report',type=Path)
    run(parser.parse_args())
