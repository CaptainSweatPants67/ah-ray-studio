"""Compile the actual Dual S3 C++/FastLED renderer to a browser WASM reactor."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MAIN = ROOT / 'fx/main'
FASTLED = ROOT / 'fx/FastLED/src'
BUILD = ROOT / '.build'


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == '{': depth += 1
        if source[end] == '}': depth -= 1
        end += 1
    return source[start:end] + '\n'


def build():
    BUILD.mkdir(exist_ok=True)
    zig = os.environ.get('ZIG') or shutil.which('zig')
    if not zig: raise SystemExit('Set ZIG to a Zig 0.13.0 executable or put zig on PATH.')
    engine = (MAIN / 'led_engine.cpp').read_text(encoding='utf-8')
    helpers = engine[engine.index('static RenderParams buildRenderParams('):
                     engine.index('static void renderIdentifyStrip(')]
    (BUILD / 'render_pipeline.inc').write_text(function(engine, 'static NodePlacement buildNodePlacement(') +
        helpers + function(engine, 'static void renderStrip('))
    (BUILD / 'virtual_lookup.inc').write_text(function((MAIN / 'node_config.cpp').read_text(encoding='utf-8'), 'bool nodeVirtualNodeForPixel('))
    crgb = (FASTLED / 'crgb.cpp').read_text(encoding='utf-8')
    (BUILD / 'fastled_rgb.inc').write_text(function(crgb, 'CRGB &CRGB::nscale8(') +
                                         function(crgb, 'CRGB &CRGB::operator+=('))
    sources = [ROOT / 'fx/host/host.cpp', *(MAIN / n for n in ('effects.cpp', 'new_effects.cpp', 'spatial.cpp')),
               *(FASTLED / n for n in ('hsv2rgb.cpp', 'fl/fill.cpp', 'fl/colorutils.cpp', 'noise.cpp', 'lib8tion.cpp'))]
    exports = ['malloc', 'free', 'lab_reset', 'lab_version', 'lab_node_capacity', 'lab_effect_count',
               'lab_spatial_count', 'lab_effect_name', 'lab_spatial_name', 'lab_value_name', 'lab_configure', 'lab_render']
    target = ROOT / 'public/studio/fx.wasm'
    # Only header-only C++ containers are used; do not link the unused threaded libc++ runtime.
    flags = ['-std=c++17', '-fno-exceptions', '-fno-rtti', '-O2', '-target', 'wasm32-wasi',
           '-DFASTLED_STUB_IMPL=1', '-DFASTLED_NO_AUTO_NAMESPACE=1',
           '-ffunction-sections', '-fdata-sections',
           '-Wno-deprecated-declarations', '-Wno-pragma-pack', '-Wno-unknown-warning-option',
           *('-I' + str(p) for p in (ROOT / 'fx/host', BUILD, MAIN, MAIN / 'include', FASTLED)),
           ]
    env = os.environ.copy()
    env['ZIG_GLOBAL_CACHE_DIR'] = str(BUILD / 'zig-cache')
    logs = []
    objects = []
    for i, source in enumerate(sources):
        obj = BUILD / f'fx-{i}.o'
        objects.append(obj)
        cmd = [str(zig), 'c++', *flags, '-c', str(source), '-o', str(obj)]
        result = subprocess.run(cmd, env=env, capture_output=True, text=True)
        logs.append(result.stdout + result.stderr)
        if result.returncode:
            (BUILD / 'compiler.log').write_text(''.join(logs))
            raise SystemExit(result.stderr[-12000:])
    cmd = [str(zig), 'cc', '-target', 'wasm32-wasi', '-mexec-model=reactor', '-Wl,--gc-sections',
           '-Wl,--export-memory', *('-Wl,--export=' + name for name in exports),
           *(str(p) for p in objects), '-o', str(target)]
    result = subprocess.run(cmd, env=env, capture_output=True, text=True)
    logs.append(result.stdout + result.stderr)
    (BUILD / 'compiler.log').write_text(''.join(logs))
    if result.returncode: raise SystemExit(result.stderr[-12000:])
    digest = hashlib.sha256()
    for p in sorted((ROOT / 'fx').rglob('*'), key=lambda p:p.relative_to(ROOT).as_posix()):
        if p.is_file():
            digest.update(p.relative_to(ROOT).as_posix().encode())
            digest.update(p.read_bytes().replace(b'\r\n',b'\n'))
    header=(MAIN / 'include/fx_package.h').read_text(encoding='utf-8')
    version=int(re.search(r'#define AH_FX_PACKAGE_VERSION (\d+)',header)[1])
    manifest = dict(packageVersion=version, sourceFingerprint=digest.hexdigest(),
                    wasmSha256=hashlib.sha256(target.read_bytes()).hexdigest(), target='wasm32-wasi', compiler='Zig 0.13.0')
    (ROOT / 'public/studio/fx-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Built {target} ({target.stat().st_size:,} bytes)')


if __name__ == '__main__': build()
