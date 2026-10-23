#!/usr/bin/env python3
"""Compare live request, receive, ring writes, and ordered effects with Df.exe."""
import argparse
import hashlib
import json
import random
import struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_ESP
from check_projectiles_original import Rig, SCRATCH, STACK, ENTITY, ENTITY_INDEX, PAYLOAD, SOURCE_SHA256
from check_projectile_hits_original import match


def run(args):
    rig=Rig(args.game);rig.setup();rig.poke=rig.poke_u;rng=random.Random(0x462f20);counts={}
    vector=SCRATCH+0x2400
    for mode in ('request','receive','spawn'):
        rows,expected=[],[]
        for case in range(args.cases):
            origin=[rng.randrange(-0x4000000,0x4000000)&0xffffffff for _ in range(3)]
            aim=[rng.getrandbits(32) for _ in range(3)]
            slot,kind=(rng.choice([0,1,255,256,259,260,1279,1280,2303,2304,3327,3328,3839]) if args.physical_owners else rng.randrange(16)),rng.randrange(28)
            flags=rng.choice([0,1,2,4,0x10,0x40,0x50,0x41,0x80,0x86,0xc0,0xd0,0xff])
            speed=rng.choice([0,20,250,950,1200]);f10,f14,f18=[rng.randrange(65536) for _ in range(3)]
            sound,host,ra,rb,team,ring,mutation=rng.randrange(4),rng.randrange(2),rng.randrange(256),rng.randrange(256),rng.choice([0,1,2,0xffff]),rng.randrange(512),rng.randrange(64)
            height=rng.randrange(-0x1000000,0x1000000)&0xffffffff;level=rng.choice([0,0,90,0xfffffff9]);active=rng.randrange(2)
            rows.append([*origin,*aim,slot,kind,flags,speed,f10,f14,f18,sound,host,ra,rb,team,ring,mutation,height,level,active])
            owner=0x7cca80+slot*0x114
            rig.u.mem_write(owner,bytes(0x114));rig.poke(owner,1 if active else 0);rig.poke(owner+0x70,team)
            rig.u.mem_write(vector,struct.pack('<6I',*origin,*aim))
            rig.poke(0x67f528,host);rig.poke(0x8cf982,ra);rig.u.mem_write(0x67dbc9,bytes([rb]));rig.poke(ENTITY_INDEX,ring)
            cls=rig.class_record(kind,1,speed,f10,f14,f18,flags);rig.poke(cls+0x20,sound)
            for i,(index_cell,pointer_cell) in enumerate(((0xa158b4,0xa158ac),(0xa158b8,0xa158a8),(0xa158b0,0xa1589c)),1):
                item=SCRATCH+0x8000+i*0x13c
                rig.poke(index_cell,i);rig.poke(pointer_cell,item);rig.poke(item+0x6c,0x7000+i);rig.poke(item+0x118,11 if i==3 else 0);rig.poke(item+0x128,12)
            rig.u.mem_write(PAYLOAD,struct.pack('<IIHHHB',origin[0],origin[1],slot,aim[0]>>16,aim[1]>>16,kind))
            trace=[]
            def u(at):return rig.peek(at)&0xffffffff
            def args_call(n):return struct.unpack('<%dI'%n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
            def delivery():
                if mutation&2:
                    rig.poke(vector,u(vector)+777);rig.poke(vector+16,u(vector+16)+0x40000);rig.poke(owner+0x70,2);rig.poke(cls+0x30,u(cls+0x30)^0x50)
                    for i in range(1,4):
                        at=SCRATCH+0x8000+i*0x13c+0x6c;rig.poke(at,u(at)+100)
            def ground():
                x,y=args_call(2);trace.append(f'G,{x},{y};')
                if mutation&1:
                    if mode=='receive':
                        rig.u.mem_write(PAYLOAD+8,bytes([(slot^1)&255]));rig.u.mem_write(PAYLOAD+10,struct.pack('<H',((aim[0]>>16)+1)&0xffff))
                    else:
                        rig.poke(vector,u(vector)^0x7f);rig.poke(vector+8,u(vector+8)+0x123);rig.poke(vector+12,u(vector+12)+0x10000)
                rig.finish(height)
            def host_message():
                (p,)=args_call(1);trace.append('H,'+rig.u.mem_read(p,15).hex()+';');delivery();rig.finish()
            def queue():
                message,p,length=args_call(3);trace.append(f'Q,{message},'+rig.u.mem_read(p,length).hex()+';');delivery();rig.finish()
            def attached():
                entity,s,value,f=args_call(4);trace.append(f'A,{(entity-ENTITY)//0x114},{s},{value},{f};')
                if mutation&4:rig.poke(cls+0x20,u(cls+0x20)+1);rig.poke(cls+0x30,u(cls+0x30)^6)
                rig.finish()
            def positional():
                s,x,y,z=args_call(4);trace.append(f'S,{s},{x},{y},{z};')
                if mutation&8:rig.poke(cls+0x20,u(cls+0x20)+1);rig.poke(cls+0x30,u(cls+0x30)^0x86)
                rig.finish()
            def loudness():
                (s,)=args_call(1);trace.append(f'L,{s};')
                if mutation&16:rig.poke(cls+0x30,u(cls+0x30)^0x84);rig.poke(owner+0x70,0xffff)
                if mutation&32:rig.poke(ENTITY_INDEX,127)
                rig.finish(level)
            def alert():trace.append('E,'+','.join(map(str,args_call(6)))+';');rig.finish()
            for address,fn in ((0x47df40,ground),(0x445b80,host_message),(0x46f3e0,queue),(0x43ef80,attached),(0x43e7d0,positional),(0x43f4d0,loudness),(0x45fcf0,alert)):rig.stubs[address]=fn
            if mode=='receive':
                rig.poke(STACK+0x44,PAYLOAD);rig.poke(STACK+0x80,9);rig.call_frame(0x46ea40)
            else:rig.call(0x46fb40 if mode=='request' else 0x462f20,vector,owner,kind)
            entity=ENTITY+((ring+1)&511)*0x114
            values=[u(ENTITY_INDEX)]
            offsets=[0,4,0x10,0x24,0x28,0x2c,0x30,0x34,0x3c,0x40,0x44,0x6c,0x7c,0x80,0x84,0x88,0xf8,0xfc]
            for offset in offsets:
                value=u(entity+offset)
                if offset==4 and value:value=(value-(SCRATCH+0x8000))//0x13c
                if offset==0xf8:value=(value-0x7cca80)//0x114 if value else 0xffffffff
                values.append(value)
            values.append(rig.u.mem_read(owner+0x6d,1)[0])
            expected.append(' '.join(map(str,values))+' '+''.join(trace))
        match(args.probe,mode,rows,expected);counts[mode]=len(rows)
    report={'source_sha256':SOURCE_SHA256,'cases':counts,'comparison':'Exact packets, ordered parent/effect calls, all entity fields, ring index and canonical owner flags with mutable terrain/delivery/classes/items/teams','foreign_fixtures':['47DF40 terrain','445B80 host delivery','46F3E0 queue','43EF80 attached sound','43E7D0 positional sound','43F4D0 loudness','45FCF0 alert']}
    if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=500);p.add_argument('--physical-owners',action='store_true');p.add_argument('--report',type=Path);run(p.parse_args())
