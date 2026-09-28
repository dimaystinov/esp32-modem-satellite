"""Install pinned dependencies. Run with Python 3 from any directory."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
def run(*args): subprocess.run(args, cwd=root, check=True)
run('arduino-cli', 'core', 'update-index', '--additional-urls', 'https://espressif.github.io/arduino-esp32/package_esp32_index.json')
run('arduino-cli', 'core', 'install', 'esp32:esp32@3.3.11', '--additional-urls', 'https://espressif.github.io/arduino-esp32/package_esp32_index.json')
run('arduino-cli', 'lib', 'install', 'ArduinoJson@6.21.5', 'U8g2@2.36.17')
dep = root / '.deps/MAVLink'
revision = 'ce3aedef37c74c4c95111cf2a3f112a2dbd7e99e'
if not dep.exists():
    dep.parent.mkdir(exist_ok=True)
    run('git', 'clone', '--no-checkout', 'https://github.com/mavlink/c_library_v2.git', str(dep))
if (dep / '.git').exists():
    run('git', '-C', str(dep), 'fetch', 'origin', revision)
    run('git', '-C', str(dep), 'checkout', '--detach', revision)
elif not (dep / '.ready').exists():
    raise SystemExit('Incomplete MAVLink dependency; move .deps/MAVLink aside and retry.')
(dep / 'MAVLink.h').write_text('#pragma once\n#include "common/mavlink.h"\n')
(dep / 'library.properties').write_text('name=MAVLinkC\nversion=2.0.0\nauthor=MAVLink\nmaintainer=MAVLink\nsentence=Generated MAVLink C headers\nparagraph=Pinned official c_library_v2 headers\ncategory=Communication\nurl=https://github.com/mavlink/c_library_v2\narchitectures=*\n')
print('Dependencies ready.')
