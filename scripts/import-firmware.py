"""Refresh the Studio's vendored FX sources from a Dual S3 checkout."""
from pathlib import Path
import shutil
import sys

ROOT = Path(__file__).resolve().parent.parent
dual = Path(sys.argv[1]).resolve()
main = ROOT / 'fx/main'
main.mkdir(parents=True, exist_ok=True)
for name in ('effects.cpp', 'new_effects.cpp', 'spatial.cpp', 'led_engine.cpp',
             'node_config.cpp', 'effects.h', 'new_effects.h', 'spatial.h',
             'effect_types.h', 'node_config.h', 'packets.h', 'board_ah_ray_dual_s3.h',
             'include/fx_package.h', 'include/ah_protocol.h'):
    target = main / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(dual / 'main' / name, target)
shutil.copytree(dual / 'components/FastLED/src', ROOT / 'fx/FastLED/src', dirs_exist_ok=True)
shutil.copy2(dual / 'components/FastLED/LICENSE', ROOT / 'fx/FastLED/LICENSE')
native = ROOT / 'fx/host'
native.mkdir(parents=True, exist_ok=True)
for name in ('Arduino.h', 'Preferences.h', 'host.cpp'):
    shutil.copy2(dual / 'emulator/native' / name, native / name)
host = native / 'host.cpp'
host.write_text(host.read_text().replace('extern "C" __declspec(dllexport)', 'extern "C"'))
print('Imported Dual S3 FX and bundled FastLED. Run build-wasm.py next.')
