"""Check fresh ARM artifact vectors, size and linked functions without hardware."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from elf_symbols import function_symbols
class ArmArtifact(unittest.TestCase):
    def test_exti_and_systick_vectors_and_source_manifest(self):
        out=ROOT/'build/newfangled';data=(out/'pnw.bin').read_bytes()
        metadata=json.loads((out/'build.json').read_text());symbols=function_symbols((out/'pnw.elf').read_bytes())
        self.assertEqual(hashlib.sha256(data).hexdigest(),metadata['sha256'])
        for name,digest in metadata['inputs'].items():
            self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),digest,name)
        self.assertLessEqual(len(data),32768)
        stack=struct.unpack_from('<I',data)[0];self.assertEqual(stack,0x20001800)
        for slot,name in [(1,'Reset_Handler'),(15,'SysTick_Handler'),(23,'EXTI4_15_IRQHandler')]:
            vector=struct.unpack_from('<I',data,4*slot)[0]
            self.assertEqual(vector&~1,symbols[name]&~1);self.assertEqual(vector&1,1)
            self.assertTrue(0x08000000<=vector<0x08000000+len(data))
        self.assertFalse(any(n.startswith(('recovered_','pnw_eeprom_')) for n in symbols))
