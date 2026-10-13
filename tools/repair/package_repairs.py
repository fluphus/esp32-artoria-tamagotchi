#!/usr/bin/env python3
"""Package reviewed original repairs with an isolated index and verify in a clean clone."""
import hashlib,json,os,subprocess,sys,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base='448dbe158595c1db9f1d2fc37caa15810c53de1c'
report=root/'reports/original-repair';(report/'evidence').mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='original-portable-') as directory:
 temp=Path(directory);env=dict(os.environ,GIT_INDEX_FILE=str(temp/'index'))
 subprocess.run(['git','read-tree',base],cwd=root,env=env,check=True)
 paths=[p for p in subprocess.check_output(['git','diff','--name-only',base],cwd=root,text=True).splitlines()
        if not p.startswith('reports/')]
 paths+= [p for p in subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=root,text=True).splitlines()
          if p.startswith(('src/','tests/','tools/repair/'))]
 subprocess.run(['git','add','--',*paths],cwd=root,env=env,check=True)
 data=subprocess.check_output(['git','diff','--cached','--binary',base],cwd=root,env=env)
 patch=report/'original-save-repairs.patch';patch.write_bytes(data)
 clone=temp/'clean'
 subprocess.run(['git','clone','--quiet','--no-hardlinks',str(root),str(clone)],check=True)
 subprocess.run(['git','checkout','--quiet','--detach',base],cwd=clone,check=True)
 receipt={'base':base,'patch_bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'commands':[]}
 with (report/'evidence/additional-portable-patch.log').open('w') as log:
  for command in (['git','apply','--check',str(patch)],['git','apply',str(patch)],[sys.executable,'tests/host/run_all.py']):
   log.write('+ '+' '.join(command)+'\n');log.flush()
   result=subprocess.run(command,cwd=clone,stdout=log,stderr=subprocess.STDOUT)
   receipt['commands'].append({'command':command,'exit_code':result.returncode})
   if result.returncode:raise RuntimeError('portable verification failed; see log')
  for path in paths:
   if (root/path).is_file():assert (clone/path).read_bytes()==(root/path).read_bytes(),path
  log.write('PASS clean baseline apply/check, complete offline regression and delivered file identity\n')
 (report/'evidence/additional-patch-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('PASS original portable patch',len(data),'bytes',receipt['sha256'])
