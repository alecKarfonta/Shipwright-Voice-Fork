"""Launch an exported pack with the opt-in Wolfos QA executable in a fresh profile.

No pack is installed into the normal game or player profile. Supply the QA
executable explicitly; use --mode review for the native comparison camera.
"""
import argparse
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--executable', type=Path, required=True)
parser.add_argument('--pack', type=Path, default=ROOT/'workspace/export/wolfos-master.o2r')
parser.add_argument('--config', type=Path, required=True, help='QA window/input template, never a normal save directory')
parser.add_argument('--mode', choices=['combat', 'review', 'eyes'], default='review')
args = parser.parse_args()
assert args.executable.is_file() and args.pack.is_file()
home = Path(tempfile.mkdtemp(prefix='soh-arpg-qa-wolfos-master-'))
app = home/'Wolfos Master Preview.app'
(app/'Contents/MacOS').mkdir(parents=True); (app/'Contents/Resources').mkdir()
executable = app/'Contents/MacOS/wolfos-master-preview'
shutil.copy2(args.executable, executable)
(app/'Contents/Info.plist').write_bytes(plistlib.dumps(dict(CFBundleExecutable=executable.name,
    CFBundleIdentifier='org.shipofharkinian.wolfos-master-'+home.name.rsplit('-', 1)[-1],
    CFBundleName='Wolfos Master Preview', CFBundlePackageType='APPL', NSHighResolutionCapable=True)))
for name in ('oot.o2r', 'soh.o2r', 'assets'):
    (app/'Contents/Resources'/name).symlink_to(REPO/'build-cmake/soh'/name)
(home/'mods').mkdir(); shutil.copy2(args.pack, home/'mods'/args.pack.name)
template = json.loads(args.config.read_text())
config = {key: template[key] for key in ('Window', 'ConfigVersion') if key in template}
config['CVars'] = {'gSettings': {'AltAssets': 1}, 'gEnhancements': {'ARPG': {'RemasteredEnemies': 0}}}
if 'gControllers' in template.get('CVars', {}): config['CVars']['gControllers'] = template['CVars']['gControllers']
config['Game'] = {'Patches Archive': str(home/'mods')}
(home/'shipofharkinian.json').write_text(json.dumps(config, indent=2)+'\n')
subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
for key in list(os.environ):
    if key.startswith('SOH_ARPG_'): os.environ.pop(key)
os.environ.update(SHIP_HOME=str(home), SOH_ARPG_WOLFOS_MODELS_QA='1', SOH_ARPG_WOLFOS_MASTER_QA='1')
if args.mode in ('review', 'eyes'):
    os.environ.update(SOH_ARPG_WOLFOS_MODELS_UI='1', SOH_ARPG_WOLFOS_COMPARE='native')
if args.mode == 'eyes':
    os.environ['SOH_ARPG_WOLFOS_EYES_UI'] = '1'
launch = dict(profile=str(home), app=str(app), mode=args.mode, pack=str(args.pack),
              result=str(home/'wolfos-model-results.json'), log=str(home/'preview.log'))
(ROOT/'workspace'/'last-master-launch.json').write_text(json.dumps(launch, indent=2)+'\n')
print(json.dumps(launch), flush=True)
log = os.open(home/'preview.log', os.O_WRONLY|os.O_CREAT|os.O_TRUNC, 0o600)
os.dup2(log, 1); os.dup2(log, 2)
os.chdir(REPO/'build-cmake/soh')
os.execv(str(executable), [executable.name])
