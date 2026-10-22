#!/usr/bin/env python3
"""Compare sound requests and bank selection with the read-only local DF1 image."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
from pathlib import Path
from unicorn import UC_HOOK_CODE, UcError
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EDX
from check_mission_results_original import Original,SOURCE_SHA

GRAPH=0x3100000
STACK=0x2008000
PARAM=STACK-0x30
CONTEXT=0x6e0518
SCRATCH=0xd099c0
RETURN=0x200d000
SITES=(0x499120,0x49d050,0x499020,0x498230,0x494dd0,0x494e80)


def run(args):
    args.report.parent.mkdir(parents=True,exist_ok=True);rng=random.Random(0x43e72049d5a0)
    original=Original(Path('.'),args.game);u=original.machine();u.mem_map(GRAPH,0x10000);u.mem_write(RETURN,b'\xc3')
    read=lambda at,width=4:int.from_bytes(u.mem_read(at,width),'little')
    def put(at,n,width=4):u.mem_write(at,int(n&((1<<(width*8))-1)).to_bytes(width,'little'))
    def argv(n):return struct.unpack('<'+'I'*n,u.mem_read(u.reg_read(UC_X86_REG_ESP)+4,n*4))
    state={};counts={};faults=[];rows=[];expected=[]
    def effect(site):
        arity={0x499120:1,0x49d050:2,0x499020:0,0x498230:2,0x494dd0:4,0x494e80:5}[site];a=list(argv(arity));state['calls']+=1;state['trace'].append(','.join(map(str,[site,*a]))+';');counts[hex(site)]=counts.get(hex(site),0)+1
        mutation,params,context,id=state['mutation'],state['params'],state['context'],state['id']
        if site==0x49d050:
            state['asset']=a[0];put(a[0]+0x23,1,1);put(a[0]+0x24,0x70000000+a[0]-GRAPH)
            if mutation&1:
                put(params,0x18000);put(params+4,7);put(params+8,255);put(GRAPH+0x108,read(GRAPH+0x108)^1);put(GRAPH+0x1c,read(GRAPH+0x1c)+0x100);put(context+0xc0,read(context+0xc0)^0x80);put(0x501500,read(0x501500)^0x20)
        if site==0x499020 and mutation&2:
            put(state['asset']+0x24,read(state['asset']+0x24)+0x4000);put(params+0xc,5);put(GRAPH+0x24,min(read(GRAPH+0x24),2));put(context+0xc4,123)
        if site==0x498230 and mutation&4:put(params+0x14,1)
        if site in (0x494dd0,0x494e80):
            sample=a[0 if site==0x494dd0 else 1];slot=state['voice']
            while slot<8 and read(SCRATCH+slot*0x14+4)!=sample:slot+=1
            state['voice']=slot+1
            if mutation&8:
                put(params+0xc,3 if read(params+0xc)==0xffffffff else 0xffffffff);put(params+8,7)
                if slot+1<8:
                    at=SCRATCH+(slot+1)*0x14;put(at+4,0x7654);put(at+8,0x10000);put(at+0xc,128);put(at+0x10,13)
            if mutation&16:put(context+0xc0,0xf000);put(0x4f2ea8+id*0x28,0)
        sp=u.reg_read(UC_X86_REG_ESP);ret=read(sp);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EAX,0x90000000+state['calls']);u.reg_write(UC_X86_REG_EIP,ret)
    for site in SITES:u.hook_add(UC_HOOK_CODE,lambda uc,at,size,user,site=site:effect(site),begin=site,end=site)
    def attenuation_hook(uc,address,size,user):
        a=argv(4);counts['49DC20_actual']=counts.get('49DC20_actual',0)+1
        if a[2]>255 or a[3]>255:counts['49DC20_wide_fields']=counts.get('49DC20_wide_fields',0)+1
    u.hook_add(UC_HOOK_CODE,attenuation_hook,begin=0x49dc20,end=0x49dc20)
    modes={};alias_cases=0;missing_banks=0
    cases=[(i%2,[0,0,8,8,1,2,4,16][i//2]) for i in range(16)]
    for i in range(args.cases):cases.append((i%2,rng.randrange(32)))
    # Every helper is also run directly, with full-word inputs and RNG seeds.
    for i in range(256):
        for mode in (2,3,4):cases.append((mode,0))
    for number,(mode,mutation) in enumerate(cases):
        id=rng.choice([0,1,356,357,432,499,511]);bank=GRAPH if mode<2 else rng.choice([0,1,2,255,256,65535,0x7fffffff,0x80000000,0xffffffff,rng.getrandbits(32)])
        params=PARAM if mode!=1 or number<16 else rng.choice([PARAM,SCRATCH,CONTEXT+0x50]);alias_cases+=int(mode==1 and params!=PARAM)
        definition=bytearray(rng.randbytes(40));struct.pack_into('<I',definition,0,GRAPH if rng.randrange(5) else 0);missing_banks+=int(mode==0 and not struct.unpack_from('<I',definition)[0]);definition[0x20]=rng.randrange(8)
        context=CONTEXT+(0xc8 if definition[0x20]&4 else 0)
        globals=bytearray(rng.randbytes(0x408));struct.pack_into('<I',globals,0,rng.choice([0,1,0xffffffff,rng.getrandbits(32)]));struct.pack_into('<I',globals,4,rng.randrange(64));globals[8:]=u.mem_read(0x501504,1024)
        contexts=bytearray(rng.randbytes(0x190));scratch=bytearray(rng.randbytes(0xa0));parameter=bytearray(rng.randbytes(32));graph=bytearray(rng.randbytes(0x2000))
        for at in (0,0xc8):struct.pack_into('<I',contexts,at+0xc0,rng.choice([0,1,128,255,256,0xffffffff]));struct.pack_into('<I',contexts,at+0xc4,rng.getrandbits(32))
        layer_count=rng.randrange(9);struct.pack_into('<I',graph,0x24,layer_count);graph[0x4c]=rng.randrange(2);struct.pack_into('<I',graph,0x1c,rng.choice([0,1,0x10000,0x80000000,0xffffffff]));struct.pack_into('<I',graph,0x20,rng.choice([0,1,100,0x7fffffff,0x80000000,0xffffffff]))
        for i in range(8):
            layer=0x100+i*0x40;struct.pack_into('<I',graph,0x28+i*4,GRAPH+layer);variants=rng.randrange(5);struct.pack_into('<HH',graph,layer,variants,rng.randrange(max(variants,1)));struct.pack_into('<I',graph,layer+4,rng.choice([0,1,100,0x7fff,0x8000,0x10000]));struct.pack_into('<I',graph,layer+8,rng.randrange(64))
            for j in range(4):
                variant=0x400+i*0x100+j*0x20;asset=0x1000+i*0x100+j*0x30;struct.pack_into('<I',graph,layer+0x10+j*4,GRAPH+variant)
                struct.pack_into('<5I',graph,variant,GRAPH+asset if rng.randrange(5) else 0,rng.choice([0,1,0x10000,0x7fffffff,0xffffffff]),rng.choice([0,1,32,0x7fffffff,0x80000000,0xffffffff]),rng.choice([0,1,255,256,0x7fffffff,0x80000000,0xffffffff]),rng.choice([0,1,127,255,256,0x7fffffff,0x80000000,0xffffffff]))
                graph[asset+0x23]=rng.randrange(2);struct.pack_into('<I',graph,asset+0x24,0x70000000+asset)
        for at,n in [(0,rng.choice([0,1,0x10000,0xffffffff])),(4,rng.randrange(256)),(8,rng.choice([0,127,255,256,0xffffffff])),(0xc,rng.choice([0xffffffff,0,3])),(0x14,rng.choice([0,1,0x7fffffff,0x80000000,0xffffffff]))]:struct.pack_into('<I',parameter,at,n)
        if number<16:
            struct.pack_into('<I',definition,0,GRAPH);struct.pack_into('<I',globals,4,2);struct.pack_into('<I',graph,0x24,8);graph[0x4c]=0
            for i in range(8):
                layer=0x100+i*0x40;struct.pack_into('<HHII',graph,layer,1,0,0,0x13)
                variant=0x400+i*0x100;asset=0x1000+i*0x100;struct.pack_into('<I',graph,variant,GRAPH+asset);graph[asset+0x23]=i%2
            struct.pack_into('<4I',parameter,0,0x10000,0,255,0xffffffff)
            for at in (0,0xc8):struct.pack_into('<I',contexts,at+0xc0,0)
        regions=[(0x4f2ea8+id*0x28,definition),(0x5014fc,globals),(CONTEXT,contexts),(SCRATCH,scratch),(PARAM,parameter),(GRAPH,graph)]
        if mode==1 and params!=PARAM:
            for at,data in regions:
                if at<=params and params+32<=at+len(data):data[params-at:params-at+32]=parameter
        row=[mode,id,bank,params,context,mutation]+[bytes(data).hex() for at,data in regions];rows.append(row)
        for at,data in regions:u.mem_write(at,bytes(data))
        state.update({'calls':0,'trace':[],'mutation':mutation,'params':params,'context':context,'id':id,'asset':0,'voice':0})
        entry=[0x43e720,0x49d5a0,0x49d4c0,0x49d560,0x49d580][mode];args_in=[id] if mode==0 else [bank,params,context] if mode==1 else [bank]
        sp=STACK if mode==0 else STACK-0x1000;u.mem_write(sp,struct.pack('<I',RETURN)+b''.join(struct.pack('<I',n) for n in args_in));u.reg_write(UC_X86_REG_ESP,sp)
        fault=False
        try:
            u.emu_start(entry,RETURN,count=200000)
            if u.reg_read(UC_X86_REG_EIP)!=RETURN:raise RuntimeError("Original sound body did not return")
        except (UcError,RuntimeError):
            eip=u.reg_read(UC_X86_REG_EIP)
            if eip!=0x49dc44:raise
            numerator=(u.reg_read(UC_X86_REG_EDX)<<32)|u.reg_read(UC_X86_REG_EAX)
            if numerator&(1<<63):numerator-=1<<64
            from unicorn.x86_const import UC_X86_REG_EBP
            denominator=read(u.reg_read(UC_X86_REG_EBP)+0x18)
            if denominator&0x80000000:denominator-=1<<32
            quotient=abs(numerator)//abs(denominator) if denominator else 0
            if numerator*denominator<0:quotient=-quotient
            assert not denominator or not -2147483648<=quotient<=2147483647
            faults.append({'case':number,'divisor':denominator,'quotient':quotient});fault=True
        result=0 if mode==0 or fault else u.reg_read(UC_X86_REG_EAX)
        expected.append(' '.join([str(result),str(int(fault))]+[u.mem_read(at,len(data)).hex() for at,data in regions]+[''.join(state['trace'])]));modes[hex(entry)]=modes.get(hex(entry),0)+1
    p=subprocess.run([str(args.probe)],input='\n'.join(' '.join(map(str,row)) for row in rows)+'\n',text=True,capture_output=True)
    if p.returncode:raise RuntimeError(p.stderr)
    actual=p.stdout.splitlines();assert len(actual)==len(expected)
    for index,(found,wanted) in enumerate(zip(actual,expected)):
        if found!=wanted:
            args.report.with_suffix('.failure.json').write_text(json.dumps({'case':index,'input':rows[index],'actual':found,'expected':wanted},indent=2)+'\n')
            a,b=found.split(),wanted.split();field=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
            raise AssertionError({'case':index,'mode':rows[index][0],'field':field,'actual':a[field][:200] if field<len(a) else '<end>','expected':b[field][:200] if field<len(b) else '<end>'})
    report={'source_sha256':SOURCE_SHA,'cases':len(rows),'bodies':modes,'foreign_calls':counts,'alias_cases':alias_cases,'null_global_banks':missing_banks,'wide_level_attenuation_calls':counts.get('49DC20_wide_fields',0),'attenuation_original_divide_faults':faults,'comparison':'Full mutable bank graph, definitions, RNG/filter/table cells, both contexts,32-byte parameters,8candidate scratch and ordered calls','foreign_fixtures':[hex(site) for site in SITES],'real_helper':'49DC20 full-DWORD attenuation and shared501504 coefficient table','device':'none','original_machine_instances':1}
    args.report.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({**report,'attenuation_original_divide_faults':len(faults)}))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('probe',type=Path);p.add_argument('--cases',type=int,default=1200);p.add_argument('--report',type=Path,required=True);run(p.parse_args())
