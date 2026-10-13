#!/usr/bin/env python3
"""Run same deterministic inputs against original/repaired production rules."""
import difflib,os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base='448dbe158595c1db9f1d2fc37caa15810c53de1c'
with tempfile.TemporaryDirectory(prefix='original-behavior-') as temp:
    temp=Path(temp);old=temp/'old';include=temp/'declarations';include.mkdir()
    names=['src/core/game_state.h','src/config/game_config.h','src/config/food_table.h',*[f'src/pet/{name}.{suffix}' for name in ('feeding','seriousness','evolution') for suffix in ('h','cpp')]]
    for name in names:
        target=old/name;target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(subprocess.check_output(['git','show',base+':'+name],cwd=root))
    # Declarations for the two existing legacy dependencies only. No Arduino runtime.
    (include/'Arduino.h').write_text('#pragma once\nstruct LogOnlySerial {template<class T> void println(T){} template<class... T> void printf(const char*,T...){} }; inline LogOnlySerial Serial;\n')
    (include/'esp_random.h').write_text('#pragma once\n#include <stdint.h>\nuint32_t esp_random();\n')
    traces=[]
    for label,folder in [('baseline',old),('repaired',root)]:
        binary=str(temp/label)
        command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Wno-unused-parameter',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-I'+str(folder/'src'),'-I'+str(include),str(root/'tests/host/behavior_trace.cpp')]+[str(folder/f'src/pet/{name}.cpp') for name in ('feeding','evolution','seriousness')]+['-o',binary]
        subprocess.run(command,cwd=root,check=True);traces.append(subprocess.check_output([binary],cwd=root,text=True))
    if traces[0]!=traces[1]:
        print(''.join(difflib.unified_diff(traces[0].splitlines(True),traces[1].splitlines(True))));raise SystemExit(1)
    print('PASS',len(traces[0].splitlines()),'original/repaired production behavior traces match including all pet fields and RNG counts')
