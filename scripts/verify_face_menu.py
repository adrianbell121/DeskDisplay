"""Opt-in board check: publish, reorder, remove, cycle, preview, and reboot faces."""
import argparse, json, re, struct, sys, time, zlib
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / '.tools/host-python/Lib/site-packages'))
from device import connect, request as usb, save_png
from migrate_board_dashboard import request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', default='COM4')
    args = parser.parse_args()
    url = (ROOT / 'data/board-dashboard.url').read_text().strip().rstrip('/')
    out = ROOT / 'artifacts/face-menu'
    out.mkdir(parents=True, exist_ok=True)
    get = lambda path: request(url, '/api/' + path)
    menu, workspace = get('faces'), get('workspace')
    (out / 'before-menu.json').write_text(json.dumps(menu, indent=2), encoding='utf-8')
    (out / 'before-workspace.json').write_text(json.dumps(workspace), encoding='utf-8')
    original = menu['items']
    assert original and len(original) <= 30, 'Need room for two temporary test faces'

    def save(items, revision=None):
        return request(url, '/api/faces', {'revision': get('faces')['revision'] if revision is None else revision, 'items': items})

    def select(id):
        return request(url, '/api/faces/select', {'id': id})

    def reject(path, data, code):
        before = get('faces')
        try:
            request(url, '/api/' + path, data)
        except HTTPError as error:
            assert error.code == code, (error.code, error.read())
        else:
            raise AssertionError('Invalid menu accepted')
        assert get('faces')['items'] == before['items']

    def reboot(link, items):
        usb(link, 'restart', 'RESTARTING')
        time.sleep(3)
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            try:
                after = request(url, '/api/faces', timeout=3)
                if after['items'] == items and after['activeIndex'] == 0:
                    return
            except OSError:
                pass
            time.sleep(1)
        raise AssertionError('Menu order or first face did not survive restart')

    colors = (0xf800, 0x07e0)
    test = [{'id': 'verify-menu-' + str(i), 'name': 'Temporary menu test ' + str(i), 'mode': 'custom',
             'scene': (b'FLS2\0\0' + struct.pack('<H', color)).hex()} for i, color in enumerate(colors)]
    with connect(args.port) as link:
        try:
            saved = save(original + test)
            assert get('faces')['items'] == original + test
            for item, color in zip(test, colors):
                select(item['id'])
                assert get('status')['sceneCrc'] == zlib.crc32(bytes.fromhex(item['scene']))
                rows = re.findall(r'(?m)^([0-9a-f]{256})\r?$', usb(link, 'capture', 'FRAME END', timeout=12))
                assert len(rows) == 64 and all(row == f'{color:04x}' * 64 for row in rows)
                save_png(out / (item['id'] + '.png'), rows, scale=4)
            reordered = [test[1], original[1], test[0], *original[2:], original[0]] if len(original) > 1 else test + original
            save(reordered)
            reject('faces', {'revision': saved['revision'], 'items': original}, 409)
            select(reordered[0]['id'])
            for index in range(1, len(reordered) + 1):
                usb(link, 'mode next', 'MODE ')
                assert get('faces')['activeIndex'] == index % len(reordered)
            for bad in ([], [original[0], original[0]], [{**test[0], 'scene': '00'}], [test[0]] * 33):
                reject('faces', {'revision': get('faces')['revision'], 'items': bad}, 400)
            reject('faces/select', {'id': 'missing-face'}, 400)
            try:
                urlopen(Request(url + '/api/faces', data=b'{}', headers={'Origin': 'http://unrelated.invalid', 'X-Face-Lab': '1'}), timeout=5)
            except HTTPError as error:
                assert error.code == 403
            else:
                raise AssertionError('Unrelated website could change menu')
            request(url, '/api/scene', bytes.fromhex(test[0]['scene']), True)
            assert get('faces')['activeIndex'] == -1
            assert get('faces')['items'] == reordered
            usb(link, 'mode next', 'MODE ')
            assert get('faces')['activeIndex'] == 0
            reboot(link, reordered)
            save([item for item in reordered if item['id'] != test[0]['id']])
            assert get('workspace') == workspace, 'Removing a menu entry changed the editable library'
            print('PASS: add, show, reorder, button cycle, validation, preview isolation, removal, and reboot.', flush=True)
        finally:
            save(original)
            select(original[0]['id'])
        reboot(link, original)
    # Wi-Fi may drop a long download immediately after reconnecting.
    for attempt in range(3):
        try:
            assert get('workspace') == workspace
            break
        except OSError:
            if attempt == 2:
                raise
    status = get('status')
    assert status['sceneCrc'] == (zlib.crc32(bytes.fromhex(original[0]['scene'])) if original[0]['mode']=='custom' else 0), 'Original face was not preserved'
    report = {'menuFaces': len(original), 'workspaceUnchanged': True, 'originalMenuRestored': True,
              'rebootPersistence': True, 'buttonCycle': True, 'twoCustomFacesRendered': True,
              'previewIsolation': True, 'invalidMenuRejected': True, 'status': status}
    (out / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('PASS:', json.dumps(report))


if __name__ == '__main__':
    main()
