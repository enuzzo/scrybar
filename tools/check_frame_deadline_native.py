#!/usr/bin/env python3
"""Exercise the actual firmware loop with deterministic clock/I/O seams.

All compiler output stays outside Dropbox. This is scheduling policy evidence,
not proof of physical frame rate, I2C timing or panel scanout.
"""
import importlib.util
import pathlib
import subprocess
import tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native_renderer',ROOT/'tools/render_mac_stats_native.py')
renderer=importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)
source=(ROOT/'scrybar.ino').read_text()
code=renderer.block(source,source.index('void loop() {'))
with tempfile.TemporaryDirectory(prefix='scrybar-loop-policy-') as folder:
    folder=pathlib.Path(folder)
    (folder/'current_firmware_loop.inc').write_text(code+'\n')
    target=folder/'loop-policy-test'
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-pedantic',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    '-I',str(folder),str(ROOT/'tests/loop_frame_deadline_test.cpp'),
                    '-o',str(target)],check=True,timeout=60)
    subprocess.run([str(target)],check=True,timeout=15)
print('PASS: actual firmware loop deadline boundary, rollover, pending and idle policy (ASan/UBSan).')
