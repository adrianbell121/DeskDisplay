"""Local HTTP and mocked CLI/USB tests. No real device or model is contacted."""
import base64
import http.client
import http.server
import json
import subprocess
import threading
import types
import unittest
from pathlib import Path
from unittest import mock
import preview_server as server
from test_scene_protocol import FakeBoard


class LocalServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.httpd=http.server.ThreadingHTTPServer(('127.0.0.1',0),server.Handler)
        cls.thread=threading.Thread(target=cls.httpd.serve_forever,daemon=True)
        cls.thread.start()
        cls.port=cls.httpd.server_port

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls.httpd.server_close()
        cls.thread.join()

    def request(self,path,body=None,origin=True):
        link=http.client.HTTPConnection('127.0.0.1',self.port,timeout=3)
        headers={'Content-Type':'application/json','X-Face-Lab':'1'}
        if origin:
            headers['Origin']=f'http://127.0.0.1:{self.port}'
        link.request('POST' if body is not None else 'GET',path,json.dumps(body) if body is not None else None,headers)
        response=link.getresponse()
        status=response.status
        raw=response.read()
        link.close()
        return status,json.loads(raw)

    def test_capabilities_are_read_only_and_do_not_load_device_module(self):
        with mock.patch.dict('sys.modules',{'device':None}):
            status,body=self.request('/api/capabilities')
        self.assertEqual(status,200)
        self.assertEqual(body['sceneVersion'],2)

    def test_cross_origin_and_malformed_requests_never_connect(self):
        fake=types.SimpleNamespace(connect=mock.Mock(side_effect=AssertionError('Must not connect')))
        with mock.patch.dict('sys.modules',{'device':fake}):
            self.assertEqual(self.request('/api/deploy',{'scene':'bad'},origin=False)[0],403)
            self.assertEqual(self.request('/api/deploy',{'scene':'bad'})[0],400)
            fake.connect.assert_not_called()

    def test_explicit_deploy_route_uses_only_fake_usb(self):
        data=(server.ROOT/'artifacts/studio-tests/island.fls').read_bytes()
        board=FakeBoard()
        connection=mock.MagicMock()
        fake=types.SimpleNamespace(connect=mock.Mock(return_value=connection),request=board.request)
        with mock.patch.dict('sys.modules',{'device':fake}):
            status,body=self.request('/api/deploy',{'scene':base64.b64encode(data).decode(),'port':'COM99'})
        self.assertEqual(status,200,body)
        fake.connect.assert_called_once_with('COM99')
        self.assertTrue(board.committed)

    def test_ai_prompt_and_worker_use_bounded_read_only_generation(self):
        prompt=server.ai_prompt({'mode':'widget','prompt':'A blue butterfly'})
        self.assertIn('A blue butterfly',prompt)
        with self.assertRaises(ValueError):
            server.ai_prompt({'mode':'shell','prompt':'ignored'})
        result={'explanation':'Test only','widgets':[],'face':None}
        def run(command,**kwargs):
            self.assertIn('read-only',command)
            self.assertNotIn('--dangerously-bypass-approvals-and-sandbox',command)
            self.assertEqual(kwargs['input'],prompt)
            output=Path(command[command.index('--output-last-message')+1])
            output.write_text(json.dumps(result),encoding='utf-8')
            return subprocess.CompletedProcess(command,0,'','')
        server.JOBS['test-job']={'status':'running','created':0}
        server.AI_LOCK.acquire()
        with mock.patch.object(server.shutil,'which',return_value='fake-codex'),mock.patch.object(server.subprocess,'run',side_effect=run):
            server.run_ai('test-job',prompt)
        self.assertEqual(server.JOBS['test-job']['status'],'done')
        self.assertEqual(server.JOBS['test-job']['result'],result)

    def test_provider_endpoints_validate_and_normalize_without_connections(self):
        with mock.patch.dict('sys.modules',{'device':None}):
            status,body=self.request('/api/providers')
            self.assertEqual(status,200)
            self.assertEqual(body['providers'][1]['status'],'not-connected')
            status,data=self.request('/api/data?date=2026-10-28')
            self.assertEqual(status,200)
            self.assertEqual(len(data['days']),5)
            self.assertEqual(self.request('/api/data/preview',data)[0],200)
            data['current']['precipProbability']=200
            self.assertEqual(self.request('/api/data/preview',data)[0],400)
            status,calendar=self.request('/api/calendar/preview',{'referenceDate':'2026-10-28','items':[{'id':'1','summary':'Test','start':{'date':'2026-10-29'}}]})
            self.assertEqual(status,200)
            self.assertFalse(calendar['connected'])
            self.assertTrue(calendar['events'][0]['allDay'])


if __name__=='__main__':
    unittest.main()
