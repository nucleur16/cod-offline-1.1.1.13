; Generated C1 condition extensions; legacy IW3 indices stay unchanged.
%ifdef ANIM_CONDITION_TYPES
dd 1, anim_values_10
dd 1, anim_values_11
dd 1, anim_values_12
dd 1, 0
dd 1, anim_values_14
dd 1, 0
dd 0, weaponStrings
dd 1, 0
dd 1, anim_values_18
%else
dd anim_condition_10, 0xffffffff
dd anim_condition_11, 0xffffffff
dd anim_condition_12, 0xffffffff
dd anim_condition_13, 0xffffffff
dd anim_condition_14, 0xffffffff
dd anim_condition_15, 0xffffffff
dd anim_condition_16, 0xffffffff
dd anim_condition_17, 0xffffffff
dd anim_condition_18, 0xffffffff
dd 0, 0xffffffff
anim_condition_10: db "damagetype",0
align 4
anim_values_10:
dd anim_value_10_0, 0xffffffff
dd anim_value_10_1, 0xffffffff
dd anim_value_10_2, 0xffffffff
dd 0, 0xffffffff
anim_value_10_0: db "damage_bullet",0
anim_value_10_1: db "damage_explosion_light",0
anim_value_10_2: db "damage_explosion",0
anim_condition_11: db "hitlocation",0
align 4
anim_values_11:
dd anim_value_11_0, 0xffffffff
dd anim_value_11_1, 0xffffffff
dd anim_value_11_2, 0xffffffff
dd anim_value_11_3, 0xffffffff
dd 0, 0xffffffff
anim_value_11_0: db "hit_torso",0
anim_value_11_1: db "hit_head",0
anim_value_11_2: db "hit_neck",0
anim_value_11_3: db "hit_legs",0
anim_condition_12: db "hitdirection",0
align 4
anim_values_12:
dd anim_value_12_0, 0xffffffff
dd anim_value_12_1, 0xffffffff
dd anim_value_12_2, 0xffffffff
dd anim_value_12_3, 0xffffffff
dd 0, 0xffffffff
anim_value_12_0: db "hit_front",0
anim_value_12_1: db "hit_left",0
anim_value_12_2: db "hit_right",0
anim_value_12_3: db "hit_back",0
anim_condition_13: db "akimbo",0
anim_condition_14: db "divedirection",0
align 4
anim_values_14:
dd anim_value_14_0, 0xffffffff
dd anim_value_14_1, 0xffffffff
dd anim_value_14_2, 0xffffffff
dd anim_value_14_3, 0xffffffff
dd anim_value_14_4, 0xffffffff
dd anim_value_14_5, 0xffffffff
dd anim_value_14_6, 0xffffffff
dd anim_value_14_7, 0xffffffff
dd anim_value_14_8, 0xffffffff
dd 0, 0xffffffff
anim_value_14_0: db "none",0
anim_value_14_1: db "forward",0
anim_value_14_2: db "forwardleft",0
anim_value_14_3: db "left",0
anim_value_14_4: db "backleft",0
anim_value_14_5: db "back",0
anim_value_14_6: db "backright",0
anim_value_14_7: db "right",0
anim_value_14_8: db "forwardright",0
anim_condition_15: db "riotshieldnext",0
anim_condition_16: db "playeranimtypeprimary",0
anim_condition_17: db "fastmantle",0
anim_condition_18: db "scripted",0
align 4
anim_values_18:
dd anim_value_18_0, 0xffffffff
dd anim_value_18_1, 0xffffffff
dd anim_value_18_2, 0xffffffff
dd 0, 0xffffffff
anim_value_18_0: db "none",0
anim_value_18_1: db "slowmo_breach_active",0
anim_value_18_2: db "slowmo_breach_passive",0
align 4
%endif
