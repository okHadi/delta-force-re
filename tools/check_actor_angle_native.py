#!/usr/bin/env python3
"""Check actor angles with original instructions on an Intel Mac or Linux host.

Use the original function bytes. Adapt the argument registers, 64-bit stack
slots, branch locations, and constant relocations for the host ABI. Execute
all integer branches and original x87 instructions, including 49BAE6.
Requires pefile, capstone, and clang++. Keep generated code in a temporary path.
"""
import argparse
import decimal
import fractions
import hashlib
import json
import platform
import random
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
import capstone
import pefile

SHA = 'abbdf55a1d189b8eb6c1c464da7bdd08ec01f6b0ccd72182ab43bb01b7f651ff'

def source_cases():
    rng = random.Random(477770)
    edges = [-2147483648, -2147483647, -1025, -1024, -1023, -1, 0, 1, 1023, 1024, 1025, 2147483646, 2147483647]
    cases = [(0, 0)] + [(x, y) for x in edges for y in edges]
    cases += [(rng.randrange(-2147483648, 2147483648), rng.randrange(-2147483648, 2147483648)) for _ in range(20000)]
    with decimal.localcontext() as ctx:
        ctx.prec = 80
        d = decimal.Decimal
        for _ in range(512):
            n = rng.randrange(1 << 22, (1 << 30) - (1 << 22))
            theta = d.from_float(3.1415926539) * d(n) * 2 / d(4294967295)
            st, ct = theta, d(1)
            sn, cs = st, ct
            for k in range(1, 100):
                st = -st * theta * theta / d((2*k)*(2*k+1))
                ct = -ct * theta * theta / d((2*k-1)*(2*k))
                sn += st; cs += ct
            ratio = sn / cs
            bound = min(2147483647, int(d(2147483647) / ratio))
            f = fractions.Fraction(ratio).limit_denominator(bound)
            for delta in [-1, 0, 1]:
                if 0 <= f.numerator + delta <= 2147483647:
                    cases.append((f.denominator, f.numerator + delta))
    return cases

def assembly(pe):
    prefix = '_' if sys.platform == 'darwin' else ''
    symbol = prefix + 'original_angle_native'
    lines = ['.text', '.globl ' + symbol, symbol + ':']
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    for start, length in [(0x477770, 0xaa), (0x49bae6, 0x1d)]:
        for ins in cs.disasm(pe.get_data(start-0x400000, length), start):
            address, data = ins.address, ins.bytes
            lines.append(f'L{address:x}:')
            if address == 0x477775:
                data = bytes.fromhex('89f99090')  # ECX receives SysV EDI.
            elif address == 0x477779:
                data = bytes.fromhex('90909090')  # SysV ESI holds argument 2.
            elif ins.mnemonic in ['sub', 'add'] and ins.op_str.startswith('esp,'):
                data = b'\x48' + data
            elif address == 0x49baf9:
                data = bytes.fromhex('d96c2408')  # Saved CW above 64-bit PUSH.
            elif address == 0x49bafe:
                data = bytes.fromhex('488d642410')  # Release two 64-bit pushes.
            elif ins.mnemonic in ['fmul', 'fdiv'] and '[0x4cd5' in ins.op_str:
                constant = int(ins.op_str.split('[0x')[1].split(']')[0], 16)
                lines += ['.byte '+','.join(hex(x) for x in data[:2]), f'.long C{constant:x} - . - 4']
                continue
            elif ins.mnemonic.startswith('j') or ins.mnemonic == 'call':
                lines.append(f'{ins.mnemonic} L{int(ins.op_str,16):x}')
                continue
            lines.append('.byte '+','.join(hex(x) for x in data))
    lines.append('.p2align 3')
    for address in [0x4cd528, 0x4cd530, 0x4cd538, 0x4cd540]:
        lines.extend([f'C{address:x}:', '.byte '+','.join(hex(x) for x in pe.get_data(address-0x400000,8))])
    if sys.platform != 'darwin':
        lines.append('.section .note.GNU-stack,"",@progbits')
    return '\n'.join(lines)+'\n'

HARNESS = r'''
#include <cstdint>
#include <iostream>
extern "C" std::uint32_t original_angle_native(std::int32_t,std::int32_t);
int main(){std::int32_t x,y;while(std::cin>>x>>y){
    std::uint16_t old;asm volatile("fnstcw %0":"=m"(old));
    const std::uint16_t cw=0x37f;asm volatile("fldcw %0"::"m"(cw));
    const auto value=original_angle_native(x,y);
    std::uint16_t after;asm volatile("fnstcw %0":"=m"(after));
    asm volatile("fldcw %0"::"m"(old));
    if(after!=cw)return 2;
    std::cout<<value<<'\n';
}}
'''

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game', type=Path)
    p.add_argument('probe', type=Path)
    p.add_argument('--report', type=Path)
    args = p.parse_args()
    if platform.machine().lower() not in ['x86_64', 'amd64'] or sys.platform not in ['darwin', 'linux']:
        raise SystemExit('Use an Intel Mac or x86-64 Linux host')
    compiler = shutil.which('clang++')
    if not compiler:
        raise SystemExit('clang++ is required')
    raw = (args.game/'Df.exe').read_bytes()
    if hashlib.sha256(raw).hexdigest() != SHA:
        raise SystemExit('Unsupported original executable hash')
    pe = pefile.PE(data=raw)
    cases = source_cases()
    payload = ''.join(f'{x} {y}\n' for x,y in cases)
    with tempfile.TemporaryDirectory(prefix='dfre-angle-') as folder:
        directory = Path(folder)
        (directory/'original.S').write_text(assembly(pe))
        (directory/'probe.cpp').write_text(HARNESS)
        executable = directory/'original_angle'
        subprocess.run([compiler,'-std=c++17',str(directory/'original.S'),str(directory/'probe.cpp'),'-o',str(executable)], check=True, capture_output=True)
        if sys.platform == 'darwin':
            subprocess.run(['codesign','--force','--sign','-',str(executable)],check=True,capture_output=True)
        expected = subprocess.run([str(executable)], input=payload, capture_output=True,text=True,check=True).stdout.splitlines()
    got = subprocess.run([str(args.probe.resolve()),'angle'], input=payload,capture_output=True,text=True,check=True).stdout.splitlines()
    if len(expected)!=len(cases) or len(got)!=len(cases):
        raise RuntimeError('Angle output count differs')
    failures = [dict(index=i,input=cases[i],expected=int(a),got=int(b)) for i,(a,b) in enumerate(zip(expected,got)) if a!=b]
    report = dict(source_sha256=SHA, cases=len(cases),mismatches=len(failures),samples=failures[:12],execution='native Intel original instructions with SysV ABI and relocation edits',control_word='037F, restored after each original call')
    if args.report:
        args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    raise SystemExit(bool(failures))

if __name__ == '__main__':
    main()
