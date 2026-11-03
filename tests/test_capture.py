import importlib.util
from pathlib import Path
import unittest

p=Path(__file__).resolve().parents[1]/'tools/capture_serial.py'
spec=importlib.util.spec_from_file_location('capture',p)
capture=importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)

class CaptureTest(unittest.TestCase):
    def test_real_sample_columns(self):
        self.assertEqual(capture.parse_sample('SAMPLE,1,200,1,75,DETECTED,CHECKING,1'),
                         [1,200,1,75,'DETECTED','CHECKING',1])
    def test_hold_is_not_a_hit(self):
        row=capture.parse_sample('SAMPLE,2,400,0,0,HOLD,PLANT_CONFIRMED,1')
        self.assertEqual(row[2],0)
        self.assertEqual(row[4],'HOLD')
    def test_diagnostics_and_partial_lines(self):
        for line in ['STATUS,400,CHECKING,0,0,0,0','SAMPLE,2,400,0,0,HOLD',
                     'SAMPLE,bad,400,0,0,HOLD,CHECKING,1','']:
            self.assertIsNone(capture.parse_sample(line))

if __name__=='__main__': unittest.main()
