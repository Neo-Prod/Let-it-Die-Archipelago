"""Portable source-snapshot build helper. Outputs must stay outside this release."""
from pathlib import Path
import argparse,json,os,subprocess,zipfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--native',action='store_true')
    parser.add_argument('--zig',default='zig')
    args=parser.parse_args()
    source=Path(__file__).resolve().parents[1]
    output=args.output.resolve()
    if output.is_relative_to(source.parent):
        parser.error('Build outputs must be outside the immutable release folder.')
    output.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,ZIG_LOCAL_CACHE_DIR=str(output/'zig-local'),ZIG_GLOBAL_CACHE_DIR=str(output/'zig-global'),TEMP=str(output/'temp'),TMP=str(output/'temp'))
    (output/'temp').mkdir(exist_ok=True)
    with zipfile.ZipFile(output/'let_it_die.apworld','w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted((source/'APWorld/let_it_die').rglob('*')):
            if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.pyc','.pyo'):
                z.write(p,p.relative_to(source/'APWorld').as_posix())
    if not args.native:return
    config=json.loads((source/'Build/build_settings.json').read_text(encoding='utf-8'))
    native=source/'Native'
    def run(label,flags):
        p=subprocess.run([args.zig,'cc',*flags],env=env,capture_output=True,text=True,timeout=120)
        (output/(label+'.log')).write_text(p.stdout+p.stderr,encoding='utf-8')
        if p.returncode:raise RuntimeError(label+' failed: '+p.stderr)
    common=config['native_flags']+['-I',str(native)]
    run('native',common+config['features']+['-c',str(source/'Build/native.c'),'-o',str(output/'native.obj')])
    run('winlite',common+['-c',str(native/'winlite.c'),'-o',str(output/'winlite.obj')])
    run('fast',config['fast_flags']+['-I',str(native),'-c',str(native/'status_scan_fast.c'),'-o',str(output/'status_scan_fast.obj')])
    run('guard',config['guard_flags']+['-I',str(native),'-c',str(native/'status_scan_guard.c'),'-o',str(output/'status_scan_guard.obj')])
    run('link',['-target','x86_64-windows-gnu','-fno-sanitize=all','-shared','-nostdlib','-Wl,--entry,DllMain',str(source/'Build/exports.def'),*[str(output/n) for n in ('native.obj','status_scan_fast.obj','winlite.obj','status_scan_guard.obj')],'-o',str(output/'LetItDieArchipelago.dll')])

if __name__=='__main__':main()
