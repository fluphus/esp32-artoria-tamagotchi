#!/usr/bin/env python3
import subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
for name in ('check_original','run_save_repair','run_behavior','run_application_repair'):
    command=[sys.executable,f'tests/host/{name}.py'];print('+',*command,flush=True)
    result=subprocess.run(command,cwd=root);print('EXIT',result.returncode,name,flush=True)
    if result.returncode:raise SystemExit(result.returncode)
print('PASS original Artoria offline regression')
