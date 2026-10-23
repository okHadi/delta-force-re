#!/usr/bin/env python3
"""Compare the canonical result adapter through all three original bodies."""
import argparse
from pathlib import Path
from check_mission_logic_results_original import run

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('game',type=Path)
    parser.add_argument('probe',type=Path)
    parser.add_argument('--cases',type=int,default=1200)
    parser.add_argument('--report',type=Path,required=True)
    run(parser.parse_args())
