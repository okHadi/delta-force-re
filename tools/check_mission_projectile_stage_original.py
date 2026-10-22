#!/usr/bin/env python3
"""Replay DF1 listener, area queue and damage helpers with explicit fixtures."""
import argparse
import hashlib
import json
import random
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP
from check_projectile_hits_original import Replay, HASH, match, pack

BASES = {3: 0x7cca80, 0: 0x7dde80, 1: 0x867e80}
COUNTS = {3: 0x8cfa18, 0: 0x8cfa1c, 1: 0x8cf904}
LOCAL, OWNER, UPDATE, PENDING, PENDING_COUNT = 0x2201000, 0x2202000, 0x2203000, 0x9e2e40, 0xa15898


def put(image, at, n):image[at:at+4] = pack(n)


def row(case):
    result = [case[k] for k in ('host', 'rules', 'water', 'local_id', 'rng', 'alternate')]
    result += case['pose'] + [case['local_present'], case['local'].hex(), OWNER, case['owner'].hex(), len(case['definitions'])]
    for at, definition in case['definitions'].items():result += [at, *definition]
    result.append(len(case['bounds']))
    for at, bounds in case['bounds'].items():result += [at, *bounds]
    for group in (3, 1, 0):result += [len(case['actors'][group]), *[a.hex() for a in case['actors'][group]]]
    result += [len(case['pending']), *[n for p in case['pending'] for n in p]]
    result += [len(case['answers']), *case['answers'], case['mutation']]
    return result


def fixture(rng, number, queue=False):
    local, owner = bytearray(rng.randbytes(0x114)), bytearray(rng.randbytes(0x114))
    put(owner, 0x70, rng.randrange(3))
    case = dict(host=rng.choice([0, 1, 1, 1]), rules=rng.choice([0, 0x40]), water=rng.choice([0, 1, -1000]),
                local_id=rng.choice([0, BASES[3]]), rng=rng.getrandbits(32), alternate=0, pose=[0]*5,
                local_present=0, local=local, owner=owner, definitions={}, bounds={}, actors={3:[],1:[],0:[]},
                pending=[], answers=[rng.choice([-1,0,1,2,3]) for _ in range(256)], mutation=rng.randrange(64))
    for group in (3, 1, 0):
        count = rng.randrange(3) if queue else int(group == 3)
        for index in range(count):
            a = bytearray(rng.randbytes(0x114));definition=0x2240000 + group * 0x1000 + index * 0x100
            model = 0x2250000 + group * 0x1000 + index * 0x100
            put(a, 0, rng.choice([0, 1, 1, 1]));put(a, 4, definition);put(a, 0x10, model if number % 17 else 0)
            put(a, 0x110, UPDATE);put(a, 0x24, rng.randint(-0xc0000, 0xc0000) if queue else rng.randint(-0x20000,0x20000))
            put(a, 0x28, 0);put(a, 0x2c, rng.randint(-0x40000, 0x40000));put(a, 0x70, rng.randrange(3));put(a, 0x78, rng.choice([0,0,0,1]))
            a[0x6c]=rng.choice([0,1,4,0x80]);case['actors'][group].append(a)
            case['definitions'][definition] = [rng.choice([0,3,3]), rng.choice([0,0xfd0])]
            case['bounds'][model] = [rng.choice([0,0x10000,0x40000]), rng.choice([0,0x8000,0x40000])]
    for index in range(rng.randrange(5) if queue else 1):
        case['pending'].append([rng.randint(-0x20000,0x20000),0,rng.randint(-0x20000,0x20000),
                                rng.choice([0,1,2,3,4,5,6,-1]) if queue else rng.choice([1,3,5]),rng.choice([0,OWNER,BASES[3]])])
    return case


