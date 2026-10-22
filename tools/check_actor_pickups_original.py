#!/usr/bin/env python3
"""Compare pickup actor/client writes and ordered packets with original DF1."""
import argparse
import hashlib
import json
import random
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, match, pack, setword

A,T,S,P=0x2201000,0x2202000,0x2203000,0x2204000
CLIENTS,STRIDE=0x6e9a90,0x9b84
SITES=[0x446f00,0x446f60,0x460470,0x4605d0,0x447220,0x4471d0,0x447280,0x4472d0,0x447360,0x446fb0,0x447610]


def run(args):
    source=(args.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==HASH
    r=Replay(source);rng=random.Random(0x446f00);counts={};foreign_counts={}
    for site in SITES:
        rows,expected=[],[]
        for index in range(args.cases):
            a,t,spawn=[bytearray(rng.randbytes(0x114)) for _ in range(3)]
            have_person=rng.randrange(2);home=[rng.getrandbits(32) for _ in range(4)]
            setword(a,4,0x2240000);setword(a,0xc,P if have_person else 0);a[0x6c]=rng.choice([0,1,4,5,0x81])
            if index%3==0:
                for at,value in zip((0x24,0x28,0x2c,0x30),home):setword(a,at,value)
            setword(t,4,0x2240100 if index%11 else 0);setword(spawn,4,0x2240200)
            hp=rng.randrange(65536);target_id=rng.choice([0,0xfc2,0xfc3,0xfc4,0xfc5]);spawn_id=rng.getrandbits(32)
            host=rng.randrange(2);rules=rng.randrange(256);local=rng.choice([0,A]);team1,team2=rng.getrandbits(32),rng.getrandbits(32)
            clients=[[rng.choice([0,1,1,255]),rng.choice([0,A,T]),rng.getrandbits(32)] for _ in range(17)]
            mutation=rng.randrange(512);mode=rng.getrandbits(32);owner=rng.choice([0,A]);position=[rng.getrandbits(32) for _ in range(3)]
            row=[a.hex(),t.hex(),spawn.hex(),have_person,*home,hp,target_id,spawn_id,host,rules,local,team1,team2,
                 *[n for c in clients for n in c],mutation,mode,owner,*position]
            for address,data in ((A,a),(T,t),(S,spawn)):r.u.mem_write(address,bytes(data))
            r.write(P+0x54,*home);r.write(0x2240000+0x94,hp);r.write(0x2240100+0x20,target_id);r.write(0x2240200+0x20,spawn_id)
            r.write(0x67f528,host);r.u.mem_write(0x67f549,bytes([rules]));r.write(0xc23820,local);r.write(0x6e815c,team1);r.write(0x6e81c4,team2)
            for i,c in enumerate(clients):
                r.u.mem_write(CLIENTS+i*STRIDE+0x18c,bytes([c[0]]));r.write(CLIENTS+i*STRIDE+8,c[1]);r.write(CLIENTS+i*STRIDE+0x1bc,c[2])
            def dump():
                output='|'.join(r.u.mem_read(address,0x114).hex() for address in (A,T,S))
                values=[r.read(0x67f528)[0],r.u.mem_read(0x67f549,1)[0],r.read(0xc23820)[0],r.read(0x6e815c)[0],r.read(0x6e81c4)[0]]
                for i in range(17):values.extend([r.u.mem_read(CLIENTS+i*STRIDE+0x18c,1)[0],r.read(CLIENTS+i*STRIDE+8)[0],r.read(CLIENTS+i*STRIDE+0x1bc)[0]])
                return output+'|'+','.join(map(str,values))
            trace=[]
            def snap(tag):
                foreign_counts[tag.split(',')[0]]=foreign_counts.get(tag.split(',')[0],0)+1
                trace.append(tag+':'+dump()+';')
            def add_client():
                r.u.mem_write(CLIENTS+5*STRIDE+0x18c,b'\x01');r.write(CLIENTS+5*STRIDE+8,A);r.write(CLIENTS+5*STRIDE+0x1bc,0)
            def client_message():
                client,message,arg1,arg2=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,4);i=(client-CLIENTS)//STRIDE
                snap(f'441750,{i},{message},{arg1},{arg2}')
                if mutation&1:add_client()
                if mutation&64:
                    r.u.mem_write(client+0x18c,b'\x00');r.write(client+8,0);r.write(client+0x1bc,r.read(client+0x1bc)[0]+1)
            def resupply():
                client=r.read(r.u.reg_read(UC_X86_REG_ESP)+4)[0];i=(client-CLIENTS)//STRIDE;snap(f'447e80,{i}')
                r.write(client+0x1bc,r.read(client+0x1bc)[0]+3)
            def sound():
                sound_id=r.read(r.u.reg_read(UC_X86_REG_ESP)+4)[0];snap(f'43e720,{sound_id}')
                if mutation&256:r.write(A+0x74,r.read(A+0x74)[0]+1)
            def remove():
                actor=r.read(r.u.reg_read(UC_X86_REG_ESP)+4)[0];snap(f'45da30,{actor}')
                if mutation&4:r.u.mem_write(actor,bytes(0x114));r.write(CLIENTS+3*STRIDE+0x1bc,77)
            def message(transport):
                message_id,data,count=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,3);payload=r.u.mem_read(data,count).hex()
                snap(f'{transport:x},{message_id},{payload}')
                if mutation&1:add_client()
                if mutation&2:r.write(0x67f528,0)
                if mutation&8:r.write(T+0x24,r.read(T+0x24)[0]+123)
                if mutation&128:r.write(0xc23820,0 if r.read(0xc23820)[0] else A)
            def objective():
                actor,pose,argument=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,3);snap(f'4606b0,{actor},{pose},{argument}')
            def allocate():
                value,x,y,z=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,4);snap(f'460d20,{value},{x},{y},{z}')
                if mutation&32 and not mutation&16:r.write(S+0x6c,0x87654321)
                r.u.reg_write(UC_X86_REG_EAX,0 if mutation&16 else S)
            for address,callback in ((0x441750,client_message),(0x447e80,resupply),(0x43e720,sound),(0x484ed0,lambda:snap('484ed0')),
                                     (0x45da30,remove),(0x4423f0,lambda:message(0x4423f0)),(0x442630,lambda:message(0x442630)),
                                     (0x4606b0,objective),(0x460d20,allocate)):r.hook(address,callback)
            arguments=(A,T) if site in (0x460470,0x4472d0,0x447360) else (A,T+0x24,mode) if site==0x446fb0 else (owner,*position) if site==0x447610 else (T,) if site==0x4471d0 else (A,)
            result=r.call(site,*arguments)
            expected.append(f'{result if site==0x4605d0 else 0} {dump()} {"".join(trace) or "-"}');rows.append(row);r.clear_hooks()
        match(args.probe,f'{site:x}',rows,expected);counts[f'{site:X}']=len(rows)
    report={'source_sha256':HASH,'cases':counts,'foreign_call_counts':foreign_counts,
            'foreign_fixtures':['441750 client message','447E80 resupply','43E720 sound','484ED0 local flag effect',
                                '45DA30 removal','4423F0/442630 transport','4606B0 objective logic','460D20 allocator'],
            'comparison':'full actor images, 17 active/actor/delivery client fields, team counters, ordered packets/effects; 4605D0 return'}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path)
    parser.add_argument('--cases',type=int,default=500);parser.add_argument('--report',type=Path);run(parser.parse_args())
