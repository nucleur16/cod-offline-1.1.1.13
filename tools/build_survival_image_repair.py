"""Pack only installed Survival zone images absent from the target game.

Use the retained AssetExplorer implementation for decryption, IWI conversion,
encrypted IWD writing, CRC readback and source/target decoded surface checks.
The filtered IFS mount reads only referenced images instead of all donor files.
Does not deploy or alter the target install. Example:
  python tools/build_survival_image_repair.py --game "D:/CoD/CODOL Builds/Call of Duty Online 1.1.1.13" --source "D:/CoD/CODOL Builds/CODOL_2.6.9.7" --ae-bin "D:/CoD/ae_survival_wt/src/Codol.SoundDumper/bin/Release/net9.0" --out "D:/CoD/CODOL Builds/survival_staging/image_repair/iw_model_18.iwd"
"""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile
from xml.sax.saxutils import escape


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--ae-bin', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    zone = args.game / 'client_shipRetail/chinese/common_gametype_survival.ff'
    if not zone.is_file():
        parser.error('Installed Survival zone is absent')
    for dll in ('CodolSoundDumper', 'Codol.Assets', 'Codol.FastFiles'):
        if not (args.ae_bin / (dll + '.dll')).is_file():
            parser.error('AssetExplorer runtime DLL is absent: ' + dll)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='codol-survival-images-') as temp:
        build = Path(temp)
        shutil.copyfile(Path(__file__).with_name('survival_image_repair') / 'Program.cs', build / 'Program.cs')
        references = ''.join('<Reference Include="' + name + '"><HintPath>' + escape(str((args.ae_bin / (name + '.dll')).resolve())) + '</HintPath></Reference>'
                             for name in ('CodolSoundDumper', 'Codol.Assets', 'Codol.FastFiles'))
        project = build / 'Repair.csproj'
        project.write_text('<Project Sdk="Microsoft.NET.Sdk"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net9.0</TargetFramework><ImplicitUsings>enable</ImplicitUsings></PropertyGroup><ItemGroup>' + references + '</ItemGroup></Project>')
        subprocess.run(['dotnet', 'build', str(project), '--nologo', '-v:q'], check=True)
        subprocess.run(['dotnet', str(build / 'bin/Debug/net9.0/Repair.dll'), str(zone.resolve()),
                        str(args.game.resolve()), str(args.source.resolve()), str(args.out.resolve())], check=True)


if __name__ == '__main__':
    main()
