#!/usr/bin/env python3
"""Replay DF1 message, audio-upkeep and timer instructions without audio output."""
import argparse
import json
import random
import struct
import zlib
from pathlib import Path
from unicorn import UcError
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ESI, UC_X86_REG_ECX, UC_X86_REG_EBX, UC_X86_REG_EIP
from check_projectiles_original import Rig, SOURCE_SHA256
from check_projectile_hits_original import match

KEYS=[0x5012e0,0x5012d4,0x5012d8,0x5012ec,0x49101c,0x5012e4,0x5012dc,0x5012f8,0xd08948,0x5012f4,0xd08954,0xd0894c,0xd08940,0x501950,0x1300010,0x501980,0x501974,0x50198c,0xcc0884,0xcc08bc,0x491014,0x491018,0x491020,0x491028,0x491024,0xcc0868,0x49102c,0x491010,0x49100c]
FUNCTIONS=[0x4906a0,0x493f70,0x49fe70,0x495240,0x490880,0x490750]
IMPORTS={0x4c67e8:0x1301000,0x4c6804:0x1301020,0x4c6684:0x1301040,0x4c66bc:0x1301060,0x4c662c:0x1301080,0x4c6594:0x13010a0,0x4c658c:0x13010c0,0x4c65a0:0x13010e0}

