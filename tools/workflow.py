#!/usr/bin/env python3
"""Guided v2.0 workflow. Standard library only; run from the student root.
A run may finish while failing the target specification. Never substitute completion
or an energy minimum for validation. No reference implementation is in this file.
"""
import argparse
import contextlib
import csv
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path
from datetime import datetime, timezone

MODES = ('inorder', 'scoreboard', 'tomasulo')
FIELDS = ('instructions','cycles','time_ns','cpi','dynamic_nj','background_nj',
          'task_nj','task_avg_mw','period_nj','period_avg_mw','issue_blocked_cycles','no_launch_cycles')

def read_csv(path):
    with Path(path).open(newline='', encoding='utf-8') as f:
        return list(csv.DictReader(f))

def save_json(path, obj):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2) + '\n', encoding='utf-8')

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def digest(obj):
    return hashlib.sha256(json.dumps(obj, sort_keys=True, separators=(',', ':')).encode()).hexdigest()

def identity(build, decisions):
    return {'release':'2.0', 'hardware_model':'1.0', 'utc':datetime.now(timezone.utc).isoformat(),
            'executable_sha256':sha(Path(build)/'sim'),
            'decision_sha256':{m:sha(Path(decisions)/(m+'_control.cpp')) for m in MODES}}

def configure(base, **updates):
    lines=[]; changes=dict(updates)
    for line in base.splitlines():
        key=line.split('=',1)[0].strip()
        lines.append(f'{key} = {changes.pop(key)}' if key in changes else line)
    if changes: raise ValueError(f'Unknown configuration fields: {changes}')
    return '\n'.join(lines)+'\n'

def experiment_specs():
    specs=[]
    for w in ('iot_serial','iot_four_streams'):
        for m in MODES:
            specs.append({'id':f'{w}_{m}_s1','workload':w,'mode':m,'scale':1,
                          'study':'A_serial' if w=='iot_serial' else 'B_four_streams'})
    for scale in (0.5,2):
        for m in MODES:
            specs.append({'id':f'iot_serial_{m}_s{scale:g}', 'workload':'iot_serial',
                          'mode':m,'scale':scale,'study':'C_sensitivity'})
    return specs

def collect(prefix):
    prefix=str(prefix)
    summary=read_csv(prefix+'_summary.csv')[0]
    timing=read_csv(prefix+'_timing.csv')
    events=read_csv(prefix+'_energy.csv')
    registers=read_csv(prefix+'_registers.csv')
    timing_sig=[[int(r[k]) for k in ('instruction_1based','issue','start','complete','write')] for r in timing]
    event_sig=sorted([[r['event'],int(r['count'])] for r in events])
    # Quantization only avoids inconsequential floating formatting differences.
    number_sig={k:(format(float(summary[k]),'.10g') if summary[k]!='NA' else 'NA') for k in FIELDS}
    return {'summary':summary,'timing':timing,'events':events,'registers':registers,
            'timing_sha256':digest(timing_sig),'events_sha256':digest(event_sig),
            'numbers_sha256':digest(number_sig)}

def execute(exe, mode, workload, config, prefix, trace=False):
    prefix=Path(prefix); prefix.parent.mkdir(parents=True,exist_ok=True)
    # Do not let an old successful CSV mask a new failed simulation.
    for suffix in ('_summary.csv','_energy.csv','_timing.csv','_registers.csv','_trace.csv'):
        Path(str(prefix)+suffix).unlink(missing_ok=True)
    cmd=[str(Path(exe).resolve()),mode,str(workload),str(config),str(prefix)]
    if trace: cmd.append('--trace')
    try:
        p=subprocess.run(cmd,text=True,capture_output=True,timeout=30)
        Path(str(prefix)+'_console.txt').write_text('$ '+' '.join(cmd)+'\n'+p.stdout+p.stderr,encoding='utf-8')
        if p.returncode: return None, (p.stderr.strip() or 'Simulation failed')
        return collect(prefix), ''
    except (OSError,subprocess.TimeoutExpired,KeyError,ValueError,IndexError) as e:
        Path(str(prefix)+'_console.txt').write_text(str(e),encoding='utf-8')
        return None,str(e)

