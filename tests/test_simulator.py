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
            subprocess.run(['cc',*shared,'-O2','-fno-builtin','-Wall','-Wextra','-Werror','-Iinclude','-Ifirmware','firmware/font.c','firmware/ui.c','firmware/clock.c','firmware/app.c','firmware/voice.c','simulator/bridge.c','-o',str(libfile)],cwd=ROOT,check=True)
            core=ctypes.CDLL(str(libfile))
            core.sim_reset.argtypes=[];core.sim_reset.restype=None
            core.sim_framebuffer.argtypes=[];core.sim_framebuffer.restype=ctypes.c_void_p
            for name in ['sim_advance','sim_turn','sim_click','sim_raw']:
                getattr(core,name).argtypes=[ctypes.c_uint32];getattr(core,name).restype=None
            core.sim_input.argtypes=[ctypes.c_uint32]*2;core.sim_input.restype=None
            core.sim_output.argtypes=[ctypes.c_uint32];core.sim_output.restype=ctypes.c_uint32
            core.sim_get.argtypes=[ctypes.c_uint32];core.sim_get.restype=ctypes.c_uint32
            core.sim_edge_count.restype=ctypes.c_uint32
            for name in ['sim_edge_time','sim_edge_level']:
                getattr(core,name).argtypes=[ctypes.c_uint32];getattr(core,name).restype=ctypes.c_uint32
            rng=random.Random(20261114)
            actions=[dict(type='click',value=1),dict(type='turn',value=1),dict(type='click',value=0)]
            for _ in range(600):
                kind=rng.choice(['advance','advance','turn','click','input'])
                action=dict(type=kind,value=rng.randrange(1000) if kind=='advance' else rng.randrange(2))
                if kind=='input':action.update(index=rng.randrange(6),value=rng.choice([0,1,24,120,4095,65535]))
                actions.append(action)
            # Exercise raw button transitions and a longer virtual-time interval.
            actions.extend([dict(type='advance',value=60000),dict(type='raw',value=0),dict(type='advance',value=21),dict(type='raw',value=15),dict(type='advance',value=21)])
            core.sim_reset();fixtures=[]
            for action in actions:
                kind=action['type'];name={'advance':'sim_advance','turn':'sim_turn','click':'sim_click','input':'sim_input','raw':'sim_raw'}[kind]
                args=[action['index'],action['value']] if kind=='input' else [action['value']]
                getattr(core,name)(*args)
                fixtures.append(dict(action=action,outputs=[core.sim_output(i) for i in range(8)],state=[core.sim_get(i) for i in range(19)],edges=[[core.sim_edge_time(i),core.sim_edge_level(i)] for i in range(max(0,core.sim_edge_count()-2048),core.sim_edge_count())],framebuffer=base64.b64encode(ctypes.string_at(core.sim_framebuffer(),512)).decode()))
            path=Path(directory)/'fixtures.json';path.write_text(json.dumps(fixtures))
            subprocess.run([node,str(ROOT/'tests/simulator_wasm.mjs'),str(wasm),str(path)],check=True)
