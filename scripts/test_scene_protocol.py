"""Offline only: simulated USB acknowledgements; never imports serial/device."""
import struct
import unittest
import zlib
from pathlib import Path
from scene_protocol import validate_scene, deploy_scene

ROOT=Path(__file__).resolve().parent.parent


class FakeBoard:
    def __init__(self, failure=None):
        self.commands=[]
        self.received=bytearray()
        self.failure=failure
        self.committed=False

    def request(self, link, command, marker=None, timeout=4):
        self.commands.append(command)
        if command=='status':
            return 'STATUS scene_version=1\n' if self.failure=='old' else 'STATUS scene_version=2\n'
        if command.startswith('scene begin '):
            _,_,size,crc=command.split()
            self.size=int(size)
            self.crc=int(crc,16)
            return 'SCENE READY\n'
        if command.startswith('scene data '):
            _,_,offset,data=command.split()
            assert int(offset)==len(self.received)
            self.received.extend(bytes.fromhex(data))
            if self.failure=='offset':
                return 'SCENE DATA 99999\n'
            if self.failure=='error':
                return 'SCENE ERROR invalid hex\n'
            return f'SCENE DATA {len(self.received)}\n'
        if command=='scene commit':
            assert len(self.received)==self.size
            assert zlib.crc32(self.received)==self.crc
            validate_scene(self.received)
            self.committed=True
            return 'SCENE OK saved=1 mode=custom\n'
        return 'SCENE ABORTED\n'


class SceneTests(unittest.TestCase):
    def test_cross_language_fixtures_and_transfer(self):
        fixtures=list((ROOT/'artifacts/studio-tests').glob('*.fls'))
        self.assertEqual(len(fixtures),7,'Run node --test preview/scene.test.cjs first')
        for fixture in fixtures:
            data=fixture.read_bytes()
            info=validate_scene(data)
            self.assertGreater(info['layers'],1)
            board=FakeBoard()
            result=deploy_scene(None,data,board.request)
            self.assertTrue(board.committed)
            self.assertEqual(board.received,data)
            self.assertIn('CRC verified',result)
            self.assertTrue(all(len(c)<128 for c in board.commands))

    def test_bad_acknowledgements_abort_without_commit(self):
        data=b'FLS2'+struct.pack('<HH',0,0)
        for failure in ['offset','error','old']:
            board=FakeBoard(failure)
            with self.assertRaises(RuntimeError):
                deploy_scene(None,data,board.request)
            self.assertFalse(board.committed)
            if failure!='old':
                self.assertEqual(board.commands[-1],'scene abort')

    def test_malformed_input_is_rejected_before_any_io(self):
        good=(ROOT/'artifacts/studio-tests/island.fls').read_bytes()
        for data in [b'',b'bad',good[:-1],good+b'\x00',b'FLS2'+struct.pack('<HH',33,0)]:
            board=FakeBoard()
            with self.assertRaises(ValueError):
                deploy_scene(None,data,board.request)
            self.assertEqual(board.commands,[])


if __name__=='__main__':
    unittest.main()
