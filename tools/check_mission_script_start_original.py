#!/usr/bin/env python3
"""Replay script queue start, removal, registry lookup and event formatting."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn as uc
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ES,UC_X86_REG_GDTR,UC_X86_REG_SS,UC_X86_REG_DS,UC_X86_REG_CS
from check_projectile_hits_original import Replay,HASH,setword
BASE,COUNT,NODES=0x8e3d74,0x8e3d70,0x2202000

def run(args):
    source=(args.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==HASH
    rig=Replay(source);rng=random.Random(0x455f10)
    # Give PUSH/POP ES the original 32-bit flat stack-segment contract.
    rig.u.mem_map(0x3000000,0x1000)
    rig.u.mem_write(0x3000000,struct.pack("<3Q",0,0x00cf9a000000ffff,0x00cf92000000ffff))
    rig.u.reg_write(UC_X86_REG_GDTR,(0,0x3000000,23,0))
    for register,selector in ((UC_X86_REG_CS,8),(UC_X86_REG_SS,16),(UC_X86_REG_DS,16),(UC_X86_REG_ES,16)):rig.u.reg_write(register,selector)
    rig.u.mem_write(0x8e3d54,b'\xa5'*0x1ec);rig.call(0x455c90,0)
    assert bytes(rig.u.mem_read(0x8e3d54,0x1ec))==bytes(0x1ec),'455C90 must clear complete queue/context'
    rows=[];expected=[];state={};modes={}
    def bad_memory(u,access,address,size,value,user):
        state["fault"]=(hex(address),size);return False
    rig.u.hook_add(uc.UC_HOOK_MEM_INVALID,bad_memory)
    def value(at):return rig.read(at)[0]
    def mutate():
        old=state['compares'];state['compares']+=1
        if old:return
        mutation=state['mutation']
        if mutation&1:rig.write(COUNT,1)
        if mutation&2:rig.write(NODES,0x2207000)
        if mutation&4:rig.write(NODES+4,NODES+16)
        if mutation&8:rig.write(BASE+15*16+12,0xdeadbeef)
    def compare():
        left,right=rig.read(rig.u.reg_read(UC_X86_REG_ESP)+4,2);ordinal=state['compares'];mode=state['mode']
        if mode==5:
            text=bytearray();at=right
            while rig.u.mem_read(at,1)[0]:text.extend(rig.u.mem_read(at,1));at+=1
            state['trace']+=f'T,{left},{text.hex()};'
        elif mode!=7:state['trace']+=f'C,{left},{right};'
        mutate();rig.u.reg_write(UC_X86_REG_EAX,int(left!=right) if mode==7 else int(ordinal!=state['match']))
    def zero():
        address,fill,size=rig.read(rig.u.reg_read(UC_X86_REG_ESP)+4,3);assert fill==0 and size==16
        if state['mode']<4:state['trace']+=f'Z,{address};'
        rig.u.mem_write(address,bytes(16));rig.u.reg_write(UC_X86_REG_EAX,address)
    rig.hook(0x49ad30,compare);rig.hook(0x49a620,zero)
    def memory(u,access,address,size,written,user):
        relevant=address==COUNT or address==0x8e3d58 or BASE<=address<BASE+256 or NODES<=address<NODES+24
        if not relevant:return
        assert size==4,(hex(address),size)
        mode=state['mode']
        if mode>=4:
            if access!=uc.UC_MEM_READ:return
            if address==0x8e3d58:state['trace']+='H;'
            elif NODES<=address<NODES+24:
                offset=(address-NODES)%8;node=address-offset;state['trace']+=f'N,{node},{offset},{value(address)};'
            return
        if access==uc.UC_MEM_READ:state['trace']+=f'R,{address},{value(address)};'
        else:state['trace']+=f'W,{address},{written&0xffffffff};'
    memhook=rig.u.hook_add(uc.UC_HOOK_MEM_READ|uc.UC_HOOK_MEM_WRITE,memory)
    rig.u.ctl_remove_cache(0x455f10,0x456070)
    def replay(mode,arg,match,mutation,count,queue,nodes):
        rows.append([mode,arg,match,mutation,count,queue.hex(),nodes.hex()]);rig.u.mem_write(BASE,bytes(queue));rig.u.mem_write(NODES,bytes(nodes));rig.write(COUNT,count);rig.write(0x8e3d58,NODES)
        state.update(mode=mode,mutation=mutation,compares=0,match=match,trace='')
        entry={0:0x455f10,1:0x455fa0,2:0x456000,3:0x455f60,4:0x455f10,5:0x43f500,6:0x455f60,7:0x456000}[mode]
        try:result=rig.call(entry,*([] if mode==1 else [arg]))
        except uc.UcError as error:raise AssertionError((mode,hex(arg),hex(rig.u.reg_read(UC_X86_REG_EIP)),hex(rig.u.reg_read(UC_X86_REG_ESP)),state.get("fault"),rig.u.reg_read(UC_X86_REG_ES),rig.u.reg_read(UC_X86_REG_GDTR),rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP),16).hex(),str(error))) from error
        if mode in (1,5):result=0
        image=bytes(rig.u.mem_read(BASE,256));current=value(COUNT)
        expected.append(f'{result} {current} {image.hex()} {rig.u.mem_read(NODES,24).hex()} {state["trace"] or "-"}')
        modes[str(mode)]=modes.get(str(mode),0)+1
        return current,image
    for index in range(args.cases):
        mode=index%8;mutation=rng.randrange(16) if mode<4 else rng.choice([0,2,4,6]) if mode in (5,6) else 0;count=rng.randrange(17)
        queue=bytearray(rng.randbytes(256)) if mode<4 else bytearray(256)
        for slot in range(16):
            if mode<4:setword(queue,slot*16,0 if rng.randrange(4)==0 else 0x2300000+slot*0x100)
            elif slot<count:
                for field in range(4):setword(queue,slot*16+field*4,0x2300000+slot*0x100 if field==0 else rng.getrandbits(32))
        nodes=bytearray(struct.pack('<6I',0x2400000,NODES+8,0x2400100,NODES+16,0x2400200,0))
        arg=0x2500000;match=rng.randrange(4)
        if mode in (0,4):arg=0x2600000+index*0x100
        if mode==5:arg=rng.choice([0,1,9,99,999,1000,0xffffffff,0xffffff9c,0x80000000,0x7fffffff])
        if mode==7:arg=(0x2300000+rng.randrange(count)*0x100+4) if count else 0x2500000
        replay(mode,arg,match,mutation,count,queue,nodes)
    # Repeated valid zero-init -> full -> remove -> reuse. Preserve original state between operations.
    queue=bytes(256);count=0;nodes=bytes(24)
    for step in range(16):count,queue=replay(4,0x2300000+step*0x100,0,0,count,queue,nodes)
    for step in range(args.reuse):
        slot=step%16;resource=struct.unpack_from('<I',queue,slot*16)[0]
        count,queue=replay(7,resource+4,0,0,count,queue,nodes)
        assert count==15 and queue[240:]==bytes(16)
        count,queue=replay(4,0x2700000+step*0x100,0,0,count,queue,nodes)
        assert count==16 and queue[244:]==bytes(12)
    rig.u.hook_del(memhook);rig.clear_hooks()
    result=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',capture_output=True,text=True)
    assert result.returncode==0,result.stderr
    output=result.stdout.splitlines();assert len(output)==len(expected)
    for index,(actual,wanted) in enumerate(zip(output,expected)):
        assert actual==wanted,(index,rows[index][:5],'actual',actual,'expected',wanted)
    report={'source_sha256':HASH,'cases':len(rows),'differences':0,'mode_counts':modes,'repeated_full_queue_reuse':args.reuse,'actual_init_bytes_zeroed':0x1ec,'emulator_environment':'Flat32-bit CS/DS/ES/SS descriptors for actual formatter PUSH/POP ES; no formatter instruction hook.','bodies':['455C90 zero init','455F10','455FA0','456000','455F60','43F500','493A80 actual event formatter'],'foreign_fixtures':['49AD30 comparator results/current queue and registry mutation','49A620 full16-byte memset during queue remove/compact'],'comparison':'Full16physical queue cells/count, registry cells, ordered reads/writes/comparator/clear calls; production initialized-prefix vector checked separately from arbitrary raw tails.'}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=2400);p.add_argument('--reuse',type=int,default=512);p.add_argument('--report',type=Path);run(p.parse_args())
