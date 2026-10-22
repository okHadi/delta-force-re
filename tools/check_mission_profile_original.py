#!/usr/bin/env python3
"""Replay local DF1 profile recording, settings, and real x87 conversion."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
import unicorn
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW, UC_X86_REG_FPSW
from check_mission_results_original import Original, SOURCE_SHA

PROFILE=0x3000000
ALTERNATE=0x3002000
RESULT=0x67f714
STACK=0x2007000
RETURN=0x2009000
SOURCES=[0x67f4e4,0x67f4e8,0x67f4ec,0x67f4f0,0x67f4f4,0x67f4f8,0x67f4fc,0x67f500,0x67f504,0x67f510,0x67f514,0x67f508,0x67f50c,0x67f540,0x67f544,0x67f54c,0x67f548,0x67f534,0x67f550,0x67f554,0x67f558,0x67f55c,0x67f564]
TARGETS=[0x4ced24,0x4ced28,0x4ced2c,0x4ced30,0x4ced34,0x4ced38,0x4ced3c,0x4ced44,0x4ced40,0x4ced4c,0x4ced48,0x4cf2d0,0x4cf2d4,0x4cee80,0x4cee84,0x4cee88,0x4cee8c,0x4cee94,0x4cee98,0x4cee9c,0x4ceea0,0x4ceea4,0x4cf2e0]
CELLS=sorted(set(SOURCES+TARGETS+[0x67f520,0x67f528,0x67f53c,0x67f718,0x67f70c,0x67dbec,0x68ba34,0x503134,0x503130]))
CONTROLS=[0x7f|(pc<<8)|(rounding<<10) for pc in (0,2,3) for rounding in range(4)]
EDGES=[0,1,2,3,255,256,65535,65536,65537,0x7fffff,0x800000,0xffffff,0x1000000,0x1000001,0x1000003,0x7fffffff,0x80000000,0x80000001,0xff000001,0xffff0000,0xffffffff]


def native(probe,mode,rows):
    p=subprocess.run([str(probe),mode],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
    if p.returncode:raise RuntimeError(p.stderr)
    result=p.stdout.splitlines();assert len(result)==len(rows)
    return result


def compare(args,tag,rows,expected,actual):
    for case,(found,wanted) in enumerate(zip(actual,expected)):
        if found!=wanted:
            failure={'section':tag,'case':case,'input':rows[case],'actual':found,'expected':wanted}
            args.report.with_suffix('.failure.json').write_text(json.dumps(failure,indent=2)+'\n')
            raise AssertionError({'section':tag,'case':case,'actual':found[:160],'expected':wanted[:160]})


def run(args):
    args.report.parent.mkdir(parents=True,exist_ok=True)
    original=Original(Path('.'),args.game);u=original.machine();rng=random.Random(0x407de0)
    w=lambda at,n:original.w32(u,at,n)
    r=lambda at:original.r32(u,at)
    state={'frame':False,'trace':[],'mutation':0,'end_events':0,'helper_calls':0,'owner_switches':0}
    def end_events(machine,address,size,user):
        state['trace'].append('E;');state['end_events']+=1;mutation=state['mutation']
        if mutation&1:w(0x67f520,r(0x67f520)^1);w(RESULT+4,2 if r(RESULT+4)==1 else 1)
        if mutation&2:
            row=16-r(RESULT+0xc);w(RESULT+0xc,row);w(RESULT+0x28+row*0x24,r(RESULT+0x28+row*0x24)+17);w(RESULT+0x30+row*0x24,r(RESULT+0x30+row*0x24)^0x80010000)
        if mutation&4:
            w(PROFILE+0x28,r(PROFILE+0x28)%5+1);w(0x68ba34,(r(0x68ba34)+1)%16);w(PROFILE+0x658,0xfffffff0);w(PROFILE+0x674,0x7ffffff0)
        if mutation&8:
            w(0x503134,r(0x503134)^0x80000000);w(0x503130,r(0x503130)+17);w(0x67dbec,r(0x67dbec)^0xf0f0f0f0);w(0x67f4e4,r(0x67f4e4)^0x12345678);w(0x67f564,r(0x67f564)^0x87654321)
        sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_ESP,sp+4);machine.reg_write(UC_X86_REG_EIP,r(sp))
    def helper(machine,address,size,user):state['helper_calls']+=1
    stores=set(TARGETS+[0x67f4e8,0x67f4ec,0x67f70c])
    def store(machine,access,address,size,value,user):
        if state['frame'] and address in stores:
            state['trace'].append(f'S,{address},{value&0xffffffff};')
            if address==0x4cf2e0 and state['mutation']&16:w(0x4cf2e4,ALTERNATE);state['owner_switches']+=1
    u.hook_add(unicorn.UC_HOOK_CODE,end_events,begin=0x42eba0,end=0x42eba0)
    u.hook_add(unicorn.UC_HOOK_CODE,helper,begin=0x49bae6,end=0x49bae6)
    write_hook=None
    w(0x4cf2e4,PROFILE)
    u.mem_write(RETURN,b'\xc3')
    stat_rows=[];stat_expected=[];stat_corpus=[]
    for control in CONTROLS:
        for ratio in EDGES:
            for count in EDGES:stat_corpus.append((ratio,count,control))
        for count in range(256):
            for ratio in (0,1,65535,65536,65537,0x7fffffff,0x80000000,0xffff0000):stat_corpus.append((ratio,count,control))
        for _ in range(args.stat_random):stat_corpus.append((rng.getrandbits(32),rng.getrandbits(32),control))
    for ratio,count,control in stat_corpus:
        # Actual original product instructions and 49BAE6; stop after FISTP/profile addition.
        u.mem_write(STACK,bytes(0x520));w(STACK+0xc,0);w(STACK+0x28,count);w(STACK+0x30,ratio);w(PROFILE+0x658,0)
        u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPSW,0);u.reg_write(UC_X86_REG_FPCW,control)
        u.emu_start(0x407e75,0x407eb9,count=1000)
        assert u.reg_read(UC_X86_REG_EIP)==0x407eb9
        assert u.reg_read(UC_X86_REG_FPCW)==control,'49BAE6 must restore the caller control word'
        stat_rows.append([ratio,count,control]);stat_expected.append(str(r(PROFILE+0x658)))
    compare(args,'stat',stat_rows,stat_expected,native(args.probe,'--stat',stat_rows))
    write_hook=u.hook_add(unicorn.UC_HOOK_MEM_WRITE,store)
    rows=[];expected=[];modes={};row_slots=set();control_cases={};settings_store_count=0
    for case in range(args.cases):
        mode=case%3;mutation=rng.randrange(32);control=CONTROLS[(case//3)%len(CONTROLS)];values={at:rng.getrandbits(32) for at in CELLS}
        values.update({0x67f520:rng.randrange(2),0x67f528:rng.randrange(2),0x67f53c:rng.randrange(6),0x68ba34:rng.randrange(16)})
        profile=bytearray(rng.randbytes(0xa90));struct.pack_into('<I',profile,0x28,rng.randrange(1,6))
        image=bytearray(rng.randbytes(0x514));row=case%17;row_slots.add(row);struct.pack_into('<I',image,0xc,row);flag=rng.choice([0,1,2]);struct.pack_into('<I',image,4,flag);values[0x67f718]=flag
        if case%4==0:struct.pack_into('<I',image,0,flag)
        for i in range(17):
            struct.pack_into('<I',image,0x28+i*0x24,rng.choice(EDGES+[rng.randrange(256)]));struct.pack_into('<I',image,0x30+i*0x24,rng.choice(EDGES+[rng.getrandbits(32)]))
        row_in=[mode,mutation,control]+[values[at] for at in CELLS]+[profile.hex(),image.hex()];rows.append(row_in)
        state.update({'frame':False,'trace':[],'mutation':mutation})
        for at,n in values.items():w(at,n)
        alternate=bytearray(n^0xa5 for n in profile);struct.pack_into('<I',alternate,0x28,struct.unpack_from('<I',profile,0x28)[0]%5+1)
        u.mem_write(PROFILE,bytes(profile));u.mem_write(ALTERNATE,bytes(alternate));u.mem_write(RESULT,bytes(image));w(0x4cf2e4,PROFILE)
        w(STACK,RETURN);u.reg_write(UC_X86_REG_ESP,STACK);u.reg_write(UC_X86_REG_FPSW,0);u.reg_write(UC_X86_REG_FPCW,control);state['frame']=True
        entry=[0x407de0,0x407980,0x401e70][mode];u.emu_start(entry,RETURN,count=20000);state['frame']=False
        assert u.reg_read(UC_X86_REG_EIP)==RETURN
        assert u.reg_read(UC_X86_REG_FPCW)==control
        trace=''.join(state['trace']);settings_store_count+=sum(t.startswith('S,') for t in state['trace'])
        expected.append(' '.join([str(r(at)) for at in CELLS]+[u.mem_read(PROFILE,0xa90).hex(),u.mem_read(ALTERNATE,0xa90).hex(),str(int(r(0x4cf2e4)==ALTERNATE)),u.mem_read(RESULT,0x514).hex(),trace]))
        modes[hex(entry)]=modes.get(hex(entry),0)+1;control_cases[hex(control)]=control_cases.get(hex(control),0)+1
    compare(args,'profile',rows,expected,native(args.probe,'--frame',rows))
    report={'source_sha256':SOURCE_SHA,'profile_cases':len(rows),'stat_cases':len(stat_rows),'stat_80000000_results':stat_expected.count(str(0x80000000)),'profile_owner_switch_fixtures':state['owner_switches'],'bodies':modes,'control_cases':control_cases,'valid_row_slots':sorted(row_slots),'x87_controls':[hex(cw) for cw in CONTROLS],'helper_49bae6_actual_calls':state['helper_calls'],'end_event_fixtures':state['end_events'],'ordered_cell_stores':settings_store_count,'profile_bytes':0xa90,'result_bytes':0x514,'comparison':'Full profile image, result snapshot source, all settings/current cells and ordered stores; real x87 precision, rounding, truncation and FISTP32','foreign_fixture':'42EBA0 end-event changes and final-settings-store profile-owner switch; read-only providers have no effects','original_machine_instances':1,'native_files':{p:hashlib.sha256(Path(p).read_bytes()).hexdigest() for p in ['include/dfre/mission_results.hpp','src/mission_results.cpp','tools/mission_profile_probe.cpp']}}
    args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1200);p.add_argument('--stat-random',type=int,default=1000);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