def compare_full(actual,expected):
    if actual['summary'].get('functional_ok')!='1':
        return 'Functional oracle FAIL; use the register output to diagnose values.'
    a=actual['timing']; b=expected['timing']
    if len(a)!=len(b):return 'Instruction-count mismatch'
    for x,y in zip(a,b):
        for key in ('issue','start','complete','write'):
            if int(x[key])!=int(y[key]):
                return f"I{x['instruction_1based']} {key}: expected {y[key]}, got {x[key]}"
    aa={e['event']:e for e in actual['events']}; bb={e['event']:e for e in expected['events']}
    if set(aa)!=set(bb):return 'Energy-event categories differ'
    for name,x in aa.items():
        if int(x['count'])!=int(bb[name]['count']):
            return f"Event {name}: expected count {bb[name]['count']}, got {x['count']}"
        for key in ('energy_per_event_pj','total_pj'):
            if not math.isclose(float(x[key]),float(bb[name][key]),rel_tol=1e-9,abs_tol=1e-9):
                return f'Energy component differs: {name}/{key}'
    for key in FIELDS:
        x,y=actual['summary'][key],expected['summary'][key]
        if x=='NA' or y=='NA':
            if x!=y:return f'{key}: expected {y}, got {x}'
        elif not math.isclose(float(x),float(y),rel_tol=1e-9,abs_tol=1e-9):
            return f'{key}: expected {y}, got {x}'
    for x,y in zip(actual['registers'],expected['registers']):
        if x['register'].startswith('R'):
            ok=int(x['value'])==int(y['value'])
        else:ok=math.isclose(float(x['value']),float(y['value']),rel_tol=1e-9,abs_tol=1e-9)
        if not ok:return f"Reference register differs: {x['register']}"
    return ''

def run_check(build='build', decisions='src', model=None, decisions_only=False, out='results/check'):
    out=Path(out); out.mkdir(parents=True,exist_ok=True)
    modes=(model,) if model else MODES
    cases=[c for c in json.loads(Path('tests/decision_cases.json').read_text()) if c['model'] in modes]
    probe=''.join(c['id']+' '+' '.join(map(str,c['args']))+'\n' for c in cases)
    pp=subprocess.run([str((Path(build)/'decision_probe').resolve())],input=probe,text=True,capture_output=True,timeout=20)
    try:
        values=[int(x) for x in pp.stdout.split()]
        if len(values)!=len(cases) or pp.returncode:raise ValueError('Wrong probe output length')
    except ValueError:values=[None]*len(cases)
    checks=[]; ok_by_model={m:True for m in modes}
    for c,val in zip(cases,values):
        ok=val==c['expected']; ok_by_model[c['model']] &= ok
        entry={**c,'actual':val,'pass':ok}; checks.append(entry)
        if not ok:
            print(f"FAIL {c['id']} inputs={c['args']} expected={c['expected']} got={val} | {c['why']}")
    print(f"DECISIONS: {sum(c['pass'] for c in checks)}/{len(checks)} passed")
    save_json(out/'decision_results.json', checks)
    print('FILE CONTENT HASH CHECKS: DISABLED (functional/timing/energy checks remain)')
    framework=subprocess.run([str((Path(build)/'framework_tests').resolve())],text=True,capture_output=True,timeout=20)
    print(framework.stdout.strip() or framework.stderr.strip())
    framework_ok=framework.returncode==0
    micro=[]
    if not decisions_only:
        expected=json.loads(Path('tests/reference_micro.json').read_text())
        for key,gold in expected.items():
            case,m=key.split('/')
            if m not in modes:continue
            prefix=out/f'{case}_{m}'
            actual,error=execute(Path(build)/'sim',m,f'tests/cases/{case}.asm','config/baseline.cfg',prefix,trace=case in ('war','load_admission','waw'))
            reason=error if actual is None else compare_full(actual,gold)
            ok=not reason; ok_by_model[m] &= ok
            micro.append({'case':case,'model':m,'pass':ok,'reason':reason})
            if not ok: print(f'FAIL {case}/{m}: {reason}')
        print(f"MICROPROGRAMS: {sum(x['pass'] for x in micro)}/{len(micro)} passed (all instruction times, all event counts, registers and energy)")
    for m in modes:ok_by_model[m] &= framework_ok
    report={**identity(build,decisions),'scope':list(modes),'decisions_only':decisions_only,
            'decision_passed':sum(c['pass'] for c in checks),'decision_total':len(checks),
            'micro_passed':sum(c['pass'] for c in micro),'micro_total':len(micro),
            'file_content_hash_checks':'DISABLED','framework_ok':framework_ok,
            'models':ok_by_model, 'pass':all(ok_by_model.values())}
    save_json(out/'validation.json',report); save_json(out/'micro_results.json',micro)
    print('CHECK: '+('PASS' if report['pass'] else 'FAIL / UNVERIFIED')+' (finite tests, not a proof for every input)')
    return report

def print_table(rows):
    print('\nWorkload       Model       Scale  Cycles   Time(ns)   Period(nJ)  D140 D70   Status')
    for r in rows:
        if r.get('error'):
            print(f"{r['workload']:14} {r['architecture']:11} ERROR: {r['error']}")
            continue
        w='serial' if r['workload']=='iot_serial' else 'four-streams'
        print(f"{w:14} {r['architecture']:11} {float(r['control_scale']):5g} {int(r['cycles']):7d} "
              f"{float(r['time_ns']):10.2f} {float(r['period_nj']):12.5f} "
              f"{r['deadline_140_met']:>4} {r['deadline_70_met']:>3}   {r['validation_status']}")

