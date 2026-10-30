import struct,unittest
from scene_protocol import validate_scene,deploy_scene
from test_scene_protocol import FakeBoard

class BoardWeatherProtocolTests(unittest.TestCase):
 def package(self):
  values=[22,19,2,1,18,6,2,3,1,5,.15,60,120,.12,1,1,42.3601,-71.0589]
  payload=bytes([0,1,1,0])+struct.pack('<18f',*values)
  return b'FLS2'+struct.pack('<HH',1,0)+struct.pack('<BbbBBBBBHHHH',17,52,30,11,11,1,1,12,65535,65535,0,76)+payload
 def test_payload_roundtrip_and_capability(self):
  data=self.package();self.assertTrue(validate_scene(data)['weather']);board=FakeBoard()
  with self.assertRaisesRegex(RuntimeError,'weather-enabled'):deploy_scene(None,data,board.request)
  def request(link,command,marker=None,timeout=4):
   return 'STATUS scene_version=2 weather_version=1\n' if command=='status' else board.request(link,command,marker,timeout)
  deploy_scene(None,data,request);self.assertEqual(board.received,data);self.assertTrue(board.committed)
 def test_corrupt_flags_parameters_and_length(self):
  good=self.package()
  for offset,value in [(24,7),(25,2),(27,1)]:
   data=bytearray(good);data[offset]=value
   with self.assertRaises(ValueError):validate_scene(data)
  data=bytearray(good);struct.pack_into('<f',data,28,float('nan'))
  with self.assertRaises(ValueError):validate_scene(data)
  with self.assertRaises(ValueError):validate_scene(good[:-1])
 def test_solar_extension_and_firmware_capability(self):
  data=bytearray(self.package());struct.pack_into('<H',data,22,84);data[27]=1;data+=struct.pack('<fHH',55,65535,12345)
  self.assertTrue(validate_scene(data)['solar'])
  with self.assertRaisesRegex(RuntimeError,'sun/moon'):
   deploy_scene(None,data,lambda *a,**kw:'STATUS scene_version=2 weather_version=1')
  for offset,value in [(22,76),(27,0),(8,18)]:
   bad=bytearray(data);bad[offset]=value
   with self.assertRaises(ValueError):validate_scene(bad)
  bad=bytearray(data);struct.pack_into('<f',bad,100,float('nan'))
  with self.assertRaises(ValueError):validate_scene(bad)
 def test_javascript_solar_fixture(self):
  from pathlib import Path
  self.assertTrue(validate_scene((Path(__file__).resolve().parent.parent/'artifacts/solar-tests/solar.fls').read_bytes())['solar'])
