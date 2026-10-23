#!/usr/bin/env python3
"""Compare 404CF0 reads and lookup calls with the supplied executable."""
import argparse
import json
import struct
import subprocess
from pathlib import Path
from unicorn import UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_EAX
from check_projectiles_original import Rig, SOURCE_SHA256

def run(args):
    rows=subprocess.run([str(args.probe.resolve())],input=''.join(f'{i}\n' for i in range(args.cases)),capture_output=True,text=True,check=True).stdout.splitlines()
    assert len(rows)==args.cases
    rig=Rig(args.game)
    rig.u.mem_map(0x2100000,0x1000)
    def get(at):return int.from_bytes(rig.u.mem_read(at,4),'little')
    def put(at,value):rig.u.mem_write(at,struct.pack('<I',value&0xffffffff))
    trace=[]
    seed=entry=0
    def memory(machine,access,at,width,value,user):
        if at in (0x503230,0x67dbc9,entry):
            trace.append(f'R{at},{width};')
            if at==0x67dbc9 and seed%3==0:put(0x503230,get(0x503230)^0xff00)
    def lookup():
        sp=rig.u.reg_read(UC_X86_REG_ESP)
        table,group,key=struct.unpack('<3I',rig.u.mem_read(sp+4,12))
        trace.append(f'C{table},{group},{key};')
        if seed%13==0:put(entry if entry else 0x2100000,get(0x2100000)^0xffff0000)
        rig.finish(entry)
    rig.stubs[0x49a092]=lookup
    rig.u.hook_add(UC_HOOK_MEM_READ,memory)
    differences=[]
    for seed,row in enumerate(rows):
        table=0x2000000+seed*4 if seed%5 else 0
        entry=0x2100000 if seed%7 else 0
        text=0x2200000+seed*16 if seed%11 else 0
        put(0x503230,table);put(0x67dbc9,(seed*17)&255);put(0x2100000,text);trace.clear()
        rig.call(0x404cf0,0x12345678^seed,0x87654321+seed)
        expected=f'{rig.u.reg_read(UC_X86_REG_EAX)} {get(0x503230)} '+''.join(trace)
        if row!=expected:differences.append({'seed':seed,'expected':expected,'port':row})
        if len(differences)>=3:break
    report={'source_sha256':SOURCE_SHA256,'cases':len(rows),'differences':len(differences),'details':differences,'scope':'404CF0 full return, table cell, exact read widths/order and required49A092 arguments; dictionary body is a synchronous fixture'}
    if args.report:args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='details'}))
    if differences:raise SystemExit(json.dumps(differences,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1024);p.add_argument('--report',type=Path);run(p.parse_args())
