#!/usr/bin/env python3
"""Compare event person writes and x87 arithmetic with the supplied game."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_FPSW,UC_X86_REG_FPCW
from check_projectile_hits_original import Replay,HASH,STACK,setword
ACTION,PERSON=0x2201000,0x794680
BASES=(0x867e80,0x7cca80,0x7dde80)
COUNTS=(0x8cf904,0x8cfa18,0x8cfa1c)
CONTROLS=[0x7f|(pc<<8)|(mode<<10) for pc in (0,2,3) for mode in range(4)]
EDGES=[0,1,2,3,5,7,25,100,360,65536,0x7fffffff,0x80000000,0xffffffff]

def compare(probe,mode,rows,expected):
    done=subprocess.run([str(probe),mode],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
    assert done.returncode==0,(mode,done.stderr)
    actual=done.stdout.splitlines();assert len(actual)==len(expected)
    for index,(found,wanted) in enumerate(zip(actual,expected)):
        assert found==wanted,(mode,index,'row_prefix',rows[index][:7],'trace',found.split()[-1],wanted.split()[-1], 'first_field',next(((n,a,b) for n,(a,b) in enumerate(zip(found.split(),wanted.split())) if a!=b),None))

def run(args):
    source=(args.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==HASH
    rig=Replay(source);rng=random.Random(0x4601a0);rows=[];expected=[]
    startup_controls=[*CONTROLS,0x127f]
    for control in startup_controls:
        rig.u.reg_write(UC_X86_REG_FPCW,control)
        rig.call(0x4a11d2)
        assert rig.u.reg_read(UC_X86_REG_FPCW)==0x037f
    divisors=[n for n in range(1,6553601) if 6553600%n==0]
    corpus=set(EDGES)
    for n in [*range(1,513),*divisors]:corpus.update((n,(-n)&0xffffffff))
    for n in range(1,513):
        q=6553600//n
        for v in (q-1,q,q+1):corpus.update((v,(-v)&0xffffffff))
    for control in CONTROLS:
        for value in [*sorted(corpus),*[rng.getrandbits(32) for _ in range(args.rate_random)]]:
            rig.write(ACTION+0x10,value);rig.write(STACK,0)
            rig.u.reg_write(UC_X86_REG_EAX,ACTION);rig.u.reg_write(UC_X86_REG_ESP,STACK);rig.u.reg_write(UC_X86_REG_FPSW,0);rig.u.reg_write(UC_X86_REG_FPCW,control)
            rig.u.emu_start(0x460047,0x460065,count=1000)
            assert rig.u.reg_read(UC_X86_REG_FPCW)==control
            rows.append([value,control]);expected.append(str(rig.read(STACK)[0]))
    compare(args.probe,'rate',rows,expected);rate_cases=len(rows)
    rows=[];expected=[];kind_counts={};state={}
    regions=[(ACTION,32),*[(base+slot*0x114,0x114) for base in BASES for slot in range(3)],*[(PERSON+slot*0xb4,0xb4) for slot in range(3)],*[(count,4) for count in COUNTS],(0x4fb2cc,64)]
    def tracked(address,size):return any(address>=base and address+size<=base+extent for base,extent in regions)
    def load(at,n):return int.from_bytes(rig.u.mem_read(at,n),'little')
    def memory(u,access,address,size,value,user):
        if not tracked(address,size):return
        if access==uc.UC_MEM_READ:
            occurrence=state['reads'].get(address,0)+1;state['reads'][address]=occurrence;mutation=state['mutation']
            if mutation&1 and address==ACTION+0x10 and occurrence==2:rig.write(address,0)
            if mutation&2 and 0x4fb2cc<=address<0x4fb30c:rig.write(ACTION+8,2);rig.write(ACTION+0x10,0);rig.write(BASES[0]+0xc,PERSON+0xb4)
            if mutation&4 and address==BASES[0]+0xc and occurrence==2:rig.write(address,PERSON+0xb4)
            if mutation&8 and address==COUNTS[0] and occurrence==2:rig.write(address,1)
            state['trace']+=f'R,{address},{size},{load(address,size)};'
        else:state['trace']+=f'W,{address},{size},{value&((1<<(size*8))-1)};'
    hook=rig.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,memory)
    rig.u.ctl_remove_cache(0x45ff20,0x460279)
    for index in range(args.cases):
        mode=index%2;kind=(index//2)%18;control=CONTROLS[(index//36)%12];mutation=(index//18)%16
        action=bytearray(rng.randbytes(32));setword(action,4,3 if index%13 else 2);setword(action,8,kind)
        side=rng.choice([1,3,0xffff8000,0xffffffff,0]);setword(action,0xc,side)
        value=rng.randrange(16) if kind==1 else rng.choice(EDGES+[rng.getrandbits(32)]);setword(action,0x10,value)
        counts=[rng.choice([-1,0,1,2,3])&0xffffffff for _ in range(3)]
        actors=[]
        for base in BASES:
            for slot in range(3):
                image=bytearray(rng.randbytes(0x114));setword(image,0xc,rng.choice([0,PERSON,PERSON+0xb4,PERSON+0x168]));setword(image,0x70,((side if (slot+index)%3 else side+1)<<16)|(rng.getrandbits(16)));actors.append(image)
        persons=[rng.randbytes(0xb4) for _ in range(3)]
        row=[mode,mutation,control,*counts,action.hex(),*[image.hex() for image in actors],*[image.hex() for image in persons]];rows.append(row)
        rig.u.mem_write(ACTION,bytes(action))
        for at,value in zip(COUNTS,counts):rig.write(at,value)
        for group,base in enumerate(BASES):
            for slot in range(3):rig.u.mem_write(base+slot*0x114,bytes(actors[group*3+slot]))
        for slot,image in enumerate(persons):rig.u.mem_write(PERSON+slot*0xb4,image)
        for slot in range(16):rig.write(0x4fb2cc+slot*4,0x12345678^(slot*0x112233))
        state.update(reads={},mutation=mutation,trace='')
        rig.u.reg_write(UC_X86_REG_FPSW,0)
        # Replay.call sets the normal CW; use a tiny entry hook to set the requested one.
        entry=0x45ff20 if mode==0 else 0x4601a0
        def control_hook(u,address,size,user):u.reg_write(UC_X86_REG_FPCW,control)
        cw_hook=rig.u.hook_add(uc.UC_HOOK_CODE,control_hook,begin=entry,end=entry)
        rig.call(entry,ACTION,*([BASES[0]] if mode==0 else []));rig.u.hook_del(cw_hook)
        assert rig.u.reg_read(UC_X86_REG_FPCW)==control
        output=[str(load(at,4)) for at in COUNTS]+[rig.u.mem_read(ACTION,32).hex()]
        output += [rig.u.mem_read(base+slot*0x114,0x114).hex() for base in BASES for slot in range(3)]
        output += [rig.u.mem_read(PERSON+slot*0xb4,0xb4).hex() for slot in range(3)]
        output.append(state['trace']);expected.append(' '.join(output));kind_counts[str(kind)]=kind_counts.get(str(kind),0)+1
    rig.u.hook_del(hook);compare(args.probe,'body',rows,expected)
    report={'source_sha256':HASH,'rate_cases':rate_cases,'body_cases':len(rows),'differences':0,'kind_counts':kind_counts,'x87_controls':CONTROLS,'startup_reset_cases':len(startup_controls),'startup_x87_control':0x037f,'comparison':'Full action, nine actor and three person records, current counts, ordered width-specific reads/writes; real 49BAE6 instructions and restored CW.','fixtures':['4FB2CC 16-word table values','Read boundaries can change action operand, subtype, person pointer, or current count.'],'source_functions':['45FF20','4601A0','4A11D2']}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2400);p.add_argument('--rate-random',type=int,default=1000);p.add_argument('--report',type=Path);run(p.parse_args())