def run(args):
    rig=Rig(args.game);rng=random.Random(0x490880);rows=[];expected=[];counts=[0]*6;faults=0;pcm_changes=0;faults_by_mode=[0]*6
    def read(at):return rig.peek(at)&0xffffffff
    def put(at,n):rig.poke_u(at,n)
    def arguments(n):return struct.unpack('<%dI'%n,rig.u.mem_read(rig.u.reg_read(UC_X86_REG_ESP)+4,n*4))
    def finish(value=0,pop=0):
        rig.finish(value)
        rig.u.reg_write(UC_X86_REG_ESP,rig.u.reg_read(UC_X86_REG_ESP)+pop*4)
    for mode in range(6):
      for case in range(args.cases):
        mutation=rng.randrange(2048)
        state={key:rng.getrandbits(32) for key in KEYS}
        state.update({0x5012e0:rng.choice([0,1,2]),0x5012d4:rng.choice([0,1,1]),0x5012d8:rng.choice([0,0,1]),0x5012ec:rng.choice([0,100,0xffffffff]),0x49101c:rng.choice([0,100,101,2400,2401,0xffffffff]),0x5012e4:rng.choice([0,0x1300100]),0x5012f8:rng.choice([0,1,1]),0xd08948:rng.choice([0,4,10]),0x5012f4:rng.choice([0,14,15,0xffffffff]),0xd08954:rng.choice([8,16,16]),0xd0894c:17,0xd08940:rng.choice([0,1,1]),0x501950:rng.choice([0,0x1300000,0x1300000]),0x1300010:rng.choice([3,4,4,5]),0x501980:rng.choice([0,1,1]),0x501974:rng.choice([0,1,0xfffff69f]),0xcc0884:rng.choice([0,1]),0xcc08bc:rng.choice([0,1]),0xcc0868:rng.choice([0,1,2,10,1001,2000,0x80000000,0xffffffff])})
        read_count=rng.choice([0,1,2,3,4,7,8,12,28,0x4000,0x8000] if mode==3 else [0,4,8,12])
        # Source ReadFile cannot return more than the requested capacity.
        if state[0xd08954]!=16:read_count=min(read_count,0x4000)
        if mode==3 and case%4==0:state.update({0x5012f8:1,0xd08948:0,0xd08940:1})
        if mode==1 and case%3==0:state.update({0x5012e0:1,0x5012d4:1,0x5012d8:0,0x5012ec:0,0x49101c:1,0x5012e4:0x1300100})
        if mode==2 and case%3==0:state.update({0x501950:0x1300000,0x1300010:4,0x501980:1,0x501974:0xfffff69f,0x49101c:0})
        read_success=rng.choice([0,1,1])
        if mode==3 and case%10 in (1,2):
            state.update({0x5012f8:1,0xd08948:0,0xd08940:1,0xd08954:16});read_count=2 if case%10==1 else 6;read_success=1;mutation &= ~256
        if mode==5 and case%10==0:state[0xcc0868]=2000;mutation &= ~(64|128)
        queue_limit=rng.randrange(3);message_count=rng.randrange(5);quit_index=rng.choice([0,1,2,0xffffffff]);cap_min=rng.choice([0,1,2,10]);cap_max=rng.choice([0,1,5,100])
        rows.append([mode,mutation,*[state[k] for k in KEYS],read_count,read_success,queue_limit,message_count,quit_index,cap_min,cap_max])
        memory=bytes((i*37+11)&255 for i in range(0x48000));rig.u.mem_write(0xcc0930,memory)
        for key,n in state.items():put(key,n)
        put(0x491030,99)
        for slot,target in IMPORTS.items():put(slot,target)
        trace=[];queues=0;reads=0;messages=0;quit_seen=False;pcm_fault=False
        def event(site,values):trace.append('C'+str(site)+''.join(','+str(n) for n in values)+';')
        def cd_callback():
            event(0x1300100,arguments(1))
            if mutation&1:put(0x5012e0,0)
            if mutation&2:put(0x5012dc,91);put(0x5012e0,7)
            finish()
        def cd_play():
            event(0x493cd0,arguments(2))
            if mutation&4:put(0x5012f8,1)
            finish()
        def midi_stop():
            event(0x49efd0,[])
            if mutation&8:put(0x501950,0);put(0x50198c,91)
            finish()
        def midi_restart():
            event(0x49ef20,[])
            if mutation&16:put(0x50198c,17)
            finish()
        def queue():
            nonlocal queues
            event(0x4b5243,[rig.u.reg_read(r) for r in (UC_X86_REG_ESI,UC_X86_REG_ECX,UC_X86_REG_EBX)]);queues+=1
            if mutation&32:put(0xd08954,16)
            finish(int(queues<=queue_limit))
        def read_file():
            nonlocal reads,pcm_fault
            file,buffer,capacity,countptr,overlap=arguments(5);assert overlap==0
            trace.append(f'R{file},{buffer},{capacity};');reads+=1
            if reads>1:put(countptr,0);finish(1,5);return
            assert read_count<=capacity
            rig.u.mem_write(buffer,bytes((i*19+3)&255 for i in range(read_count)));put(countptr,read_count)
            if mutation&256:put(0xd08954,8 if read(0xd08954)==16 else 16);put(0xd0894c,87)
            finish(read_success,5)
        def seek_file():
            file,offset,high,origin=arguments(4);assert high==origin==0
            trace.append(f'S{file},{offset};')
            if mutation&512:put(0x5012f4,14)
            finish(0,4)
        def peek():
            nonlocal messages,quit_seen
            dst,window,low,high,remove=arguments(5);assert (window,low,high,remove)==(0,0,0,1)
            trace.append('P;')
            if messages>=message_count:finish(0,5);return
            kind=0x12 if messages==quit_index else 0x100+messages;messages+=1;put(dst+4,kind);quit_seen|=kind==0x12
            finish(1,5)
        def translate():trace.append('T'+str(read(arguments(1)[0]+4))+';');finish(0,1)
        def dispatch():
            trace.append('D'+str(read(arguments(1)[0]+4))+';')
            if mutation&1024:put(0x5012e0,0)
            finish(0,1)
        def caps():
            dst,size=arguments(2);assert size==8;trace.append('G;');put(dst,cap_min);put(dst+4,cap_max);finish(0,2)
        def begin():
            event(0x4c658c,arguments(1))
            if mutation&64:put(0xcc0868,1)
            finish(0,1)
        def timer():
            event(0x4c65a0,arguments(5))
            if mutation&128:put(0xcc0868,2)
            finish(123,5)
        def guard_pair_loop():
            nonlocal pcm_fault
            # ECX is the actual output count immediately after the source loads it.
            n=rig.u.reg_read(UC_X86_REG_ECX)
            if not n or n&1:pcm_fault=True;rig.u.emu_stop()
        rig.stubs.update({0x1300100:cd_callback,0x493cd0:cd_play,0x49efd0:midi_stop,0x49ef20:midi_restart,0x4b5243:queue,0x1301000:read_file,0x1301020:seek_file,0x1301040:peek,0x1301060:translate,0x1301080:dispatch,0x13010a0:caps,0x13010c0:begin,0x13010e0:timer,0x49532a:guard_pair_loop})
        fault=False
        try:rig.call(FUNCTIONS[mode],*([0]*5 if mode==0 else []))
        except (UcError,RuntimeError):
            if pcm_fault:fault=True
            elif mode==5 and rig.u.reg_read(UC_X86_REG_EIP)==0x4907e6:
                signed=read(0xcc0868);signed=signed if signed<0x80000000 else signed-0x100000000
                q=(abs(0x78000000*signed)//1000)*(-1 if signed<0 else 1)
                assert q< -0x80000000 or q>0x7fffffff;fault=True
            else:raise
        counts[mode]+=1;faults+=int(fault);faults_by_mode[mode]+=int(fault)
        after=bytes(rig.u.mem_read(0xcc0930,0x48000));pcm_changes+=after!=memory
        expected.append(' '.join(map(str,[int(fault),int(not quit_seen),*[read(k) for k in KEYS],read(0x491030),zlib.crc32(after)]))+' '+(''.join(trace) or '-'))
    match(args.probe,'',rows,expected)
    report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'mismatches':0,'cases_by_clock_cd_midi_pcm_pump_configure':counts,'source_fault_boundaries':faults,'faults_by_clock_cd_midi_pcm_pump_configure':faults_by_mode,'pcm_pair_loop_guard':'stop before first write when actual ECX is zero or odd; original loop cannot terminate before leaving its buffers','changed_pcm_buffers':pcm_changes,'comparison':'all shared cells, whole PCM input/ring CRC32, ordered calls, callback mutations, quit exit, timer wrap/carry/configure','original_bodies':['490880','493F70','495240','495120','49FE70','4906A0','490750'],'foreign_fixtures':['PeekMessageA','TranslateMessage','DispatchMessageA','ReadFile','SetFilePointer','timeGetDevCaps','timeBeginPeriod','timeSetEvent','CD callback/493CD0','MIDI49EFD0/49EF20','PCM4B5243'],'audio_output':'none'}
    if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=120);p.add_argument('--report',type=Path);run(p.parse_args())
