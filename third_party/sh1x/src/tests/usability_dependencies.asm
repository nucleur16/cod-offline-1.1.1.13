; Fail if this activation fixture accidentally reaches unrelated hint/gameplay
; services from the same retained assembly object. The tested handle service
; and Scr_IsSystemActive have explicit, checked implementations in the C test.
extern abort
%macro unexpected 1
global %1
%1:
    jmp abort
%endmacro
SECTION .text
; The retained activation object also references the inventory-missile and
; corpse-damage services that were added after this fixture. The activation
; paths under test never reach them, so they belong with the other
; unexpected-call guards rather than dragging their implementations in.
unexpected InventoryMissileHint
unexpected InventoryMissileUseAllowed
unexpected IsInventoryMissile
unexpected NotifyDamageEntity
unexpected WeaponBitsRead
unexpected Touch_Item
unexpected BG_CanItemBeGrabbed
unexpected BG_GetPlayerViewDirection
unexpected BG_GetWeaponDef
unexpected BG_PlayerWeaponCountPrimaryTypes
unexpected CM_AreaEntities
unexpected G_DObjGetWorldTagPos
unexpected G_GetHintStringIndex
unexpected G_GetPlayerViewOrigin
unexpected G_IsTurretUsable
unexpected G_TraceCapsuleComplete
unexpected G_VehUsable
unexpected SV_EntityContact
unexpected Scr_AddEntity
unexpected Scr_Notify
unexpected Vec2DistanceSq
SECTION .bss
%macro unused_data 1
global %1
%1: resd 1
%endmacro
unused_data bg_maxGrenadeIndicatorSpeed
unused_data weapbits_result
unused_data entityHandlers
unused_data g_useholdspawndelay
unused_data g_useholdtime
unused_data level
unused_data playerMaxs
unused_data playerMins
unused_data player_MGUseRadius
unused_data player_throwbackInnerRadius
unused_data player_throwbackOuterRadius
unused_data scr_const
unused_data vec3_origin
SECTION .note.GNU-stack noalloc noexec nowrite progbits
