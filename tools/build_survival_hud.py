"""Preserve C1's native HUD, hiding its MP score and streak menus in Survival.

Input is MenuCommands' resolved 1.1.1.13 patch_common_mp zone-menus dump, kept as
.menu source (sources/survival_hud_visibility.original.menu) so rebuilds need no game
extraction. The written menufile (menufile/survival_hud_visibility.menu, which the zone
build compiles) also carries the selectable HUD styles' items for these two menus
(styled(); docs/HUD_STYLES_20261112.md).
"""
import copy
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from zonekit import menu_source, menus

ROOT = Path(__file__).resolve().parents[1]
DIR = ROOT / 'zones/survival_hud_postgfx_mp/menufile'
NAME = 'ui_mp/scriptmenus/survival_hud_visibility.menu'
ORIGINAL = ROOT / 'zones/survival_hud_postgfx_mp/sources/survival_hud_visibility.original.menu'
HEADER = [NAME + ': written by tools/build_hud_styles.py (tools/build_survival_hud.py styled()) from',
          'sources/survival_hud_visibility.original.menu, components/survival_status.menu and the HUD styles: edit',
          'those, not this file. IW menu language: zonekit/menu_source.py.']


def original():
    """1.1.1.13's own killstreaks_hd and scorebar_hd (patch_common_mp), from their .menu source."""
    return menu_source.load(ORIGINAL, NAME)


def component():
    """The Survival status items (tools/build_survival_status.py)."""
    return menu_source.load(DIR.parent / 'components/survival_status.menu',
                            'ui_mp/scriptmenus/survival_status_component.menu')

def operator(name):
    return {'kind': 'expressionEntry', 'raw': struct.pack('<III', 0, menus.OP_1113[name], 0).hex(), 'p': {}}

def string(value):
    return {'kind': 'expressionEntry', 'raw': struct.pack('<III', 1, 2, 0).hex(), 'p': {'8': value}}

def scoped(expression, survival):
    old = expression['p']['4'] if expression else [
        {'kind':'expressionEntry', 'raw':struct.pack('<III', 1, 0, 1).hex(), 'p':{}}]
    entries = [operator('('), operator('dvarstring'), string('g_gametype'), operator(')'),
               operator('==' if survival else '!='), string('survival'), operator('&&'), operator('(')] + old + [operator(')')]
    return {'kind':'Statement', 'raw':struct.pack('<IIIII',len(entries),0,0,0,0).hex(), 'p':{'4':entries}}

def author(original):
    result = copy.deepcopy(original)
    for name in ('killstreaks_hd', 'scorebar_hd'):
        candidates = [m for m in result['p']['8'] if m['p']['0'] == name]
        if len(candidates) != 1:
            raise ValueError('Expected exactly one C1 ' + name + ' menu')
        menu = candidates[0]
        if name == 'killstreaks_hd':
            menu['p']['228'] = scoped(menu['p']['228'], False)
        else:
            # Reuse the stock HUD registration and common visibility filters.
            # Only the MP items disappear; the authored Survival items occupy
            # this same loaded menu rather than opening a popup for the HUD.
            for item in menu['p']['284']:
                item['p']['324'] = scoped(item['p'].get('324'), False)
            component = globals()['component']()
            menu['p']['284'].extend(copy.deepcopy(component['p']['8'][0]['p']['284']))
            raw = bytearray.fromhex(menu['raw'])
            struct.pack_into('<I',raw,0xB0,len(menu['p']['284']))
            menu['raw'] = raw.hex()
    return result

def styled(original):
    """author(), then the selectable HUD styles (tools/build_hud_styles.py, k33_hudStyle) folded into the same two
    menus: C1 keeps the first menu it links under a name, so killstreaks_hd and scorebar_hd exist once, here, with
    both the Survival gate and the style gate."""
    import importlib.util
    spec = importlib.util.spec_from_file_location('build_hud_styles', ROOT / 'tools/build_hud_styles.py')
    styles = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(styles)
    return styles.fold_survival(author(original), original)

def main():
    result = styled(original())
    (DIR / 'survival_hud_visibility.menu').write_text(menu_source.decompile_menufile(result, HEADER), newline='\n')

if __name__ == '__main__':
    main()