def save_rows(path,rows):
    names=[]
    for r in rows:
        for k in r:
            if k not in names:names.append(k)
    with Path(path).open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,fieldnames=names);writer.writeheader();writer.writerows(rows)

def run_workloads(action,build,decisions):
    directory=Path('results/demo' if action=='demo' else 'results/evaluation')
    directory.mkdir(parents=True,exist_ok=True)
    print(f'Project 1 | Guided release v2.0 | {action.upper()}')
    validation=None
    if action=='evaluate':validation=run_check(build,decisions)
    signatures=json.loads(Path('tests/reference_signatures.json').read_text())
    base=Path('config/baseline.cfg').read_text()
    specs=experiment_specs() if action=='evaluate' else experiment_specs()[:3]
    rows=[]; evidence=[]
    for spec in specs:
        prefix=directory/spec['id']; cfg=Path(str(prefix)+'.cfg')
        cfg.write_text(configure(base,control_scale=spec['scale'],rs_entries=8))
        workload=Path('workloads')/(spec['workload']+'.asm')
        actual,error=execute(Path(build)/'sim',spec['mode'],workload,cfg,prefix,trace=spec['scale']==1)
        if actual is None:
            rows.append({'workload':spec['workload'],'architecture':spec['mode'],
                         'validation_status':'RUN_ERROR','error':error})
            continue
        s=actual['summary']
        gold=signatures[spec['id']]
        # Validate the simulated behavior, not the exact text/bytes of config or assembly files.
        match=(actual['summary']['functional_ok']=='1' and all(actual[k]==gold[k] for k in
               ('timing_sha256','events_sha256','numbers_sha256')))
        valid=action=='evaluate' and validation['models'][spec['mode']] and match
        status='VERIFIED_FOR_THIS_RUN' if valid else 'UNVERIFIED'
        row={'workload':spec['workload'],'study':spec['study'],**s,
             'deadline_140_met':str(int(float(s['time_ns'])<=140+1e-9)),
             'deadline_70_met':str(int(float(s['time_ns'])<=70+1e-9)),
             'reference_match':int(match),'validation_status':status,'source_prefix':str(prefix)}
        rows.append(row)
        evidence.append({'run':spec['id'],'status':status,'reference_match':match,
                         'workload_sha256':sha(workload),'config_sha256':sha(cfg),
                         'timing_sha256':actual['timing_sha256'],'events_sha256':actual['events_sha256'],
                         'summary_sha256':sha(str(prefix)+'_summary.csv')})
    save_rows(directory/'comparison.csv',rows)
    print_table(rows)
    all_valid=bool(rows) and all(r['validation_status']=='VERIFIED_FOR_THIS_RUN' for r in rows)
    meta={**identity(build,decisions),'action':action,'all_verified':all_valid,'runs':evidence}
    save_json(directory/'provenance.json',meta)
    print('\nSaved:',directory/'comparison.csv')
    if action=='demo':
        print('DEMO ONLY: completed does not mean conforming. Preserve this BEFORE evidence.')
        print('The starter is intentionally conservative and contains incorrect decisions.')
    elif all_valid:
        print(f'EVALUATION: PASS ({len(rows)} verified records). Explain results; do not choose by speed alone.')
    else:
        print('EVALUATION: UNVERIFIED records retained for debugging. Do not use them as final design evidence.')
    return all_valid if action=='evaluate' else all(not r.get('error') for r in rows)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('action',choices=('demo','check','evaluate'))
    ap.add_argument('--build',default='build');ap.add_argument('--decisions',default='src')
    ap.add_argument('--model',choices=MODES);ap.add_argument('--decisions-only',action='store_true')
    args=ap.parse_args()
    log_dir=Path('results')/({'demo':'demo','check':'check','evaluate':'evaluation'}[args.action])
    log_dir.mkdir(parents=True,exist_ok=True)
    class Tee:
        def __init__(self,a,b):self.a,self.b=a,b
        def write(self,text):self.a.write(text);self.b.write(text);return len(text)
        def flush(self):self.a.flush();self.b.flush()
    with (log_dir/'console.txt').open('w',encoding='utf-8') as log:
        with contextlib.redirect_stdout(Tee(sys.stdout,log)):
            if args.action=='check':
                ok=run_check(args.build,args.decisions,args.model,args.decisions_only)['pass']
            else:
                ok=run_workloads(args.action,args.build,args.decisions)
    return 0 if ok else 1

if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as e:
        print(f'WORKFLOW ERROR: {e}',file=sys.stderr);sys.exit(2)
