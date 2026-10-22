#!/usr/bin/env python3
"""Compare the complete 443C50 frame and ordered effects with original DF1."""
import argparse
import json
import random
import struct
import subprocess
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig, STACK, SOURCE_SHA256
from check_projectile_hits_original import match

CELLS=[0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67dbbc,0x67f538,0x67dbe8,0x6e4730,0x6e4734,0x6e4738,0x6e473c,0x6e4740,0x6e4744,0x6e4748,0x6e474c,0x6e4750,0x6e4754,0x6e4758,0x6e475c,0x8cfa18,0x8cfa1c]
SITES=[0x460e40,0x442ae0,0x4477f0,0x42eaf0,0x44a200,0x4438c0,0x448c30,0x445c80,0x45fd80,0x444630,0x448590,0x446200,0x4482e0,0x447930,0x447fb0,0x444e30,0x441920,0x444f60,0x4421d0]
CALLBACKS=[0x429bc0,0x450e40,0x451fd0,0x452e10,0x451d40,0x4528c0,0x4519a0,0x453f30,0]
BASES={3:0x7cca80,0:0x7dde80}
CLIENTS=0x6e9a90
STRIDE=0x9b84


def run(args):
    rig=Rig(args.game);rig.setup();rig.poke=rig.poke_u;rng=random.Random(0x443c50);rows,expected=[],[];counts={};packets={}
    def read(at):return rig.peek(at)&0xffffffff
    for case in range(args.cases):
        values={at:0 for at in CELLS};values.update({0x67f520:rng.randrange(2),0x67f528:rng.randrange(2),0x67dbc0:rng.randrange(2),0x67f534:rng.randrange(2),0x6e558c:rng.randrange(2),0x67dbbc:rng.getrandbits(32),0x67f538:rng.choice([0,1,2,0x80000000,0xffffffff]),0x67dbe8:rng.choice([0xffffffff,0,1001,0x7fffffff,0x80000000,rng.getrandbits(32)]),0x8cfa18:4,0x8cfa1c:len(CALLBACKS)})
        if case%3==0:values[0x67f528]=1;values[0x67f520]=1;values[0x67f538]=0
        for at,limit in [(0x6e4730,1),(0x6e4734,15),(0x6e4738,62),(0x6e473c,310),(0x6e4740,62),(0x6e4744,496),(0x6e4748,434),(0x6e474c,62),(0x6e4750,1860),(0x6e4754,62),(0x6e4758,1),(0x6e475c,62)]:values[at]=rng.choice([0,limit-1,limit,limit+1,0xffffffff,0x7fffffff,0x80000000])
        peers=rng.randrange(4);mutation=rng.randrange(64);row=[values[at] for at in CELLS]+[peers,mutation]
        clients=[]
        for i in range(17):
            c=bytearray(rng.randbytes(512));c[0x18c]=rng.randrange(2);struct.pack_into('<I',c,8,rng.choice([0,BASES[3]+rng.randrange(4)*0x114]));struct.pack_into('<H',c,0x1c4,rng.randrange(6));clients.append(c);row.append(c.hex());rig.u.mem_write(CLIENTS+i*STRIDE,bytes(c))
        actors={}
        for group in (3,0):
            actors[group]=[]
            for i in range(values[0x8cfa18 if group==3 else 0x8cfa1c]):
                a=bytearray(rng.randbytes(0x114));a[0x6c]=rng.choice([0,1,4,5,0x80]);a[0x6d]=rng.choice([0,1,0xff]);a[0x6e]=rng.choice([0,0x20,0xff]);
                if group==0:struct.pack_into('<I',a,0x110,CALLBACKS[i]);a[0x6d]|=1
                actors[group].append(a);row.append(a.hex());rig.u.mem_write(BASES[group]+i*0x114,bytes(a))
        for at,n in values.items():rig.poke(at,n)
        rig.u.mem_write(STACK-0x1000,bytes([0xa5])*0x1000);trace=[]
        def stage(site):
            nonlocal peers
            argv=[]
            if site in (0x447fb0,0x441920):
                (value,)=struct.unpack('<I',rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,4));argv=[(value-CLIENTS)//STRIDE if site==0x441920 else value]
            trace.append('C,'+','.join(map(str,[site,*argv]))+';');counts[hex(site)]=counts.get(hex(site),0)+1
            result=0
            if site==0x460e40:
                if peers:peers-=1;result=1
            if site==0x4477f0 and mutation&1:rig.poke(0x6e4734,15)
            if site==0x42eaf0 and mutation&2:rig.u.mem_write(CLIENTS+0x18c,bytes([rig.u.mem_read(CLIENTS+0x18c,1)[0]^1]));rig.poke(0x6e473c,310)
            if site==0x447fb0 and mutation&4:rig.poke(CLIENTS+argv[0]*STRIDE+0x198,0);rig.u.mem_write(CLIENTS+((argv[0]+1)%17)*STRIDE+0x18c,b'\x01')
            if site==0x441920 and mutation&8:rig.u.mem_write(BASES[0]+0x6d,bytes([rig.u.mem_read(BASES[0]+0x6d,1)[0]|1]));rig.poke(BASES[0]+0x24,read(BASES[0]+0x24)+99);rig.poke(0x8cfa1c,1)
            rig.finish(result)
        def send():
            id,p,length=struct.unpack('<3I',rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,12));payload=bytes(rig.u.mem_read(p,length));trace.append(f'S,{id},'+payload.hex()+';');packets[hex(id)]=packets.get(hex(id),0)+1
            if id!=0x24:
                group=3 if id==0xe else 0;index=struct.unpack_from('<H',payload,2 if id==0xe else 12 if id==0x3b else 0)[0];at=BASES[group]+index*0x114
                if mutation&16:rig.u.mem_write(at+0x6d,b'\xff');rig.u.mem_write(at+0x6e,bytes([rig.u.mem_read(at+0x6e,1)[0]|0x20]))
                if mutation&32:rig.poke(0x67f520,0)
            rig.finish()
        for site in SITES:rig.stubs[site]=lambda site=site:stage(site)
        rig.stubs[0x442630]=send
        rig.call(0x443c50)
        result=[str(read(at)) for at in CELLS]
        result += [rig.u.mem_read(CLIENTS+i*STRIDE,512).hex() for i in range(17)]
        for group in (3,0):result += [rig.u.mem_read(BASES[group]+i*0x114,0x114).hex() for i in range(len(actors[group]))]
        rows.append(row);expected.append(' '.join(result)+' '+''.join(trace))
    process=subprocess.run([str(args.probe),'--frame'],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
    if process.returncode:raise RuntimeError(process.stderr)
    actual=process.stdout.splitlines();assert len(actual)==len(expected)
    for index,(found,wanted) in enumerate(zip(actual,expected)):
        if found!=wanted:
            a,b=found.split(),wanted.split();field=next(i for i,(left,right) in enumerate(zip(a,b)) if left!=right)
            if args.report:args.report.with_suffix('.failure.json').write_text(json.dumps({'case':index,'input':rows[index],'actual':found,'expected':wanted},indent=2))
            detail={'case':index,'field':field,'actual':a[field][:150],'expected':b[field][:150]}
            if field==len(a)-1:
                left,right=a[field].split(';'),b[field].split(';');step=next(i for i,(x,y) in enumerate(zip(left,right)) if x!=y);detail.update({'step':step,'actual_call':left[step],'expected_call':right[step]})
            raise AssertionError(detail)
    report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'stage_calls':counts,'packets':packets,'comparison':'Full443C50 call order,22canonicalcells,17currentclientimages,currentactorreplicationbytes and flagclear writes','foreign_fixtures':[hex(site) for site in SITES]+['442630 reliable packet delivery'],'stack_padding':'A5 explicit input for34/37 packet tails'}
    if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=500);p.add_argument('--report',type=Path);run(p.parse_args())
