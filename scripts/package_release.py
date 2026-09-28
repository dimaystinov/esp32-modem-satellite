"""Package only tracked source and verified merged firmware; run after git add."""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, zipfile
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(); p.add_argument('version'); a=p.parse_args()
version=a.version
if not version.startswith('v') or any(c not in 'v0123456789.-' for c in version):
    raise SystemExit('Expected version like v1.0.0')
out=root/'dist'/version; out.mkdir(parents=True,exist_ok=True)
build=root/'build/c3'; prefix=f'modem-c3-{version}'
merged=(build/'modem_c3.ino.merged.bin').read_bytes()
assert len(merged)==4*1024*1024
for suffix,offset in [('bootloader',0),('partitions',0x8000),('app',0x10000)]:
    src=build/('modem_c3.ino.bin' if suffix=='app' else f'modem_c3.ino.{suffix}.bin')
    data=src.read_bytes(); assert merged[offset:offset+len(data)]==data, suffix
    shutil.copyfile(src,out/f'{prefix}-{suffix}.bin')
shutil.copyfile(build/'modem_c3.ino.merged.bin',out/f'{prefix}-full.bin')
shutil.copyfile(root/'docs/FLASHING.md',out/'FLASHING.md')
shutil.copyfile(root/'docs/RELEASE_NOTES.md',out/'RELEASE_NOTES.md')
source=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
with zipfile.ZipFile(out/f'{prefix}-kit.zip','w',zipfile.ZIP_DEFLATED) as z:
    for path in filter(None,source): z.write(root/path,f'{prefix}/{path}')
    z.write(out/f'{prefix}-full.bin',f'{prefix}/{prefix}-full.bin')
(out/'build-info.json').write_text(json.dumps({'version':version,'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'chip':'esp32c3','flash_size':4194304,'merged_offset':'0x0','arduino_cli':'1.5.1','esp32_core':'3.3.11','ArduinoJson':'6.21.5','U8g2':'2.36.17','mavlink_commit':'ce3aedef37c74c4c95111cf2a3f112a2dbd7e99e','hardware_tested':False},indent=2)+'\n')
(out/'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n' for f in sorted(out.iterdir()) if f.is_file() and f.name!='SHA256SUMS.txt'))
print(out)
