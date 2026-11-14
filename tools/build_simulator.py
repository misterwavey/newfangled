#!/usr/bin/env python3
"""Compile the real C UI to WebAssembly and produce an offline simulator page."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['firmware/font.c','firmware/ui.c','firmware/clock.c','firmware/app.c','firmware/voice.c','simulator/bridge.c']
EXPORTS=['sim_reset','sim_advance','sim_raw','sim_turn','sim_click','sim_input','sim_get','sim_framebuffer','sim_output','sim_hold_program','sim_reset_phase','sim_edge_count','sim_edge_time','sim_edge_level']
def main():
    out=ROOT/'build/simulator';out.mkdir(parents=True,exist_ok=True)
    candidates=[os.environ.get('PNW_WASM_CC'),'/usr/local/opt/llvm/bin/clang','/opt/homebrew/opt/llvm/bin/clang',shutil.which('clang')]
    cc=None
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            result=subprocess.run([candidate,'--print-targets'],capture_output=True,text=True,check=True)
            if 'wasm32' in result.stdout:cc=candidate;break
    if not cc:raise SystemExit('Need Clang with wasm32 target; set PNW_WASM_CC to a suitable LLVM clang.')
    objects=[]
    for i,source in enumerate(SOURCES):
        obj=out/f'{i}.o'
        subprocess.run([cc,'--target=wasm32-unknown-unknown','-Oz','-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror','-Iinclude','-Ifirmware','-c',source,'-o',str(obj)],cwd=ROOT,check=True);objects.append(str(obj))
    ld=os.environ.get('PNW_WASM_LD') or shutil.which('wasm-ld')
    sibling=Path(cc).with_name('wasm-ld')
    if not ld and sibling.exists():ld=str(sibling)
    if not ld:
        ld=next((str(p) for p in (Path.home()/'.rustup/toolchains').glob('*/lib/rustlib/*/bin/rust-lld')),None)
    if not ld:raise SystemExit('Need wasm-ld or rust-lld; set PNW_WASM_LD.')
    command=[ld]+(['-flavor','wasm'] if Path(ld).name=='rust-lld' else [])
    subprocess.run([*command,'--no-entry','--initial-memory=131072','--max-memory=131072',*['--export='+name for name in EXPORTS],'-o',str(out/'simulator.wasm'),*objects],check=True)
    wasm=(out/'simulator.wasm').read_bytes()
    inputs=SOURCES+['firmware/ui.h','firmware/clock.h','firmware/app.h','firmware/voice.h','firmware/font.h','include/pnw_board.h','simulator/index.html','simulator/style.css','simulator/app.js']
    report=dict(application='Pam’s Newfangled Workout simulator',engine='Eight outputs: ratios, gate/triangle/ramps/random, 10–300 BPM',wasm_bytes=len(wasm),wasm_sha256=hashlib.sha256(wasm).hexdigest(),
                inputs={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in inputs},hardware_emulation=False,stock_assets_used=False)
    (out/'build.json').write_text(json.dumps(report,indent=2)+'\n')
    html=(ROOT/'simulator/index.html').read_text().replace('/* INLINE_STYLE */',(ROOT/'simulator/style.css').read_text())
    html=html.replace('/* INLINE_APP */',(ROOT/'simulator/app.js').read_text()).replace('__WASM_BASE64__',base64.b64encode(wasm).decode()).replace('__CORE_HASH__',report['wasm_sha256'])
    (out/'index.html').write_text(html)
    print(json.dumps(dict(page=str(out/'index.html'),wasm_bytes=len(wasm),wasm_sha256=report['wasm_sha256']),indent=2))
if __name__=='__main__':main()
