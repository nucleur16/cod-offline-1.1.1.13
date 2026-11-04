"""Movie capture through real SDK AutoTest or an explicitly static FFDec fallback.

SDK4.3 Platform_WinAPI.cpp unconditionally calls SetForegroundWindow. The SDK
therefore runs on an owned private desktop, NEVER switched to. No desktop input
or game-process interaction occurs. Only SDK AutoTest executes movie input/AS.
"""
from __future__ import annotations
from contextlib import contextmanager
import ctypes
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import uuid
import xml.etree.ElementTree as ET
import zlib

CAPTURE_PLAYER = Path(r'D:\CoD\Scaleform\GFX_4.3_SDK\Bin\x64\Msvc11\GFxPlayer\GFxPlayer_GL_Release_Static.exe')


def scenario_xml(actions):
    """SDK FxPlayerAutotest.cpp commands; x/y are movie coordinates."""
    root = ET.Element('AutoTest')
    def instruction(name, **attrs):
        ET.SubElement(root, 'Instruction', name=name, **{k: str(v) for k, v in attrs.items()})
    for action in actions:
        op = action['op']
        if op == 'advance':
            delta = float(action.get('seconds', 1 / 30))
            if not math.isfinite(delta) or delta < 0:
                raise ValueError('Movie advance must be finite and nonnegative')
            instruction('AutoAdvance', delta=delta)
        elif op == 'capture':
            instruction('TakeScreenshot')
        elif op == 'frame':
            frame = int(action['frame'])
            if not 1 <= frame <= 10000:
                raise ValueError('Movie frame must be1..10000')
            instruction('Restart')
            for _ in range((frame - 1) // 10): instruction('StepForward_10')
            for _ in range((frame - 1) % 10): instruction('StepForward_1')
        elif op in ('click', 'move'):
            pos = dict(x=int(action['x']), y=int(action['y']))
            instruction('MouseMove', **pos)
            if op == 'click':
                instruction('MouseButton', keyDown=1, button=int(action.get('button', 0)), **pos)
                instruction('MouseButton', keyDown=0, button=int(action.get('button', 0)), **pos)
        elif op == 'key':
            instruction('KeyPress', key=action['key'], keyDown=1)
            instruction('KeyPress', key=action['key'], keyDown=0)
        else:
            raise ValueError('Unknown movie action: ' + op)
    instruction('Quit')
    return ET.tostring(root, encoding='unicode')


@contextmanager
def _private_desktop():
    if os.name != 'nt':
        raise RuntimeError('SDK capture requires Windows private-desktop isolation')
    user = ctypes.WinDLL('user32', use_last_error=True)
    user.CreateDesktopW.argtypes = [ctypes.c_wchar_p, ctypes.c_void_p, ctypes.c_void_p,
                                   ctypes.c_uint, ctypes.c_uint, ctypes.c_void_p]
    user.CreateDesktopW.restype = ctypes.c_void_p
    user.CloseDesktop.argtypes = [ctypes.c_void_p]
    user.CloseDesktop.restype = ctypes.c_int
    name = 'K33UiCapture_' + uuid.uuid4().hex
    handle = user.CreateDesktopW(name, None, None, 0, 0x1ff, None)
    if not handle: raise ctypes.WinError(ctypes.get_last_error())
    try:
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        startup.lpDesktop = name
        yield startup
    finally:
        user.CloseDesktop(handle)


def _timeline(movie):
    if movie[:3] in (b'CFX', b'CWS'): movie = movie[:8] + zlib.decompress(movie[8:])
    if len(movie) < 14: raise ValueError('Truncated movie header')
    pos = 8 + (5 + 4 * (movie[8] >> 3) + 7) // 8
    fps, frames = struct.unpack_from('<HH', movie, pos)
    return fps / 256, frames


def resolve_character(movie, character):
    """Resolve a numeric ID or exact SymbolClass name to an actual DefineSprite."""
    from zonekit import gfx
    data = bytes(movie)
    if data[:3] in (b'CFX', b'CWS'):
        data = (b'GFX' if data[:3] == b'CFX' else b'FWS') + data[3:8] + zlib.decompress(data[8:])
    symbols, sprites = {}, {}
    for code, start, end, _ in gfx.tags(data):
        word, = struct.unpack_from('<H', data, start)
        pos = start + (6 if word & 63 == 63 else 2)
        if code == 39:
            if pos + 4 > end: raise ValueError('Truncated DefineSprite')
            ident, count = struct.unpack_from('<HH', data, pos)
            sprites[ident] = count
        elif code == 76:
            if pos + 2 > end: raise ValueError('Truncated SymbolClass')
            count, = struct.unpack_from('<H', data, pos); pos += 2
            for _ in range(count):
                if pos + 2 > end: raise ValueError('Truncated SymbolClass ID')
                ident, = struct.unpack_from('<H', data, pos); pos += 2
                stop = data.find(b'\0', pos, end)
                if stop < 0: raise ValueError('Unterminated SymbolClass name')
                name = data[pos:stop].decode('utf-8');pos = stop + 1
                symbols[name] = ident
    if isinstance(character, int) or str(character).isdigit():
        ident = int(character)
    elif character in symbols:
        ident = symbols[character]
    else:
        raise ValueError('No exact SymbolClass name: ' + str(character))
    if ident not in sprites:
        raise ValueError('Character is not a DefineSprite: ' + str(character))
    return dict(id=ident, name=next((n for n, i in symbols.items() if i == ident), None), frames=sprites[ident])


def static_frames(actions, fps, count):
    """FFDec supports frame choice only; never silently treat input as AS."""
    cursor, result = 1, []
    for action in actions:
        op = action['op']
        if op == 'advance': cursor += max(0, int(float(action.get('seconds', 1 / 30)) * fps))
        elif op == 'frame': cursor = int(action['frame'])
        elif op == 'capture': result.append(max(1, min(cursor, count)))
        else: raise ValueError('FFDec static frames cannot execute movie action: ' + op)
    return sorted(set(result or [1]))


def _image_info(paths, outdir):
    result = []
    try:
        from PIL import Image
    except ImportError:
        return result
    for path in paths:
        with Image.open(path) as image:
            extrema = image.convert('RGBA').getextrema()
            result.append(dict(file=str(path.relative_to(outdir)), width=image.width,
                               height=image.height, uniform=all(a == b for a, b in extrema)))
    return result


def capture_movie(movie, outdir, game, actions=None, resolution='1280x720', player=CAPTURE_PLAYER,
                  *, backend='auto', ffdec=None, character=None):
    """auto tries SDK then labeled FFDec. Only SDK executes mouse/key/AS.

    FFDec accepts frame/advance/capture only. character selects a numeric
    DefineSprite ID or exact SymbolClass name, with backend=ffdec required.
    Uniform SDK frames are reported
    as such, not mistaken for initialized game HUD. Run off the GUI thread.
    """
    from ui_inspector_assets import prepare_movie_assets
    from ui_inspector import movie_bytes, DEFAULT_FFDEC
    if backend not in ('auto', 'sdk', 'ffdec'): raise ValueError('Unknown capture backend: ' + backend)
    if character is not None and backend != 'ffdec':
        raise ValueError('Character capture requires backend=ffdec; it is static sprite rendering only')
    if not re.fullmatch(r'[1-9]\d{0,3}[x:][1-9]\d{0,3}', resolution): raise ValueError('Expected resolution WIDTHxHEIGHT')
    outdir, game = Path(outdir).resolve(), Path(game).resolve()
    if outdir == game or game in outdir.parents: raise ValueError('Capture output must be outside the game installation')
    outdir.mkdir(parents=True, exist_ok=True)
    ffdec = Path(ffdec or DEFAULT_FFDEC)
    data = movie_bytes(Path(movie).resolve(), game, ffdec)
    selected = resolve_character(data, character) if character is not None else None
    path = outdir / 'preview.gfx'
    path.write_bytes(data)
    assets = prepare_movie_assets(data, outdir, game)
    actions = actions if actions is not None else [dict(op='advance', seconds=.25), dict(op='capture'), dict(op='advance', seconds=.5), dict(op='capture')]
    (outdir / 'scenario.xml').write_text(scenario_xml(actions), encoding='utf-8')
    run_root = outdir / ('capture_' + uuid.uuid4().hex[:12])
    run_root.mkdir()
    report = dict(movie=str(movie), actions=actions, captures=[], assets=assets, attempts=[], backend=None,
                  limitations=['CoD engine callbacks and live session state are not supplied.'])
    if selected is not None: report['character'] = selected
    paths = []
    if backend in ('auto', 'sdk'):
        # SDK GetScreenshotFilename: <autodir>/<test URL>/<test stem>_<index>.
        target = run_root / 'sdk/scenario.xml'
        target.mkdir(parents=True)
        command = [str(player), '-nh', '-ns', '-nck', '-res', resolution.replace('x', ':'), '-t', '10',
                   '-autotest', 'scenario.xml', '-autodir', str((run_root / 'sdk').relative_to(outdir)), str(path)]
        attempt = dict(backend='sdk43-autotest', player=str(player), command=command,
                       isolation='private Win32 desktop, never switched to', captures=[])
        log = run_root / 'sdk-player.log'
        try:
            with _private_desktop() as startup:
                run = subprocess.run(command, cwd=outdir, capture_output=True, timeout=30,
                                     startupinfo=startup, creationflags=subprocess.CREATE_NO_WINDOW)
            log.write_bytes(run.stdout + run.stderr)
            attempt['exit_code'] = run.returncode
            paths = sorted(p for p in target.iterdir() if p.suffix.lower() in ('.png', '.tga', '.bmp'))
            if run.returncode or not paths:
                attempt['error'] = 'SDK returned no completed capture' if not paths else 'SDK failed after capture'
                paths = []
        except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
            log.write_text(str(error), encoding='utf-8')
            attempt['error'] = str(error)
        attempt['log'] = str(log.relative_to(outdir))
        attempt['captures'] = [str(p.relative_to(outdir)) for p in paths]
        report['attempts'].append(attempt)
        if paths:
            report.update(backend='sdk43-autotest', player=str(player), exit_code=attempt['exit_code'], log=attempt['log'],
                          capabilities=dict(actionscript=True, movie_input=True, static_only=False))
    if not paths and backend in ('auto', 'ffdec'):
        static_backend = 'ffdec-static-sprite' if selected is not None else 'ffdec-static-frames'
        attempt = dict(backend=static_backend, player=str(ffdec))
        log = run_root / 'ffdec.log'
        try:
            fps, count = _timeline(data)
            if selected is not None: count = selected['frames']
            frames = static_frames(actions, fps, count)
            target = run_root / 'static'
            target.mkdir()
            kind = 'sprite' if selected is not None else 'frame'
            selection = (str(selected['id']) + ':' if selected is not None else '') + ','.join(map(str, frames))
            command = [str(ffdec), '-onerror', 'abort', '-exportTimeout', '25', '-select', selection]
            if selected is not None: command += ['-selectid', str(selected['id'])]
            command += ['-format', kind + ':png', '-export', kind, str(target), str(path)]
            attempt.update(command=command, frames=frames)
            startup = None
            if os.name == 'nt':
                startup = subprocess.STARTUPINFO();startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow = 0
            run = subprocess.run(command, cwd=outdir, capture_output=True, timeout=35, startupinfo=startup)
            log.write_bytes(run.stdout + run.stderr)
            attempt['exit_code'] = run.returncode
            paths = sorted(target.rglob('*.png'))
            if run.returncode or not paths:
                attempt['error'] = 'FFDec returned no completed static frame'; paths = []
            else:
                report.update(backend=static_backend, player=str(ffdec), exit_code=run.returncode,
                              log=str(log.relative_to(outdir)), capabilities=dict(actionscript=False, movie_input=False, static_only=True))
                report['limitations'].append('Static timeline only: ActionScript and interaction were not executed.')
        except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
            attempt['error'] = str(error);log.write_text(str(error), encoding='utf-8')
        attempt['log'] = str(log.relative_to(outdir))
        report['attempts'].append(attempt)
    report['captures'] = [str(p.relative_to(outdir)) for p in paths]
    report['capture_info'] = _image_info(paths, outdir)
    if report['capture_info'] and all(i['uniform'] for i in report['capture_info']):
        report['limitations'].append('All captured frames are uniform: the movie may require CoD initialization callbacks; visible HUD content is not verified.')
    report['scope'] = ('Real SDK rendering/ActionScript with internal movie input only.' if report['backend'] == 'sdk43-autotest'
                       else 'FFDec static timeline rendering; no ActionScript or interactive navigation.')
    (outdir / 'capture-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if not paths: raise RuntimeError('Movie capture failed; inspect ' + str(outdir / 'capture-report.json'))
    return report
