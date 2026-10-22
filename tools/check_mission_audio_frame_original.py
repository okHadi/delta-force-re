#!/usr/bin/env python3
"""Compare the real 43F0B0 scheduler and 49DC20 attenuation with a muted backend."""
import argparse
import json
import random
import struct
from pathlib import Path
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX, UC_X86_REG_EIP
from check_projectiles_original import Rig, SCRATCH, SOURCE_SHA256
from check_projectile_hits_original import match

REG=0x6e0700
SEL=0x6e0470
CAND=0x6e0270
KEYS=0x6e06d0
HANDLES=0x6e06e8
ACTORS=SCRATCH+0x20000
DEFS=SCRATCH+0x60000


def run(args):
    rig=Rig(args.game);rig.setup();rng=random.Random(0x43f0b0);rig.poke=rig.poke_u
    rows,expected=[],[];backend_counts={"update":0,"stop":0,"start":0};capped=0
    for case in range(args.cases):
        counter=rng.choice([0,6,7,7,7,7,8,0x7fffffff,0xffffffff])
        listener=[rng.randrange(-1000000,1000000)&0xffffffff for _ in range(3)]
        yaw=rng.getrandbits(32);mutation=rng.randrange(64);count=rng.choice([0,1,2,5,6,7,12,80,100])
        if case%5==0:counter=7;count=100;capped+=1
        row=[counter,*listener,yaw,mutation,count]
        rig.u.mem_write(CAND,bytes(0x6e4700-CAND))
        rig.poke(0x4f7ea8,counter)
        for i,n in enumerate(listener):rig.poke(0x6e06a8+i*4,n)
        rig.poke(0x6e06b4,yaw)
        for i in range(count):
            sample=i+100;radius=rng.choice([1,10000,100000,10000000]);scale=rng.randrange(256);ceiling=rng.randrange(256)
            active=rng.choice([0,1,1,1,1]);rate=rng.choice([0,65536,32768,0xffffffff]);pos=[(n+rng.randrange(-100000,100000))&0xffffffff for n in listener]
            if case%5==0:radius=10000000;active=1 if i<64 else 0
            identity=rng.choice([i+1,100,200,300]);row += [sample,radius,scale,ceiling,active,rate,*pos,identity]
            d,a=DEFS+i*0x40,ACTORS+i*0x114
            rig.u.mem_write(d,bytes(0x40));rig.poke(d+0x18,sample);rig.poke(d+0x1c,radius);rig.u.mem_write(d+0x24,bytes([scale,ceiling]))
            rig.u.mem_write(a,bytes(0x114));rig.poke(a,active);rig.poke(a+0x20,rate)
            for axis,n in enumerate(pos):rig.poke(a+0x24+axis*4,n)
            for axis,n in enumerate([d,a,identity,0x12345678]):rig.poke(REG+i*16+axis*4,n)
        keys=[rng.choice([0,100,200,300,999,i+1]) for i in range(6)]
        handles=[rng.choice([0,0,50+i]) for i in range(6)];row+=keys+handles
        for i,n in enumerate(keys):rig.poke(KEYS+i*4,n)
        for i,n in enumerate(handles):rig.poke(HANDLES+i*4,n)
        trace=[];starts=0
        def read(at):return rig.peek(at)&0xffffffff
        def call_args(n):return struct.unpack('<%dI'%n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
        def update():
            backend_counts['update']+=1;trace.append('U,'+','.join(map(str,call_args(4)))+';')
            if mutation&2:rig.poke(SEL+12,read(SEL+12)+1);rig.poke(SEL+28+24,17)
            if mutation&8:rig.poke(0x4f7ea8,92)
            rig.finish()
        def stop():
            backend_counts['stop']+=1;trace.append('T,'+str(call_args(1)[0])+';')
            if mutation&1:rig.poke(HANDLES+4,0)
            if mutation&16:rig.poke(HANDLES,888)
            rig.finish()
        def start():
            nonlocal starts
            backend_counts['start']+=1;trace.append('S,'+','.join(map(str,call_args(5)))+';')
            if mutation&4:rig.poke(HANDLES+20,777)
            starts+=1;rig.finish(0 if mutation&32 else 1000+starts)
        rig.stubs.update({0x494fa0:update,0x495070:stop,0x494f40:start})
        rig.call(0x43f0b0)
        values=[read(0x4f7ea8)]
        for i in range(count):
            d,a,identity,reserved=struct.unpack('<4I',rig.u.mem_read(REG+i*16,16))
            values += [((d-DEFS)//0x40)+1 if d else 0,((a-ACTORS)//0x114)+1 if a else 0,identity,reserved]
        values += list(struct.unpack('<128I',rig.u.mem_read(CAND,512)))
        values += list(struct.unpack('<42I',rig.u.mem_read(SEL,168)))
        values += list(struct.unpack('<6I',rig.u.mem_read(KEYS,24)))
        values += list(struct.unpack('<6I',rig.u.mem_read(HANDLES,24)))
        rows.append(row);expected.append(' '.join(map(str,values))+' '+(''.join(trace) or '-'))
    match(args.probe,'',rows,expected)
    attenuation_rows,attenuation_expected=[],[];faults=0
    edges=[0,1,0xffffffff,0x7fffffff,0x80000000,0xffff0000,0x10000]
    pairs=[(distance,range_) for distance in edges for range_ in edges]
    pairs += [(rng.getrandbits(32),rng.getrandbits(32)) for _ in range(1000)]
    for distance,range_ in pairs:
        for scale,ceiling in [(0,0),(255,255),(255,1),(1,255)]:
            attenuation_rows.append([distance,range_,scale,ceiling])
            try:
                rig.call(0x49dc20,distance,range_,scale,ceiling)
                attenuation_expected.append(str(rig.u.reg_read(UC_X86_REG_EAX)))
            except (UcError,RuntimeError):
                # Unicorn can stop at IDIV without raising UC_ERR_EXCEPTION.
                # Admit only the actual fault instruction and a real x86 divide fault.
                sd=distance if distance<0x80000000 else distance-0x100000000
                sr=range_ if range_<0x80000000 else range_-0x100000000
                quotient=(abs(sd*65536)//abs(sr))*(-1 if (sd<0)!=(sr<0) else 1) if sr else 0
                assert rig.u.reg_read(UC_X86_REG_EIP)==0x49dc44
                assert not sr or quotient < -0x80000000 or quotient > 0x7fffffff
                attenuation_expected.append('fault');faults+=1
    match(args.probe,'attenuation',attenuation_rows,attenuation_expected)
    report={'source_sha256' :SOURCE_SHA256,'cases':len(rows),'attenuation_cases':len(attenuation_rows),'attenuation_original_divide_faults':faults,'forced_first64_cap_cases':capped,'backend_call_counts':backend_counts,'comparison':'counter, 1024-registration scan, first64 candidate cap, top6 selection, selected scratch, channel handles/identities, ordered backend calls and backend mutations','foreign_fixtures':['494FA0 update','495070 stop','494F40 start'],'real_helpers':['477670 length','477770 angle','49DC20 attenuation'],'audio_device':'none'}
    if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1000);p.add_argument('--report',type=Path);run(p.parse_args())
