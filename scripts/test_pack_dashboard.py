import gzip,re,tempfile,unittest
from pathlib import Path
from pack_dashboard import pack

class DashboardBundleTests(unittest.TestCase):
 def test_board_page_has_all_dependencies_and_no_computer_actions(self):
  root=Path(__file__).resolve().parent.parent/'preview'
  with tempfile.TemporaryDirectory() as folder:
   destination=Path(folder)/'assets.h';names=pack(root,destination);source=destination.read_text()
   compressed=re.findall(r'static const unsigned char asset_\d+\[\]=\{([\d,]+)\};',source)
   assets={name:gzip.decompress(bytes(map(int,content.split(',')))).decode() for name,content in zip(names,compressed)}
   page=assets['index.html']
   self.assertNotIn('id="ai-open"',page);self.assertNotIn('id="ai-dialog"',page);self.assertNotIn('id="deploy-open"',page)
   self.assertIn('window.BoardEditorBoot=',page);self.assertIn('id="export"',page);self.assertIn('id="widget-import"',page)
   self.assertNotRegex(page,r'<script[^>]+src=');self.assertNotIn('rel="stylesheet"',page)
   for dependency in re.findall(r'(?:src|href)="([^"?]+\.(?:js|css))"',page):self.assertIn(dependency,assets)
   self.assertNotIn('wifi.local.json',source);self.assertNotIn('workspace.json',source)
