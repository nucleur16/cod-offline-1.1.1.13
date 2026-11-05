# -*- coding: utf-8 -*-
"""Reviewed twelve-key migration; all unrelated zone-pack identity checks stay exact."""
import hashlib

PARTY_LOCALE = {
    'CODOL_MODE_INFECT_NAME': ('Infected', '感染模式'),
    'CODOL_MODE_INFECT_DESC': ('Survive against cyborg infected. Fallen survivors join the infected.',
                             '在机械感染者的袭击中生存。阵亡的幸存者将加入感染者阵营。'),
    'CODOL_MODE_GUN_NAME': ('Gun Game', '枪械游戏'),
    'CODOL_MODE_GUN_DESC': ('Advance through weapons by eliminating players.', '击败敌人以晋级并更换武器。'),
    'CODOL_MODE_OIC_NAME': ('One in the Chamber', '弹无虚发'),
    'CODOL_MODE_OIC_DESC': ('Survive with limited ammunition.', '在弹药有限的条件下生存。'),
    'CODOL_MODE_SHRP_NAME': ('Sharpshooter', '神射手'),
    'CODOL_MODE_SHRP_DESC': ('Fight with changing weapons.', '使用不断轮换的武器作战。'),
    'CODOL_LOADING_MODE_GUN': ('Gun Game', '枪械游戏'),
    'CODOL_LOADING_MODE_OIC': ('One in the Chamber', '弹无虚发'),
    'CODOL_LOADING_MODE_SHRP': ('Sharpshooter', '神射手'),
    'CODOL_LOADING_MODE_INFECT': ('Infected', '感染模式'),
}


def expected_effective(old, language, encode_localize):
    """Require original English bytes; replace only these twelve ZH/native winners.

    Old shared 000_* zones link first, so later legacy Chinese strings cannot win.
    This explicit expectation is the deliberate localized behavior change, not a
    wildcard exception for arbitrary differing or missing assets.
    """
    result = dict(old)
    for name, (english, chinese) in PARTY_LOCALE.items():
        key = (27, name.lower())
        expected = hashlib.md5(encode_localize(name, english.encode('ascii'))).hexdigest()
        if key not in old or old[key][0] != 'pre_gfx' or old[key][2] != expected:
            raise ValueError('Original party/loading English contract changed: '+name)
        if language in ('zh', 'native'):
            blob = encode_localize(name, chinese.encode('gbk'))
            result[key] = ('pre_gfx', 'reviewed Chinese party/loading migration', hashlib.md5(blob).hexdigest())
    return result
