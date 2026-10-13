#!/usr/bin/env python3
"""Real original Python tool connected to the production main.cpp protocol."""
import argparse,collections,importlib.util,os,select,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--bridge',required=True);args=p.parse_args()
spec=importlib.util.spec_from_file_location('save_tool',root/'SaveManager/save_manager.py')
mod=importlib.util.module_from_spec(spec);sys.modules[spec.name]=mod;spec.loader.exec_module(mod)
checks=0
def check(value):
 global checks;checks+=1;assert value
class Mock:
 def __init__(self,lines):self.lines=iter(lines)
 def readline(self):return next(self.lines,b'')
 def write(self,data):return len(data)
 def flush(self):pass
 def __enter__(self):return self
 def __exit__(self,*args):pass
class Bridge:
 def __init__(self,drop=False):
  self.process=subprocess.Popen([args.bridge,'--bridge'],cwd=root,stdin=subprocess.PIPE,stdout=subprocess.PIPE,bufsize=0)
  self.buffer=b'';self.fragments=collections.deque();self.drop=drop;self.dropped=False;self.sent=[]
 def write(self,data):self.sent.append(data);return self.process.stdin.write(data)
 def flush(self):self.process.stdin.flush()
 def readline(self):
  if self.fragments:return self.fragments.popleft()
  if b'\n' not in self.buffer:
   if not select.select([self.process.stdout],[],[],0.01)[0]:return b''
   self.buffer+=os.read(self.process.stdout.fileno(),8192)
  if b'\n' not in self.buffer:return b''
  line,_,self.buffer=self.buffer.partition(b'\n');line+=b'\n'
  if self.drop and not self.dropped and b'[SaveImport] RECV ' in line:self.dropped=True;return b''
  self.fragments.append(line[len(line)//2:]);return line[:len(line)//2]
 def __enter__(self):return self
 def __exit__(self,*args):
  self.process.stdin.close();self.process.wait(timeout=5);self.process.stdout.close()
with tempfile.TemporaryDirectory(prefix='original-tool-') as temp:
 target=Path(temp)/'backup.bin';target.write_bytes(b'preserve-existing')
 fragment=Mock([b'SAVE_EXPORT_BE',b'GIN slot=0 bytes=84\n'])
 check(mod._read_line(fragment)=='');check(mod._read_line(fragment)=='SAVE_EXPORT_BEGIN slot=0 bytes=84')
 for ending in (b'SAVE_EXPORT_END slot=0 bytes=84\n',b'SAVE_EXPORT_END slot=1 bytes=1\n'):
  mod._open_serial=lambda *a:Mock([b'SAVE_EXPORT_BEGIN slot=0 bytes=84\n',b'SAVE_EXPORT_DATA AA\n',ending])
  try:mod.cmd_export('MOCK',115200,0.2,0,target);raise AssertionError('incomplete export accepted')
  except ValueError:pass
  check(target.read_bytes()==b'preserve-existing')
 for pet_only in (False,True):
  bridge=Bridge();mod._open_serial=lambda *a:bridge
  mod.cmd_export('MOCK',115200,1,0,target,pet_only=pet_only)
  check(len(target.read_bytes())==(72 if pet_only else 84))
 for fixture in ('adult-white.full.bin','adult-white.pet.bin'):
  bridge=Bridge(drop=True);mod._open_serial=lambda *a:bridge
  mod.cmd_import('MOCK',115200,0.2,1,root/'tests/fixtures/v9'/fixture)
  check(bridge.dropped)
  data=[line for line in bridge.sent if line.startswith(b'SAVE_IMPORT_DATA')]
  check(data[0]==data[1] and b'@0 ' in data[0])
 # Unindexed old firmware: an ACK timeout must never resend an append packet.
 old=Mock([b'[SaveImport] READY slot=1 target_bytes=84 (legacy=72)\n']);old.sent=[]
 old.write=lambda data:old.sent.append(data) or len(data);mod._open_serial=lambda *a:old
 try:mod.cmd_import('MOCK',115200,0.01,1,root/'tests/fixtures/v9/adult-white.full.bin');raise AssertionError('missing ACK accepted')
 except TimeoutError:pass
 check(len([line for line in old.sent if line.startswith(b'SAVE_IMPORT_DATA')])==1)
print('PASS original Python/production protocol',checks,'checks')
