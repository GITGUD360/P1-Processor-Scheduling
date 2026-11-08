#!/usr/bin/env python3
"""Readable, screenshot-friendly view of YOUR saved run; no plotting packages."""
import argparse
import csv
from pathlib import Path

def rows(p):
    with Path(p).open(newline='',encoding='utf-8') as f:return list(csv.DictReader(f))

def main():
    ap=argparse.ArgumentParser();ap.add_argument('prefix');ap.add_argument('--first',type=int,default=1)
    ap.add_argument('--last',type=int,default=12);args=ap.parse_args()
    prefix=args.prefix
    s=rows(prefix+'_summary.csv')[0]
    print('Saved run:',prefix)
    print('Model:',s['architecture'],'Cycles:',s['cycles'],'Time(ns):',s['time_ns'])
    print('Functional oracle:', 'PASS' if s['functional_ok']=='1' else 'FAIL')
    print('Validation is recorded in comparison.csv / validation.json, NOT inferred from this display.')
    print('\n I   Issue Start Done Write  Assembly')
    for r in rows(prefix+'_timing.csv'):
        if args.first<=int(r['instruction_1based'])<=args.last:
            print(f"{int(r['instruction_1based']):2} {int(r['issue']):7} {int(r['start']):5} {int(r['complete']):4} {int(r['write']):5}  {r['assembly']}")
    print('\nEvent counts (modeled hardware activity, not host instructions):')
    for r in rows(prefix+'_energy.csv'):
        print(f"{r['event']:32} {int(r['count']):8}   {float(r['total_pj']):12.4f} pJ")
if __name__=='__main__':main()
