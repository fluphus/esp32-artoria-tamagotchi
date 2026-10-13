#!/usr/bin/env python3
"""Verify original rule/resource identity, v9 provenance and TARGET ABI."""
import hashlib,json,os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base='448dbe158595c1db9f1d2fc37caa15810c53de1c'
manifest=json.loads((root/'tests/fixtures/v9/manifest.json').read_text())
# Every existing tracked file outside the narrow repair list remains byte-identical.
allowed={'platformio.ini','src/main.cpp','src/core/save_manager.cpp','src/core/save_manager.h','src/display/display_config.h','src/display/display_renderer.cpp','src/core/time_manager.cpp','src/display/DisplayManager.cpp','src/input/button_driver.cpp','src/input/button_driver.h','SaveManager/save_manager.py'}
# Documentation edits are authorized for publication; rule/ABI/resource identity remains strict.
allowed.update({'README.md','README_zh.md','README_ja.md'})
files=subprocess.check_output(['git','ls-tree','-r','-z','--name-only',base],cwd=root,text=True).rstrip('\0').split('\0')
for name in files:
    if name in allowed:continue
    expected=subprocess.check_output(['git','show',base+':'+name],cwd=root)
    assert (root/name).read_bytes()==expected, 'unexpected original change: '+name
assert hashlib.sha256((root/'src/core/game_state.h').read_bytes()).hexdigest()==manifest['source_header_sha256']
for name,digest in manifest['fixtures'].items():
    assert hashlib.sha256((root/'tests/fixtures/v9'/name).read_bytes()).hexdigest()==digest,name
layout=manifest['layout']
source='#include "core/game_state.h"\n#include "pet/gallery.h"\n#include <stddef.h>\n'
for typ,key in [('PetState','pet'),('SaveHeader','header'),('DeviceState','device'),('DailyFeedState','daily'),('GalleryData','gallery')]:
    source+=f'static_assert(sizeof({typ})=={layout[key+"_size"]},"{typ} size");\n'
for typ,key in [('PetState','pet'),('SaveHeader','header'),('DeviceState','device')]:
    for field,offset in layout[key+'_offsets'].items():source+=f'static_assert(offsetof({typ},{field})=={offset},"{typ}.{field}");\n'
compiler=os.environ.get('XTENSA_CXX',str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++'))
with tempfile.TemporaryDirectory(prefix='original-target-abi-') as temp:
    p=Path(temp);(p/'abi.cpp').write_text(source)
    subprocess.run([compiler,'-std=gnu++17','-Isrc','-c',str(p/'abi.cpp'),'-o',str(p/'abi.o')],cwd=root,check=True)
print('PASS original files outside repair scope unchanged; original v9 header, 8 target fixtures and current Xtensa ABI sizes/offsets verified')