def run(args):
    source = (args.game/'Df.exe').read_bytes()
    assert hashlib.sha256(source).hexdigest() == HASH
    r=Replay(source);rng=random.Random(0x465bd0);counts={}
    totals={'queries':0,'updates':0,'local_hits':0,'listener':0,'queue_appends':0,'rng_draws':0}

    def setup(case):
        r.write(0x67f528,case['host']);r.u.mem_write(0x67f549,bytes([case['rules']]))
        r.write(0xbd5248,case['water']);r.write(0xc23820,case['local_id']);r.write(0xbd0060,case['rng'])
        r.write(0x6ba3ec,case['alternate']);r.write(0x6ba3f4,*case['pose'])
        r.u.mem_write(LOCAL,bytes(case['local']));r.u.mem_write(OWNER,bytes(case['owner']))
        for at, definition in case['definitions'].items():
            r.write(at+0x28,definition[0]);r.write(at+0x20,definition[1])
        for at,bounds in case['bounds'].items():r.write(at+0x64,*bounds)
        for group in (3,1,0):
            r.write(COUNTS[group],len(case['actors'][group]))
            for index,a in enumerate(case['actors'][group]):r.u.mem_write(BASES[group]+index*0x114,bytes(a))
        r.write(PENDING_COUNT,len(case['pending']))
        for index,p in enumerate(case['pending']):r.write(PENDING+index*20,*p)
        trace=[];queries=[0];appended=[False]
        def dump():
            output=str(r.read(0xbd0060)[0])
            for group in (3,1,0):
                count=r.read(COUNTS[group])[0];output+=f'|{count}'
                for index in range(count):output+=','+r.u.mem_read(BASES[group]+index*0x114,0x114).hex()
            count=r.read(PENDING_COUNT)[0];output+=f'|{count}'
            for index in range(count):output+=''.join(f',{n}' for n in r.read(PENDING+index*20,5))
            return output+'|'+r.u.mem_read(LOCAL,0x114).hex()
        def snapshot(tag):trace.append(tag+':'+dump()+';')
        def segment():
            actor,start,skip,end=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,4);assert skip==0
            snapshot('q,'+','.join(map(str,[actor,*r.read(start,3),*r.read(end,3)])))
            if case['mutation']&2:r.write(0xbd0060,r.read(0xbd0060)[0]^0x12345678)
            if case['mutation']&32 and r.read(PENDING_COUNT)[0]:r.write(PENDING+12,1)
            r.u.reg_write(UC_X86_REG_EAX,case['answers'][queries[0]]&0xffffffff);queries[0]+=1;totals['queries']+=1
        def update():
            actor,message=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,2);snapshot(f'u,{UPDATE},{actor},{message}');totals['updates']+=1
            if case['mutation']&4:r.u.mem_write(actor+0x6c,bytes([r.u.mem_read(actor+0x6c,1)[0]|4]))
            if case['mutation']&8:r.write(actor+0x74,0x11223344)
            if case['mutation']&16 and r.read(PENDING_COUNT)[0]:r.write(PENDING+12,1)
            if case['mutation']&1 and not appended[0]:
                appended[0]=True;n=r.read(PENDING_COUNT)[0];r.write(PENDING+n*20,0,0,0,4,0);r.write(PENDING_COUNT,n+1);totals['queue_appends']+=1
        def local_hit():snapshot('l');totals['local_hits']+=1
        def listener():
            values=r.read(r.u.reg_read(UC_X86_REG_ESP)+4,5);snapshot('v,'+','.join(map(str,values)));totals['listener']+=1
        r.hook(0x42e2b0,segment);r.hook(UPDATE,update);r.hook(0x484a20,local_hit);r.hook(0x43eba0,listener)
        import unicorn
        def observe_rng(u,at,size,data):totals['rng_draws']+=1
        r.hooks.append(r.u.hook_add(unicorn.UC_HOOK_CODE,observe_rng,begin=0x477860,end=0x477860))
        r.u.ctl_remove_cache(0x477860,0x477861)
        return dump,trace,queries

    cases,expected=[],[]
    for index in range(args.cases):
        case=fixture(rng,index);case['actors']={3:[],1:[],0:[]};case['pending']=[];case['definitions']={};case['bounds']={}
        case['alternate']=rng.choice([0,1,2]);case['pose']=[rng.getrandbits(32) for _ in range(5)]
        case['local_present']=rng.randrange(2);case['local_id']=LOCAL if case['local_present'] else 0
        dump,trace,queries=setup(case);r.call(0x45e050)
        cases.append(row(case));expected.append(f'{dump()} {queries[0]} {"".join(trace) or "-"}');r.clear_hooks()
    match(args.probe,'listener',cases,expected);counts['45E050']=len(cases)

    cases,expected=[],[]
    sites=[0x465830,0x465a60,0x465b30]
    distances=[-0x80000000,-1,0,0x3ffff,0x40000,0x7ffff,0x80000,0x100000,0x7fffffff]
    for index in range(args.cases):
        case=fixture(rng,index);site=sites[index%3];distance=distances[index//3%len(distances)]
        dump,trace,queries=setup(case);r.call(site,BASES[3],PENDING,distance)
        cases.append(row(case)+[site,3,0,0,distance]);expected.append(f'{dump()} {queries[0]} {"".join(trace) or "-"}');r.clear_hooks()
    match(args.probe,'apply',cases,expected);counts['465830_465A60_465B30']=len(cases)

    cases,expected=[],[]
    for index in range(args.cases):
        case=fixture(rng,index,True)
        incoming_range=rng.choice([0,0x10000,0xa0000]);incoming_site=rng.choice(sites)
        dump,trace,queries=setup(case)
        # The original default switch path consumes EDI and its uninitialized
        # local callback slot. Supply both as explicit incoming fixtures.
        r.write(0x2008000-0x18,incoming_site)
        r.u.mem_write(0x22f0000,b'\xbf'+pack(incoming_range)+b'\xe9'+pack(0x465bd0-(0x22f0000+10)))
        r.u.ctl_remove_cache(0x22f0000,0x22f0010);r.call(0x22f0000)
        cases.append(row(case)+[1,incoming_range,incoming_site]);expected.append(f'{dump()} {queries[0]} {"".join(trace) or "-"}');r.clear_hooks()
    match(args.probe,'queue',cases,expected);counts['465BD0']=len(cases)
    report={'source_sha256':HASH,'cases':counts,'call_counts':totals,
            'foreign_fixtures':['42E2B0 segment results','actor +110 updates','484A20 local-hit effect','43EBA0 listener effect',
                                'initial invalid-type dispatch EDI and stack callback'],
            'angle_coverage':'area origins and target Y are equal; facing helper has separate native original-instruction checks'}
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('game',type=Path);parser.add_argument('probe',type=Path)
    parser.add_argument('--cases',type=int,default=1500);parser.add_argument('--report',type=Path);run(parser.parse_args())
