"""Resolve real GFx external images for an offline player, without changing the game.

Tag layouts follow GFx SDK3.3 GFxTagLoaders.cpp DefineExternalImage[2].
Installed AssetExplorer assemblies handle encrypted IWD/IFS, IWI and wavelets.
No zone scan, network fetch, checkerboard or replacement texture is used.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import subprocess
import tempfile
import zlib
from xml.sax.saxutils import escape


def external_image_refs(moviebytes):
    """Return actual tag1001/1009 records; malformed movies raise ValueError."""
    data = bytes(moviebytes)
    if len(data) < 12 or data[:3] not in (b'GFX', b'CFX', b'FWS', b'CWS'):
        raise ValueError('Expected a GFx/SWF movie')
    if data[:3] in (b'CFX', b'CWS'):
        data = data[:8] + zlib.decompress(data[8:])
    if struct.unpack_from('<I', data, 4)[0] != len(data):
        raise ValueError('Movie declared length differs from decoded bytes')
    pos = 8 + (5 + 4 * (data[8] >> 3) + 7) // 8 + 4
    refs = []
    while pos < len(data):
        if pos + 2 > len(data):
            raise ValueError('Truncated movie tag header')
        word, = struct.unpack_from('<H', data, pos)
        pos += 2
        code, length = word >> 6, word & 63
        if length == 63:
            if pos + 4 > len(data):
                raise ValueError('Truncated long tag header')
            length, = struct.unpack_from('<I', data, pos)
            pos += 4
        end = pos + length
        if end > len(data):
            raise ValueError('Movie tag exceeds the file')
        if code in (1001, 1009):
            id_size = 2 if code == 1001 else 4
            if length < id_size + 8:
                raise ValueError('Truncated external-image tag')
            char_id, = struct.unpack_from('<H' if id_size == 2 else '<I', data, pos)
            fmt, width, height = struct.unpack_from('<HHH', data, pos + id_size)
            cursor = pos + id_size + 6

            def string():
                nonlocal cursor
                if cursor >= end:
                    raise ValueError('Missing external-image string')
                count = data[cursor]
                cursor += 1
                if cursor + count > end:
                    raise ValueError('External-image string exceeds its tag')
                value = data[cursor:cursor + count].decode('utf-8', errors='strict')
                cursor += count
                return value

            export, filename = string(), string()
            refs.append(dict(tag=code, id=char_id, format=fmt, width=width,
                             height=height, export=export, filename=filename))
        pos = end
        if code == 0:
            if pos != len(data):
                raise ValueError('Bytes follow the movie End tag')
            break
    return refs


def _relative(filename):
    value = filename.replace('\\', '/')
    p = PurePosixPath(value)
    if not value or p.is_absolute() or ':' in value or '..' in p.parts or any(ord(c) < 32 for c in value):
        raise ValueError('External image has an unsafe or nonlocal filename: ' + repr(filename))
    if p.suffix.lower() != '.tga':
        raise ValueError('External image requires an unsupported player format: ' + filename)
    return p


def _runtime(ae_bin=None):
    candidates = [ae_bin, os.environ.get('CODOL_ASSETEXPLORER_BIN'),
                  r'D:/CoD/ae_survival_wt/src/Codol.SoundDumper/bin/Release/net9.0',
                  r'D:/CoD/CODOL Builds/CODOL-AssetExplorer/src/Codol.SoundDumper/bin/Release/net9.0']
    for candidate in candidates:
        if candidate and all((Path(candidate) / (name + '.dll')).is_file()
                             for name in ('Codol.Assets', 'Codol.FastFiles')):
            return Path(candidate).resolve()
    raise FileNotFoundError('Installed AssetExplorer runtime not found; set CODOL_ASSETEXPLORER_BIN')


_DECODER = r'''
using Codol.Assets.Game;
using Codol.Assets.Textures;
using System.Text.Json;
using System.Security.Cryptography;
using var game = GameInstallation.Open(args[0]);
var decoder = new TextureExtractor(game);
var requests = JsonSerializer.Deserialize<List<Request>>(File.ReadAllText(args[1]))!;
var results = new List<object>();
foreach (var request in requests) {
 try {
  LoadedTexture? texture = null;
  foreach (var name in request.names) {
   if (!decoder.TryFindIwd(name, out _, out _, out _)) continue;
   texture = decoder.Load(name); break;
  }
  if (texture is null) throw new FileNotFoundException("No matching installed IWI: " + string.Join(", ", request.names));
  if (texture.FaceCount != 1) throw new InvalidDataException("External UI image is not a 2D texture");
  var rgba = texture.GetSurface(0,0);
  if (rgba.Width < 1 || rgba.Height < 1 || rgba.Width > 16384 || rgba.Height > 16384)
   throw new InvalidDataException("Invalid decoded image dimensions");
  // Genuine decoded pixels, uncompressed32-bit TGA, top-left origin,8alpha bits.
  var tga = new byte[18 + rgba.Pixels.Length]; tga[2] = 2;
  BitConverter.GetBytes((ushort)rgba.Width).CopyTo(tga,12);
  BitConverter.GetBytes((ushort)rgba.Height).CopyTo(tga,14); tga[16]=32;tga[17]=0x28;
  for(int i=0;i<rgba.Pixels.Length;i+=4) {
   tga[18+i]=rgba.Pixels[i+2];tga[19+i]=rgba.Pixels[i+1];
   tga[20+i]=rgba.Pixels[i];tga[21+i]=rgba.Pixels[i+3];
  }
  Directory.CreateDirectory(Path.GetDirectoryName(request.output)!);
  File.WriteAllBytes(request.output,tga);
  results.Add(new {filename=request.filename, output=request.output, source=texture.Source,
    image=texture.Name, width=rgba.Width,height=rgba.Height,sha256=Convert.ToHexString(SHA256.HashData(tga))});
 } catch(Exception e) {results.Add(new {filename=request.filename,error=e.Message});}
}
File.WriteAllText(args[2],JsonSerializer.Serialize(results));
record Request(string filename,string output,string[] names);
'''


def _decoder(runtime):
    identity = _DECODER + ''.join(str(p.stat().st_mtime_ns) for p in sorted(runtime.glob('*.dll')))
    cache = Path(tempfile.gettempdir()) / 'codol-ui-inspector' / hashlib.sha256(identity.encode()).hexdigest()[:20]
    binary = cache / 'bin/Release/net9.0/UiImages.dll'
    if binary.is_file():
        return binary
    cache.mkdir(parents=True, exist_ok=True)
    (cache / 'Program.cs').write_text(_DECODER, encoding='utf-8')
    references = ''.join('<Reference Include="' + name + '"><HintPath>' +
                         escape(str(runtime / (name + '.dll'))) + '</HintPath></Reference>'
                         for name in ('Codol.Assets', 'Codol.FastFiles'))
    project = cache / 'UiImages.csproj'
    project.write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType>'
                       '<TargetFramework>net9.0</TargetFramework><ImplicitUsings>enable</ImplicitUsings>'
                       '<Nullable>enable</Nullable></PropertyGroup><ItemGroup>' + references + '</ItemGroup></Project>')
    # No packages or downloads: only the installed SDK and explicit local assemblies.
    config = cache / 'NuGet.Config'
    config.write_text('<configuration><packageSources><clear /></packageSources></configuration>')
    run = subprocess.run(['dotnet', 'build', str(project), '-c', 'Release', '--configfile', str(config),
                          '--nologo', '-v:q'], capture_output=True, text=True, timeout=90)
    if run.returncode:
        raise RuntimeError('Local image decoder build failed: ' + (run.stdout + run.stderr)[-3000:])
    return binary


def prepare_movie_assets(moviebytes, outdir, game, *, ae_bin=None):
    """Write genuine external TGAs beside a staged movie; return an explicit report.

    ``resolved`` contains provenance/dimensions/hash, ``unresolved`` records each
    filename and actionable reason. ``errors`` are setup/parser failures. This
    does not assert that the player rendered any image. Run off the GUI thread.
    """
    report = dict(references=[], resolved=[], unresolved=[], errors=[])
    outdir, game = Path(outdir).resolve(), Path(game).resolve()
    if outdir == game or game in outdir.parents:
        raise ValueError('Preview output must be outside the game installation')
    try:
        report['references'] = external_image_refs(moviebytes)
    except (ValueError, UnicodeError, zlib.error, struct.error) as error:
        report['errors'].append(str(error))
        return report
    pending = []
    seen = set()
    for ref in report['references']:
        filename = ref['filename']
        if filename.lower() in seen:
            continue
        seen.add(filename.lower())
        try:
            rel = _relative(filename)
            target = outdir.joinpath(*rel.parts)
            if outdir not in target.resolve().parents:
                raise ValueError('External image resolves outside preview output')
            # Actual loose TGAs can be used directly; no invented cached output.
            source = next((p for p in (game / rel, game / 'main' / rel, game / 'main/images' / rel)
                           if p.is_file()), None)
            if source:
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
                report['resolved'].append(dict(filename=filename, output=str(target), source=str(source),
                                               sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
                continue
            names = [str(rel.with_suffix('')), ref['export'].lstrip(',')]
            if names[0].startswith('images/'):
                names.append(names[0][7:])
            pending.append(dict(filename=filename, output=str(target), names=list(dict.fromkeys(n for n in names if n))))
        except (ValueError, OSError) as error:
            report['unresolved'].append(dict(filename=filename, error=str(error)))
    if not pending:
        return report
    try:
        runtime = _runtime(ae_bin)
        binary = _decoder(runtime)
        with tempfile.TemporaryDirectory(prefix='codol-ui-image-requests-') as temp:
            request, response = Path(temp) / 'request.json', Path(temp) / 'result.json'
            request.write_text(json.dumps(pending), encoding='utf-8')
            run = subprocess.run(['dotnet', str(binary), str(game), str(request), str(response)],
                                 capture_output=True, text=True, timeout=120)
            if run.returncode or not response.is_file():
                raise RuntimeError('Installed asset extraction failed: ' + (run.stdout + run.stderr)[-2000:])
            for item in json.loads(response.read_text(encoding='utf-8')):
                report['unresolved' if 'error' in item else 'resolved'].append(item)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        report['errors'].append(str(error))
        report['unresolved'].extend(dict(filename=p['filename'], error=str(error)) for p in pending)
    return report
