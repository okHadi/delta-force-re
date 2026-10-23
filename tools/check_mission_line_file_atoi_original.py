#!/usr/bin/env python3
"""Prove the shared integer reader against actual49AFA0 instructions."""
import argparse,hashlib,json,random,subprocess
from pathlib import Path
import check_mission_script_registry_original as registry

def run(a):
 source=(a.game/'Df.exe').read_bytes();assert hashlib.sha256(source).hexdigest()==registry.HASH
 registry.FUNCTIONS[5]=0x49afa0;machine=registry.Original(source);rng=random.Random(0x49afa0);cases=[]
 values=[b'',b'0',b'-2147483648',b'2147483648',b'-2147483649',b'4294967295',b'4294967296',b'  +42x',b'\t\r\n-91',b'9999999999999999999999999',bytes([255,128,0])]
 for i in range(a.cases):
  c=registry.case(5);d=dict(c[5]);s=values[i%len(values)] if i%2 else rng.choice([b'',b' ',b'\t'])*rng.randrange(4)+rng.choice([b'',b'+',b'-'])+str(rng.getrandbits(64)).encode()+rng.choice([b'',b'abc'])
  d[registry.N][:]=registry.text(s);cases.append(c)
 expected=[machine.case(c) for c in cases]
 rows=[' '.join(map(str,c[:5]))+' '+str(len(c[5]))+' '+' '.join(str(at)+' '+bytes(data).hex() for at,data in c[5])+' -' for c in cases]
 done=subprocess.run([str(a.registry_probe),str(a.game/'Df.exe')],input='\n'.join(rows)+'\n',text=True,capture_output=True);assert done.returncode==0,done.stderr
 actual=done.stdout.splitlines();assert len(actual)==len(expected)
 for i,(x,y) in enumerate(zip(actual,expected)):assert x==y,(i,x,y)
 report={'source_sha256':registry.HASH,'original':'49AFA0','shared_port':'mission_script_registry_id49DFF0','cases':len(cases),'differences':0,'comparison':'Complete memory, exactorderedbyte/ctype reads, writes and calls; signedwrap/whitespace/sign/highbyte edges.'};print(json.dumps(report,indent=2))
 if a.report:a.report.write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--game',type=Path,required=True);p.add_argument('--registry-probe',type=Path,required=True);p.add_argument('--cases',type=int,default=512);p.add_argument('--report',type=Path);run(p.parse_args())
