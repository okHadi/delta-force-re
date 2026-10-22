#!/usr/bin/env python3
"""Replay offline result bodies 4403C0, 444CA0 and 471010 from the local DF1 image."""
import argparse
import json
import random
import struct
import subprocess
from pathlib import Path
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ECX
from check_projectiles_original import Rig, STACK, PAYLOAD, SOURCE_SHA256

CELLS=[0x67f520,0x67f528,0x67dbc0,0x67f534,0x6e558c,0x67f52c,0x67f53c,0x6e4758,0x6e5774,0x6e576c,0x6e5770,0xc23820]
CLIENTS=0x6e9a90
STRIDE=0x9b84
ACTORS=0x7cca80
BLOCK=0x67f714
SQUADS=0x6e80c8
NAME=PAYLOAD+0x1000


def run(args):
    rig=Rig(args.game);rig.setup();rng=random.Random(0x471010);rows=[];expected=[];counts={};faults=[]
    def read(at):return rig.peek(at)&0xffffffff
    def argv(n):return struct.unpack('<'+'I'*n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
    for case in range(args.cases):
        mode=case%3;flag=rng.choice([0,1,2,0xffffffff]);mutation=rng.randrange(32)
        values={at:rng.getrandbits(32) for at in CELLS}
        for at in CELLS[:7]:values[at]=rng.randrange(2)
        values[0x67f53c]=rng.randrange(6);values[0xc23820]=rng.choice([0,ACTORS+0x114*rng.randrange(17)])
        block=rng.randbytes(0x514);squads=rng.randbytes(0x138);clients=[];actors=[]
        packet=bytearray(rng.randbytes(0xce)) if mode!=1 else bytearray([0xa5]*0xce)
        packet[1]=rng.randrange(18) if mode==2 else packet[1]
        if mode==2:
            for i in range(17):packet[3+i]=rng.randrange(17)
            for i in range(3):
                struct.pack_into('<h',packet,0xb9+2*i,rng.choice([0,-32768,-1,1,2,32767]));struct.pack_into('<i',packet,0xbf+4*i,rng.choice([0,-2147483648,-32768,-1,1,32767,2147483647,rng.randint(-10000,10000)]))
        row=[mode,flag,mutation]+[values[at] for at in CELLS]+[block.hex(),squads.hex()]
        for i in range(17):
            c=bytearray(rng.randbytes(512));c[0x18c]=rng.randrange(2);c[0x18d]=rng.randrange(2);struct.pack_into('<I',c,8,ACTORS+0x114*rng.randrange(17));struct.pack_into('<H',c,0x1c4,rng.randrange(6));clients.append(c);row.append(c.hex());rig.u.mem_write(CLIENTS+i*STRIDE,bytes(c))
        for i in range(17):
            a=bytearray(rng.randbytes(0x114));struct.pack_into('<I',a,0xc,rng.choice([0,NAME+0x100+i*0x20]));actors.append(a);row.append(a.hex());rig.u.mem_write(ACTORS+i*0x114,bytes(a))
        if mode==2 and case%9==2 and values[0xc23820]:
            actor_slot=(values[0xc23820]-ACTORS)//0x114;packet[0]=rng.randrange(3);struct.pack_into('<I',actors[actor_slot],0x6e,packet[0]<<16)
            row[-17+actor_slot]=actors[actor_slot].hex();rig.u.mem_write(ACTORS+actor_slot*0x114,bytes(actors[actor_slot]))
            for i in range(3):struct.pack_into('<i',packet,0xbf+4*i,rng.randint(-32768,32767));struct.pack_into('<h',packet,0xb9+2*i,rng.choice([0,-32768,-1,1,32767]))
        row.append(packet.hex());rows.append(row)
        for at,n in values.items():rig.poke_u(at,n)
        rig.u.mem_write(BLOCK,block);rig.u.mem_write(SQUADS,squads);rig.u.mem_write(PAYLOAD,bytes(packet));rig.u.mem_write(NAME,b'CLIENT\0');
        for i in range(17):rig.u.mem_write(NAME+0x104+i*0x20,b'PERSON\0')
        rig.u.mem_write(STACK-0x1000,bytes([0xa5])*0x1000)
        trace=[];packet_at=PAYLOAD;names=0;had_builder=False
        def stage(site):
            args2=[]
            if site in (0x446100,0x448d20,0x43e720):args2=list(argv(1))
            elif site==0x448130:args2=[(a-CLIENTS)//STRIDE for a in argv(2)]
            trace.append('C,'+','.join(map(str,[site,*args2]))+';');counts[hex(site)]=counts.get(hex(site),0)+1
            result=0
            if site==0x446100 and mutation&1:rig.poke_u(0x67dbc0,0)
            if site==0x446da0 and mutation&2:
                rig.poke_u(0x67f53c,(read(0x67f53c)+1)%6);at=CLIENTS+16*STRIDE+0x18c;rig.u.mem_write(at,bytes([rig.u.mem_read(at,1)[0]^1]))
            if site==0x448130:
                a,c=[read(CLIENTS+slot*STRIDE+0x1a0) for slot in args2];result=0xffffffff if a<c else 1 if a>c else 0
                if mutation&16:
                    rig.poke_u(CLIENTS+args2[1]*STRIDE+0x1a0,a+7);rig.poke_u(CLIENTS+args2[0]*STRIDE+0x1a8,254);rig.poke_u(0x67f53c,(read(0x67f53c)+1)%6)
            rig.finish(result)
        def builder():
            nonlocal packet_at,had_builder
            packet_at=argv(1)[0];had_builder=True
        def publish():
            nonlocal packet_at
            client,id,packet_at,length=argv(4);slot=(client-CLIENTS)//STRIDE
            assert length==0xce
            trace.append(f'P,{slot},{id},'+rig.u.mem_read(packet_at,length).hex()+';')
            if mutation&4:
                at=CLIENTS+((slot+1)%17)*STRIDE+0x18c;rig.u.mem_write(at,bytes([rig.u.mem_read(at,1)[0]^1]));rig.poke_u(0x67f53c,(read(0x67f53c)+1)%6);rig.u.mem_write(packet_at+2,bytes([rig.u.mem_read(packet_at+2,1)[0]^1]))
            rig.finish()
        def name():
            nonlocal names
            assert argv(2)==(0x4ccccc,0x4cccc0)
            actor=rig.u.reg_read(UC_X86_REG_ECX);names+=1;trace.append(f'N,{actor};')
            if mutation&8:
                rig.poke_u(0xc23820,actor);rig.poke_u(0x67f71c,1)
                for offset in (0x25,0x8b):rig.u.mem_write(PAYLOAD+offset,bytes([rig.u.mem_read(PAYLOAD+offset,1)[0]^0x80]))
            rig.finish(NAME)
        for site in (0x446100,0x448d20,0x446da0,0x448130,0x43e720):rig.stubs[site]=lambda site=site:stage(site)
        rig.stubs[0x4403c0]=builder;rig.stubs[0x441750]=publish;rig.stubs[0x404cf0]=name
        fault=False
        try:rig.call([0x4403c0,0x444ca0,0x471010][mode],flag if mode==1 else PAYLOAD)
        except (UcError,RuntimeError):
            eip=rig.u.reg_read(UC_X86_REG_EIP)
            # Confirm the actual signed division instruction, then retain its preceding writes.
            if mode!=2 or eip!=0x4711b2 or rig.u.mem_read(eip,2)!=b'\xf7\xf9':raise
            numerator=(rig.u.reg_read(UC_X86_REG_EDX)<<32)|rig.u.reg_read(UC_X86_REG_EAX)
            if numerator&(1<<63):numerator-=1<<64
            divisor=rig.u.reg_read(UC_X86_REG_ECX)
            if divisor&(1<<31):divisor-=1<<32
            quotient=abs(numerator)//abs(divisor) if divisor else 0
            if numerator*divisor<0:quotient=-quotient
            assert divisor and not -2147483648<=quotient<=2147483647
            fault=True;faults.append({'case':case,'eip':hex(eip),'quotient':quotient})
        if mode==1 and not had_builder:
            # Packet is a caller-owned input; no active client means no produced packet to inspect.
            packet_out=bytes(packet)
            if read(0x67dbc0) and (not values[0x67dbc0] or mutation&1):
                packet_out=bytearray(packet);packet_out[0]=flag&255
                if values[0x67f520]:packet_out[2]=0 if not values[0x67f534] or values[0x6e558c] else 1
        else:packet_out=rig.u.mem_read(packet_at,0xce)
        result=[str(int(fault))]+[str(read(at)) for at in CELLS]+[rig.u.mem_read(BLOCK,0x514).hex(),rig.u.mem_read(SQUADS,0x138).hex()]+[rig.u.mem_read(CLIENTS+i*STRIDE,512).hex() for i in range(17)]+[bytes(packet_out).hex(),''.join(trace)]
        expected.append(' '.join(result))
    process=subprocess.run([str(args.probe),'--live-result'],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
    if process.returncode:
        args.report.with_suffix(".input.txt").write_text("\n".join(" ".join(map(str,row)) for row in rows)+"\n")
        raise RuntimeError(f"After {len(process.stdout.splitlines())} rows: {process.stderr}")
    actual=process.stdout.splitlines();assert len(actual)==len(expected),(len(actual),len(expected))
    for index,(found,wanted) in enumerate(zip(actual,expected)):
        if found!=wanted:
            args.report.parent.mkdir(parents=True,exist_ok=True);args.report.with_suffix('.failure.json').write_text(json.dumps({'case':index,'input':rows[index],'actual':found,'expected':wanted},indent=2))
            a,b=found.split(),wanted.split();field=next(i for i,(left,right) in enumerate(zip(a,b)) if left!=right)
            detail={'case':index,'mode':rows[index][0],'field':field,'actual':a[field][:150],'expected':b[field][:150]}
            if field==len(a)-1:
                left,right=a[field].split(';'),b[field].split(';');step=next(i for i,(x,y) in enumerate(zip(left,right)) if x!=y);detail.update({'step':step,'actual_call':left[step],'expected_call':right[step]})
            raise AssertionError(detail)
    report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'bodies':['4403C0','444CA0','471010'],'calls':counts,'signed_division_faults':faults,'comparison':'Exact 206-byte packets, current client images, result display bytes, ordered effects and mutable provider changes','foreign_fixtures':['446100','448D20','446DA0','448130','441750 publication capture','404CF0 fallback name','43E720 muted sound effect']}
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({**report,'signed_division_faults':len(faults)}))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=600);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
