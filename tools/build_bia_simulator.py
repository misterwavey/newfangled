#!/usr/bin/env python3
"""Build an offline BIA controller workbench with the actual firmware C engine."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
SOURCES=['firmware/font.c','firmware/sync.c','firmware/cv.c','firmware/store.c','firmware/bia/engine.c','firmware/bia/ui.c','firmware/bia/settings.c','simulator/bia/bridge.c']
EXPORTS=['bs_button','bs_perform','bs_abort','bs_reset','bs_reboot','bs_advance','bs_turn','bs_click','bs_hold','bs_phase_reset','bs_input','bs_edit','bs_get','bs_output','bs_pixels','bs_buffer','bs_patch_read','bs_patch_import','bs_validate','bs_preset_read','bs_preset_import','bs_preset','bs_audition','bs_edges','bs_edge']
def main():
    out=ROOT/'build/bia-simulator';out.mkdir(parents=True,exist_ok=True)
    candidates=[os.environ.get('PNW_WASM_CC'),'/usr/local/opt/llvm/bin/clang','/opt/homebrew/opt/llvm/bin/clang',shutil.which('clang')]
    cc=next((c for c in candidates if c and Path(c).exists() and 'wasm32' in subprocess.check_output([c,'--print-targets'],text=True)),None)
    if not cc:raise SystemExit('Need Clang with wasm32 target; set PNW_WASM_CC.')
    objects=[]
    for i,source in enumerate(SOURCES):
        obj=out/f'{i}.o';subprocess.run([cc,'--target=wasm32-unknown-unknown','-Oz','-DBIA_BUILD','-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror','-Iinclude','-Ifirmware','-c',source,'-o',str(obj)],cwd=ROOT,check=True);objects.append(str(obj))
    ld=os.environ.get('PNW_WASM_LD') or shutil.which('wasm-ld');sibling=Path(cc).with_name('wasm-ld')
    if not ld and sibling.exists():ld=str(sibling)
    if not ld:ld=next((str(p) for p in (Path.home()/'.rustup/toolchains').glob('*/lib/rustlib/*/bin/rust-lld')),None)
    if not ld:raise SystemExit('Need wasm-ld or rust-lld.')
    subprocess.run([ld,*(['-flavor','wasm'] if Path(ld).name=='rust-lld' else []),'--no-entry','--initial-memory=131072','--max-memory=131072',*['--export='+n for n in EXPORTS],'-o',str(out/'simulator.wasm'),*objects],check=True)
    wasm=(out/'simulator.wasm').read_bytes();html=(ROOT/'simulator/bia/index.html').read_text().replace('/* APP */',(ROOT/'simulator/bia/presets.js').read_text()+'\n'+(ROOT/'simulator/bia/app.js').read_text()).replace('__WASM__',base64.b64encode(wasm).decode())
    (out/'index.html').write_text(html)
    inputs=SOURCES+['firmware/bia/bia.h','firmware/bia/settings.h','firmware/store.h','firmware/preset_format.h','firmware/cv.h','firmware/sync.h','firmware/font.h','include/pnw_board.h','firmware/controls.h','simulator/bia/index.html','simulator/bia/app.js','simulator/bia/presets.js']
    report=dict(application='Newfangled BIA controller simulator v0.2',wasm_bytes=len(wasm),wasm_sha256=hashlib.sha256(wasm).hexdigest(),hardware_emulation=False,bia_audio_emulation=False,inputs={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in inputs})
    (out/'build.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(page=str(out/'index.html'),wasm_bytes=len(wasm)),indent=2))
if __name__=='__main__':main()
