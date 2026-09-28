"""Build C3; optionally flash via --port. Never upload after a failed build."""
from pathlib import Path
import argparse, subprocess, sys
root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--port', help='Upload after build, e.g. COM5 or /dev/cu.usbmodem101')
a = p.parse_args()
subprocess.run([sys.executable, str(root/'scripts/embed_web.py')], check=True)
fqbn = 'esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio,FlashSize=4M'
subprocess.run(['arduino-cli', 'compile', '--fqbn', fqbn, '--libraries', '.deps', '--build-path', 'build/cache', '--output-dir', 'build/c3', 'firmware/modem_c3'], cwd=root, check=True)
if a.port:
    subprocess.run(['arduino-cli', 'upload', '--fqbn', fqbn, '--port', a.port, '--input-dir', 'build/c3', 'firmware/modem_c3'], cwd=root, check=True)
