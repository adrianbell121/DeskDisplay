"""Wi-Fi provisioning via short USB commands; never prints credentials."""
import json
from pathlib import Path


def read_credentials(path):
    data=json.loads(Path(path).read_text(encoding='utf-8-sig'))
    if not isinstance(data,dict) or not isinstance(data.get('ssid'),str) or not isinstance(data.get('password'),str):
        raise ValueError('Wi-Fi file must contain string fields ssid and password')
    ssid=data['ssid'].encode('utf-8');password=data['password'].encode('utf-8')
    if not 1<=len(ssid)<=32 or b'\x00' in ssid:
        raise ValueError('Set ssid to a Wi-Fi name of 1–32 UTF-8 bytes')
    if password and (not 8<=len(password)<=63 or any(c<32 or c>126 for c in password)):
        raise ValueError('Password must be empty or 8–63 printable ASCII characters')
    return ssid,password


def provision(link,ssid,password,request):
    def send(command,expected):
        try:
            response=request(link,command,'WIFI ',timeout=5)
        except Exception:
            raise RuntimeError('Wi-Fi provisioning timed out; credentials were not printed') from None
        replies=[s.strip() for s in response.splitlines() if s.startswith('WIFI ')]
        if not replies or replies[-1]!=expected:
            raise RuntimeError('Wi-Fi provisioning was rejected; check firmware and credential lengths')
    send('wifi begin','WIFI READY')
    try:
        send('wifi ssid '+ssid.hex(),'WIFI SSID OK')
        for offset in range(0,len(password),32):
            send(f'wifi pass {offset} {password[offset:offset+32].hex()}','WIFI PASS OK')
        send('wifi commit','WIFI OK saved=1 connecting=1')
    except Exception:
        try: request(link,'wifi abort','WIFI ',timeout=1)
        except Exception: pass
        raise
    return 'Wi-Fi credentials saved on the board. Connecting; internet time will sync automatically.'
