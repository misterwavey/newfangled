"""Native/WASM parity for deterministic inputs, timing wrap and actual pixels."""
import base64
import ctypes
import json
from pathlib import Path
import random
import shutil
import sys
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class SimulatorParity(unittest.TestCase):
    def test_native_and_wasm_sequences(self):
        node=shutil.which('node')
        if not node:self.skipTest('Node needed for WebAssembly execution')
        wasm=ROOT/'build/simulator/simulator.wasm'
        if not wasm.exists():self.skipTest('run make simulator first')
        report=json.loads(wasm.with_name('build.json').read_text())
        import hashlib
        for name,digest in report['inputs'].items():self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),digest,'stale simulator: '+name)
        with tempfile.TemporaryDirectory(prefix='pnw-sim-test-') as directory:
            libfile=Path(directory)/('core.dylib' if sys.platform=='darwin' else 'core.so')
            shared=['-dynamiclib'] if sys.platform=='darwin' else ['-shared','-fPIC']
            subprocess.run(['cc',*shared,'-O2','-fno-builtin','-Wall','-Wextra','-Werror','-Iinclude','-Ifirmware','firmware/font.c','firmware/ui.c','firmware/clock.c','firmware/app.c','firmware/settings.c','firmware/store.c','firmware/cv.c','firmware/sync.c','firmware/voice.c','simulator/bridge.c','-o',str(libfile)],cwd=ROOT,check=True)
            core=ctypes.CDLL(str(libfile))
            core.sim_reset.argtypes=[];core.sim_reset.restype=None
            core.sim_framebuffer.argtypes=[];core.sim_framebuffer.restype=ctypes.c_void_p
            for name in ['sim_advance','sim_turn','sim_click','sim_raw']:
                getattr(core,name).argtypes=[ctypes.c_uint32];getattr(core,name).restype=None
            core.sim_hold_program.argtypes=[];core.sim_hold_program.restype=None
            core.sim_input.argtypes=[ctypes.c_uint32]*2;core.sim_input.restype=None
            core.sim_output.argtypes=[ctypes.c_uint32];core.sim_output.restype=ctypes.c_uint32
            core.sim_get.argtypes=[ctypes.c_uint32];core.sim_get.restype=ctypes.c_uint32
            core.sim_cv_route.argtypes=[ctypes.c_uint32]*3;core.sim_cv_route.restype=ctypes.c_uint32
            core.sim_pitch.argtypes=[ctypes.c_uint32]*2;core.sim_pitch.restype=ctypes.c_uint32
            core.sim_groove.argtypes=[ctypes.c_uint32]*2;core.sim_groove.restype=ctypes.c_uint32
            core.sim_rhythm.argtypes=[ctypes.c_uint32]*2;core.sim_rhythm.restype=ctypes.c_uint32
            core.sim_effective.argtypes=[ctypes.c_uint32]*2;core.sim_effective.restype=ctypes.c_uint32
            core.sim_edge_count.restype=ctypes.c_uint32
            for name in ['sim_edge_time','sim_edge_level']:
                getattr(core,name).argtypes=[ctypes.c_uint32];getattr(core,name).restype=ctypes.c_uint32
            rng=random.Random(20261114)
            actions=[dict(type='hold',value=0),dict(type='click',value=1),dict(type='turn',value=1),
                     dict(type='click',value=1),dict(type='hold',value=0),dict(type='click',value=0),
                     dict(type='input',index=3,value=1),dict(type='advance',value=5000),
                     dict(type='input',index=3,value=0),dict(type='advance',value=500),
                     dict(type='input',index=3,value=1),dict(type='advance',value=1000)]
            # Configure OUT1 LEVEL base=0, CV1 at full depth, via encoder actions.
            cv_actions=[dict(type='turn',value=1),dict(type='click',value=1)]
            cv_actions += [dict(type='turn',value=1) for _ in range(4)]
            cv_actions += [dict(type='click',value=1)]
            cv_actions += [dict(type='turn',value=0) for _ in range(100)]
            cv_actions += [dict(type='click',value=1),dict(type='turn',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='click',value=1)]
            cv_actions += [dict(type='turn',value=1) for _ in range(4)]
            cv_actions += [dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='hold',value=0),dict(type='click',value=0),dict(type='input',index=0,value=4095),dict(type='advance',value=250),dict(type='input',index=0,value=0),dict(type='advance',value=250),dict(type='reset',value=0)]
            # Save a patch through UI, edit it, then power-cycle and load again.
            preset_actions=[dict(type='hold',value=0),dict(type='turn',value=0),dict(type='click',value=1),
                            dict(type='turn',value=1),dict(type='click',value=1),dict(type='hold',value=0),
                            dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='reboot',value=0)]
            rhythm_actions=[dict(type='turn',value=1),dict(type='click',value=1)]+[dict(type='turn',value=1)]*8+[dict(type='click',value=1)]+[dict(type='turn',value=1)]*7+[dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='turn',value=1),dict(type='turn',value=1),dict(type='click',value=1),dict(type='hold',value=0),dict(type='click',value=0),dict(type='advance',value=8000),dict(type='reset',value=0)]
            groove_actions=[dict(type='turn',value=1),dict(type='click',value=1)]+[dict(type='turn',value=0)]*5+[dict(type='click',value=1)]+[dict(type='turn',value=1)]*3+[dict(type='click',value=1),dict(type='turn',value=0),dict(type='click',value=1)]+[dict(type='turn',value=1)]*33+[dict(type='click',value=1),dict(type='hold',value=0),dict(type='click',value=0),dict(type='advance',value=10000),dict(type='reset',value=0)]
            pitch_actions=[dict(type='turn',value=1),dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1)]+[dict(type='turn',value=1)]*4+[dict(type='click',value=1)]+[dict(type='turn',value=1)]*14+[dict(type='click',value=1)]+[dict(type='turn',value=1)]*10+[dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=1)]+[dict(type='turn',value=1)]*2+[dict(type='click',value=1),dict(type='hold',value=0),dict(type='click',value=0),dict(type='advance',value=6000),dict(type='reset',value=0)]
            actions=pitch_actions+groove_actions+rhythm_actions+preset_actions+cv_actions+actions
            for _ in range(600):
                kind=rng.choice(['advance','advance','turn','click','input'])
                action=dict(type=kind,value=rng.randrange(1000) if kind=='advance' else rng.randrange(2))
                if kind=='input':action.update(index=rng.randrange(6),value=rng.choice([0,1,24,120,4095,65535]))
                actions.append(action)
            # Exercise raw button transitions and a longer virtual-time interval.
            actions.extend([dict(type='advance',value=60000),dict(type='raw',value=0),dict(type='advance',value=21),dict(type='raw',value=15),dict(type='advance',value=21)])
            core.sim_reset();fixtures=[]
            for action in actions:
                kind=action['type'];name={'advance':'sim_advance','turn':'sim_turn','click':'sim_click','input':'sim_input','raw':'sim_raw','hold':'sim_hold_program','reset':'sim_reset','reboot':'sim_reboot'}[kind]
                args=[] if kind in ['hold','reset','reboot'] else [action['index'],action['value']] if kind=='input' else [action['value']]
                getattr(core,name)(*args)
                fixtures.append(dict(action=action,pitch=[[core.sim_pitch(i,k) for k in range(6)] for i in range(8)],grooves=[[core.sim_groove(i,k) for k in range(4)] for i in range(8)],rhythms=[[core.sim_rhythm(i,k) for k in range(8)] for i in range(8)],outputs=[core.sim_output(i) for i in range(8)],effective=[[core.sim_effective(i,f) for f in range(6)] for i in range(8)],routes=[[[core.sim_cv_route(i,f,k) for k in range(3)] for f in range(6)] for i in range(8)],state=[core.sim_get(i) for i in range(42)],edges=[[core.sim_edge_time(i),core.sim_edge_level(i)] for i in range(max(0,core.sim_edge_count()-2048),core.sim_edge_count())],framebuffer=base64.b64encode(ctypes.string_at(core.sim_framebuffer(),512)).decode()))
            path=Path(directory)/'fixtures.json';path.write_text(json.dumps(fixtures))
            subprocess.run([node,str(ROOT/'tests/simulator_wasm.mjs'),str(wasm),str(path)],check=True)
