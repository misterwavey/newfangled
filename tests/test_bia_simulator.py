"""BIA native/WASM parity, cross-language wire format and ARM image checks."""
import base64
import ctypes
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import bia_format as fmt
class BiaSimulator(unittest.TestCase):
    def test_core_and_formats(self):
        report=json.loads((ROOT/'build/bia-simulator/build.json').read_text())
        for n,d in report['inputs'].items():self.assertEqual(hashlib.sha256((ROOT/n).read_bytes()).hexdigest(),d,n)
        with tempfile.TemporaryDirectory(prefix='bia-parity-') as directory:
            lib=Path(directory)/('bia.dylib' if sys.platform=='darwin' else 'bia.so')
            flags=['-dynamiclib'] if sys.platform=='darwin' else ['-shared','-fPIC']
            subprocess.run(['cc',*flags,'-DBIA_BUILD','-O2','-fno-builtin','-Iinclude','-Ifirmware','firmware/font.c','firmware/sync.c','firmware/cv.c','firmware/store.c','firmware/bia/engine.c','firmware/bia/ui.c','firmware/bia/settings.c','simulator/bia/bridge.c','-o',str(lib)],cwd=ROOT,check=True)
            c=ctypes.CDLL(str(lib));c.bs_pixels.restype=ctypes.c_void_p;c.bs_buffer.restype=ctypes.c_void_p
            for name,n in [('bs_get',1),('bs_output',1),('bs_edge',2),('bs_edit',4),('bs_patch_read',0),('bs_patch_import',0)]:
                getattr(c,name).argtypes=[ctypes.c_uint32]*n;getattr(c,name).restype=ctypes.c_uint32
            c.bs_reset();self.assertTrue(c.bs_patch_read());payload=ctypes.string_at(c.bs_buffer(),334);p=fmt.decode(payload);self.assertEqual(fmt.encode(p),payload)
            p['scenes'][7][6]=1001;p['steps'][3]=dict(scene=8,probability=37,ratchets=8,enabled=True);p['variation']=100
            p['motion'][7].update(fold_offset=-73,fold_ms=345,harm_offset=61,harm_ms=500,retrigger=False,accent=[-100,37,100],variation=[0,100,37,50,20,0]);p['step_accents']=[i*6 for i in range(16)];p['fill_mask']=0xbeef;p['performance']=dict(effect='VAR',strengths=[100,37,61])
            ctypes.memmove(c.bs_buffer(),fmt.encode(p),334);self.assertTrue(c.bs_patch_import());self.assertTrue(c.bs_patch_read());self.assertEqual(ctypes.string_at(c.bs_buffer(),334),fmt.encode(p))
            old=fmt.encode(p,schema=1);ctypes.memmove(c.bs_buffer(),old,334);self.assertTrue(c.bs_patch_import());self.assertTrue(c.bs_patch_read());self.assertEqual(ctypes.string_at(c.bs_buffer(),334),fmt.encode(fmt.decode(old)))
            bank=fmt.bank_image({1:payload,8:fmt.encode(p)},8,0xffffffff,0xfffffffe);state=fmt.scan(bank+b'\xff'*4096)
            self.assertEqual(state['boot_slot'],8);self.assertEqual(fmt.validate(fmt.library(state)),state['slots']);self.assertFalse(state['future'])
            # Return to defaults before a deterministic parity scenario.
            c.bs_reset();rng=random.Random(20261115);actions=[('bs_edit',[4,0,0,50]),('bs_edit',[4,0,1,2]),('bs_edit',[4,0,2,-20]),('bs_edit',[5,0,0,75]),('bs_click',[0]),('bs_button',[0,1]),('bs_advance',[350]),('bs_turn',[1]),('bs_click',[1]),('bs_button',[0,0]),('bs_advance',[1500]),('bs_click',[0]),('bs_audition',[3]),('bs_advance',[500]),('bs_click',[0])]
            for _ in range(400):
                op=rng.randrange(6)
                if op==0:actions.append(('bs_advance',[rng.randrange(1,301)]))
                elif op==1:actions.append(('bs_edit',[2,rng.randrange(8),rng.randrange(7),rng.randrange(1024)]))
                elif op==2:actions.append(('bs_edit',[1,rng.randrange(16),rng.randrange(4),rng.randrange(9)]))
                elif op==3:actions.append(('bs_turn',[rng.randrange(2)]))
                elif op==4:actions.append(('bs_click',[rng.randrange(2)]))
                else:actions.append(('bs_input',[rng.randrange(6),rng.randrange(4096)]))
            fixtures=[]
            for name,args in actions:
                getattr(c,name)(*args)
                fixtures.append(dict(name=name,args=args,state=[c.bs_get(i) for i in range(28)],outputs=[c.bs_output(i) for i in range(8)],pixels=base64.b64encode(ctypes.string_at(c.bs_pixels(),512)).decode()))
            file=Path(directory)/'fixtures.json';file.write_text(json.dumps(fixtures))
            subprocess.run(['node','tests/bia_wasm.mjs','build/bia-simulator/simulator.wasm',str(file)],cwd=ROOT,check=True)
            # Browser and Python exporters must agree on every code and setting.
            patchfile=Path(directory)/'patch.json';patchfile.write_text(json.dumps(p))
            result=subprocess.check_output(['node','tests/bia_wasm.mjs','json',str(patchfile)],cwd=ROOT,text=True)
            self.assertEqual(base64.b64decode(result.strip()),fmt.encode(p))
    def test_arm(self):
        out=ROOT/'build/bia';report=json.loads((out/'build.json').read_text());b=(out/'pnw.bin').read_bytes()
        self.assertEqual(hashlib.sha256(b).hexdigest(),report['sha256']);self.assertLessEqual(len(b),32768);self.assertFalse(report['hardware_tested'])
        self.assertEqual(b[192:208],b'BIEEPR01'+struct.pack('<II',0x2000,8192));self.assertEqual(struct.unpack_from('<I',b)[0],0x20001800)
        for n,d in report['inputs'].items():self.assertEqual(hashlib.sha256((ROOT/n).read_bytes()).hexdigest(),d,n)
