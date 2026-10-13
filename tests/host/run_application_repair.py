#!/usr/bin/env python3
import os,shlex,subprocess,tempfile,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
sources=['tests/host/application_repair_test.cpp','src/core/save_manager.cpp','src/core/save_io.cpp','src/core/save_workflows.cpp','src/core/time_manager.cpp']+[f'src/pet/{n}.cpp' for n in ('feeding','evolution','seriousness','gallery')]+[f'src/input/{n}.cpp' for n in ('button_driver','input_manager','menu_controller')]+[f'src/display/{n}.cpp' for n in ('DisplayManager','display_renderer','display_assets','asset_loader')]+[f'src/presentation/{n}.cpp' for n in ('animation_queue','animation_director','idle_resolver','pet_state_query')]+['src/display/generated_assets/generated_asset_data.cpp']
with tempfile.TemporaryDirectory(prefix='original-app-') as temp:
 command=[os.environ.get('CXX','g++'),'-std=c++17','-Wall','-Wextra','-Wno-unused-parameter','-Wno-format','-Wno-return-type','-DDISPLAY_BACKEND_SERIAL_PLACEHOLDER=1','-DDISPLAY_BACKEND_TFT_ESPI=0',*shlex.split(os.environ.get('HOST_TEST_FLAGS','')),'-Itests/host/peripherals','-Isrc',*sources,'-o',str(Path(temp)/'test')]
 print('+',shlex.join(command),flush=True);subprocess.run(command,cwd=root,check=True);subprocess.run([str(Path(temp)/'test')],cwd=root,check=True);subprocess.run([sys.executable,'tests/host/tool_repair_test.py','--bridge',str(Path(temp)/'test')],cwd=root,check=True)
