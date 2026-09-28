from pathlib import Path
import os, subprocess
r=Path(__file__).resolve().parents[1]
(r/'build').mkdir(exist_ok=True)
cxx=os.environ.get('CXX','clang++')
base=[cxx,'-std=c++17','-fsanitize=address,undefined','-DCONFIG_IDF_TARGET_ESP32C3','-DARDUINO_USB_CDC_ON_BOOT=1','-Itests/stubs','-I.deps/MAVLink','-Wno-address-of-packed-member']
for name,sources in [('mavlink',['MavlinkUploader','Logger','AppState']),('core',['MissionParser','ProtocolCrc']),('frame',['FrameReceiver','ProtocolCrc','Logger','AppState'])]:
    subprocess.run(base+[f'tests/{name}_test.cpp']+[f'firmware/modem_c3/src/{s}.cpp' for s in sources]+['-o',f'build/{name}_test'],cwd=r,check=True)
    subprocess.run([str(r/f'build/{name}_test')],cwd=r,check=True)
subprocess.run([os.environ.get('NODE','node'),'tests/web_test.js'],cwd=r,check=True)
