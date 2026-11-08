#!/usr/bin/env python3
"""Create a NEW serial workload; never edit the two frozen baseline inputs."""
import argparse
from pathlib import Path

def generate(samples=64):
    if not 1<=samples<=256:raise ValueError('samples must be 1..256')
    lines=['# Generated growth workload: one serial feedback channel.',
           '.reg R1 0','.reg R2 1','.reg F0 4.0','.reg F24 0.5','.reg F25 1.0','.reg F26 2.0']
    lines += [f'.mem {i} {2*(i+1)}.0' for i in range(samples)]
    for i in range(samples):
        lines += ['',f'# Sample {i+1}',f'LD F8, [{i}]','FADD F9, F0, F8','FMUL F10, F9, F24',
                  'FADD F11, F10, F25','FDIV F0, F11, F26','IADD R1, R1, R2']
    return '\n'.join(lines)+'\n'
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--samples',type=int,default=64)
    ap.add_argument('--out',default='results/inputs/iot_serial_64.asm');args=ap.parse_args()
    out=Path(args.out)
    if 'workloads' in out.parts:raise SystemExit('Use results/inputs; frozen workloads must not be overwritten.')
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(generate(args.samples));print(out)
