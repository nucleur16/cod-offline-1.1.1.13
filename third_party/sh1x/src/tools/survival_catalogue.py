"""Write the locally playable Survival map catalogue from mounted scripts and map archives."""
import argparse
from pathlib import Path


def available_maps(game):
    game = Path(game)
    root = game / 'main/sh1x_rawfiles'
    common = game / 'client_shipRetail/chinese/common_gametype_survival.ff'
    if not common.is_file() or not (root / 'maps/mp/gametypes/survival.gsc').is_file():
        return []
    dispatch = (root / 'maps/mp/gametypes/_survival_levels.gsc').read_text()
    maps = []
    for placement in sorted((root / 'maps/mp/survival').glob('mp_*.gsc')):
        name = placement.stem
        text = placement.read_text()
        if ('case "' + name + '":') not in dispatch:
            continue
        if not all(token in text for token in ('level.survival_player_starts[', 'level.survival_leaders[', 'level.survival_kiosk_spots[')):
            continue
        if not any((folder / (name + '.ff')).is_file() for folder in (game / 'client_shipRetail/chinese', game / 'usermaps' / name)):
            continue
        maps.append(name)
    return maps


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--game', required=True, type=Path)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    maps = available_maps(args.game)
    target = args.out or args.game / 'p2p/survival_maps.txt'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text('# Installed MP archives with authored Survival starts, squads and kiosks.\n' + '\n'.join(maps) + '\n', encoding='ascii')
    print(str(len(maps)) + ' Survival maps: ' + ', '.join(maps))
    return 0 if maps else 1

if __name__ == '__main__':
    raise SystemExit(main())

