#!/usr/bin/env python3
import os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='original-save-repair-') as temp:
    binary=str(Path(temp)/'test')
    command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Isrc','tests/host/save_repair_test.cpp','src/core/save_manager.cpp','src/core/save_io.cpp','src/core/save_workflows.cpp','-o',binary]
    print('+',shlex.join(command),flush=True)
    subprocess.run(command,cwd=root,check=True)
    subprocess.run([binary],cwd=root,check=True)
