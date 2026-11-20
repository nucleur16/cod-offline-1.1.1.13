"""Writes the GSC wall table (maps/_zombiemode_weapons.gsc extra_wall_buy_table) from wallbuys.json, with each wall's
terminal (terminals.py). Usage: python gen_wall_table.py <wallbuys.json> <_zombiemode_weapons.gsc> (rewrites the
function in place, between its generated-comment line and the closing brace)."""
import json, sys

T5W = {}


def num(x):
    x = round(float(x), 3)
    if x == int(x):
        return str(int(x))
    return repr(x)


def vec(v):
    return '( ' + ', '.join(num(x) for x in v) + ' )'


def table(spec):
    out = ['// Generated from wallbuys.json (zm_perkclip_staging tool_src/wallbuy/gen_wall_table.py); "mapname" -> the walls,',
           '// each with CoDOL\'s weapon case (codo_weapon_box, tool_src/wallbuy/terminals.py).',
           'extra_wall_buy_table( map )', '{', '\tw = [];']
    for m in spec['maps']:
        out += [f'\tif ( map == "{m["map"]}" )', '\t{']
        for b in m['buys']:
            t = b['terminal']
            out.append(f'\t\tw[ w.size ] = extra_wall_buy( "{b["weapon"]}", {vec(b["use_origin"])}, "{b["model"]["model"]}", '
                       f'{vec(b["model"]["origin"])}, {vec(b["model"]["angles"])}, {vec(t["origin"])}, {vec(t["angles"])} );'
                       f'\t// {b["id"]}: T5 {b["t5_weapon"]}')
        out.append('\t}')
    out += ['\treturn w;', '}']
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    spec = json.load(open(sys.argv[1]))
    path = sys.argv[2]
    s = open(path, newline='').read()
    start = s.index('// Generated from wallbuys.json')
    head = s.index('extra_wall_buy_table( map )', start)
    end = s.index('\n}\n', head) + 3
    s = s[:start] + table(spec) + s[end:]
    open(path, 'w', newline='').write(s)
    print(sum(len(m['buys']) for m in spec['maps']), 'walls written')
