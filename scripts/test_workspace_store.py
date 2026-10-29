import unittest,tempfile
from pathlib import Path
from unittest.mock import patch
import workspace_store as w
class SharedTests(unittest.TestCase):
 def test_atomic_roundtrip_conflicts_recovery_and_restart(self):
  with tempfile.TemporaryDirectory() as folder,patch.object(w,'ROOT',Path(folder)):
   state={'version':2,'faces':[{'id':'adrian','widgets':[],'name':'Island — Adrian'}],'library':[]}
   self.assertEqual(w.read()['revision'],0);w.save(state,0);self.assertEqual(w.read()['state'],state)
   with self.assertRaises(w.Conflict):w.save(state,0)
   changed={**state,'selected':'adrian'};w.save(changed,1);self.assertEqual(w.read()['state'],changed);self.assertEqual(len(list((Path(folder)/'recovery').glob('*.json'))),1)
   w.recovery(changed);self.assertEqual(len(list((Path(folder)/'recovery').glob('*.json'))),2)
 def test_invalid_data_never_replaces_saved_design(self):
  with tempfile.TemporaryDirectory() as folder,patch.object(w,'ROOT',Path(folder)):
   with self.assertRaises(ValueError):w.save({'version':2,'faces':[],'library':[]},0)
   self.assertEqual(w.read()['revision'],0)
