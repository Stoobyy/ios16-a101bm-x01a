#!/usr/bin/env python3
"""Create a local two-line Wi-Fi credential file without putting a password in shell history."""
import argparse,getpass,os
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('output',type=Path)
a=p.parse_args()
ssid=input('SSID: ')
password=getpass.getpass('WPA2 passphrase: ')
if not 1 <= len(ssid.encode()) <= 32 or '\n' in ssid or '\r' in ssid:
 p.error('SSID must contain 1..32 UTF-8 bytes without newlines')
if not (8 <= len(password.encode()) <= 63 or len(password)==64 and all(c in '0123456789abcdefABCDEF' for c in password)):
 p.error('invalid WPA2 passphrase length/format')
a.output.parent.mkdir(parents=True,exist_ok=True)
fd=os.open(a.output,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
with os.fdopen(fd,'w') as f:f.write(ssid+'\n'+password+'\n')
print('Created local credentials file; do not commit or share it.')
