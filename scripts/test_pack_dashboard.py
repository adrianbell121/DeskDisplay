import gzip,re,tempfile,unittest,json
from pathlib import Path
from pack_dashboard import pack,automata_config

class DashboardBundleTests(unittest.TestCase):
 def test_automata_settings_reject_bad_timings_and_colors(self):
  config=json.loads((Path(__file__).resolve().parent.parent/'cellular-automata-config.json').read_text())
  with tempfile.TemporaryDirectory() as folder:
   root=Path(folder)
   for change in [{'time_reverse_frames':-1},{'time_reverse_frames':121},{'time_reverse_frames':120,'update_ms':60000},{'update_ms':0},{'time_hold_seconds':60},{'clear_every_minutes':7},{'fade_duration_seconds':0},{'color':'green'},{'gradient':'diagonal'}]:
    (root/'cellular-automata-config.json').write_text(json.dumps({**config,**change}))
    with self.assertRaises(ValueError):automata_config(root/'preview')
 def test_board_page_has_all_dependencies_and_no_computer_actions(self):
  root=Path(__file__).resolve().parent.parent/'preview'
  with tempfile.TemporaryDirectory() as folder:
   destination=Path(folder)/'assets.h';names=pack(root,destination);source=destination.read_text()
   compressed=re.findall(r'static const unsigned char asset_\d+\[\]=\{([\d,]+)\};',source)
   assets={name:gzip.decompress(bytes(map(int,content.split(',')))).decode() for name,content in zip(names,compressed)}
   page=assets['index.html']
   self.assertNotIn('id="ai-open"',page);self.assertNotIn('id="ai-dialog"',page);self.assertNotIn('id="deploy-open"',page)
   self.assertIn('window.BoardEditorBoot=',page);self.assertIn('id="export"',page);self.assertIn('id="widget-import"',page)
   self.assertIn('root.CellularAutomata=api',page)
   self.assertIn('board-face-menu-title',page)
   self.assertNotIn('board-face-menu.js',(root/'index.html').read_text())
   self.assertNotRegex(page,r'<script[^>]+src=');self.assertNotIn('rel="stylesheet"',page)
   for dependency in re.findall(r'(?:src|href)="([^"?]+\.(?:js|css))"',page):self.assertIn(dependency,assets)
   self.assertNotIn('wifi.local.json',source);self.assertNotIn('workspace.json',source)
