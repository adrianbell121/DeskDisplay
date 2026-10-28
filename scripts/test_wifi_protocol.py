import json
import tempfile
import unittest
from pathlib import Path
from wifi_protocol import read_credentials,provision

class WiFiTests(unittest.TestCase):
    def read(self,ssid,password):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'wifi.json';p.write_text(json.dumps(dict(ssid=ssid,password=password)),encoding='utf-8')
            return read_credentials(p)
    def test_utf8_ssid_password_bounds_and_serial_chunks(self):
        ssid,password=self.read('Test 🌴','x'*63);commands=[];received=bytearray()
        def request(link,cmd,marker=None,timeout=4):
            commands.append(cmd)
            if cmd=='wifi begin': return 'WIFI READY\n'
            if cmd.startswith('wifi ssid '):return 'WIFI SSID OK\n'
            if cmd.startswith('wifi pass '):
                _,_,offset,data=cmd.split();self.assertEqual(int(offset),len(received));received.extend(bytes.fromhex(data));return 'WIFI PASS OK\n'
            return 'WIFI OK saved=1 connecting=1\n'
        provision(None,ssid,password,request)
        self.assertEqual(received,password)
        self.assertTrue(all(len(c)<128 for c in commands))
        self.assertEqual(self.read('Open network','')[1],b'')
    def test_invalid_credentials(self):
        for ssid,password in [('', '12345678'),('x'*33,'12345678'),('x','short'),('x','x'*64),('x','password\n'),('x','пароль123')]:
            with self.assertRaises(ValueError):self.read(ssid,password)
    def test_failures_do_not_echo_secrets(self):
        secret=b'never-echo-this'
        with self.assertRaises(RuntimeError) as error:
            provision(None,b'test',secret,lambda *a,**k:'WIFI ERROR '+secret.decode())
        self.assertNotIn(secret.decode(),str(error.exception))

if __name__=='__main__':unittest.main()
