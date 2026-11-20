%include "src/asmsource/item_index.inc"
%define WB_FILE 1
%ifdef OL_SERVER
; Call a cdecl CoDOL helper with one playerState argument from inside the
; retained state machine, preserving every register and giving the callee
; the 16-byte stack alignment the MinGW build assumes.
%macro CALL_PS 2
	pushad
	mov eax, %2
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], eax
	call %1
	mov esp, esi
	popad
%endmacro
; ZF clear when this playerState's hand-0 magazine has run dry on a bullet
; weapon: C1's burst-continuation helper 0x41aa70 asks 0x418ec0 first, and a
; spent magazine owes no more rounds of the burst. Preserves every register.
%macro BURST_CLIP_SPENT 1
	pushad
	mov eax, %1
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], eax
	mov dword [esp+0x4], 0x0
	call WeaponStateHandClipSpent
	mov esp, esi
	test eax, eax
	popad
%endmacro
	extern WeaponStateHandClipSpent
	extern WeaponOffhandPhaseTime
	extern WeaponOffhandThrowTimes
	extern WeaponSprintOutTime
	extern ApplySpreadAbility
	extern WeaponAdsFireOnlyDelay
	extern WeaponStateOffhandEndNotify
%endif
%include "src/asmsource/weapon_bits.inc"
%include "src/online/player/player_eflags.inc"

;Imports of bg_weapons:
	extern strcpy
	extern Com_SurfaceTypeToName
	extern Com_sprintf
	extern Com_Error
	extern ParseConfigStringToStruct
	extern DB_FindXAssetHeaderReal
	extern BG_LoadWeaponDefInternal
	extern SetConfigString
	extern Com_DPrintf
	extern Q_stricmp
	extern PM_AddEvent
	extern PM_SetProneMovementOverride
	extern BG_AddPredictableEventToPlayerstate
	extern BG_AnimScriptEvent
	extern Trace_GetEntityHitId
	extern BG_LoadDefaultWeaponDef
	extern bg_itemlist
	extern BG_LoadPlayerAnimTypes
	extern BG_InitWeaponStrings
	extern randomf
	extern perk_weapSpreadMultiplier
	extern GetLeanFraction
	extern bg_bobAmplitudeStanding
	extern bg_bobAmplitudeProne
	extern sinf
	extern sin
	extern bg_bobAmplitudeSprinting
	extern _ZZ14PM_GetMoveAnimP13playerState_s17PmStanceFrontBackiiE13moveAnimTable
	extern bg_bobAmplitudeDucked
	extern memset
	extern strcmp
	extern floorf
	extern bg_aimSpreadMoveSpeedThreshold
	extern fs_gameDirVar
	extern useFastFile
	extern DB_IsXAssetDefault
	extern BG_ClearSurfaceTypeSounds
	extern SL_RemoveRefToString
	extern BG_SetConditionValue
	extern player_scopeExitOnDamage
	extern player_adsExitDelay
	extern Com_LoadInfoString
	extern Com_Memset
	extern AnglesSubtract
	extern DiffTrack
	extern DiffTrackAngle
	extern player_breath_hold_time
	extern player_breath_gasp_time
	extern perk_extraBreath
	extern player_breath_hold_lerp
	extern player_breath_gasp_lerp
	extern player_breath_gasp_scale
	extern perk_weapReloadMultiplier
	extern PM_MeleeChargeStart
	extern Mantle_IsWeaponInactive
	extern player_sustainAmmo
	extern BG_SetConditionBit
	extern player_breath_fire_delay
	extern perk_weapRateMultiplier
	extern player_burstFireCooldown
;	extern Profile_Begin
;	extern Profile_End

;Exports of bg_weapons:
	global bg_numAmmoTypes
	global bg_weapAmmoTypes
	global bg_numSharedAmmoCaps
	global bg_sharedAmmoCaps
	global bg_numWeapClips
	global bg_weapClips
	global penetrationDepthTableLoaded
	global BG_StringCopy
	global BG_ParsePenetrationDepthTable
	global BG_LoadWeaponDef_FastFile
	global BG_LoadWeaponDef_LoadObj
	global BG_SetupSharedAmmoIndexes
	global PM_ReloadClip
	global PM_Weapon_OffHandPrepare
	global BG_CalculateWeaponPosition_GunRecoil_SingleAngle
	global PM_BeginWeaponChange
	global PM_Weapon_ReloadDelayedAction
	global PM_SetReloadingState
	global PM_BeginWeaponReload
	global _ZZ24PM_Weapon_CheckForReloadP7pmove_tE35MY_RELOADSTART_INTERUPT_IGNORE_FRAC
	global PM_Weapon_CheckForReload
	global PM_Weapon_CheckForOffHand
	global BG_WeaponAmmo
	global BG_GetBobCycle
	global BG_AdvanceTrace
	global BG_GetWeaponDef
	global BG_AmmoForWeapon
	global BG_CanHoldBreath
	global BG_ClipForWeapon
	global BG_GetNumWeapons
	global BG_IsWeaponValid
	global BG_ClearWeaponDef
	global BG_GetWeaponIndex
	global BG_GetAmmoPlayerMax
	global BG_TakePlayerWeapon
	global BG_UsingSniperScope
	global BG_WeaponFireRecoil
	global BG_WeaponIsClipOnly
	global PM_ExitAimDownSight
	global PM_ResetWeaponState
	global BG_WeaponBlocksProne
	global BG_GetSpreadForWeapon
	global BG_CalculateViewAngles
	global BG_CanPlayerHaveWeapon
	global BG_GetTotalAmmoReserve
	global BG_ThrowingBackGrenade
	global PM_WeaponAmmoAvailable
	global BG_FillInAllWeaponItems
	global BG_GetMaxPickupableAmmo
	global BG_GetVerticalBobFactor
	global BG_IsAimDownSightWeapon
	global PM_AdjustAimSpreadScale
	global BG_CalculateWeaponAngles
	global BG_GetWeaponIndexForName
	global BG_FindWeaponIndexForName
	global BG_GetHorizontalBobFactor
	global BG_ShutdownWeaponDefFiles
	global PM_UpdateAimDownSightFlag
	global PM_UpdateAimDownSightLerp
	global BG_GetFirstEquippedOffhand
	global BG_GetViewmodelWeaponIndex
	global BG_AssertOffhandIndexOrNone
	global BG_GetFirstAvailableOffhand
	global BG_LoadPenetrationDepthTable
	global BG_PlayerHasCompatibleWeapon
	global BG_GetSurfacePenetrationDepth
	global BG_PlayerWeaponsFull_Primaries
	global PM_InteruptWeaponWithProneMove
	global BG_CalculateWeaponPosition_Sway
	global BG_PlayerWeaponCountPrimaryTypes
	global PM_Weapon
	global bg_lastParsedWeaponIndex
	global bg_weaponDefs
	global penetrationDepthTable


SECTION .text


;BG_StringCopy(unsigned char*, char const*)
BG_StringCopy:
	push ebp
	mov ebp, esp
	sub esp, 0x18
	mov eax, [ebp+0xc]
	mov [esp+0x4], eax
	mov eax, [ebp+0x8]
	mov [esp], eax
	call strcpy
	leave
	ret


;BG_ParsePenetrationDepthTable(char const*, float*, char const*)
BG_ParsePenetrationDepthTable:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x1e9c
	mov [ebp-0x1e7c], eax
	mov [ebp-0x1e80], edx
	mov [ebp-0x1e84], ecx
	xor esi, esi
	lea ebx, [ebp-0x174]
	lea edi, [ebp-0x1e74]
	jmp BG_ParsePenetrationDepthTable_10
BG_ParsePenetrationDepthTable_30:
	mov [ebx], edi
	lea eax, [esi*4]
	mov [ebx+0x4], eax
	mov dword [ebx+0x8], 0x6
	add esi, 0x1
	add edi, 0x100
	add ebx, 0xc
	cmp esi, 0x1d
	jz BG_ParsePenetrationDepthTable_20
BG_ParsePenetrationDepthTable_10:
	mov [esp], esi
	call Com_SurfaceTypeToName
	mov [esp+0x10], eax
	mov eax, [ebp-0x1e7c]
	mov [esp+0xc], eax
	mov dword [esp+0x8], _cstring_s_s
	mov dword [esp+0x4], 0x100
	mov [esp], edi
	call Com_sprintf
	test eax, eax
	jns BG_ParsePenetrationDepthTable_30
	mov [esp], esi
	call Com_SurfaceTypeToName
	mov [esp+0xc], eax
	mov eax, [ebp-0x1e7c]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_bullet_penetrati
	mov dword [esp], 0x2
	call Com_Error
	mov [ebx], edi
	lea eax, [esi*4]
	mov [ebx+0x4], eax
	mov dword [ebx+0x8], 0x6
	add esi, 0x1
	add edi, 0x100
	add ebx, 0xc
	cmp esi, 0x1d
	jnz BG_ParsePenetrationDepthTable_10
BG_ParsePenetrationDepthTable_20:
	mov dword [esp+0x18], BG_StringCopy
	mov dword [esp+0x14], 0x0
	mov dword [esp+0x10], 0x0
	mov eax, [ebp-0x1e84]
	mov [esp+0xc], eax
	mov dword [esp+0x8], 0x1d
	lea eax, [ebp-0x174]
	mov [esp+0x4], eax
	mov eax, [ebp-0x1e80]
	mov [esp], eax
	call ParseConfigStringToStruct
	test eax, eax
	jnz BG_ParsePenetrationDepthTable_40
	mov eax, [ebp-0x1e7c]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_error_parsing_bu
	mov dword [esp], 0x2
	call Com_Error
BG_ParsePenetrationDepthTable_40:
	add esp, 0x1e9c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;BG_LoadWeaponDef_FastFile(char const*)
BG_LoadWeaponDef_FastFile:
	push ebp
	mov ebp, esp
	sub esp, 0x18
	mov eax, [ebp+0x8]
	cmp byte [eax], 0x0
	jnz BG_LoadWeaponDef_FastFile_10
	xor eax, eax
	leave
	ret
BG_LoadWeaponDef_FastFile_10:
	mov [esp+0x4], eax
	mov dword [esp], 0x17
	call DB_FindXAssetHeaderReal
	leave
	ret


;BG_LoadWeaponDef_LoadObj(char const*)
BG_LoadWeaponDef_LoadObj:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov esi, [ebp+0x8]
	cmp byte [esi], 0x0
	jnz BG_LoadWeaponDef_LoadObj_10
	xor ebx, ebx
BG_LoadWeaponDef_LoadObj_20:
	mov eax, ebx
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
BG_LoadWeaponDef_LoadObj_10:
	mov [esp+0x4], esi
	mov dword [esp], _cstring_mp
	call BG_LoadWeaponDefInternal
	mov ebx, eax
	test eax, eax
	jnz BG_LoadWeaponDef_LoadObj_20
	mov dword [esp+0x4], _cstring_defaultweapon_mp
	mov dword [esp], _cstring_mp
	call BG_LoadWeaponDefInternal
	mov ebx, eax
	test eax, eax
	jz BG_LoadWeaponDef_LoadObj_30
BG_LoadWeaponDef_LoadObj_40:
	mov [esp+0x4], esi
	mov [esp], ebx
	call SetConfigString
	jmp BG_LoadWeaponDef_LoadObj_20
BG_LoadWeaponDef_LoadObj_30:
	mov dword [esp+0x4], _cstring_bg_loadweapondef
	mov dword [esp], 0x2
	call Com_Error
	jmp BG_LoadWeaponDef_LoadObj_40
	nop


;BG_SetupSharedAmmoIndexes(unsigned int)
BG_SetupSharedAmmoIndexes:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x3c
	mov edi, eax
	mov ebx, [eax*4+bg_weaponDefs]
	mov dword [ebx+0x344], 0xffffffff
	mov eax, [ebx+0x340]
	cmp byte [eax], 0x0
	jnz BG_SetupSharedAmmoIndexes_10
BG_SetupSharedAmmoIndexes_60:
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_SetupSharedAmmoIndexes_10:
	mov [esp+0xc], eax
	mov eax, [ebx]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_s_s1
	mov dword [esp], 0x11
	call Com_DPrintf
	mov eax, [bg_numSharedAmmoCaps]
	test eax, eax
	jnz BG_SetupSharedAmmoIndexes_20
	mov dword [ebp-0x24], 0x0
	xor edx, edx
	mov ecx, [ebp-0x24]
BG_SetupSharedAmmoIndexes_50:
	mov [ecx*4+bg_sharedAmmoCaps], ebx
	mov [ebx+0x344], edx
	add eax, 0x1
	mov [bg_numSharedAmmoCaps], eax
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_SetupSharedAmmoIndexes_20:
	mov dword [ebp-0x24], 0x0
	mov esi, bg_sharedAmmoCaps
BG_SetupSharedAmmoIndexes_40:
	mov eax, [ebx+0x340]
	mov [esp+0x4], eax
	mov eax, [esi]
	mov eax, [eax+0x340]
	mov [esp], eax
	call Q_stricmp
	test eax, eax
	jz BG_SetupSharedAmmoIndexes_30
	add dword [ebp-0x24], 0x1
	mov eax, [bg_numSharedAmmoCaps]
	add esi, 0x4
	cmp [ebp-0x24], eax
	jb BG_SetupSharedAmmoIndexes_40
	mov edx, [ebp-0x24]
	mov ecx, edx
	jmp BG_SetupSharedAmmoIndexes_50
BG_SetupSharedAmmoIndexes_30:
	mov eax, [ebp-0x24]
	mov [ebx+0x344], eax
	; C1 0x41f310 assigns the shared-cap pool by case-insensitive name only:
	; the first weapon registering a name defines the pool and later weapons
	; with a different cap (flare_mp 1 vs frag_grenade_mp 2 on
	; WEAPON_FRAGGRENADE) join it. IW3's cap-mismatch Com_Error below is not
	; C1 behaviour and is bypassed.
	jmp BG_SetupSharedAmmoIndexes_60
	mov edx, [eax*4+bg_sharedAmmoCaps]
	mov eax, [edx+0x348]
	cmp eax, [ebx+0x348]
	jz BG_SetupSharedAmmoIndexes_60
	mov eax, [ebp-0x24]
	test eax, eax
	jz BG_SetupSharedAmmoIndexes_60
	cmp edi, 0x1
	jbe BG_SetupSharedAmmoIndexes_60
	mov dword [ebp-0x20], 0x1
	mov dword [ebp-0x1c], bg_weaponDefs
	jmp BG_SetupSharedAmmoIndexes_70
BG_SetupSharedAmmoIndexes_80:
	add dword [ebp-0x20], 0x1
	add dword [ebp-0x1c], 0x4
	cmp edi, [ebp-0x20]
	jz BG_SetupSharedAmmoIndexes_60
	mov ecx, [ebp-0x24]
	mov edx, [ecx*4+bg_sharedAmmoCaps]
BG_SetupSharedAmmoIndexes_70:
	mov eax, [ebp-0x1c]
	mov esi, [eax+0x4]
	mov eax, [esi+0x340]
	mov [esp+0x4], eax
	mov eax, [edx+0x340]
	mov [esp], eax
	call Q_stricmp
	test eax, eax
	jnz BG_SetupSharedAmmoIndexes_80
	mov eax, [esi+0x348]
	mov ecx, [ebp-0x24]
	mov edx, [ecx*4+bg_sharedAmmoCaps]
	cmp eax, [edx+0x348]
	jnz BG_SetupSharedAmmoIndexes_80
	mov [esp+0x18], eax
	mov eax, [esi]
	mov [esp+0x14], eax
	mov eax, [ebx+0x348]
	mov [esp+0x10], eax
	mov eax, [ebx]
	mov [esp+0xc], eax
	mov eax, [ebx+0x340]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_shared_ammo_cap_
	mov dword [esp], 0x2
	call Com_Error
	jmp BG_SetupSharedAmmoIndexes_80
	nop


;PM_ReloadClip(playerState_s*)
PM_ReloadClip:
%ifdef OL_SERVER
	; Direct native C1 dual-mag phase; EAX is the retained ps argument.
	extern FastMagReloadClip
	push ebp
	mov ebp, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], eax
	call FastMagReloadClip
	test eax, eax
	mov eax, [esp]
	mov esp, ebp
	pop ebp
	jz PM_ReloadClip_retained
	ret
PM_ReloadClip_retained:
%endif
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov ecx, eax
	mov eax, [eax+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov edi, [ecx+0xec]
	sub edi, 0x9
	cmp edi, 0x1
	jbe PM_ReloadClip_10
PM_ReloadClip_70:
	mov edx, [eax+0x328]
	mov [ebp-0x20], edx
	mov edx, [eax+0x330]
	mov [ebp-0x1c], edx
	mov edx, [ebp-0x20]
	mov esi, [ecx+edx*4+0x15c]
	mov ebx, [eax+0x338]
	mov edx, [ebp-0x1c]
	mov edx, [ecx+edx*4+0x35c]
	mov [ebp-0x24], edx
	mov edx, ebx
	sub edx, [ebp-0x24]
	cmp esi, edx
	cmovl edx, esi
	cmp edi, 0x1
	jbe PM_ReloadClip_20
	mov eax, [eax+0x534]
	test eax, eax
	jnz PM_ReloadClip_30
PM_ReloadClip_50:
	test edx, edx
	jz PM_ReloadClip_40
	mov eax, edx
PM_ReloadClip_60:
	sub esi, eax
	mov edx, [ebp-0x20]
	mov [ecx+edx*4+0x15c], esi
	add eax, [ebp-0x24]
	mov edx, [ebp-0x1c]
	mov [ecx+edx*4+0x35c], eax
	mov dword [esp+0x4], 0x14
	mov [esp], ecx
	call PM_AddEvent
PM_ReloadClip_40:
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_ReloadClip_30:
	cmp ebx, eax
	jle PM_ReloadClip_50
	cmp edx, eax
	jg PM_ReloadClip_60
	jmp PM_ReloadClip_50
PM_ReloadClip_10:
	mov edx, [eax+0x538]
	test edx, edx
	jnz PM_ReloadClip_70
	jmp PM_ReloadClip_40
PM_ReloadClip_20:
	mov edi, [eax+0x538]
	cmp ebx, edi
	jle PM_ReloadClip_50
	cmp edx, edi
	jle PM_ReloadClip_50
	mov edx, edi
	jmp PM_ReloadClip_50
	nop


;PM_Weapon_OffHandPrepare(playerState_s*)
PM_Weapon_OffHandPrepare:
	push ebp
	mov ebp, esp
	push ebx
	sub esp, 0x14
	mov ebx, eax
	mov edx, [eax+0xe0]
	mov eax, [edx*4+bg_weaponDefs]
	mov dword [ebx+0xec], 0x10
	mov eax, [eax+0x378]
	mov [ebx+0x3c], eax
	mov dword [ebx+0x40], 0x0
	or dword [ebx+0x10], 0x2
	cmp dword [ebx+0x48], 0x7ff
	jz PM_Weapon_OffHandPrepare_10
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_OffHandPrepare_20
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x12
	mov [ebx+0x624], eax
PM_Weapon_OffHandPrepare_20:
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandPrepare 0x41c4cd..0x41c4fe: weaponTime /= value28
	; (modOffhandSpeedScale) after the prepare, throwback and detonate arms.
	CALL_PS WeaponOffhandPhaseTime, ebx
%endif
	mov [esp], ebx
	call PM_SetProneMovementOverride
	add esp, 0x14
	pop ebx
	pop ebp
	ret
PM_Weapon_OffHandPrepare_10:
%ifdef OL_SERVER
	; C1 0x41c41b..0x41c481 makes this decision at exactly this point -- after
	; the throwback check, before EV_PREP_OFFHAND -- and takes the detonation
	; when the definition carries +0xbe0 and its total ammo is zero. It then
	; overwrites the phase and both timers and plays animation 32:
	;   0x41c433 state 20, 0x41c43d weaponTime = def+0x5bc,
	;   0x41c455 weapFlags bit 2 stays raised, 0x41c45c weaponDelay = def+0x5a8,
	;   0x41c478 animation 0x20.
	; The retained detonation at PM_Weapon_520 already establishes that C1's
	; def+0x5bc and def+0x5a8 are the retained def+0x37c and def+0x368, and
	; that the retained animation for a detonation is 27; both are reused here
	; rather than inventing a field or an animation. EBX is the playerState.
	extern WeaponStateOffhandDetonates
	mov [esp], ebx
	call WeaponStateOffhandDetonates
	test eax, eax
	jz PM_Weapon_OffHandPrepare_30
	mov edx, [ebx+0xe0]
	mov edx, [edx*4+bg_weaponDefs]
	mov dword [ebx+0xec], 0x1b
	mov eax, [edx+0x37c]
	mov [ebx+0x3c], eax
	mov eax, [edx+0x368]
	mov [ebx+0x40], eax
	or dword [ebx+0x10], 0x2
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_OffHandPrepare_20
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1b
	mov [ebx+0x624], eax
	jmp PM_Weapon_OffHandPrepare_20
PM_Weapon_OffHandPrepare_30:
	mov edx, [ebx+0xe0]
%endif
	mov [esp+0x8], ebx
	mov [esp+0x4], edx
	mov dword [esp], 0x20
	call BG_AddPredictableEventToPlayerstate
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_OffHandPrepare_20
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1a
	mov [ebx+0x624], eax
%ifdef OL_SERVER
	CALL_PS WeaponOffhandPhaseTime, ebx
%endif
	mov [esp], ebx
	call PM_SetProneMovementOverride
	add esp, 0x14
	pop ebx
	pop ebp
	ret
	nop


;BG_CalculateWeaponPosition_GunRecoil_SingleAngle(float*, float*, float, float, float, float, float, float)
BG_CalculateWeaponPosition_GunRecoil_SingleAngle:
	push ebp
	mov ebp, esp
	sub esp, 0x18
	mov ecx, eax
	movaps xmm7, xmm0
	movss [ebp-0xc], xmm3
	movss xmm6, dword [ebp+0x8]
	movss xmm3, dword [ebp+0xc]
	movss xmm5, dword [eax]
	movss xmm4, dword [_data16_7fffffff]
	movaps xmm0, xmm5
	andps xmm0, xmm4
	ucomiss xmm0, [_float_0_25000000]
	jae BG_CalculateWeaponPosition_GunRecoil_SingleAngle_10
	jp BG_CalculateWeaponPosition_GunRecoil_SingleAngle_10
	movss xmm0, dword [edx]
	andps xmm0, xmm4
	ucomiss xmm0, [_float_1_00000000]
	jae BG_CalculateWeaponPosition_GunRecoil_SingleAngle_10
	jp BG_CalculateWeaponPosition_GunRecoil_SingleAngle_10
	xor eax, eax
	mov [ecx], eax
	mov [edx], eax
	mov eax, 0x1
	leave
	ret
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_10:
	movaps xmm0, xmm7
	mulss xmm0, [edx]
	addss xmm0, xmm5
	movss [ecx], xmm0
	ucomiss xmm0, xmm1
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_20
	movss [ecx], xmm1
	pxor xmm4, xmm4
	movss xmm0, dword [edx]
	ucomiss xmm0, xmm4
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_30
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_140:
	movss [edx], xmm4
	movss xmm0, dword [ecx]
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_150:
	ucomiss xmm0, xmm4
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_40
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_90:
	mulss xmm2, xmm7
	movss xmm0, dword [edx]
	subss xmm0, xmm2
	movss [edx], xmm0
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_170:
	mulss xmm6, xmm0
	mulss xmm6, xmm7
	movaps xmm1, xmm0
	subss xmm1, xmm6
	movss [edx], xmm1
	ucomiss xmm1, xmm4
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_50
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_110:
	mulss xmm3, xmm7
	movaps xmm0, xmm1
	subss xmm0, xmm3
	movss [edx], xmm0
	ucomiss xmm4, xmm0
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_60
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_160:
	ucomiss xmm0, [ebp-0xc]
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_70
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_120:
	movss xmm0, dword [ebp-0xc]
	movss [edx], xmm0
	xor eax, eax
	leave
	ret
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_20:
	xorps xmm1, [_data16_80000000]
	ucomiss xmm1, xmm0
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_80
	pxor xmm4, xmm4
	movss xmm0, dword [ecx]
	ucomiss xmm0, xmm4
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_90
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_40:
	ucomiss xmm4, xmm0
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_100
	movss xmm0, dword [edx]
	mulss xmm6, xmm0
	mulss xmm6, xmm7
	movaps xmm1, xmm0
	subss xmm1, xmm6
	movss [edx], xmm1
	ucomiss xmm1, xmm4
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_110
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_50:
	movaps xmm0, xmm7
	mulss xmm0, xmm3
	addss xmm0, xmm1
	movaps xmm1, xmm4
	cmpss xmm1, xmm0, 0x5
	andps xmm0, xmm1
	orps xmm0, xmm4
	movss [edx], xmm0
	ucomiss xmm0, [ebp-0xc]
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_120
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_70:
	movss xmm3, dword [ebp-0xc]
	xorps xmm3, [_data16_80000000]
	ucomiss xmm3, xmm0
	jbe BG_CalculateWeaponPosition_GunRecoil_SingleAngle_130
	movss [edx], xmm3
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_130:
	xor eax, eax
	leave
	ret
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_80:
	movss [ecx], xmm1
	pxor xmm4, xmm4
	ucomiss xmm4, [edx]
	ja BG_CalculateWeaponPosition_GunRecoil_SingleAngle_140
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_30:
	movaps xmm0, xmm1
	jmp BG_CalculateWeaponPosition_GunRecoil_SingleAngle_150
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_60:
	movss [edx], xmm4
	movaps xmm0, xmm4
	jmp BG_CalculateWeaponPosition_GunRecoil_SingleAngle_160
BG_CalculateWeaponPosition_GunRecoil_SingleAngle_100:
	mulss xmm2, xmm7
	addss xmm2, [edx]
	movss [edx], xmm2
	movaps xmm0, xmm2
	jmp BG_CalculateWeaponPosition_GunRecoil_SingleAngle_170


;PM_BeginWeaponChange(playerState_s*, unsigned int, unsigned char)
PM_BeginWeaponChange:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov esi, eax
	mov edi, edx
	mov [ebp-0x1a], cl
	test edx, edx
	jz PM_BeginWeaponChange_10
	sar edx, 0x5
	mov ecx, edi
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST esi, edx, eax, WB_OWNED
	jz PM_BeginWeaponChange_20
PM_BeginWeaponChange_10:
	mov edx, [esi+0xec]
	lea eax, [edx-0x3]
	cmp eax, 0x1
	jbe PM_BeginWeaponChange_20
	test edi, edi
	jnz PM_BeginWeaponChange_30
PM_BeginWeaponChange_70:
	cmp edx, 0x7
	jz PM_BeginWeaponChange_40
	cmp edx, 0x9
	jz PM_BeginWeaponChange_40
	cmp edx, 0xb
	jz PM_BeginWeaponChange_40
	cmp edx, 0xa
	jz PM_BeginWeaponChange_40
	cmp edx, 0x8
	jnz PM_BeginWeaponChange_50
PM_BeginWeaponChange_40:
	mov [esp+0x8], esi
	mov [esp+0x4], edx
	mov dword [esp], 0x2
	call BG_AddPredictableEventToPlayerstate
PM_BeginWeaponChange_50:
%ifdef OL_SERVER
	; The weapon-change announcement's old place, before the quick/drop
	; decision; it queues here only with c1EventParity 0 (bisecting).
	; C1 queues it on the drop arm alone (below, weapon_change_event.c).
	extern WeaponChangeAnnounce
	mov dword [esp+0x8], 0x0
	mov [esp+0x4], edi
	mov [esp], esi
	call WeaponChangeAnnounce
	; C1 0x4197c2..0x4197ce, before the quick/drop decision: the variable-zoom level, its
	; time and its latch (ps+0x5c4/+0x5c8/+0x5cc) all return to zero, so the
	; next scope starts at its first zoom level.
	extern VariableZoomBeginWeaponChange
	mov [esp], esi
	call VariableZoomBeginWeaponChange
%endif
	mov dword [esi+0x40], 0x0
	mov ebx, [esi+0xe8]
	test ebx, ebx
	jnz PM_BeginWeaponChange_60
PM_BeginWeaponChange_80:
	mov dword [esi+0x3c], 0x0
	cmp byte [ebp-0x1a], 0x1
	sbb eax, eax
	add eax, 0x4
	mov [esi+0xec], eax
	mov dword [esi+0x44], 0x0
	mov [esp], esi
	call PM_SetProneMovementOverride
PM_BeginWeaponChange_20:
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_BeginWeaponChange_30:
	mov eax, [edi*4+bg_weaponDefs]
	cmp dword [eax+0x130], 0x4
	mov eax, 0x1
	movzx ecx, byte [ebp-0x1a]
	cmovz ecx, eax
	mov [ebp-0x1a], cl
	jmp PM_BeginWeaponChange_70
PM_BeginWeaponChange_60:
	mov edx, ebx
	sar edx, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST esi, edx, eax, WB_OWNED
	jz PM_BeginWeaponChange_80
	mov eax, [esi+0x44]
	test eax, eax
	jg PM_BeginWeaponChange_80
%ifdef OL_SERVER
	; C1 0x4197f2..0x419800 ORs weapFlags 0x10000 into the quick condition.
	; switchToWeaponImmediate sets it (0x5110c7, with weapon 0) and the end of
	; the raise clears it (0x41a0c0 from states 1/2, PM_Weapon_RaiseDone
	; here), so every change until the immediate weapon is up is quick.
	test dword [esi+0x10], 0x10000
	jnz PM_BeginWeaponChange_80
%endif
%ifdef OL_SERVER
	; The weapon-change announcement, C1 event 0x1c, carrying the weapon being
	; changed to. C1 PM_BeginWeaponChange queues it at 0x419888 only on the
	; drop path: its quick path (0x4197d4..0x419800: no current weapon, no
	; equipped record for it, or C1 ps+0x34 = retained ps+0x44 above 0) writes
	; weaponstate 3/4 and returns at 0x41984c without any event, exactly like
	; PM_BeginWeaponChange_80 here. Queuing it before that decision gave the
	; server an event C1 never predicts on every quick change, so every newer
	; predicted event was renumbered and replayed. Here, as at 0x419888, it
	; comes before the alternate/put-away event. ESI is the playerState, EDI
	; the weapon being changed to, EBX the weapon being left (callee-saved).
	mov dword [esp+0x8], 0x1
	mov [esp+0x4], edi
	mov [esp], esi
	call WeaponChangeAnnounce
%endif
	mov ebx, [ebx*4+bg_weaponDefs]
%ifdef OL_SERVER
	; C1 PMF_SPRINTING is 0x4000; C1 0x41985c skips BG_IsAltSwitch while it
	; is set. IW3's sign-bit test read C1's shellshock bit 0x8000 instead.
	test dword [esi+0xc], 0x4000
	jnz PM_BeginWeaponChange_90
%else
	cmp word [esi+0xc], 0x0
	js PM_BeginWeaponChange_90
%endif
	test edi, edi
	jz PM_BeginWeaponChange_90
%ifdef OL_SERVER
	; C1 decides the alternate switch with BG_IsAltSwitch (0x41986f -> 0x419670),
	; which also takes an alternate-mode weapon back to the primary that owns it
	; (inventory type 3 and ps+0x340). Composed CoDOL alternates carry no
	; altWeaponIndex back link, so the forward test alone sent gl_* -> rifle
	; through the ordinary drop. ESI is the playerState, EDI the new weapon and
	; ps->weapon still the weapon being left; the result returns in EAX, which
	; both successors reload before use.
	extern WeaponIsAltSwitch
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp+0x8], edi
	mov eax, [esi+0xe8]
	mov [esp+0x4], eax
	mov [esp], esi
	call WeaponIsAltSwitch
	mov esp, [esp+0xc]
	mov [esp+0x1c], eax
	popad
	test eax, eax
	jnz PM_BeginWeaponChange_100
%else
	cmp edi, [ebx+0x540]
	jz PM_BeginWeaponChange_100
%endif
PM_BeginWeaponChange_90:
	xor edi, edi
PM_BeginWeaponChange_140:
	mov eax, [ebx+0x330]
	mov eax, [esi+eax*4+0x35c]
	test eax, eax
	setz byte [ebp-0x19]
	mov dword [esi+0x44], 0x0
	test edi, edi
	jz PM_BeginWeaponChange_110
	mov dword [esp+0x4], 0x18
	mov [esp], esi
	call PM_AddEvent
%ifdef OL_SERVER
	cmp dword [esi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [esi+0x4], 0x6
%endif
	jg PM_BeginWeaponChange_120
	mov eax, [esi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x11
	mov [esi+0x624], eax
PM_BeginWeaponChange_120:
	cmp byte [ebp-0x1a], 0x1
	sbb eax, eax
	add eax, 0x4
	mov [esi+0xec], eax
%ifdef OL_SERVER
	; Start the second hand's own put-away with the same alternate/quick
	; selection the retained code just made (edi is the alternate-switch flag
	; set at PM_BeginWeaponChange_100, [ebp-0x1a] the quick-switch byte).
	; C1 begins a change once for both hands (0x419d77/0x419d7d), so the
	; change's own events stay with the retained code and are not doubled.
	extern WeaponStateBeginChange
	pushad
	movzx eax, byte [ebp-0x1a]
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp+0x8], eax
	mov [esp+0x4], edi
	mov [esp], esi
	call WeaponStateBeginChange
	mov esp, [esp+0xc]
	popad
%endif
	mov [esp], esi
	call PM_SetProneMovementOverride
	test edi, edi
	jz PM_BeginWeaponChange_130
	mov eax, [ebx+0x3ac]
	mov [esi+0x3c], eax
	jmp PM_BeginWeaponChange_20
PM_BeginWeaponChange_100:
	mov edi, 0x1
	jmp PM_BeginWeaponChange_140
PM_BeginWeaponChange_110:
	mov dword [esp+0x4], 0x17
	mov [esp], esi
	call PM_AddEvent
	mov edx, [esi+0xc]
%ifdef OL_SERVER
	; C1 0x41991c skips the drop animation loop (0x419930..0x41994f) only
	; while sprinting, 0x4000. The sign-bit test here skipped it for C1's
	; shellshock 0x8000, so a shocked switch published a stale animation.
	test edx, 0x4000
	jnz PM_BeginWeaponChange_150
%else
	test dx, dx
	js PM_BeginWeaponChange_150
%endif
	cmp byte [ebp-0x19], 0x0
	jz PM_BeginWeaponChange_160
%ifdef OL_SERVER
	cmp dword [esi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [esi+0x4], 0x6
%endif
	jg PM_BeginWeaponChange_150
	mov eax, [esi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x15
	mov [esi+0x624], eax
PM_BeginWeaponChange_150:
	and dl, 0x4
	jnz PM_BeginWeaponChange_120
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x6
	mov [esp], esi
	call BG_AnimScriptEvent
	jmp PM_BeginWeaponChange_120
PM_BeginWeaponChange_130:
%ifdef OL_SERVER
	; C1 0x419a41 takes the quick drop time (+0x604) before it asks whether
	; the magazine is empty (0x419a54, +0x614). The animation keeps the
	; opposite order (empty first, 0x419930), exactly as the retained
	; _110/_160 selection above already does.
	cmp byte [ebp-0x1a], 0x0
	jz PM_BeginWeaponChange_135
	mov eax, [ebx+0x3b4]
	mov [esi+0x3c], eax
	jmp PM_BeginWeaponChange_20
PM_BeginWeaponChange_135:
%endif
	cmp byte [ebp-0x19], 0x0
	jz PM_BeginWeaponChange_170
	mov eax, [ebx+0x3c4]
	mov [esi+0x3c], eax
	jmp PM_BeginWeaponChange_20
PM_BeginWeaponChange_170:
	cmp byte [ebp-0x1a], 0x0
	jz PM_BeginWeaponChange_180
	mov eax, [ebx+0x3b4]
	mov [esi+0x3c], eax
	jmp PM_BeginWeaponChange_20
PM_BeginWeaponChange_160:
	cmp byte [ebp-0x1a], 0x0
	jz PM_BeginWeaponChange_190
%ifdef OL_SERVER
	cmp dword [esi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [esi+0x4], 0x6
%endif
	jg PM_BeginWeaponChange_150
	mov eax, [esi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x13
	mov [esi+0x624], eax
	jmp PM_BeginWeaponChange_150
PM_BeginWeaponChange_180:
	mov eax, [ebx+0x3a4]
	mov [esi+0x3c], eax
	jmp PM_BeginWeaponChange_20
PM_BeginWeaponChange_190:
%ifdef OL_SERVER
	cmp dword [esi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [esi+0x4], 0x6
%endif
	jg PM_BeginWeaponChange_150
	mov eax, [esi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0xa
	mov [esi+0x624], eax
	jmp PM_BeginWeaponChange_150
	nop


;PM_Weapon_ReloadDelayedAction(playerState_s*)
PM_Weapon_ReloadDelayedAction:
%ifdef OL_SERVER
	; Direct native C1 dual-mag phase; EAX is the retained ps argument.
	extern FastMagReloadDelayedAction
	push ebp
	mov ebp, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], eax
	call FastMagReloadDelayedAction
	test eax, eax
	mov eax, [esp]
	mov esp, ebp
	pop ebp
	jz PM_Weapon_ReloadDelayedAction_retained
	ret
PM_Weapon_ReloadDelayedAction_retained:
%endif
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov ebx, eax
	mov ecx, [eax+0xe8]
	mov esi, [ecx*4+bg_weaponDefs]
	mov eax, [esi+0x4e8]
	test eax, eax
	jz PM_Weapon_ReloadDelayedAction_10
	mov eax, ecx
	sar eax, 0x5
	lea edx, [eax*4]
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST_B ebx, edx, eax, WB_RECHAMBER
	jnz PM_Weapon_ReloadDelayedAction_20
PM_Weapon_ReloadDelayedAction_10:
	mov eax, ebx
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	jmp PM_ReloadClip
PM_Weapon_ReloadDelayedAction_20:
	not eax
	WB_AND_B ebx, edx, eax, WB_RECHAMBER
	mov dword [esp+0x4], 0x1d
	mov [esp], ebx
	call PM_AddEvent
	mov eax, [ebx+0xec]
	sub eax, 0x9
	cmp eax, 0x1
	jbe PM_Weapon_ReloadDelayedAction_30
PM_Weapon_ReloadDelayedAction_80:
	mov edx, [ebx+0x3c]
	test edx, edx
	jz PM_Weapon_ReloadDelayedAction_10
	cmp eax, 0x1
	jbe PM_Weapon_ReloadDelayedAction_40
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov eax, [ebx+eax*4+0x35c]
	test eax, eax
	jnz PM_Weapon_ReloadDelayedAction_50
	mov eax, [esi+0x12c]
	test eax, eax
	jz PM_Weapon_ReloadDelayedAction_60
PM_Weapon_ReloadDelayedAction_50:
	mov ecx, [esi+0x388]
PM_Weapon_ReloadDelayedAction_100:
	mov eax, [esi+0x394]
	test eax, eax
	jz PM_Weapon_ReloadDelayedAction_70
	cmp ecx, eax
	cmovg ecx, eax
PM_Weapon_ReloadDelayedAction_70:
	mov eax, [esi+0x374]
	cmp ecx, eax
	mov edx, 0x1
	cmovle eax, edx
	sub ecx, eax
	test ecx, ecx
	jle PM_Weapon_ReloadDelayedAction_10
	mov [ebx+0x40], ecx
PM_Weapon_ReloadDelayedAction_90:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
PM_Weapon_ReloadDelayedAction_30:
	mov ecx, [esi+0x39c]
	test ecx, ecx
	jnz PM_Weapon_ReloadDelayedAction_80
	jmp PM_Weapon_ReloadDelayedAction_90
PM_Weapon_ReloadDelayedAction_60:
	mov ecx, [esi+0x390]
	jmp PM_Weapon_ReloadDelayedAction_100
PM_Weapon_ReloadDelayedAction_40:
	mov ecx, [esi+0x39c]
	mov eax, [esi+0x398]
	cmp ecx, eax
	jl PM_Weapon_ReloadDelayedAction_70
	mov ecx, eax
	jmp PM_Weapon_ReloadDelayedAction_70
	nop


;PM_SetReloadingState(playerState_s*)
PM_SetReloadingState:
%ifdef OL_SERVER
	; Direct native C1 dual-mag phase; EAX is the retained ps argument.
	extern FastMagSetReloadingState
	push ebp
	mov ebp, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], eax
	call FastMagSetReloadingState
	test eax, eax
	mov eax, [esp]
	mov esp, ebp
	pop ebp
	jz PM_SetReloadingState_retained
	ret
PM_SetReloadingState_retained:
%endif
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov ebx, eax
	mov eax, [eax+0xe8]
	mov edx, [eax*4+bg_weaponDefs]
	mov eax, [edx+0x330]
	mov esi, [ebx+eax*4+0x35c]
	test esi, esi
	jnz PM_SetReloadingState_10
	mov ecx, [edx+0x12c]
	test ecx, ecx
	jz PM_SetReloadingState_20
PM_SetReloadingState_10:
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jle PM_SetReloadingState_30
PM_SetReloadingState_110:
	mov eax, [edx+0x388]
	mov [ebx+0x3c], eax
	mov dword [esp+0x4], 0xf
	mov [esp], ebx
	call PM_AddEvent
PM_SetReloadingState_130:
	xor eax, eax
	cmp dword [ebx+0xec], 0xa
	setz al
	add eax, 0x7
	mov [ebx+0xec], eax
	mov edi, [ebx+0xe8]
	mov edx, [edi*4+bg_weaponDefs]
	mov [ebp-0x1c], edx
	sub eax, 0x9
	cmp eax, 0x1
	jbe PM_SetReloadingState_40
	mov eax, [edx+0x330]
	mov eax, [ebx+eax*4+0x35c]
	test eax, eax
	jz PM_SetReloadingState_50
	mov eax, edx
PM_SetReloadingState_100:
	mov esi, [eax+0x388]
	mov edx, eax
PM_SetReloadingState_170:
	mov eax, [edx+0x394]
	test eax, eax
	jz PM_SetReloadingState_60
	cmp esi, eax
	cmovg esi, eax
	mov ecx, [ebp-0x1c]
PM_SetReloadingState_150:
	mov eax, [ecx+0x4e8]
	test eax, eax
	jz PM_SetReloadingState_70
	mov edx, edi
	sar edx, 0x5
	and edi, 0x1f
	mov eax, 0x1
	mov ecx, edi
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_RECHAMBER
	jz PM_SetReloadingState_70
	test esi, esi
	jz PM_SetReloadingState_80
PM_SetReloadingState_180:
	mov edx, [ebp-0x1c]
	mov eax, [edx+0x374]
	cmp esi, eax
	cmovle eax, esi
	test eax, eax
	mov ecx, 0x1
	cmovz eax, ecx
	mov [ebx+0x40], eax
PM_SetReloadingState_140:
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_SetReloadingState_50:
	mov eax, [edx+0x12c]
	test eax, eax
	jz PM_SetReloadingState_90
	mov eax, [ebp-0x1c]
	jmp PM_SetReloadingState_100
PM_SetReloadingState_30:
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0xd
	mov [ebx+0x624], eax
	jmp PM_SetReloadingState_110
PM_SetReloadingState_20:
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jle PM_SetReloadingState_120
	mov eax, [edx+0x390]
	mov [ebx+0x3c], eax
	mov dword [esp+0x4], 0x10
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_SetReloadingState_130
PM_SetReloadingState_70:
	test esi, esi
	jz PM_SetReloadingState_140
	mov [ebx+0x40], esi
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_SetReloadingState_60:
	mov ecx, edx
	jmp PM_SetReloadingState_150
PM_SetReloadingState_40:
	mov esi, [edx+0x39c]
	test esi, esi
	jz PM_SetReloadingState_160
	mov eax, [edx+0x398]
	cmp esi, eax
	jl PM_SetReloadingState_160
	mov esi, eax
	mov ecx, [ebp-0x1c]
	jmp PM_SetReloadingState_150
PM_SetReloadingState_90:
	mov esi, [edx+0x390]
	mov edx, [ebp-0x1c]
	jmp PM_SetReloadingState_170
PM_SetReloadingState_120:
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0xe
	mov [ebx+0x624], eax
	mov eax, [edx+0x390]
	mov [ebx+0x3c], eax
	mov dword [esp+0x4], 0x10
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_SetReloadingState_130
PM_SetReloadingState_80:
	mov esi, [ebx+0x3c]
	jmp PM_SetReloadingState_180
PM_SetReloadingState_160:
	mov ecx, [ebp-0x1c]
	jmp PM_SetReloadingState_150
	nop


;PM_BeginWeaponReload(playerState_s*)
PM_BeginWeaponReload:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov ebx, eax
	mov edx, [eax+0xe8]
	mov esi, [edx*4+bg_weaponDefs]
	mov eax, [eax+0xec]
	test eax, eax
	jz PM_BeginWeaponReload_10
	cmp eax, 0x5
	jz PM_BeginWeaponReload_10
	cmp eax, 0x6
	jz PM_BeginWeaponReload_10
	sub eax, 0x16
	cmp eax, 0x2
	ja PM_BeginWeaponReload_20
PM_BeginWeaponReload_10:
	test edx, edx
	jz PM_BeginWeaponReload_20
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	cmp edx, eax
	jae PM_BeginWeaponReload_20
	mov eax, [esi+0x500]
	test eax, eax
	jz PM_BeginWeaponReload_30
PM_BeginWeaponReload_130:
	mov dword [ebx+0xf0], 0x0
	mov dword [esp+0x4], 0xe
	mov [esp], ebx
	call PM_AddEvent
	mov dword [esp+0x4], 0x13
	mov [esp], ebx
	call PM_AddEvent
	mov eax, [esi+0x530]
	test eax, eax
	jz PM_BeginWeaponReload_40
	mov eax, [esi+0x398]
	test eax, eax
	jz PM_BeginWeaponReload_40
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_BeginWeaponReload_50
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0xf
	mov [ebx+0x624], eax
PM_BeginWeaponReload_50:
	mov eax, [esi+0x398]
	mov [ebx+0x3c], eax
	mov dword [ebx+0xec], 0x9
	mov dword [esp+0x4], 0x11
	mov [esp], ebx
	call PM_AddEvent
	mov edi, [ebx+0xe8]
	mov eax, [edi*4+bg_weaponDefs]
	mov [ebp-0x1c], eax
	mov eax, [ebx+0xec]
	sub eax, 0x9
	cmp eax, 0x1
	jbe PM_BeginWeaponReload_60
	mov ecx, [ebp-0x1c]
	mov eax, [ecx+0x330]
	mov eax, [ebx+eax*4+0x35c]
	test eax, eax
	jnz PM_BeginWeaponReload_70
	mov eax, [ecx+0x12c]
	test eax, eax
	jz PM_BeginWeaponReload_80
	mov eax, [ebp-0x1c]
	jmp PM_BeginWeaponReload_90
PM_BeginWeaponReload_70:
	mov eax, ecx
PM_BeginWeaponReload_90:
	mov esi, [eax+0x388]
	mov edx, eax
PM_BeginWeaponReload_150:
	mov eax, [edx+0x394]
	test eax, eax
	jz PM_BeginWeaponReload_100
	cmp esi, eax
	cmovg esi, eax
	mov ecx, [ebp-0x1c]
PM_BeginWeaponReload_140:
	mov eax, [ecx+0x4e8]
	test eax, eax
	jz PM_BeginWeaponReload_110
	mov edx, edi
	sar edx, 0x5
	and edi, 0x1f
	mov eax, 0x1
	mov ecx, edi
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_RECHAMBER
	jz PM_BeginWeaponReload_110
	test esi, esi
	jnz PM_BeginWeaponReload_120
	mov esi, [ebx+0x3c]
PM_BeginWeaponReload_120:
	mov edx, [ebp-0x1c]
	mov eax, [edx+0x374]
	cmp esi, eax
	cmovle eax, esi
	test eax, eax
	mov ecx, 0x1
	cmovz eax, ecx
	mov [ebx+0x40], eax
PM_BeginWeaponReload_20:
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_BeginWeaponReload_40:
	mov eax, ebx
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_SetReloadingState
PM_BeginWeaponReload_30:
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0xa
	mov [esp], ebx
	call BG_AnimScriptEvent
	jmp PM_BeginWeaponReload_130
PM_BeginWeaponReload_110:
	test esi, esi
	jz PM_BeginWeaponReload_20
	mov [ebx+0x40], esi
	jmp PM_BeginWeaponReload_20
PM_BeginWeaponReload_100:
	mov ecx, edx
	jmp PM_BeginWeaponReload_140
PM_BeginWeaponReload_80:
	mov esi, [ecx+0x390]
	mov edx, [ebp-0x1c]
	jmp PM_BeginWeaponReload_150
PM_BeginWeaponReload_60:
	mov edx, [ebp-0x1c]
	mov esi, [edx+0x39c]
	test esi, esi
	jz PM_BeginWeaponReload_100
	mov eax, [edx+0x398]
	cmp esi, eax
	jl PM_BeginWeaponReload_160
	mov esi, eax
	mov ecx, [ebp-0x1c]
	jmp PM_BeginWeaponReload_140
PM_BeginWeaponReload_160:
	mov ecx, [ebp-0x1c]
	jmp PM_BeginWeaponReload_140
	nop


;PM_Weapon_CheckForReload(pmove_t*)
PM_Weapon_CheckForReload:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov esi, [ebp+0x8]
	mov ebx, [esi]
	mov edi, [ebx+0xe8]
	mov eax, [edi*4+bg_weaponDefs]
	mov [ebp-0x20], eax
	mov ecx, [ebx+0xec]
	lea eax, [ecx-0xc]
	cmp eax, 0x8
	jbe PM_Weapon_CheckForReload_10
%ifdef OL_SERVER
	; C1 PM_Weapon_CheckForReload 0x41a783..0x41a78b: none in 31..33.
	lea eax, [ecx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_CheckForReload_10
%endif
	mov edx, [esi+0x8]
	shr edx, 0x4
	and edx, 0x1
	mov eax, [ebx+0x10]
	test al, 0x1
	jnz PM_Weapon_CheckForReload_20
	mov eax, [ebp-0x20]
	mov eax, [eax+0x530]
	test eax, eax
	jnz PM_Weapon_CheckForReload_30
PM_Weapon_CheckForReload_70:
	cmp ecx, 0x1
	jl PM_Weapon_CheckForReload_40
	cmp ecx, 0x4
	jg PM_Weapon_CheckForReload_50
PM_Weapon_CheckForReload_10:
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_Weapon_CheckForReload_30:
	cmp ecx, 0x9
	jz PM_Weapon_CheckForReload_60
	cmp ecx, 0x7
	jnz PM_Weapon_CheckForReload_70
PM_Weapon_CheckForReload_60:
%ifdef OL_SERVER
	; C1 0x41b1f0 resolves which button fires a hand from the weapon and the
	; akimbo flag, so bit 0 is only the answer for a bare first hand: an akimbo
	; first hand fires on 0x100000, remote-detonated equipment fires on 0x100000
	; for either hand, and the second hand takes whichever bit is left over.
	; Ask for this hand's resolved request instead of reading bit 0 directly.
	extern WeaponStateFireRequest
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [esi+0x8], 0x1
%endif
	jz PM_Weapon_CheckForReload_70
%ifdef OL_SERVER
	; The edge is against the old command's trigger of the same hand, which
	; for an akimbo hand 0 is 0x100000 (C1 0x41ab59), not bit 0. ESI is the
	; pmove and EBX the playerState, as in the request test above.
	extern WeaponStateHandZeroMask
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateHandZeroMask
	mov esp, [esp+0xc]
	test [esi+0x28], eax
	popad
%else
	test byte [esi+0x28], 0x1
%endif
	jnz PM_Weapon_CheckForReload_70
	cmp ecx, 0x9
	jz PM_Weapon_CheckForReload_80
	cmp ecx, 0x7
	jnz PM_Weapon_CheckForReload_70
	mov cl, 0x8
	mov dword [ebx+0xec], 0x8
PM_Weapon_CheckForReload_50:
	lea eax, [ecx-0x7]
	cmp eax, 0x4
	ja PM_Weapon_CheckForReload_40
	mov esi, [esi+0xe8]
	test esi, esi
	jz PM_Weapon_CheckForReload_10
	mov eax, [edi*4+bg_weaponDefs]
	mov ecx, [eax+0x500]
	test ecx, ecx
	jnz PM_Weapon_CheckForReload_10
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0xa
	mov [esp], ebx
	call BG_AnimScriptEvent
	jmp PM_Weapon_CheckForReload_10
PM_Weapon_CheckForReload_20:
	and eax, 0xfffffffe
	mov [ebx+0x10], eax
%ifdef OL_SERVER
	; The queued reload request in weapFlags bit 0 is consumed here for the
	; engine hand. Forward it so the second hand's own CheckForReload sees the
	; same request instead of inferring one from the first hand's state.
	extern WeaponStateReloadRequest
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateReloadRequest
	mov esp, [esp+0xc]
	popad
%endif
	mov edx, 0x1
	mov eax, [ebp-0x20]
	mov eax, [eax+0x530]
	test eax, eax
	jz PM_Weapon_CheckForReload_70
	jmp PM_Weapon_CheckForReload_30
PM_Weapon_CheckForReload_40:
	mov eax, [edi*4+bg_weaponDefs]
	mov edi, [eax+0x330]
	mov esi, [eax+0x328]
	mov [ebp-0x1c], esi
	test edx, edx
	jnz PM_Weapon_CheckForReload_90
PM_Weapon_CheckForReload_110:
	xor edx, edx
PM_Weapon_CheckForReload_150:
	mov edi, [ebx+edi*4+0x35c]
	test edi, edi
	jnz PM_Weapon_CheckForReload_100
	mov eax, [ebp-0x1c]
	mov esi, [ebx+eax*4+0x15c]
	test esi, esi
	jz PM_Weapon_CheckForReload_100
	cmp ecx, 0x5
	jz PM_Weapon_CheckForReload_100
	lea eax, [ecx-0x16]
	cmp eax, 0x2
	jbe PM_Weapon_CheckForReload_100
	mov eax, ebx
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_BeginWeaponReload
PM_Weapon_CheckForReload_100:
	test edx, edx
	jz PM_Weapon_CheckForReload_10
	mov eax, ebx
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_BeginWeaponReload
PM_Weapon_CheckForReload_90:
	mov edx, [ebx+esi*4+0x15c]
	test edx, edx
	jz PM_Weapon_CheckForReload_110
	mov esi, [ebx+edi*4+0x35c]
	mov edx, [eax+0x338]
	cmp esi, edx
	jge PM_Weapon_CheckForReload_110
	cmp dword [eax+0x52c], 0x0
	jz PM_Weapon_CheckForReload_120
	mov eax, [eax+0x534]
	test eax, eax
	jz PM_Weapon_CheckForReload_130
	cmp edx, eax
	jg PM_Weapon_CheckForReload_140
PM_Weapon_CheckForReload_130:
	test esi, esi
	jnz PM_Weapon_CheckForReload_110
PM_Weapon_CheckForReload_120:
	mov edx, 0x1
	jmp PM_Weapon_CheckForReload_150
PM_Weapon_CheckForReload_80:
	mov eax, [ebp-0x20]
	mov eax, [eax+0x398]
	mov [ebp-0x24], eax
	test eax, eax
	jz PM_Weapon_CheckForReload_50
	sub eax, [ebx+0x3c]
	cvtsi2ss xmm0, eax
	cvtsi2ss xmm1, dword [ebp-0x24]
	divss xmm0, xmm1
	ucomiss xmm0, [_ZZ24PM_Weapon_CheckForReloadP7pmove_tE35MY_RELOADSTART_INTERUPT_IGNORE_FRAC]
	jbe PM_Weapon_CheckForReload_50
	mov cl, 0xa
	mov dword [ebx+0xec], 0xa
	jmp PM_Weapon_CheckForReload_50
PM_Weapon_CheckForReload_140:
	sub edx, esi
	cmp eax, edx
	jg PM_Weapon_CheckForReload_110
	mov edx, 0x1
	jmp PM_Weapon_CheckForReload_150


;PM_Weapon_CheckForOffHand(pmove_t*)
PM_Weapon_CheckForOffHand:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x3c
	mov edi, eax
	mov ebx, [eax]
%ifdef OL_SERVER
	; WEAP_OFFHAND_DISABLED: separate from the global weapons gate.
	test dword [ebx+0x10], 0x1000
	jnz PM_Weapon_CheckForOffHand_10
%endif
	test dword [ebx+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz PM_Weapon_CheckForOffHand_10
	cmp byte [ebx+0x10], 0x0
	js PM_Weapon_CheckForOffHand_10
%ifdef OL_SERVER
	; C1 0x41cac8 refuses the offhand while sprinting (0x4000), not while
	; shellshocked (0x8000, the sign bit the IW3 test read).
	test dword [ebx+0xc], 0x4000
	jnz PM_Weapon_CheckForOffHand_10
%else
	cmp word [ebx+0xc], 0x0
	js PM_Weapon_CheckForOffHand_10
%endif
	mov eax, [ebx+0xe8]
	test eax, eax
	jnz PM_Weapon_CheckForOffHand_20
PM_Weapon_CheckForOffHand_100:
	mov edx, [ebx+0xec]
%ifdef OL_SERVER
	; C1 0x41cae2..0x41caf2 refuses states 16..20 and allows only its offhand
	; end, 21. The retained 15..19 below is the same set minus the phase the
	; retained engine does not have, so the new one joins it here.
	cmp edx, 0x1b
	jz PM_Weapon_CheckForOffHand_10
	; C1 0x41caf8..0x41cb00 refuses the dive phases 31..33 (native 28..30).
	lea eax, [edx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_CheckForOffHand_10
%endif
	lea eax, [edx-0xf]
	cmp eax, 0x4
	jbe PM_Weapon_CheckForOffHand_10
	lea eax, [edx-0x19]
	cmp eax, 0x1
	jbe PM_Weapon_CheckForOffHand_10
	CMD_OFFHAND esi, edi, eax
	mov edx, esi
	sar edx, 0x5
	mov ecx, esi
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_OWNED
	jz PM_Weapon_CheckForOffHand_30
	mov [ebx+0xe0], esi
PM_Weapon_CheckForOffHand_30:
%ifdef OL_SERVER
	; C1 does not search the inventory for an offhand of a class. It picks one
	; of three *selected* weapons out of the playerState with CoDOL buttons
	; 0x4000/0x8000/0x10000 (PM_Weapon_CheckForOffHand 0x41cb41..0x41cba2),
	; each on a rising edge against oldcmd and each gated by the same bit of
	; the weapon-flags word, then applies the empty-magazine and ground gates
	; at 0x41cbaf and 0x41cbcd. C1 0x40c7e0 accepts an explicit weapon in
	; any slot before its class-based empty-slot fallback. The third button
	; never reaches the native command at all --
	; ToEngineUsercmd masks it away -- so the selector reads the raw
	; CoDOL word this pmove is executing. EBX is the playerState, EDI the
	; pmove; the call uses the outgoing argument area the retained calls in
	; this function already use, and clobbers the same registers.
	extern WeaponStateSelectOffhand
	mov [esp+0x4], edi
	mov [esp], ebx
	call WeaponStateSelectOffhand
	mov esi, eax
	test eax, eax
	jz PM_Weapon_CheckForOffHand_10
	jmp PM_Weapon_CheckForOffHand_190
%endif
	mov eax, [edi+0x8]
	test ah, 0x40
	jnz PM_Weapon_CheckForOffHand_40
	test ax, ax
	jns PM_Weapon_CheckForOffHand_10
	mov eax, [ebx+0xe4]
	test eax, eax
	jz PM_Weapon_CheckForOffHand_50
	mov dword [ebp-0x20], 0x3
	mov dword [ebp-0x1c], 0x3
	jmp PM_Weapon_CheckForOffHand_60
PM_Weapon_CheckForOffHand_110:
	mov dword [esp+0x4], 0xd
	mov [esp], ebx
	call PM_AddEvent
	mov edi, [bg_lastParsedWeaponIndex]
	add edi, 0x1
	cmp edi, 0x1
	jle PM_Weapon_CheckForOffHand_10
	mov edx, 0x1
	mov esi, bg_weaponDefs
	jmp PM_Weapon_CheckForOffHand_70
PM_Weapon_CheckForOffHand_80:
	add edx, 0x1
	add esi, 0x4
	cmp edi, edx
	jz PM_Weapon_CheckForOffHand_10
PM_Weapon_CheckForOffHand_70:
	mov eax, [esi+0x4]
	mov ecx, [ebp-0x1c]
	cmp [eax+0x144], ecx
	jnz PM_Weapon_CheckForOffHand_80
	mov eax, edx
	sar eax, 0x5
	mov [ebp-0x2c], eax
	mov ecx, edx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, eax
	mov eax, [ebp-0x2c]
	WB_TEST ebx, eax, ecx, WB_OWNED
	jz PM_Weapon_CheckForOffHand_80
	test edx, edx
	jz PM_Weapon_CheckForOffHand_10
	cmp dword [ebp-0x20], 0x1
	jz PM_Weapon_CheckForOffHand_90
	mov dword [esp+0x4], 0x44
	mov [esp], ebx
	call PM_AddEvent
PM_Weapon_CheckForOffHand_10:
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_Weapon_CheckForOffHand_20:
	mov eax, [eax*4+bg_weaponDefs]
	cmp dword [eax+0x12c], 0x1
	jnz PM_Weapon_CheckForOffHand_100
	mov edx, [eax+0x5b4]
	test edx, edx
	jnz PM_Weapon_CheckForOffHand_100
	cmp dword [eax+0x5a8], 0x1
	sbb eax, eax
	and eax, 0xfff80001
	add eax, 0x80000
	test [edi+0x8], eax
	jnz PM_Weapon_CheckForOffHand_10
	jmp PM_Weapon_CheckForOffHand_100
PM_Weapon_CheckForOffHand_40:
	mov dword [ebp-0x20], 0x1
	mov dword [ebp-0x1c], 0x1
PM_Weapon_CheckForOffHand_60:
	mov eax, [ebp-0x1c]
	mov [esp+0x4], eax
	mov [esp], ebx
	call BG_GetFirstAvailableOffhand
	mov esi, eax
	test eax, eax
	jz PM_Weapon_CheckForOffHand_110
	; The CoDOL selector above joins the retained path here, with the chosen
	; weapon in EAX and ESI exactly as BG_GetFirstAvailableOffhand leaves it.
	; Everything from here on -- EV_SWITCH_OFFHAND, ps->offHandIndex, the
	; grenade-type check and the throwback branch -- is the retained code and
	; is what C1 does too: it queues its own event 51 with the selected weapon
	; at 0x41cbef and stores it to ps+0x328 at 0x41cc13.
PM_Weapon_CheckForOffHand_190:
	mov [esp+0x8], ebx
	mov [esp+0x4], eax
	mov dword [esp], 0x22
	call BG_AddPredictableEventToPlayerstate
	mov [ebx+0xe0], esi
	mov esi, [esi*4+bg_weaponDefs]
	cmp dword [esi+0x12c], 0x1
	jz PM_Weapon_CheckForOffHand_120
	mov eax, [esi]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_s_only_grenades_
	mov dword [esp], 0x2
	call Com_Error
PM_Weapon_CheckForOffHand_120:
	mov edi, [esi+0x144]
	test edi, edi
	jz PM_Weapon_CheckForOffHand_130
PM_Weapon_CheckForOffHand_180:
	cmp dword [ebx+0x5a8], 0x7ff
	jz PM_Weapon_CheckForOffHand_140
PM_Weapon_CheckForOffHand_170:
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandInit reads the state it is leaving before it
	; writes 16 (0x41c25e..0x41c277); keep it for the weapon time below.
	mov eax, [ebx+0xec]
	mov [ebp-0x30], eax
%endif
	mov dword [ebx+0xec], 0xf
	mov dword [ebx+0x40], 0x0
	and dword [ebx+0x10], 0xfffffffd
	mov dword [ebx+0x48], 0x7ff
%ifdef OL_SERVER
	; C1's PM_Weapon_OffHandInit queues two events, not one: 0x41c239 the
	; weapon-change announcement with the offhand weapon as its parameter, and
	; 0x41c2aa the reset-ADS. The retained event 0xe below IS that second one
	; -- native EV_RESET_ADS, which event_map.inc already translates to
	; C1's 0x11 -- so only the first is missing. ESI holds the weapon this
	; selection chose and EBX the playerState. ESI already holds the weapon's
	; definition by this point, so the weapon itself comes back out of
	; ps->offHandIndex, which PM_Weapon_CheckForOffHand_190 has just written.
	mov eax, [ebx+0xe0]
	mov [esp+0x8], ebx
	mov [esp+0x4], eax
	mov dword [esp], 0x88
	call BG_AddPredictableEventToPlayerstate
%endif
	mov dword [esp+0x4], 0xe
	mov [esp], ebx
	call PM_AddEvent
	and dword [ebx+0xc], 0xffffffef
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_CheckForOffHand_150
%ifdef OL_SERVER
	; C1 0x41c2e3: an offhand raised during a drop (C1 3..5, native 3/4
	; including the alternate drop) keeps the drop's remaining time and its
	; animation. 0x41c302..0x41c314: out of a sprint phase (C1 23..25,
	; native 22..24) the time is zero and the sprint animation stays.
	mov ecx, [ebp-0x30]
	lea edx, [ecx-0x3]
	cmp edx, 0x1
	jbe PM_Weapon_CheckForOffHand_200
	lea edx, [ecx-0x16]
	cmp edx, 0x2
	ja PM_Weapon_CheckForOffHand_210
	mov dword [ebx+0x3c], 0x0
	jmp PM_Weapon_CheckForOffHand_200
PM_Weapon_CheckForOffHand_210:
%endif
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x3b4]
	mov [ebx+0x3c], eax
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandInit 0x41c33d..0x41c36a: weaponTime /= value28.
	; The no-weapon 100 ms arm below (C1 0x41c37d) is not divided.
	CALL_PS WeaponOffhandPhaseTime, ebx
%endif
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD (0x41c327)
	jg PM_Weapon_CheckForOffHand_200
%else
	cmp dword [ebx+0x4], 0x6
	jg PM_Weapon_CheckForOffHand_10
%endif
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x13
	mov [ebx+0x624], eax
%ifdef OL_SERVER
PM_Weapon_CheckForOffHand_200:
	; C1 0x41c387..0x41c39d: a throwing knife (serialized weapon class 9)
	; skips the init phase and prepares on this same frame.
	extern WeaponStateOffhandPreparesAtInit
	mov [esp], ebx
	call WeaponStateOffhandPreparesAtInit
	test eax, eax
	jnz PM_Weapon_CheckForOffHand_160
%endif
	jmp PM_Weapon_CheckForOffHand_10
PM_Weapon_CheckForOffHand_50:
	mov dword [ebp-0x20], 0x2
	mov dword [ebp-0x1c], 0x2
	jmp PM_Weapon_CheckForOffHand_60
PM_Weapon_CheckForOffHand_140:
	mov esi, [ebx+0xe8]
	test esi, esi
	jz PM_Weapon_CheckForOffHand_160
	cmp dword [ebx+0xec], 0x14
	jnz PM_Weapon_CheckForOffHand_170
PM_Weapon_CheckForOffHand_160:
	mov eax, ebx
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_Weapon_OffHandPrepare
PM_Weapon_CheckForOffHand_130:
	mov eax, [esi]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_s_no_offhand_cla
	mov dword [esp], 0x2
	call Com_Error
	jmp PM_Weapon_CheckForOffHand_180
PM_Weapon_CheckForOffHand_150:
	mov dword [ebx+0x3c], 0x64
%ifdef OL_SERVER
	jmp PM_Weapon_CheckForOffHand_200
%else
	jmp PM_Weapon_CheckForOffHand_10
%endif
PM_Weapon_CheckForOffHand_90:
	mov dword [esp+0x4], 0x43
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_Weapon_CheckForOffHand_10


;BG_WeaponAmmo(playerState_s const*, int)
BG_WeaponAmmo:
	push ebp
	mov ebp, esp
	push ebx
	mov ebx, [ebp+0x8]
	mov eax, [ebp+0xc]
	mov eax, [eax*4+bg_weaponDefs]
	mov edx, [eax+0x328]
	mov ecx, [eax+0x330]
	mov eax, [ebx+edx*4+0x15c]
	add eax, [ebx+ecx*4+0x35c]
	pop ebx
	pop ebp
	ret


;BG_GetBobCycle(playerState_s const*)
BG_GetBobCycle:
	push ebp
	mov ebp, esp
	sub esp, 0x4
	mov eax, [ebp+0x8]
	movzx eax, byte [eax+0x8]
	cvtsi2ss xmm0, eax
	divss xmm0, dword [_float_255_00000000]
	cvtss2sd xmm0, xmm0
	mulsd xmm0, [_double_3_14159265]
	addsd xmm0, xmm0
	addsd xmm0, [_double_6_28318531]
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x4], xmm0
	fld dword [ebp-0x4]
	leave
	ret
	nop


;BG_AdvanceTrace(BulletFireParams*, BulletTraceResults*, float)
BG_AdvanceTrace:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov ebx, [ebp+0x8]
	mov esi, [ebp+0xc]
	mov [esp], esi
	call Trace_GetEntityHitId
	movzx edx, ax
	mov [ebx+0x4], edx
	cmp ax, 0x7fe
	jz BG_AdvanceTrace_10
BG_AdvanceTrace_20:
	lea ecx, [ebx+0x1c]
	lea edx, [esi+0x30]
	mov eax, [esi+0x30]
	mov [ebx+0x1c], eax
	mov eax, [edx+0x4]
	mov [ecx+0x4], eax
	mov eax, [edx+0x8]
	mov [ecx+0x8], eax
	mov eax, 0x1
BG_AdvanceTrace_40:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
BG_AdvanceTrace_10:
	movss xmm0, dword [ebp+0x10]
	ucomiss xmm0, [_float_0_00000000]
	jbe BG_AdvanceTrace_20
	lea eax, [esi+0x4]
	movss xmm1, dword [esi+0x4]
	mulss xmm1, [ebx+0x34]
	movss xmm0, dword [eax+0x4]
	mulss xmm0, [ebx+0x38]
	addss xmm1, xmm0
	movss xmm0, dword [eax+0x8]
	mulss xmm0, [ebx+0x3c]
	addss xmm1, xmm0
	xorps xmm1, [_data16_80000000]
	ucomiss xmm1, [_float_0_12500000]
	jae BG_AdvanceTrace_30
	jp BG_AdvanceTrace_30
	movss xmm1, dword [ebp+0x10]
	mulss xmm1, [_float_8_00000000]
	lea edx, [ebx+0x1c]
	lea eax, [esi+0x30]
	movaps xmm0, xmm1
	mulss xmm0, [ebx+0x34]
	addss xmm0, [esi+0x30]
	movss [ebx+0x1c], xmm0
	movaps xmm0, xmm1
	mulss xmm0, [ebx+0x38]
	addss xmm0, [eax+0x4]
	movss [edx+0x4], xmm0
	mulss xmm1, [ebx+0x3c]
	addss xmm1, [eax+0x8]
	movss [edx+0x8], xmm1
	xor eax, eax
	jmp BG_AdvanceTrace_40
BG_AdvanceTrace_30:
	movss xmm0, dword [ebp+0x10]
	divss xmm0, xmm1
	movaps xmm1, xmm0
	lea edx, [ebx+0x1c]
	lea eax, [esi+0x30]
	mulss xmm0, [ebx+0x34]
	addss xmm0, [esi+0x30]
	movss [ebx+0x1c], xmm0
	movaps xmm0, xmm1
	mulss xmm0, [ebx+0x38]
	addss xmm0, [eax+0x4]
	movss [edx+0x4], xmm0
	mulss xmm1, [ebx+0x3c]
	addss xmm1, [eax+0x8]
	movss [edx+0x8], xmm1
	mov eax, 0x1
	jmp BG_AdvanceTrace_40


;BG_GetWeaponDef(unsigned int)
BG_GetWeaponDef:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	pop ebp
	ret
	nop


;BG_AmmoForWeapon(int)
BG_AmmoForWeapon:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x328]
	pop ebp
	ret
	nop


;BG_CanHoldBreath(playerState_s const*)
BG_CanHoldBreath:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax+0x5d8]
	test eax, eax
	setz al
	movzx eax, al
	pop ebp
	ret


;BG_ClipForWeapon(int)
BG_ClipForWeapon:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	pop ebp
	ret
	nop


;BG_GetNumWeapons()
BG_GetNumWeapons:
	push ebp
	mov ebp, esp
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	pop ebp
	ret
	nop
	add [eax], al


;BG_IsWeaponValid(playerState_s const*, unsigned int)
BG_IsWeaponValid:
	push ebp
	mov ebp, esp
	mov ecx, [ebp+0xc]
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	cmp ecx, eax
	jae BG_IsWeaponValid_10
	mov eax, ecx
	sar eax, 0x5
	and ecx, 0x1f
	mov edx, 0x1
	shl edx, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, eax, edx, WB_OWNED
	jnz BG_IsWeaponValid_20
BG_IsWeaponValid_10:
	xor eax, eax
	pop ebp
	ret
BG_IsWeaponValid_20:
	mov eax, 0x1
	pop ebp
	ret


;BG_ClearWeaponDef()
BG_ClearWeaponDef:
	push ebp
	mov ebp, esp
	sub esp, 0x8
	call BG_LoadDefaultWeaponDef
	mov [bg_weaponDefs], eax
	mov [bg_weapAmmoTypes], eax
	mov dword [bg_numAmmoTypes], 0x1
	mov [bg_sharedAmmoCaps], eax
	mov dword [bg_numSharedAmmoCaps], 0x1
	mov [bg_weapClips], eax
	mov dword [bg_numWeapClips], 0x1
	mov eax, bg_itemlist
	lea edx, [eax+ITEM_COUNT*4-4]
BG_ClearWeaponDef_10:
	mov dword [eax+0x4], 0x0
	add eax, 0x4
	cmp edx, eax
	jnz BG_ClearWeaponDef_10
	call BG_LoadPlayerAnimTypes
	leave
	jmp BG_InitWeaponStrings
	nop


;BG_GetWeaponIndex(WeaponDef const*)
BG_GetWeaponIndex:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	mov esi, [ebp+0x8]
	mov ebx, [bg_lastParsedWeaponIndex]
	cmp esi, [bg_weaponDefs]
	jz BG_GetWeaponIndex_10
	xor ecx, ecx
	mov edx, bg_weaponDefs
BG_GetWeaponIndex_20:
	add ecx, 0x1
	cmp ecx, ebx
	ja BG_GetWeaponIndex_10
	mov eax, [edx+0x4]
	add edx, 0x4
	cmp eax, esi
	jnz BG_GetWeaponIndex_20
	mov eax, ecx
	pop ebx
	pop esi
	pop ebp
	ret
BG_GetWeaponIndex_10:
	xor ecx, ecx
	mov eax, ecx
	pop ebx
	pop esi
	pop ebp
	ret


;BG_GetAmmoPlayerMax(playerState_s const*, unsigned int, unsigned int)
BG_GetAmmoPlayerMax:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x8
	mov eax, [ebp+0xc]
	mov edi, [eax*4+bg_weaponDefs]
	mov eax, [edi+0x344]
	test eax, eax
	js BG_GetAmmoPlayerMax_10
BG_GetAmmoPlayerMax_60:
	mov eax, [eax*4+bg_sharedAmmoCaps]
	mov eax, [eax+0x348]
	mov [ebp-0x10], eax
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetAmmoPlayerMax_10:
	mov ecx, [edi+0x500]
	test ecx, ecx
	jnz BG_GetAmmoPlayerMax_20
	mov ecx, [bg_lastParsedWeaponIndex]
	mov [ebp-0x14], ecx
	mov dword [ebp-0x10], 0x0
	xor ebx, ebx
	mov esi, bg_weaponDefs
BG_GetAmmoPlayerMax_50:
	cmp ebx, [ebp+0x10]
	jz BG_GetAmmoPlayerMax_30
	mov edx, ebx
	sar edx, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, edx, eax, WB_OWNED
	jz BG_GetAmmoPlayerMax_30
	mov edx, [esi]
	mov eax, [edx+0x328]
	cmp eax, [edi+0x328]
	jz BG_GetAmmoPlayerMax_40
BG_GetAmmoPlayerMax_30:
	add ebx, 0x1
	add esi, 0x4
	cmp ebx, [ebp-0x14]
	jbe BG_GetAmmoPlayerMax_50
	mov eax, [ebp-0x10]
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetAmmoPlayerMax_20:
	mov edi, [edi+0x338]
	mov [ebp-0x10], edi
	mov eax, edi
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetAmmoPlayerMax_40:
	mov eax, [edx+0x344]
	test eax, eax
	jns BG_GetAmmoPlayerMax_60
	mov eax, [edx+0x334]
	add [ebp-0x10], eax
	jmp BG_GetAmmoPlayerMax_30


;BG_TakePlayerWeapon(playerState_s*, unsigned int, int)
BG_TakePlayerWeapon:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x1c
%ifdef OL_SERVER
	; Losing a weapon invalidates the CoDOL state that referred to it: the
	; cached primary of an equipped alternate mode, and the second hand when
	; the dual-wield weapon itself is taken. Runs before the ownership bit and
	; ps->weapon are cleared, so the weapon being removed is still known.
	extern WeaponStateTakeWeapon
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov eax, [ebp+0xc]
	mov [esp+0x4], eax
	mov eax, [ebp+0x8]
	mov [esp], eax
	call WeaponStateTakeWeapon
	mov esp, [esp+0xc]
	popad
%endif
	mov eax, [ebp+0xc]
	sar eax, 0x5
	lea edx, [eax*4]
	mov ecx, [ebp+0xc]
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST_B ecx, edx, eax, WB_OWNED
	jz BG_TakePlayerWeapon_10
	mov edi, ecx
	mov ecx, [ebp+0xc]
	mov ebx, [ecx*4+bg_weaponDefs]
	; edi is the playerState itself now; the 0x55c bitset moved out of it
	not eax
	WB_AND_B edi, edx, eax, WB_OWNED
	mov eax, [ebp+0x10]
	test eax, eax
	jnz BG_TakePlayerWeapon_20
	mov edx, [ebx+0x540]
	test edx, edx
	jnz BG_TakePlayerWeapon_30
BG_TakePlayerWeapon_70:
	mov ecx, [ebp+0x8]
BG_TakePlayerWeapon_120:
	mov edx, [ebp+0xc]
	cmp edx, [ecx+0xe8]
	jz BG_TakePlayerWeapon_40
BG_TakePlayerWeapon_90:
	mov eax, 0x1
%ifdef OL_SERVER
	; All ownership bits, including alternate modes, have now been removed.
	; Reconcile recorded offhand slots before the next snapshot can capture them.
	extern WeaponStateRefreshOffhands
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov eax, [ebp+0x8]
	mov [esp], eax
	call WeaponStateRefreshOffhands
	mov esp, [esp+0xc]
	popad
%endif
	add esp, 0x1c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_TakePlayerWeapon_30:
	mov eax, edx
	sar eax, 0x5
	lea ebx, [eax*4]
	mov ecx, edx
	and ecx, 0x1f
	mov esi, 0x1
	shl esi, cl
	mov eax, [ebp+0x8]
	WB_TEST_B eax, ebx, esi, WB_OWNED
	jz BG_TakePlayerWeapon_50
	mov eax, [ebp+0x10]
	test eax, eax
	jnz BG_TakePlayerWeapon_60
BG_TakePlayerWeapon_80:
	not esi
	WB_AND_B edi, ebx, esi, WB_OWNED
	mov eax, [edx*4+bg_weaponDefs]
	mov edx, [eax+0x540]
	test edx, edx
	jz BG_TakePlayerWeapon_70
	mov eax, edx
	sar eax, 0x5
	lea ebx, [eax*4]
	mov ecx, edx
	and ecx, 0x1f
	mov esi, 0x1
	shl esi, cl
	mov eax, [ebp+0x8]
	WB_TEST_B eax, ebx, esi, WB_OWNED
	jnz BG_TakePlayerWeapon_80
BG_TakePlayerWeapon_50:
	mov ecx, eax
	mov edx, [ebp+0xc]
	cmp edx, [ecx+0xe8]
	jnz BG_TakePlayerWeapon_90
BG_TakePlayerWeapon_40:
	mov dword [ecx+0xe8], 0x0
	jmp BG_TakePlayerWeapon_90
BG_TakePlayerWeapon_20:
	mov eax, [ecx*4+bg_weaponDefs]
	mov esi, [eax+0x328]
	mov [esp+0x8], ecx
	mov [esp+0x4], ecx
	mov eax, [ebp+0x8]
	mov [esp], eax
	call BG_GetAmmoPlayerMax
	mov edx, eax
	test eax, eax
	jnz BG_TakePlayerWeapon_100
	xor eax, eax
	mov edx, [ebp+0x8]
BG_TakePlayerWeapon_110:
	mov [edx+esi*4+0x15c], eax
	mov ecx, [ebp+0xc]
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov dword [edx+eax*4+0x35c], 0x0
	mov edx, [ebx+0x540]
	test edx, edx
	jz BG_TakePlayerWeapon_70
	jmp BG_TakePlayerWeapon_30
BG_TakePlayerWeapon_10:
	xor eax, eax
	add esp, 0x1c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_TakePlayerWeapon_100:
	mov ecx, [ebp+0xc]
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x328]
	mov ecx, [ebp+0x8]
	mov eax, [ecx+eax*4+0x15c]
	cmp edx, eax
	cmovl eax, edx
	mov edx, ecx
	jmp BG_TakePlayerWeapon_110
BG_TakePlayerWeapon_60:
	mov eax, [edx*4+bg_weaponDefs]
	mov eax, [eax+0x328]
	mov ecx, [ebp+0x8]
	mov dword [ecx+eax*4+0x15c], 0x0
	mov eax, [edx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov dword [ecx+eax*4+0x35c], 0x0
	not esi
	and [edi+ebx], esi
	mov eax, [edx*4+bg_weaponDefs]
	mov edx, [eax+0x540]
	test edx, edx
	jz BG_TakePlayerWeapon_70
	mov eax, edx
	sar eax, 0x5
	lea ebx, [eax*4]
	mov ecx, edx
	and ecx, 0x1f
	mov esi, 0x1
	shl esi, cl
	mov ecx, [ebp+0x8]
	WB_TEST_B ecx, ebx, esi, WB_OWNED
	jnz BG_TakePlayerWeapon_60
	jmp BG_TakePlayerWeapon_120


;BG_UsingSniperScope(playerState_s const*)
BG_UsingSniperScope:
	push ebp
	mov ebp, esp
	mov edx, [ebp+0x8]
	mov eax, [edx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x438]
	test eax, eax
	jz BG_UsingSniperScope_10
	movss xmm0, dword [edx+0xf4]
	ucomiss xmm0, [_float_0_00000000]
	jbe BG_UsingSniperScope_10
	mov eax, 0x1
	pop ebp
	ret
BG_UsingSniperScope_10:
	xor eax, eax
	pop ebp
	ret
	nop


;BG_WeaponFireRecoil(playerState_s const*, float*, float*)
BG_WeaponFireRecoil:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x7c
	mov edx, [ebp+0x8]
	mov edi, [ebp+0xc]
	mov esi, [ebp+0x10]
	test byte [edx+0x10], 0x2
	jz BG_WeaponFireRecoil_10
	mov eax, [edx+0xe0]
	mov ebx, [eax*4+bg_weaponDefs]
	movss xmm0, dword [edx+0xf4]
	movss [ebp-0x20], xmm0
%ifdef OL_SERVER
	; Serve this fire event with the kick-restriction window of the hand that
	; produced it. C1 keeps one window per hand at ps+0x274 + hand*0x1c
	; (written 0x41b580/0x41b59b, ticked 0x41ad0d); this retained signature has
	; no hand argument, so the owning hand comes from the recorded queue order
	; of ps->events. With nothing recorded the helper returns the engine's own
	; ps->weaponRestrictKickTime, which is the retained behaviour exactly.
	; popad reloads ECX from the saved slot, and xmm0 is restored from the
	; spill this function just made, since a C call may clobber it.
	extern WeaponStateRecoilRestrictTime
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], edx
	call WeaponStateRecoilRestrictTime
	mov esp, [esp+0xc]
	mov [esp+0x18], eax
	popad
	movss xmm0, dword [ebp-0x20]
%else
	mov ecx, [edx+0x50]
%endif
	test ecx, ecx
	jle BG_WeaponFireRecoil_20
BG_WeaponFireRecoil_50:
	ucomiss xmm0, [_float_1_00000000]
	jnz BG_WeaponFireRecoil_30
	jp BG_WeaponFireRecoil_30
	movss xmm1, dword [_float_0_01000000]
	movss [ebp-0x1c], xmm1
	mulss xmm1, [ebx+0x6dc]
	movss [ebp-0x1c], xmm1
BG_WeaponFireRecoil_70:
	movss xmm0, dword [ebx+0x700]
	movss [ebp-0x60], xmm0
	movss xmm1, dword [ebx+0x704]
	subss xmm1, xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x5c]
	movss xmm1, dword [ebp-0x78]
	mulss xmm1, [ebp-0x5c]
	addss xmm1, [ebp-0x60]
	movss xmm0, dword [ebx+0x708]
	movss [ebp-0x58], xmm0
	movss xmm0, dword [ebx+0x70c]
	subss xmm0, [ebp-0x58]
	movss [ebp-0x54], xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x7c]
	movss xmm0, dword [ebp-0x7c]
	mulss xmm0, [ebp-0x54]
	addss xmm0, [ebp-0x58]
	movss xmm1, dword [ebp-0x78]
BG_WeaponFireRecoil_80:
%ifdef OL_SERVER
	; player_recoilScaleOn/Off. [ebp-0x1c] is the single scale this function
	; applies to the view kick below and to the gun-speed accumulation at
	; BG_WeaponFireRecoil_90, which is exactly where IW4 (0x4a60c8) and C1
	; (0x004222b6) fold in ps->recoilScale/100 when the weapon-flags bit 0x400
	; is armed. PlayerRecoilScale returns 1.0 when it is not.
	; xmm0/xmm1 hold the pending kick angles and are caller-saved, so they are
	; spilled across the call; ebx/esi/edi and [ebp-0x20] are untouched.
	extern PlayerRecoilScale
	sub esp, 0x10
	movss [esp+0x4], xmm0
	movss [esp+0x8], xmm1
	mov eax, [ebp+0x8]
	mov [esp], eax
	call PlayerRecoilScale
	fstp dword [esp+0xc]
	movss xmm0, dword [ebp-0x1c]
	mulss xmm0, [esp+0xc]
	movss [ebp-0x1c], xmm0
	movss xmm0, dword [esp+0x4]
	movss xmm1, dword [esp+0x8]
	add esp, 0x10
%endif
	mulss xmm0, [ebp-0x1c]
	mulss xmm1, [ebp-0x1c]
	xorps xmm1, [_data16_80000000]
	movss [esi], xmm1
	movss [esi+0x4], xmm0
	mulss xmm0, [_float__0_50000000]
	movss [esi+0x8], xmm0
	movss xmm1, dword [ebp-0x20]
	ucomiss xmm1, [_float_0_00000000]
	jbe BG_WeaponFireRecoil_40
	movss xmm0, dword [ebx+0x6e0]
	movss [ebp-0x40], xmm0
	movss xmm1, dword [ebx+0x6e4]
	subss xmm1, xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x3c]
	movss xmm1, dword [ebp-0x78]
	mulss xmm1, [ebp-0x3c]
	addss xmm1, [ebp-0x40]
	movss xmm0, dword [ebx+0x6e8]
	movss [ebp-0x38], xmm0
	movss xmm0, dword [ebx+0x6ec]
	subss xmm0, [ebp-0x38]
	movss [ebp-0x34], xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x7c]
	movss xmm0, dword [ebp-0x7c]
	mulss xmm0, [ebp-0x34]
	addss xmm0, [ebp-0x38]
BG_WeaponFireRecoil_90:
	movss xmm1, dword [ebp-0x78]
	mulss xmm1, [ebp-0x1c]
	addss xmm1, [edi]
	movss [edi], xmm1
	mulss xmm0, [ebp-0x1c]
	addss xmm0, [edi+0x4]
	movss [edi+0x4], xmm0
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_WeaponFireRecoil_10:
	mov eax, [edx+0xe8]
	mov ebx, [eax*4+bg_weaponDefs]
	movss xmm0, dword [edx+0xf4]
	movss [ebp-0x20], xmm0
%ifdef OL_SERVER
	; Same per-hand kick-restriction read as the offhand arm above; exactly one
	; of the two arms runs per call, so the record is consumed once per event.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], edx
	call WeaponStateRecoilRestrictTime
	mov esp, [esp+0xc]
	mov [esp+0x18], eax
	popad
	movss xmm0, dword [ebp-0x20]
%else
	mov ecx, [edx+0x50]
%endif
	test ecx, ecx
	jg BG_WeaponFireRecoil_50
BG_WeaponFireRecoil_20:
	movaps xmm1, xmm0
	movss xmm0, dword [_float_1_00000000]
	ucomiss xmm1, xmm0
	jnz BG_WeaponFireRecoil_60
	jp BG_WeaponFireRecoil_60
	movss [ebp-0x1c], xmm0
	jmp BG_WeaponFireRecoil_70
BG_WeaponFireRecoil_30:
	movss xmm0, dword [_float_0_01000000]
	movss [ebp-0x1c], xmm0
	mulss xmm0, [ebx+0x724]
BG_WeaponFireRecoil_60:
	movss [ebp-0x1c], xmm0
	movss xmm1, dword [ebx+0x748]
	movss [ebp-0x50], xmm1
	movss xmm1, dword [ebx+0x74c]
	subss xmm1, [ebp-0x50]
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x4c]
	movss xmm1, dword [ebp-0x78]
	mulss xmm1, [ebp-0x4c]
	addss xmm1, [ebp-0x50]
	movss xmm0, dword [ebx+0x750]
	movss [ebp-0x48], xmm0
	movss xmm0, dword [ebx+0x754]
	subss xmm0, [ebp-0x48]
	movss [ebp-0x44], xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x7c]
	movss xmm0, dword [ebp-0x7c]
	mulss xmm0, [ebp-0x44]
	addss xmm0, [ebp-0x48]
	movss xmm1, dword [ebp-0x78]
	jmp BG_WeaponFireRecoil_80
BG_WeaponFireRecoil_40:
	movss xmm1, dword [ebx+0x728]
	movss [ebp-0x30], xmm1
	movss xmm1, dword [ebx+0x72c]
	subss xmm1, [ebp-0x30]
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x2c]
	movss xmm1, dword [ebp-0x78]
	mulss xmm1, [ebp-0x2c]
	addss xmm1, [ebp-0x30]
	movss xmm0, dword [ebx+0x730]
	movss [ebp-0x28], xmm0
	movss xmm0, dword [ebx+0x734]
	subss xmm0, [ebp-0x28]
	movss [ebp-0x24], xmm0
	movss [ebp-0x78], xmm1
	call randomf
	fstp dword [ebp-0x7c]
	movss xmm0, dword [ebp-0x7c]
	mulss xmm0, [ebp-0x24]
	addss xmm0, [ebp-0x28]
	jmp BG_WeaponFireRecoil_90
	nop
	add [eax], al


;BG_WeaponIsClipOnly(int)
BG_WeaponIsClipOnly:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x500]
	pop ebp
	ret
	nop
	add [eax], al


;PM_ExitAimDownSight(playerState_s*)
PM_ExitAimDownSight:
	push ebp
	mov ebp, esp
	push ebx
	sub esp, 0x14
	mov ebx, [ebp+0x8]
	mov dword [esp+0x4], 0xe
	mov [esp], ebx
	call PM_AddEvent
	and dword [ebx+0xc], 0xffffffef
	add esp, 0x14
	pop ebx
	pop ebp
	ret


;PM_ResetWeaponState(playerState_s*)
PM_ResetWeaponState:
	push ebp
	mov ebp, esp
	mov edx, [ebp+0x8]
	and dword [edx+0x10], 0xfffffffd
	and dword [edx+0xc], 0xfffffdff
	mov dword [edx+0x3c], 0x0
	mov dword [edx+0x40], 0x0
	mov dword [edx+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [edx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edx+0x4], 0x6
%endif
	jg PM_ResetWeaponState_10
	mov eax, [edx+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [edx+0x624], eax
PM_ResetWeaponState_10:
%ifdef OL_SERVER
	; C1 clears both hand machines together (0x419d77/0x419d7d) and lets
	; PM_StartWeaponAnim drop the second hand outright when the weapon is not
	; dual wield (0x4180ec). Loadout, primary cache and magazines survive.
	extern WeaponStateResetHands
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov eax, [ebp+0x8]
	mov [esp], eax
	call WeaponStateResetHands
	mov esp, esi
	popad
%endif
	pop ebp
	ret
	nop


;BG_WeaponBlocksProne(unsigned int)
BG_WeaponBlocksProne:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x54c]
	test eax, eax
	setnz al
	movzx eax, al
	pop ebp
	ret
	nop


;BG_GetSpreadForWeapon(playerState_s const*, WeaponDef const*, float*, float*)
BG_GetSpreadForWeapon:
	push ebp
	mov ebp, esp
	push ebx
	mov edx, [ebp+0x8]
	mov eax, [ebp+0xc]
	mov ebx, [ebp+0x10]
	mov ecx, [ebp+0x14]
	cmp dword [edx+0x100], 0x2
	jz BG_GetSpreadForWeapon_10
	movss xmm2, dword [edx+0x118]
	ucomiss xmm2, [_float_40_00000000]
	jbe BG_GetSpreadForWeapon_20
	subss xmm2, [_float_40_00000000]
	divss xmm2, dword [_float_20_00000000]
	movss xmm1, dword [eax+0x454]
	movss xmm0, dword [eax+0x450]
	subss xmm0, xmm1
	mulss xmm0, xmm2
	addss xmm1, xmm0
	movss [ebx], xmm1
	movss xmm1, dword [eax+0x460]
	movss xmm0, dword [eax+0x45c]
	subss xmm0, xmm1
	mulss xmm2, xmm0
	addss xmm1, xmm2
	movss [ecx], xmm1
BG_GetSpreadForWeapon_60:
	cmp dword [edx+0x100], 0x1
	jz BG_GetSpreadForWeapon_30
BG_GetSpreadForWeapon_50:
%ifdef OL_SERVER
	; C1 BG_GetSpreadForWeapon 0x422736..0x42276d multiplies min and max
	; spread by value6 (modBulletSpreadScale). The IW3 perk bit 2 /
	; perk_weapSpreadMultiplier branch does not exist in C1 and nothing sets
	; ps+0x5fc for a CoDOL player. EDX ps, EBX min, ECX max.
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], edx
	mov [esp+0x4], ebx
	mov [esp+0x8], ecx
	call ApplySpreadAbility
	mov esp, esi
	popad
%else
	test byte [edx+0x5fc], 0x2
	jz BG_GetSpreadForWeapon_40
	mov edx, perk_weapSpreadMultiplier
	mov eax, [edx]
	movss xmm0, dword [ebx]
	mulss xmm0, [eax+0xc]
	movss [ebx], xmm0
	mov eax, [edx]
	movss xmm0, dword [ecx]
	mulss xmm0, [eax+0xc]
	movss [ecx], xmm0
%endif
BG_GetSpreadForWeapon_40:
	pop ebx
	pop ebp
	ret
BG_GetSpreadForWeapon_20:
	subss xmm2, [_float_11_00000000]
	divss xmm2, dword [_float_29_00000000]
	movss xmm1, dword [eax+0x458]
	movss xmm0, dword [eax+0x454]
	subss xmm0, xmm1
	mulss xmm0, xmm2
	addss xmm1, xmm0
	movss [ebx], xmm1
	movss xmm1, dword [eax+0x464]
	movss xmm0, dword [eax+0x460]
	subss xmm0, xmm1
	mulss xmm2, xmm0
	addss xmm1, xmm2
	movss [ecx], xmm1
	cmp dword [edx+0x100], 0x1
	jnz BG_GetSpreadForWeapon_50
BG_GetSpreadForWeapon_30:
	cvtsi2ss xmm0, dword [edx+0xfc]
	movss [ecx], xmm0
	jmp BG_GetSpreadForWeapon_50
BG_GetSpreadForWeapon_10:
	cvtsi2ss xmm0, dword [edx+0xfc]
	movss [ebx], xmm0
	cvtsi2ss xmm0, dword [edx+0xfc]
	movss [ecx], xmm0
	jmp BG_GetSpreadForWeapon_60


;BG_CalculateViewAngles(viewState_t*, float*)
BG_CalculateViewAngles:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0xcc
	mov edi, [ebp+0x8]
	mov esi, [ebp+0xc]
	pxor xmm4, xmm4
	movss [esi], xmm4
	lea eax, [esi+0x4]
	mov [ebp-0x48], eax
	movss [esi+0x4], xmm4
	lea eax, [esi+0x8]
	mov [ebp-0x44], eax
	movss [esi+0x8], xmm4
	mov edx, [edi+0x4]
	test edx, edx
	jz BG_CalculateViewAngles_10
	mov ebx, [edi]
	test byte [ebx+0x10], 0x2
	jz BG_CalculateViewAngles_20
	mov eax, [ebx+0xe0]
BG_CalculateViewAngles_480:
	mov eax, [eax*4+bg_weaponDefs]
	movss xmm0, dword [ebx+0xf4]
	movaps xmm1, xmm0
	mulss xmm1, [_float__0_50000000]
	movss xmm2, dword [_float_1_00000000]
	addss xmm1, xmm2
	movss [ebp-0x3c], xmm1
	ucomiss xmm0, xmm4
	jp BG_CalculateViewAngles_30
	jz BG_CalculateViewAngles_40
BG_CalculateViewAngles_30:
	mov eax, [eax+0x438]
	test eax, eax
	jnz BG_CalculateViewAngles_50
BG_CalculateViewAngles_40:
	mov eax, [edi+0x8]
	sub eax, edx
	cvtsi2ss xmm0, eax
	movss xmm1, dword [_float_100_00000000]
	ucomiss xmm1, xmm0
	ja BG_CalculateViewAngles_60
BG_CalculateViewAngles_500:
	subss xmm0, xmm1
	divss xmm0, dword [_float__400_00000000]
	addss xmm0, xmm2
	ucomiss xmm0, xmm4
	jbe BG_CalculateViewAngles_70
	movaps xmm1, xmm2
	subss xmm1, xmm0
	movss [esp], xmm1
	movss [ebp-0x98], xmm2
	call GetLeanFraction
	fstp dword [ebp-0x40]
	movss xmm2, dword [ebp-0x98]
	movaps xmm0, xmm2
	subss xmm0, [ebp-0x40]
	mulss xmm0, [ebp-0x3c]
	movaps xmm1, xmm0
	mulss xmm1, [edi+0xc]
	addss xmm1, [esi]
	movss [esi], xmm1
	mulss xmm0, [edi+0x10]
	mov eax, [ebp-0x44]
	addss xmm0, [eax]
	movss [eax], xmm0
BG_CalculateViewAngles_10:
	mov ebx, [edi]
BG_CalculateViewAngles_70:
	test byte [ebx+0x10], 0x2
	jnz BG_CalculateViewAngles_80
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov ecx, [eax+0x438]
	test ecx, ecx
	jnz BG_CalculateViewAngles_90
BG_CalculateViewAngles_160:
	test byte [ebx+0x10], 0x2
	jnz BG_CalculateViewAngles_100
	mov eax, [ebx+0xe8]
BG_CalculateViewAngles_400:
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x38], eax
	mov eax, [eax+0x438]
	test eax, eax
	jz BG_CalculateViewAngles_110
	movzx eax, byte [ebx+0x8]
	cvtsi2ss xmm0, eax
	divss xmm0, dword [_float_255_00000000]
	cvtss2sd xmm0, xmm0
	mulsd xmm0, [_double_3_14159265]
	addsd xmm0, xmm0
	movsd xmm1, qword [_double_6_28318531]
	addsd xmm0, xmm1
	cvtsd2ss xmm0, xmm0
	cvtss2sd xmm0, xmm0
	addsd xmm0, [_double_0_78539816]
	addsd xmm0, xmm1
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x30], xmm0
	movss xmm0, dword [_float_0_16000000]
	movss [ebp-0x34], xmm0
	mulss xmm0, [edi+0x14]
	movss [ebp-0x34], xmm0
	mov eax, [ebx+0x114]
	cmp eax, 0xb
	jz BG_CalculateViewAngles_120
	cmp eax, 0x28
	jz BG_CalculateViewAngles_130
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jnz BG_CalculateViewAngles_140
%else
	cmp word [ebx+0xc], 0x0
	js BG_CalculateViewAngles_140
%endif
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateViewAngles_150
BG_CalculateViewAngles_80:
	mov eax, [ebx+0xe0]
	mov eax, [eax*4+bg_weaponDefs]
	mov ecx, [eax+0x438]
	test ecx, ecx
	jz BG_CalculateViewAngles_160
BG_CalculateViewAngles_90:
	mov edx, [eax+0x4ec]
	test edx, edx
	jz BG_CalculateViewAngles_170
	movss xmm0, dword [ebx+0xf4]
	movss xmm1, dword [eax+0x490]
	movss xmm3, dword [eax+0x48c]
	subss xmm3, xmm1
	mulss xmm3, xmm0
	addss xmm3, xmm1
	movss xmm1, dword [eax+0x498]
	movss xmm2, dword [eax+0x494]
	subss xmm2, xmm1
	mulss xmm2, xmm0
	addss xmm2, xmm1
	pxor xmm4, xmm4
BG_CalculateViewAngles_490:
	mov edx, [ebx+0xb0]
	test dl, 0x8
	jnz BG_CalculateViewAngles_180
BG_CalculateViewAngles_420:
	and dl, 0x4
	jz BG_CalculateViewAngles_190
	movss xmm1, dword [eax+0x49c]
	jmp BG_CalculateViewAngles_200
BG_CalculateViewAngles_120:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
BG_CalculateViewAngles_150:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm2, 0x5
	andps xmm2, xmm0
	andnps xmm0, xmm1
	orps xmm0, xmm2
	movss [ebp-0x1c], xmm0
	movss xmm0, dword [ebp-0x30]
	addss xmm0, xmm0
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0xbc]
	movss xmm1, dword [ebp-0xbc]
	movss xmm0, dword [ebp-0x30]
	mulss xmm0, [_float_4_00000000]
	cvtss2sd xmm0, xmm0
	addsd xmm0, [_double_1_57079633]
	movsd [esp], xmm0
	movss [ebp-0x88], xmm1
	call sin
	fstp qword [ebp-0x58]
	cvtsd2ss xmm0, [ebp-0x58]
	mulss xmm0, [_float_0_20000000]
	movss xmm1, dword [ebp-0x88]
	addss xmm1, xmm0
	mulss xmm1, [_float_0_75000000]
	mulss xmm1, [ebp-0x1c]
	movss [ebp-0x1c], xmm1
	xor dword [ebp-0x1c], 0x80000000
	mov eax, [ebx+0x114]
	cmp eax, 0xb
	jz BG_CalculateViewAngles_210
	cmp eax, 0x28
	jz BG_CalculateViewAngles_220
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateViewAngles_230
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateViewAngles_230
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0xc]
BG_CalculateViewAngles_530:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm2, 0x5
	andps xmm2, xmm0
	movaps xmm5, xmm0
	andnps xmm5, xmm1
	orps xmm5, xmm2
	movss xmm1, dword [ebp-0x30]
	movss [esp], xmm1
	movss [ebp-0xb8], xmm5
	call sinf
	fstp dword [ebp-0x5c]
	movss xmm5, dword [ebp-0xb8]
	mulss xmm5, [ebp-0x5c]
	xorps xmm5, [_ZZ14PM_GetMoveAnimP13playerState_s17PmStanceFrontBackiiE13moveAnimTable+0x2a0]
	cvtss2sd xmm0, [ebp-0x30]
	subsd xmm0, [_double_0_47123892]
	cvtsd2ss xmm4, xmm0
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [_float_1_50000000]
	mov eax, [ebx+0x114]
	cmp eax, 0xb
	jz BG_CalculateViewAngles_240
	cmp eax, 0x28
	jz BG_CalculateViewAngles_250
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateViewAngles_260
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateViewAngles_260
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
BG_CalculateViewAngles_540:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm3, 0x5
	movaps xmm2, xmm3
	andps xmm2, xmm0
	movaps xmm3, xmm0
	andnps xmm3, xmm1
	orps xmm3, xmm2
	movss [esp], xmm4
	movss [ebp-0xa8], xmm3
	movss [ebp-0xb8], xmm5
	call sinf
	fstp dword [ebp-0xbc]
	movss xmm4, dword [ebp-0xbc]
	movss xmm3, dword [ebp-0xa8]
	mulss xmm4, xmm3
	pxor xmm1, xmm1
	movaps xmm0, xmm1
	subss xmm0, xmm4
	movaps xmm2, xmm1
	cmpss xmm2, xmm0, 0x2
	andps xmm4, xmm2
	orps xmm4, xmm1
	movss xmm2, dword [ebx+0xf4]
	movss xmm5, dword [ebp-0xb8]
	ucomiss xmm2, xmm1
	jp BG_CalculateViewAngles_270
	jz BG_CalculateViewAngles_280
BG_CalculateViewAngles_270:
	movss xmm0, dword [_float_1_00000000]
	movaps xmm1, xmm0
	mov eax, [ebp-0x38]
	subss xmm1, [eax+0x448]
	mulss xmm1, xmm2
	subss xmm0, xmm1
	movss xmm1, dword [ebp-0x1c]
	mulss xmm1, xmm0
	movss [ebp-0x1c], xmm1
	mulss xmm5, xmm0
	mulss xmm4, xmm0
	movaps xmm0, xmm1
BG_CalculateViewAngles_520:
	mulss xmm0, xmm2
	movss [ebp-0x1c], xmm0
	addss xmm0, [esi]
	movss [esi], xmm0
	mulss xmm5, xmm2
	addss xmm5, [esi+0x4]
	mov eax, [ebp-0x48]
	movss [eax], xmm5
	mulss xmm2, xmm4
	addss xmm2, [esi+0x8]
	mov eax, [ebp-0x44]
	movss [eax], xmm2
	mov ebx, [edi]
BG_CalculateViewAngles_110:
	test byte [ebx+0x10], 0x2
	jz BG_CalculateViewAngles_290
	mov eax, [ebx+0xe0]
BG_CalculateViewAngles_390:
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x24], eax
	test dword [ebx+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz BG_CalculateViewAngles_300
	movss xmm0, dword [ebx+0xf4]
	movss [ebp-0x2c], xmm0
	pxor xmm0, xmm0
	movss xmm1, dword [ebp-0x2c]
	ucomiss xmm1, xmm0
	jp BG_CalculateViewAngles_310
	jz BG_CalculateViewAngles_300
BG_CalculateViewAngles_310:
	movss xmm2, dword [eax+0x44c]
	movss [ebp-0x28], xmm2
	ucomiss xmm2, xmm0
	jp BG_CalculateViewAngles_320
	jz BG_CalculateViewAngles_300
BG_CalculateViewAngles_320:
	movzx eax, byte [ebx+0x8]
	cvtsi2ss xmm0, eax
	divss xmm0, dword [_float_255_00000000]
	cvtss2sd xmm0, xmm0
	mulsd xmm0, [_double_3_14159265]
	addsd xmm0, xmm0
	addsd xmm0, [_double_6_28318531]
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x20], xmm0
	movss xmm2, dword [edi+0x14]
	mov eax, [ebx+0x114]
	cmp eax, 0xb
	jz BG_CalculateViewAngles_330
	cmp eax, 0x28
	jz BG_CalculateViewAngles_340
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateViewAngles_350
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateViewAngles_350
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0x10]
BG_CalculateViewAngles_510:
	movss xmm1, dword [_float_45_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm3, 0x5
	movaps xmm2, xmm3
	andps xmm2, xmm0
	movaps xmm3, xmm0
	andnps xmm3, xmm1
	orps xmm3, xmm2
	movss xmm0, dword [ebp-0x20]
	addss xmm0, xmm0
	movss [esp], xmm0
	movss [ebp-0xa8], xmm3
	call sinf
	fstp dword [ebp-0xbc]
	movss xmm1, dword [ebp-0xbc]
	movss xmm0, dword [ebp-0x20]
	mulss xmm0, [_float_4_00000000]
	cvtss2sd xmm0, xmm0
	addsd xmm0, [_double_1_57079633]
	movsd [esp], xmm0
	movss [ebp-0x88], xmm1
	call sin
	fstp qword [ebp-0x68]
	cvtsd2ss xmm0, [ebp-0x68]
	mulss xmm0, [_float_0_20000000]
	movss xmm1, dword [ebp-0x88]
	addss xmm1, xmm0
	mulss xmm1, [_float_0_75000000]
	movss xmm3, dword [ebp-0xa8]
	mulss xmm3, xmm1
	movss xmm0, dword [ebp-0x2c]
	mulss xmm0, [ebp-0x28]
	mulss xmm3, xmm0
	movss xmm0, dword [esi]
	subss xmm0, xmm3
	movss [esi], xmm0
	movss xmm2, dword [edi+0x14]
	mov eax, [ebx+0x114]
	cmp eax, 0xb
	jz BG_CalculateViewAngles_360
	cmp eax, 0x28
	jz BG_CalculateViewAngles_370
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateViewAngles_380
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateViewAngles_380
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
BG_CalculateViewAngles_550:
	movss xmm1, dword [_float_45_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm3, 0x5
	movaps xmm2, xmm3
	andps xmm2, xmm0
	movaps xmm3, xmm0
	andnps xmm3, xmm1
	orps xmm3, xmm2
	movss xmm1, dword [ebp-0x20]
	movss [esp], xmm1
	movss [ebp-0xa8], xmm3
	call sinf
	fstp dword [ebp-0x6c]
	movss xmm3, dword [ebp-0xa8]
	mulss xmm3, [ebp-0x6c]
	movss xmm0, dword [ebx+0xf4]
	mov eax, [ebp-0x24]
	mulss xmm0, [eax+0x44c]
	mulss xmm3, xmm0
	mov eax, [ebp-0x48]
	movss xmm0, dword [eax]
	subss xmm0, xmm3
	movss [eax], xmm0
BG_CalculateViewAngles_300:
	add esp, 0xcc
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_CalculateViewAngles_290:
	mov eax, [ebx+0xe8]
	jmp BG_CalculateViewAngles_390
BG_CalculateViewAngles_100:
	mov eax, [ebx+0xe0]
	jmp BG_CalculateViewAngles_400
BG_CalculateViewAngles_170:
	movss xmm3, dword [eax+0x490]
	pxor xmm4, xmm4
	ucomiss xmm3, xmm4
	jnz BG_CalculateViewAngles_410
	jp BG_CalculateViewAngles_410
	movss xmm3, dword [_float_80_00000000]
	movss xmm2, dword [_float_1_00000000]
	mov edx, [ebx+0xb0]
	test dl, 0x8
	jz BG_CalculateViewAngles_420
BG_CalculateViewAngles_180:
	movss xmm1, dword [eax+0x4a0]
BG_CalculateViewAngles_200:
	ucomiss xmm4, [ebx+0xf4]
	jp BG_CalculateViewAngles_430
	jz BG_CalculateViewAngles_440
BG_CalculateViewAngles_430:
	movss xmm4, dword [edi+0x1c]
	ucomiss xmm1, xmm4
	jp BG_CalculateViewAngles_450
	jz BG_CalculateViewAngles_440
BG_CalculateViewAngles_450:
	ucomiss xmm1, xmm4
	jbe BG_CalculateViewAngles_460
	movss xmm0, dword [_float_0_50000000]
	mulss xmm0, [edi+0x18]
	addss xmm0, xmm4
	movss [edi+0x1c], xmm0
	ucomiss xmm0, xmm1
	ja BG_CalculateViewAngles_470
BG_CalculateViewAngles_440:
	movaps xmm1, xmm3
	mulss xmm1, [edi+0x1c]
	mulss xmm1, [ebx+0xf4]
	movss xmm0, dword [ebx+0x5d4]
	mulss xmm1, xmm0
	mov edx, [edi+0x20]
	mulss xmm0, [edi+0x18]
	mulss xmm0, [_float_1000_00000000]
	mulss xmm2, xmm0
	cvttss2si eax, xmm2
	add [edx], eax
	mov eax, [edi+0x20]
	cvtsi2ss xmm0, dword [eax]
	mulss xmm0, [_float_0_00070000]
	movss [esp], xmm0
	movss [ebp-0x88], xmm1
	call sinf
	fstp dword [ebp-0xbc]
	movss xmm0, dword [ebp-0xbc]
	movss xmm1, dword [ebp-0x88]
	mulss xmm0, xmm1
	mulss xmm0, [_float_0_01000000]
	mov eax, [ebp-0x48]
	addss xmm0, [eax]
	movss [eax], xmm0
	mov eax, [edi+0x20]
	cvtsi2ss xmm0, dword [eax]
	mulss xmm0, [_float_0_00100000]
	movss [esp], xmm0
	movss [ebp-0x88], xmm1
	call sinf
	fstp dword [ebp-0x4c]
	movss xmm1, dword [ebp-0x88]
	mulss xmm1, [ebp-0x4c]
	mulss xmm1, [_float_0_01000000]
	addss xmm1, [esi]
	movss [esi], xmm1
	mov ebx, [edi]
	jmp BG_CalculateViewAngles_160
BG_CalculateViewAngles_20:
	mov eax, [ebx+0xe8]
	jmp BG_CalculateViewAngles_480
BG_CalculateViewAngles_140:
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateViewAngles_150
BG_CalculateViewAngles_410:
	movss xmm2, dword [eax+0x498]
	jmp BG_CalculateViewAngles_490
BG_CalculateViewAngles_190:
	movss xmm1, dword [_float_1_00000000]
	jmp BG_CalculateViewAngles_200
BG_CalculateViewAngles_50:
	mulss xmm0, [_float_0_50000000]
	addss xmm0, xmm2
	mulss xmm0, xmm1
	movss [ebp-0x3c], xmm0
	mov eax, [edi+0x8]
	sub eax, edx
	cvtsi2ss xmm0, eax
	movss xmm1, dword [_float_100_00000000]
	ucomiss xmm1, xmm0
	jbe BG_CalculateViewAngles_500
BG_CalculateViewAngles_60:
	divss xmm0, xmm1
	movss [esp], xmm0
	call GetLeanFraction
	fstp dword [ebp-0xbc]
	movss xmm1, dword [ebp-0xbc]
	mulss xmm1, [ebp-0x3c]
	movaps xmm0, xmm1
	mulss xmm0, [edi+0xc]
	addss xmm0, [esi]
	movss [esi], xmm0
	mulss xmm1, [edi+0x10]
	mov eax, [ebp-0x44]
	addss xmm1, [eax]
	movss [eax], xmm1
	mov ebx, [edi]
	jmp BG_CalculateViewAngles_70
BG_CalculateViewAngles_340:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0x10]
	jmp BG_CalculateViewAngles_510
BG_CalculateViewAngles_460:
	movss xmm0, dword [_float_0_50000000]
	mulss xmm0, [edi+0x18]
	subss xmm4, xmm0
	maxss xmm1, xmm4
BG_CalculateViewAngles_470:
	movss [edi+0x1c], xmm1
	jmp BG_CalculateViewAngles_440
BG_CalculateViewAngles_280:
	movss xmm0, dword [ebp-0x1c]
	jmp BG_CalculateViewAngles_520
BG_CalculateViewAngles_210:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateViewAngles_530
BG_CalculateViewAngles_240:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_540
BG_CalculateViewAngles_260:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_540
BG_CalculateViewAngles_230:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateViewAngles_530
BG_CalculateViewAngles_220:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateViewAngles_530
BG_CalculateViewAngles_250:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_540
BG_CalculateViewAngles_130:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movss xmm2, dword [ebp-0x34]
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateViewAngles_150
BG_CalculateViewAngles_330:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0x10]
	jmp BG_CalculateViewAngles_510
BG_CalculateViewAngles_360:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_550
BG_CalculateViewAngles_350:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0x10]
	jmp BG_CalculateViewAngles_510
BG_CalculateViewAngles_380:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_550
BG_CalculateViewAngles_370:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateViewAngles_550


;BG_CanPlayerHaveWeapon(unsigned int)
BG_CanPlayerHaveWeapon:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	cmp dword [eax*4+bg_weaponDefs], 0xfffffff4
	setnz al
	movzx eax, al
	pop ebp
	ret


;BG_GetTotalAmmoReserve(playerState_s const*, unsigned int)
BG_GetTotalAmmoReserve:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x42c
	mov ebx, [ebp+0xc]
	mov eax, [ebx*4+bg_weaponDefs]
	mov edx, [eax+0x328]
	mov [ebp-0x41c], edx
	mov esi, [eax+0x330]
	lea eax, [ebp-0x218]
	mov dword [esp+0x8], 0x200
	mov dword [esp+0x4], 0x0
	mov [esp], eax
	call memset
	lea eax, [ebp-0x418]
	mov dword [esp+0x8], 0x200
	mov dword [esp+0x4], 0x0
	mov [esp], eax
	call memset
	mov edi, [ebx*4+bg_weaponDefs]
	mov ecx, [edi+0x344]
	test ecx, ecx
	js BG_GetTotalAmmoReserve_10
	mov ecx, [bg_lastParsedWeaponIndex]
	mov [ebp-0x424], ecx
	test ecx, ecx
	jz BG_GetTotalAmmoReserve_20
	mov dword [ebp-0x420], 0x0
	mov ebx, 0x1
	mov esi, bg_weaponDefs
	jmp BG_GetTotalAmmoReserve_30
BG_GetTotalAmmoReserve_50:
	add ebx, 0x1
	add esi, 0x4
	cmp [ebp-0x424], ebx
	jb BG_GetTotalAmmoReserve_40
BG_GetTotalAmmoReserve_30:
	mov edx, ebx
	sar edx, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, edx, eax, WB_OWNED
	jz BG_GetTotalAmmoReserve_50
	mov edx, [esi+0x4]
	mov eax, [edx+0x344]
	cmp eax, [edi+0x344]
	jnz BG_GetTotalAmmoReserve_50
	mov eax, [edx+0x500]
	test eax, eax
	jz BG_GetTotalAmmoReserve_60
	mov eax, [edx+0x330]
	cmp dword [ebp+eax*4-0x418], 0x0
	jnz BG_GetTotalAmmoReserve_50
	mov dword [ebp+eax*4-0x418], 0x1
	mov eax, [edx+0x330]
	mov edx, [ecx+eax*4+0x35c]
	add [ebp-0x420], edx
	add ebx, 0x1
	add esi, 0x4
	cmp [ebp-0x424], ebx
	jae BG_GetTotalAmmoReserve_30
BG_GetTotalAmmoReserve_40:
	mov eax, [ebp-0x420]
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetTotalAmmoReserve_20:
	mov dword [ebp-0x420], 0x0
	mov eax, [ebp-0x420]
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetTotalAmmoReserve_60:
	mov eax, [edx+0x328]
	mov ecx, [ebp+eax*4-0x218]
	test ecx, ecx
	jnz BG_GetTotalAmmoReserve_50
	mov dword [ebp+eax*4-0x218], 0x1
	mov eax, [edx+0x328]
	mov ecx, [ebp+0x8]
	mov ecx, [ecx+eax*4+0x15c]
	add [ebp-0x420], ecx
	jmp BG_GetTotalAmmoReserve_50
BG_GetTotalAmmoReserve_10:
	mov edx, [edi+0x500]
	test edx, edx
	jnz BG_GetTotalAmmoReserve_70
	mov edx, [ebp-0x41c]
	mov ecx, [ebp+0x8]
	mov edx, [ecx+edx*4+0x15c]
	mov [ebp-0x420], edx
	mov eax, edx
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetTotalAmmoReserve_70:
	mov eax, [ebp+0x8]
	mov esi, [eax+esi*4+0x35c]
	mov [ebp-0x420], esi
	mov eax, esi
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret


;BG_ThrowingBackGrenade(playerState_s const*)
BG_ThrowingBackGrenade:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	cmp dword [eax+0x48], 0x7ff
	setnz al
	movzx eax, al
	pop ebp
	ret
	nop


;PM_WeaponAmmoAvailable(playerState_s*)
PM_WeaponAmmoAvailable:
	push ebp
	mov ebp, esp
	mov edx, [ebp+0x8]
	mov eax, [edx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov eax, [edx+eax*4+0x35c]
	pop ebp
	ret


;BG_FillInAllWeaponItems()
BG_FillInAllWeaponItems:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x3c
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	mov [ebp-0x2c], eax
	cmp eax, 0x1
	jbe BG_FillInAllWeaponItems_10
	mov dword [ebp-0x30], 0x1
	mov eax, bg_weaponDefs
BG_FillInAllWeaponItems_50:
	lea edx, [eax+0x4]
	mov [ebp-0x34], edx
	mov eax, [eax+0x4]
	mov [ebp-0x24], eax
	mov esi, [bg_numAmmoTypes]
	test esi, esi
	jnz BG_FillInAllWeaponItems_20
	xor eax, eax
	xor edx, edx
BG_FillInAllWeaponItems_80:
	mov ecx, [ebp-0x24]
	mov [eax*4+bg_weapAmmoTypes], ecx
	mov [ecx+0x328], edx
	lea eax, [esi+0x1]
	mov [bg_numAmmoTypes], eax
BG_FillInAllWeaponItems_140:
	mov eax, [ebp-0x30]
	call BG_SetupSharedAmmoIndexes
	mov ebx, [ebp-0x34]
	mov ebx, [ebx]
	mov [ebp-0x1c], ebx
	mov esi, [bg_numWeapClips]
	test esi, esi
	jnz BG_FillInAllWeaponItems_30
	xor eax, eax
	xor edx, edx
BG_FillInAllWeaponItems_120:
	mov ebx, [ebp-0x1c]
	mov [eax*4+bg_weapClips], ebx
	mov [ebx+0x330], edx
	lea eax, [esi+0x1]
	mov [bg_numWeapClips], eax
BG_FillInAllWeaponItems_130:
	xor edx, edx
	mov ecx, [ebp-0x30]
	mov ebx, bg_itemlist
	lea eax, [ebx+ecx*4]
BG_FillInAllWeaponItems_40:
	mov dword [eax], 0x1
	add edx, 0x1
	add eax, ITEM_STRIDE*4
	cmp edx, ITEM_MODELS
	jnz BG_FillInAllWeaponItems_40
	add dword [ebp-0x30], 0x1
	mov eax, [ebp-0x34]
	mov edx, [ebp-0x30]
	cmp [ebp-0x2c], edx
	jnz BG_FillInAllWeaponItems_50
BG_FillInAllWeaponItems_10:
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_FillInAllWeaponItems_20:
	mov ebx, [eax+0x324]
	mov [ebp-0x28], ebx
	xor edi, edi
	mov ebx, bg_weapAmmoTypes
BG_FillInAllWeaponItems_70:
	mov eax, [ebx]
	mov edx, [ebp-0x28]
	mov [esp+0x4], edx
	mov eax, [eax+0x324]
	mov [esp], eax
	call strcmp
	test eax, eax
	jz BG_FillInAllWeaponItems_60
	add edi, 0x1
	add ebx, 0x4
	cmp esi, edi
	jnz BG_FillInAllWeaponItems_70
	mov edx, esi
	mov eax, esi
	jmp BG_FillInAllWeaponItems_80
BG_FillInAllWeaponItems_30:
	mov edx, [ebx+0x32c]
	mov [ebp-0x20], edx
	xor edi, edi
	mov ebx, bg_weapClips
	mov ecx, edx
	jmp BG_FillInAllWeaponItems_90
BG_FillInAllWeaponItems_110:
	mov ecx, [ebp-0x20]
BG_FillInAllWeaponItems_90:
	mov eax, [ebx]
	mov [esp+0x4], ecx
	mov eax, [eax+0x32c]
	mov [esp], eax
	call strcmp
	test eax, eax
	jz BG_FillInAllWeaponItems_100
	add edi, 0x1
	add ebx, 0x4
	cmp edi, esi
	jnz BG_FillInAllWeaponItems_110
	mov edx, esi
	mov eax, esi
	jmp BG_FillInAllWeaponItems_120
BG_FillInAllWeaponItems_100:
	mov eax, [ebp-0x1c]
	mov [eax+0x330], edi
	jmp BG_FillInAllWeaponItems_130
BG_FillInAllWeaponItems_60:
	mov ecx, [ebp-0x24]
	mov [ecx+0x328], edi
	jmp BG_FillInAllWeaponItems_140
	nop


;BG_GetMaxPickupableAmmo(playerState_s const*, unsigned int)
BG_GetMaxPickupableAmmo:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x42c
	mov esi, [ebp+0xc]
	lea eax, [ebp-0x218]
	mov dword [esp+0x8], 0x200
	mov dword [esp+0x4], 0x0
	mov [esp], eax
	call memset
	lea eax, [ebp-0x418]
	mov dword [esp+0x8], 0x200
	mov dword [esp+0x4], 0x0
	mov [esp], eax
	call memset
	mov edi, [esi*4+bg_weaponDefs]
	mov ebx, [edi+0x328]
	mov edx, [edi+0x330]
	mov eax, [edi+0x344]
	test eax, eax
	js BG_GetMaxPickupableAmmo_10
	mov eax, [eax*4+bg_sharedAmmoCaps]
	mov eax, [eax+0x348]
	mov [ebp-0x41c], eax
	mov eax, [bg_lastParsedWeaponIndex]
	mov [ebp-0x420], eax
	test eax, eax
	jz BG_GetMaxPickupableAmmo_20
	mov ebx, 0x1
	mov esi, bg_weaponDefs
	jmp BG_GetMaxPickupableAmmo_30
BG_GetMaxPickupableAmmo_40:
	add ebx, 0x1
	add esi, 0x4
	cmp ebx, [ebp-0x420]
	ja BG_GetMaxPickupableAmmo_20
BG_GetMaxPickupableAmmo_30:
	mov eax, ebx
	sar eax, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov edx, 0x1
	shl edx, cl
	mov ecx, edx
	mov edx, [ebp+0x8]
	WB_TEST edx, eax, ecx, WB_OWNED
	jz BG_GetMaxPickupableAmmo_40
	mov edx, [esi+0x4]
	mov eax, [edx+0x344]
	cmp eax, [edi+0x344]
	jnz BG_GetMaxPickupableAmmo_40
	mov eax, [edx+0x500]
	test eax, eax
	jz BG_GetMaxPickupableAmmo_50
	mov eax, [edx+0x330]
	mov ecx, [ebp+eax*4-0x418]
	test ecx, ecx
	jnz BG_GetMaxPickupableAmmo_40
	mov dword [ebp+eax*4-0x418], 0x1
	mov eax, [edx+0x330]
	mov edx, [ebp+0x8]
	mov edx, [edx+eax*4+0x35c]
	sub [ebp-0x41c], edx
	add ebx, 0x1
	add esi, 0x4
	cmp ebx, [ebp-0x420]
	jbe BG_GetMaxPickupableAmmo_30
BG_GetMaxPickupableAmmo_20:
	mov eax, [ebp-0x41c]
BG_GetMaxPickupableAmmo_70:
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetMaxPickupableAmmo_50:
	mov eax, [edx+0x328]
	mov ecx, [ebp+eax*4-0x218]
	test ecx, ecx
	jnz BG_GetMaxPickupableAmmo_40
	mov dword [ebp+eax*4-0x218], 0x1
	mov eax, [edx+0x328]
	mov edx, [ebp+0x8]
	mov edx, [edx+eax*4+0x15c]
	sub [ebp-0x41c], edx
	jmp BG_GetMaxPickupableAmmo_40
BG_GetMaxPickupableAmmo_10:
	mov eax, [edi+0x500]
	test eax, eax
	jz BG_GetMaxPickupableAmmo_60
	mov edi, [edi+0x338]
	mov [ebp-0x41c], edi
	mov eax, [ebp+0x8]
	mov eax, [eax+edx*4+0x35c]
	sub [ebp-0x41c], eax
	mov eax, [ebp-0x41c]
	add esp, 0x42c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetMaxPickupableAmmo_60:
	mov dword [esp+0x8], 0x0
	mov [esp+0x4], esi
	mov edx, [ebp+0x8]
	mov [esp], edx
	call BG_GetAmmoPlayerMax
	mov [ebp-0x41c], eax
	mov edx, [ebp+0x8]
	mov edx, [edx+ebx*4+0x15c]
	sub eax, edx
	mov [ebp-0x41c], eax
	jmp BG_GetMaxPickupableAmmo_70
	nop


;BG_GetVerticalBobFactor(playerState_s const*, float, float, float)
BG_GetVerticalBobFactor:
	push ebp
	mov ebp, esp
	sub esp, 0x68
	mov edx, [ebp+0x8]
	movss xmm3, dword [ebp+0xc]
	movss xmm0, dword [ebp+0x10]
	mov eax, [edx+0x114]
	cmp eax, 0xb
	jz BG_GetVerticalBobFactor_10
	cmp eax, 0x28
	jz BG_GetVerticalBobFactor_20
%ifdef OL_SERVER
	test dword [edx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_GetVerticalBobFactor_30
%else
	cmp word [edx+0xc], 0x0
	jns BG_GetVerticalBobFactor_30
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
BG_GetVerticalBobFactor_40:
	movss xmm0, dword [ebp+0x14]
	minss xmm0, xmm2
	movaps xmm2, xmm0
	movaps xmm0, xmm3
	addss xmm0, xmm3
	movss [esp], xmm0
	movss [ebp-0x38], xmm2
	movss [ebp-0x48], xmm3
	call sinf
	fstp dword [ebp-0x4c]
	movss xmm1, dword [ebp-0x4c]
	movss xmm3, dword [ebp-0x48]
	mulss xmm3, [_float_4_00000000]
	cvtss2sd xmm0, xmm3
	addsd xmm0, [_double_1_57079633]
	movsd [esp], xmm0
	movss [ebp-0x28], xmm1
	call sin
	fstp qword [ebp-0x10]
	cvtsd2ss xmm0, [ebp-0x10]
	mulss xmm0, [_float_0_20000000]
	movss xmm1, dword [ebp-0x28]
	addss xmm1, xmm0
	mulss xmm1, [_float_0_75000000]
	movss xmm2, dword [ebp-0x38]
	mulss xmm2, xmm1
	movss [ebp-0x4c], xmm2
	fld dword [ebp-0x4c]
	leave
	ret
BG_GetVerticalBobFactor_30:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
	jmp BG_GetVerticalBobFactor_40
BG_GetVerticalBobFactor_10:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
	jmp BG_GetVerticalBobFactor_40
BG_GetVerticalBobFactor_20:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
	jmp BG_GetVerticalBobFactor_40


;BG_IsAimDownSightWeapon(unsigned int)
BG_IsAimDownSightWeapon:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x4ec]
	pop ebp
	ret
	nop


;PM_AdjustAimSpreadScale(pmove_t*, pml_t*)
PM_AdjustAimSpreadScale:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x7c
	mov eax, [ebp+0x8]
	mov ebx, [eax]
	mov eax, [ebx+0xe8]
	mov esi, [eax*4+bg_weaponDefs]
	movss xmm2, dword [esi+0x468]
	pxor xmm5, xmm5
	ucomiss xmm2, xmm5
	jp PM_AdjustAimSpreadScale_10
	jz PM_AdjustAimSpreadScale_20
PM_AdjustAimSpreadScale_10:
	cvtsi2ss xmm3, dword [ebx+0xfc]
	movss xmm1, dword [esi+0x450]
	movaps xmm4, xmm3
	subss xmm4, xmm1
	movss xmm0, dword [esi+0x45c]
	subss xmm0, xmm1
	divss xmm4, xmm0
	mov eax, [ebx+0x70]
	mov [ebp-0x2c], eax
	cmp eax, 0x7ff
	jz PM_AdjustAimSpreadScale_30
PM_AdjustAimSpreadScale_140:
	mov eax, [ebx+0xb0]
	test al, 0x8
	jz PM_AdjustAimSpreadScale_40
	mulss xmm2, [esi+0x47c]
	movss xmm1, dword [esi+0x458]
	movaps xmm4, xmm3
	subss xmm4, xmm1
	movss xmm0, dword [esi+0x464]
	subss xmm0, xmm1
	divss xmm4, xmm0
PM_AdjustAimSpreadScale_130:
	cmp dword [ebx+0x100], 0x1
	jz PM_AdjustAimSpreadScale_50
PM_AdjustAimSpreadScale_150:
	mov eax, [ebp+0xc]
	movss xmm0, dword [eax+0x24]
	movss [ebp-0x1c], xmm0
	mulss xmm2, xmm0
	movss [ebp-0x20], xmm2
	movss xmm0, dword [_float_1_00000000]
	ucomiss xmm0, [ebx+0xf4]
	jnz PM_AdjustAimSpreadScale_60
	jp PM_AdjustAimSpreadScale_60
	movaps xmm0, xmm2
	mulss xmm0, [_float__255_00000000]
	jmp PM_AdjustAimSpreadScale_70
PM_AdjustAimSpreadScale_20:
	movss xmm4, dword [_float_1_00000000]
	movss xmm0, dword [_float__255_00000000]
PM_AdjustAimSpreadScale_70:
	addss xmm0, [ebx+0x628]
	movss [ebx+0x628], xmm0
PM_AdjustAimSpreadScale_220:
	cmp dword [ebx+0x100], 0x1
	jz PM_AdjustAimSpreadScale_80
PM_AdjustAimSpreadScale_110:
	movss xmm0, dword [ebx+0x628]
	ucomiss xmm5, xmm0
	ja PM_AdjustAimSpreadScale_90
PM_AdjustAimSpreadScale_120:
	ucomiss xmm0, [_float_255_00000000]
	jbe PM_AdjustAimSpreadScale_100
	mov dword [ebx+0x628], 0x437f0000
PM_AdjustAimSpreadScale_100:
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_AdjustAimSpreadScale_80:
	movaps xmm0, xmm4
	mulss xmm0, [ebx+0x628]
	ucomiss xmm0, [_float_255_00000000]
	jae PM_AdjustAimSpreadScale_110
	jp PM_AdjustAimSpreadScale_110
	mov dword [ebx+0x100], 0x0
	movss [ebx+0x628], xmm0
	ucomiss xmm5, xmm0
	jbe PM_AdjustAimSpreadScale_120
PM_AdjustAimSpreadScale_90:
	mov dword [ebx+0x628], 0x0
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_AdjustAimSpreadScale_40:
	test al, 0x4
	jz PM_AdjustAimSpreadScale_130
	mulss xmm2, [esi+0x478]
	movss xmm1, dword [esi+0x454]
	movaps xmm4, xmm3
	subss xmm4, xmm1
	movss xmm0, dword [esi+0x460]
	subss xmm0, xmm1
	divss xmm4, xmm0
	jmp PM_AdjustAimSpreadScale_130
PM_AdjustAimSpreadScale_30:
	cmp dword [ebx+0x4], 0x1
	jz PM_AdjustAimSpreadScale_140
	mulss xmm2, [_float_0_50000000]
	cmp dword [ebx+0x100], 0x1
	jnz PM_AdjustAimSpreadScale_150
PM_AdjustAimSpreadScale_50:
	movaps xmm0, xmm2
	mov eax, [ebp+0xc]
	mulss xmm0, [eax+0x24]
	divss xmm0, xmm4
	mulss xmm0, [_float__255_00000000]
	jmp PM_AdjustAimSpreadScale_70
PM_AdjustAimSpreadScale_60:
	movss xmm0, dword [esi+0x470]
	movss [ebp-0x28], xmm0
	ucomiss xmm0, xmm5
	jnz PM_AdjustAimSpreadScale_160
	jp PM_AdjustAimSpreadScale_160
	movaps xmm3, xmm5
PM_AdjustAimSpreadScale_240:
	movss xmm2, dword [esi+0x474]
	ucomiss xmm2, xmm5
	jp PM_AdjustAimSpreadScale_170
	jz PM_AdjustAimSpreadScale_180
PM_AdjustAimSpreadScale_170:
	mov eax, [ebp+0x8]
	cmp word [eax+0x1a], 0x0
	jnz PM_AdjustAimSpreadScale_190
PM_AdjustAimSpreadScale_180:
	cmp dword [ebp-0x2c], 0x7ff
	jz PM_AdjustAimSpreadScale_200
PM_AdjustAimSpreadScale_250:
	movss xmm0, dword [ebp-0x1c]
	mulss xmm0, xmm3
	ucomiss xmm0, xmm5
	jbe PM_AdjustAimSpreadScale_210
	mulss xmm0, [_float_255_00000000]
	addss xmm0, [ebx+0x628]
	movss [ebx+0x628], xmm0
	jmp PM_AdjustAimSpreadScale_220
PM_AdjustAimSpreadScale_160:
	mov edi, [ebp+0x8]
	movaps xmm3, xmm5
	mov dword [ebp-0x24], 0x2
PM_AdjustAimSpreadScale_230:
	cvtsi2ss xmm0, dword [edi+0xc]
	mulss xmm0, [_float_0_00549316]
	cvtsi2ss xmm1, dword [edi+0x2c]
	mulss xmm1, [_float_0_00549316]
	subss xmm0, xmm1
	mulss xmm0, [_float_0_00277778]
	movaps xmm1, xmm0
	addss xmm1, [_float_0_50000000]
	movss [esp], xmm1
	movss [ebp-0x48], xmm0
	movss [ebp-0x58], xmm3
	movss [ebp-0x68], xmm4
	movss [ebp-0x78], xmm5
	call floorf
	fstp dword [ebp-0x30]
	movss xmm0, dword [ebp-0x48]
	subss xmm0, [ebp-0x30]
	mulss xmm0, [_float_360_00000000]
	andps xmm0, [_data16_7fffffff]
	mulss xmm0, [_float_0_01000000]
	mulss xmm0, [ebp-0x28]
	divss xmm0, dword [ebp-0x1c]
	movss xmm3, dword [ebp-0x58]
	addss xmm3, xmm0
	add edi, 0x4
	sub dword [ebp-0x24], 0x1
	movss xmm4, dword [ebp-0x68]
	movss xmm5, dword [ebp-0x78]
	jnz PM_AdjustAimSpreadScale_230
	jmp PM_AdjustAimSpreadScale_240
PM_AdjustAimSpreadScale_200:
	cmp dword [ebx+0x4], 0x1
	jz PM_AdjustAimSpreadScale_250
	mov eax, 0x2
	movss xmm0, dword [_float_1_27999997]
PM_AdjustAimSpreadScale_260:
	addss xmm3, xmm0
	sub eax, 0x1
	jnz PM_AdjustAimSpreadScale_260
	jmp PM_AdjustAimSpreadScale_250
PM_AdjustAimSpreadScale_190:
	movss xmm1, dword [ebx+0x28]
	movss xmm0, dword [ebx+0x2c]
	mulss xmm1, xmm1
	mulss xmm0, xmm0
	addss xmm1, xmm0
	mov eax, bg_aimSpreadMoveSpeedThreshold
	mov eax, [eax]
	movss xmm0, dword [eax+0xc]
	mulss xmm0, xmm0
	ucomiss xmm1, xmm0
	jbe PM_AdjustAimSpreadScale_180
	sqrtss xmm0, xmm1
	mulss xmm2, xmm0
	cvtsi2ss xmm0, dword [ebx+0x60]
	divss xmm2, xmm0
	addss xmm3, xmm2
	jmp PM_AdjustAimSpreadScale_180
PM_AdjustAimSpreadScale_210:
	movss xmm0, dword [ebp-0x20]
	mulss xmm0, [_float__255_00000000]
	jmp PM_AdjustAimSpreadScale_70


;BG_CalculateWeaponAngles(weaponState_t*, float*)
BG_CalculateWeaponAngles:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x10c
	mov edx, [ebp+0x8]
	mov eax, [edx]
	mov ebx, [ebp+0xc]
	mov dword [ebx], 0x0
	add ebx, 0x4
	mov [ebp-0x60], ebx
	mov edi, [ebp+0xc]
	mov dword [edi+0x4], 0x0
	add edi, 0x8
	mov [ebp-0x64], edi
	mov edx, [ebp+0xc]
	mov dword [edx+0x8], 0x0
	movss xmm0, dword [eax+0x5c]
	pxor xmm1, xmm1
	ucomiss xmm0, xmm1
	jp BG_CalculateWeaponAngles_10
	jnz BG_CalculateWeaponAngles_10
BG_CalculateWeaponAngles_790:
	mov ebx, [ebp+0x8]
	mov edx, [ebx]
	test byte [edx+0x10], 0x2
	jz BG_CalculateWeaponAngles_20
	mov eax, [edx+0xe0]
BG_CalculateWeaponAngles_700:
	mov eax, [eax*4+bg_weaponDefs]
	mov ecx, [eax+0x4ec]
	test ecx, ecx
	jz BG_CalculateWeaponAngles_30
	movss xmm0, dword [edx+0xf4]
	mulss xmm0, [eax+0x6cc]
	mov edi, [ebp+0xc]
	addss xmm0, [edi]
	movss [edi], xmm0
	mov eax, [ebp+0x8]
	mov edx, [eax]
BG_CalculateWeaponAngles_30:
	test byte [edx+0x10], 0x2
	jz BG_CalculateWeaponAngles_40
	mov eax, [edx+0xe0]
	mov esi, [eax*4+bg_weaponDefs]
	mov ecx, [edx+0xb0]
	mov eax, ecx
	shr eax, 0x3
	and al, 0x1
	jz BG_CalculateWeaponAngles_50
BG_CalculateWeaponAngles_670:
	movss xmm3, dword [esi+0x2b8]
BG_CalculateWeaponAngles_690:
	mov ebx, [ebp+0x8]
	movss xmm0, dword [ebx+0x4]
	ucomiss xmm0, xmm3
	jbe BG_CalculateWeaponAngles_60
	cmp dword [edx+0xec], 0x7
	jz BG_CalculateWeaponAngles_60
	movaps xmm1, xmm0
	subss xmm1, xmm3
	cvtsi2ss xmm0, dword [edx+0x60]
	subss xmm0, xmm3
	divss xmm1, xmm0
	pxor xmm0, xmm0
	subss xmm0, xmm1
	pxor xmm2, xmm2
	ucomiss xmm0, xmm2
	jb BG_CalculateWeaponAngles_70
	pxor xmm1, xmm1
	movss xmm0, dword [_float__1_00000000]
BG_CalculateWeaponAngles_870:
	pxor xmm2, xmm2
	cmpss xmm2, xmm0, 0x6
	movaps xmm0, xmm2
	movaps xmm2, xmm1
	andps xmm2, xmm0
	movss xmm1, dword [_float_1_00000000]
	andnps xmm0, xmm1
	movaps xmm1, xmm0
	orps xmm1, xmm2
	test al, al
	jnz BG_CalculateWeaponAngles_80
	and cl, 0x4
	jz BG_CalculateWeaponAngles_90
	lea eax, [esi+0x264]
	movaps xmm0, xmm1
	mulss xmm0, [esi+0x264]
	jmp BG_CalculateWeaponAngles_100
BG_CalculateWeaponAngles_60:
	mov dword [ebp-0x24], 0x0
	mov dword [ebp-0x20], 0x0
	mov dword [ebp-0x1c], 0x0
BG_CalculateWeaponAngles_840:
	movss xmm1, dword [edx+0xf4]
	pxor xmm0, xmm0
	ucomiss xmm1, xmm0
	jp BG_CalculateWeaponAngles_110
	jz BG_CalculateWeaponAngles_120
BG_CalculateWeaponAngles_110:
	movss xmm0, dword [_float_1_00000000]
	subss xmm0, xmm1
	movaps xmm1, xmm0
	mulss xmm1, [ebp-0x24]
	movss [ebp-0x24], xmm1
	movaps xmm1, xmm0
	mulss xmm1, [ebp-0x20]
	movss [ebp-0x20], xmm1
	mulss xmm0, [ebp-0x1c]
	movss [ebp-0x1c], xmm0
BG_CalculateWeaponAngles_120:
	mov eax, [ebp+0x8]
	mov ebx, 0x1
	movss xmm6, dword [_float_0_10000000]
	movss xmm5, dword [_float__0_10000000]
	movss xmm4, dword [_float_11_00000000]
BG_CalculateWeaponAngles_180:
	lea ecx, [eax+0xc]
	movss xmm3, dword [eax+0xc]
	movss xmm2, dword [ebp+ebx*4-0x28]
	ucomiss xmm3, xmm2
	jp BG_CalculateWeaponAngles_130
	jz BG_CalculateWeaponAngles_140
BG_CalculateWeaponAngles_130:
	movss xmm0, dword [edx+0x118]
	ucomiss xmm0, xmm4
	jnz BG_CalculateWeaponAngles_150
	jp BG_CalculateWeaponAngles_150
	movaps xmm1, xmm2
	subss xmm1, xmm3
	mov edi, [ebp+0x8]
	mulss xmm1, [edi+0x8]
	mulss xmm1, [esi+0x2ac]
BG_CalculateWeaponAngles_300:
	ucomiss xmm2, xmm3
	jbe BG_CalculateWeaponAngles_160
	mov edi, [ebp+0x8]
	movss xmm0, dword [edi+0x8]
	mulss xmm0, xmm6
	maxss xmm0, xmm1
	addss xmm0, xmm3
	movss [ecx], xmm0
	ucomiss xmm0, xmm2
	ja BG_CalculateWeaponAngles_170
BG_CalculateWeaponAngles_140:
	add ebx, 0x1
	add eax, 0x4
	cmp ebx, 0x4
	jnz BG_CalculateWeaponAngles_180
BG_CalculateWeaponAngles_290:
	movss xmm1, dword [edx+0xf4]
	pxor xmm0, xmm0
	ucomiss xmm1, xmm0
	jp BG_CalculateWeaponAngles_190
	jz BG_CalculateWeaponAngles_200
BG_CalculateWeaponAngles_190:
	ucomiss xmm1, [_float_0_50000000]
	jb BG_CalculateWeaponAngles_210
BG_CalculateWeaponAngles_710:
	test byte [edx+0x10], 0x2
	jz BG_CalculateWeaponAngles_220
	mov eax, [edx+0xe0]
BG_CalculateWeaponAngles_660:
	mov ecx, [eax*4+bg_weaponDefs]
	mov eax, [ecx+0x4ec]
	test eax, eax
	jz BG_CalculateWeaponAngles_230
	movss xmm0, dword [edx+0xf4]
	movss xmm1, dword [ecx+0x490]
	movss xmm5, dword [ecx+0x48c]
	subss xmm5, xmm1
	mulss xmm5, xmm0
	addss xmm5, xmm1
	movss xmm1, dword [ecx+0x498]
	movss xmm3, dword [ecx+0x494]
	subss xmm3, xmm1
	mulss xmm3, xmm0
	addss xmm3, xmm1
BG_CalculateWeaponAngles_650:
	mov eax, [edx+0xb0]
	test al, 0x8
	jz BG_CalculateWeaponAngles_240
	movss xmm4, dword [ecx+0x4a0]
BG_CalculateWeaponAngles_630:
	mov eax, [ecx+0x438]
	test eax, eax
	jz BG_CalculateWeaponAngles_250
	pxor xmm1, xmm1
	ucomiss xmm1, [edx+0xf4]
	jz BG_CalculateWeaponAngles_260
BG_CalculateWeaponAngles_310:
	mov edi, [ebp+0x8]
	movss xmm1, dword [edi+0x8]
	mov ebx, edi
	mulss xmm5, [ebx+0x18]
	movss [ebp-0x2c], xmm5
	mov edi, [ecx+0x438]
	test edi, edi
	jz BG_CalculateWeaponAngles_270
BG_CalculateWeaponAngles_330:
	movss xmm0, dword [_float_1_00000000]
	subss xmm0, [edx+0xf4]
	mulss xmm0, xmm5
	movss [ebp-0x2c], xmm0
	mov edi, [ebp+0x8]
	jmp BG_CalculateWeaponAngles_280
BG_CalculateWeaponAngles_160:
	mov edi, [ebp+0x8]
	movss xmm0, dword [edi+0x8]
	mulss xmm0, xmm5
	minss xmm0, xmm1
	addss xmm0, xmm3
	maxss xmm2, xmm0
BG_CalculateWeaponAngles_170:
	movss [ecx], xmm2
	add ebx, 0x1
	add eax, 0x4
	cmp ebx, 0x4
	jnz BG_CalculateWeaponAngles_180
	jmp BG_CalculateWeaponAngles_290
BG_CalculateWeaponAngles_150:
	movaps xmm1, xmm2
	subss xmm1, xmm3
	mov edi, [ebp+0x8]
	mulss xmm1, [edi+0x8]
	mulss xmm1, [esi+0x2a8]
	jmp BG_CalculateWeaponAngles_300
BG_CalculateWeaponAngles_260:
	jp BG_CalculateWeaponAngles_310
BG_CalculateWeaponAngles_250:
	mov eax, [ebp+0x8]
	movss xmm2, dword [eax+0x18]
	ucomiss xmm4, xmm2
	jp BG_CalculateWeaponAngles_320
	jnz BG_CalculateWeaponAngles_320
	movss xmm1, dword [eax+0x8]
	mov ebx, eax
BG_CalculateWeaponAngles_740:
	mulss xmm5, [ebx+0x18]
	movss [ebp-0x2c], xmm5
	mov edi, [ecx+0x438]
	test edi, edi
	jnz BG_CalculateWeaponAngles_330
BG_CalculateWeaponAngles_270:
	mov edi, ebx
BG_CalculateWeaponAngles_280:
	mov edx, [edi+0x50]
	mulss xmm1, [_float_1000_00000000]
	mulss xmm3, xmm1
	cvttss2si eax, xmm3
	add [edx], eax
	mov eax, [edi+0x50]
	cvtsi2ss xmm0, dword [eax]
	mulss xmm0, [_float_0_00050000]
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0xfc]
	movss xmm0, dword [ebp-0xfc]
	mulss xmm0, [ebp-0x2c]
	mulss xmm0, [_float_0_01000000]
	mov eax, [ebp-0x64]
	addss xmm0, [eax]
	movss [eax], xmm0
	mov eax, [edi+0x50]
	cvtsi2ss xmm0, dword [eax]
	mulss xmm0, [_float_0_00070000]
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0xfc]
	movss xmm0, dword [ebp-0xfc]
	mulss xmm0, [ebp-0x2c]
	mulss xmm0, [_float_0_01000000]
	mov edx, [ebp-0x60]
	addss xmm0, [edx]
	movss [edx], xmm0
	mov eax, [edi+0x50]
	cvtsi2ss xmm0, dword [eax]
	mulss xmm0, [_float_0_00100000]
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0x48]
	movss xmm0, dword [ebp-0x2c]
	mulss xmm0, [ebp-0x48]
	movss xmm1, dword [_float_0_01000000]
	mulss xmm1, xmm0
	movss [ebp-0x2c], xmm1
	mov eax, [ebp+0xc]
	addss xmm1, [eax]
	movss [eax], xmm1
	mov ebx, [edi]
	test byte [ebx+0x10], 0x2
	jz BG_CalculateWeaponAngles_340
	mov eax, [ebx+0xe0]
BG_CalculateWeaponAngles_780:
	mov edi, [eax*4+bg_weaponDefs]
	movzx eax, byte [ebx+0x8]
	cvtsi2ss xmm0, eax
	divss xmm0, dword [_float_255_00000000]
	cvtss2sd xmm0, xmm0
	mulsd xmm0, [_double_3_14159265]
	addsd xmm0, xmm0
	movsd xmm1, qword [_double_6_28318531]
	addsd xmm0, xmm1
	cvtsd2ss xmm0, xmm0
	cvtss2sd xmm0, xmm0
	addsd xmm0, [_double_0_78539816]
	addsd xmm0, xmm1
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x40], xmm0
	movss xmm0, dword [_float_0_16000000]
	movss [ebp-0x44], xmm0
	mov eax, [ebp+0x8]
	mulss xmm0, [eax+0x4]
	movss [ebp-0x44], xmm0
	mov esi, [ebx+0x114]
	cmp esi, 0xb
	jz BG_CalculateWeaponAngles_350
	cmp esi, 0x28
	jz BG_CalculateWeaponAngles_360
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateWeaponAngles_370
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateWeaponAngles_370
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0x10]
BG_CalculateWeaponAngles_810:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm2, 0x5
	andps xmm2, xmm0
	andnps xmm0, xmm1
	orps xmm0, xmm2
	movss [ebp-0x30], xmm0
	movss xmm0, dword [ebp-0x40]
	addss xmm0, xmm0
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0xfc]
	movss xmm1, dword [ebp-0xfc]
	movss xmm0, dword [ebp-0x40]
	mulss xmm0, [_float_4_00000000]
	cvtss2sd xmm0, xmm0
	addsd xmm0, [_double_1_57079633]
	movsd [esp], xmm0
	movss [ebp-0x98], xmm1
	call sin
	fstp qword [ebp-0x50]
	cvtsd2ss xmm0, [ebp-0x50]
	mulss xmm0, [_float_0_20000000]
	movss xmm1, dword [ebp-0x98]
	addss xmm1, xmm0
	mulss xmm1, [_float_0_75000000]
	mulss xmm1, [ebp-0x30]
	movss [ebp-0x30], xmm1
	xor dword [ebp-0x30], 0x80000000
	cmp esi, 0xb
	jz BG_CalculateWeaponAngles_380
	cmp esi, 0x28
	jz BG_CalculateWeaponAngles_390
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateWeaponAngles_400
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateWeaponAngles_400
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0xc]
BG_CalculateWeaponAngles_820:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm2, 0x5
	andps xmm2, xmm0
	movaps xmm4, xmm0
	andnps xmm4, xmm1
	orps xmm4, xmm2
	movss xmm0, dword [ebp-0x40]
	movss [esp], xmm0
	movss [ebp-0xc8], xmm4
	call sinf
	fstp dword [ebp-0x54]
	movss xmm4, dword [ebp-0xc8]
	mulss xmm4, [ebp-0x54]
	xorps xmm4, [_data16_80000000]
	cvtss2sd xmm0, [ebp-0x40]
	subsd xmm0, [_double_0_47123892]
	cvtsd2ss xmm5, xmm0
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [_float_1_50000000]
	cmp esi, 0xb
	jz BG_CalculateWeaponAngles_410
	cmp esi, 0x28
	jz BG_CalculateWeaponAngles_420
%ifdef OL_SERVER
	test dword [ebx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_CalculateWeaponAngles_430
%else
	cmp word [ebx+0xc], 0x0
	jns BG_CalculateWeaponAngles_430
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
BG_CalculateWeaponAngles_800:
	movss xmm1, dword [_float_10_00000000]
	movaps xmm0, xmm1
	cmpss xmm0, xmm3, 0x5
	movaps xmm2, xmm3
	andps xmm2, xmm0
	movaps xmm3, xmm0
	andnps xmm3, xmm1
	orps xmm3, xmm2
	movss [esp], xmm5
	movss [ebp-0xb8], xmm3
	movss [ebp-0xc8], xmm4
	call sinf
	fstp dword [ebp-0xfc]
	movss xmm2, dword [ebp-0xfc]
	movss xmm3, dword [ebp-0xb8]
	mulss xmm2, xmm3
	pxor xmm0, xmm0
	subss xmm0, xmm2
	pxor xmm1, xmm1
	cmpss xmm1, xmm0, 0x2
	andps xmm2, xmm1
	pxor xmm0, xmm0
	orps xmm2, xmm0
	movss xmm3, dword [ebx+0xf4]
	movss xmm4, dword [ebp-0xc8]
	ucomiss xmm3, xmm0
	jp BG_CalculateWeaponAngles_440
	jz BG_CalculateWeaponAngles_450
BG_CalculateWeaponAngles_440:
	movss xmm0, dword [_float_1_00000000]
	movaps xmm1, xmm0
	subss xmm1, [edi+0x448]
	mulss xmm1, xmm3
	subss xmm0, xmm1
	movss xmm1, dword [ebp-0x30]
	mulss xmm1, xmm0
	movss [ebp-0x30], xmm1
	mulss xmm4, xmm0
	mulss xmm2, xmm0
BG_CalculateWeaponAngles_450:
	mov esi, [edi+0x438]
	test esi, esi
	jz BG_CalculateWeaponAngles_460
	movss xmm0, dword [_float_1_00000000]
	subss xmm0, xmm3
	movss xmm1, dword [ebp-0x30]
	mulss xmm1, xmm0
	movss [ebp-0x30], xmm1
	mulss xmm4, xmm0
	mulss xmm2, xmm0
	movaps xmm0, xmm1
BG_CalculateWeaponAngles_750:
	mov eax, [ebp+0xc]
	addss xmm0, [eax]
	movss [eax], xmm0
	mov eax, [ebp-0x60]
	addss xmm4, [eax]
	movss [eax], xmm4
	mov edx, [ebp-0x64]
	addss xmm2, [edx]
	movss [edx], xmm2
	mov ebx, [ebp+0x8]
	mov ecx, [ebx+0x20]
	test ecx, ecx
	jz BG_CalculateWeaponAngles_470
	mov edx, [ebx]
	test byte [edx+0x10], 0x2
	jz BG_CalculateWeaponAngles_480
	mov eax, [edx+0xe0]
BG_CalculateWeaponAngles_830:
	mov eax, [eax*4+bg_weaponDefs]
	movss xmm1, dword [edx+0xf4]
	movss xmm0, dword [_float_0_50000000]
	movaps xmm2, xmm1
	mulss xmm2, xmm0
	addss xmm2, xmm0
	movaps xmm3, xmm2
	mulss xmm3, [_float_100_00000000]
	movaps xmm4, xmm2
	mulss xmm4, [_float_400_00000000]
	pxor xmm0, xmm0
	ucomiss xmm1, xmm0
	jp BG_CalculateWeaponAngles_490
	jz BG_CalculateWeaponAngles_500
BG_CalculateWeaponAngles_490:
	mov ebx, [eax+0x438]
	test ebx, ebx
	jnz BG_CalculateWeaponAngles_510
BG_CalculateWeaponAngles_500:
	mov ebx, [ebp+0x8]
	mov eax, [ebx+0x1c]
	sub eax, ecx
	cvtsi2ss xmm0, eax
	ucomiss xmm3, xmm0
	ja BG_CalculateWeaponAngles_520
BG_CalculateWeaponAngles_850:
	subss xmm0, xmm3
	divss xmm0, xmm4
	movss xmm1, dword [_float_1_00000000]
	subss xmm1, xmm0
	movaps xmm0, xmm1
	pxor xmm1, xmm1
	ucomiss xmm0, xmm1
	jbe BG_CalculateWeaponAngles_530
	movss xmm1, dword [_float_1_00000000]
	subss xmm1, xmm0
	movss [esp], xmm1
	movss [ebp-0xa8], xmm2
	call GetLeanFraction
	fstp dword [ebp-0x3c]
	movss xmm1, dword [_float_1_00000000]
	subss xmm1, [ebp-0x3c]
	movss xmm2, dword [ebp-0xa8]
	mulss xmm1, xmm2
	movaps xmm0, xmm1
	mov eax, [ebp+0x8]
	mulss xmm0, [eax+0x24]
	movss xmm3, dword [_float_0_50000000]
	mulss xmm0, xmm3
	mov edx, [ebp+0xc]
	addss xmm0, [edx]
	movss [edx], xmm0
	movaps xmm2, xmm1
	mulss xmm2, [eax+0x28]
	mov ebx, [ebp-0x60]
	movss xmm0, dword [ebx]
	subss xmm0, xmm2
	movss [ebx], xmm0
	mulss xmm1, [eax+0x28]
	mulss xmm1, xmm3
	mov edi, [ebp-0x64]
	addss xmm1, [edi]
	movss [edi], xmm1
	mov edx, [eax]
BG_CalculateWeaponAngles_530:
	test byte [edx+0x10], 0x2
	jnz BG_CalculateWeaponAngles_540
BG_CalculateWeaponAngles_760:
	mov eax, [edx+0xe8]
BG_CalculateWeaponAngles_770:
	mov ebx, [eax*4+bg_weaponDefs]
	mov ecx, [ebx+0x4ec]
	test ecx, ecx
	jz BG_CalculateWeaponAngles_550
	movss xmm1, dword [edx+0xf4]
	movss xmm0, dword [ebx+0x738]
	movss xmm2, dword [ebx+0x6f0]
	subss xmm2, xmm0
	mulss xmm2, xmm1
	addss xmm0, xmm2
	movss [ebp-0x34], xmm0
	movss xmm0, dword [ebx+0x73c]
	movss xmm7, dword [ebx+0x6f4]
	subss xmm7, xmm0
	mulss xmm7, xmm1
	addss xmm7, xmm0
	movss xmm0, dword [ebx+0x740]
	movss xmm6, dword [ebx+0x6f8]
	subss xmm6, xmm0
	mulss xmm6, xmm1
	addss xmm6, xmm0
	movss xmm0, dword [ebx+0x744]
	movss xmm5, dword [ebx+0x6fc]
	subss xmm5, xmm0
	mulss xmm5, xmm1
	addss xmm5, xmm0
	mov eax, [ebp+0x8]
	movss xmm4, dword [eax+0x8]
	pxor xmm0, xmm0
	ucomiss xmm4, xmm0
	jbe BG_CalculateWeaponAngles_560
	mov edx, eax
	add eax, 0x38
	mov [ebp-0x6c], eax
	lea edi, [edx+0x2c]
	lea eax, [edx+0x3c]
	mov [ebp-0x70], eax
	add edx, 0x30
	mov [ebp-0x68], edx
	jmp BG_CalculateWeaponAngles_570
BG_CalculateWeaponAngles_600:
	subss xmm4, [_float_0_00500000]
	movss [ebp-0x38], xmm4
	movss xmm4, dword [_float_0_00500000]
BG_CalculateWeaponAngles_610:
	movss xmm1, dword [ebx+0x4a4]
	movss [esp+0x4], xmm5
	movss [esp], xmm6
	movaps xmm3, xmm7
	movss xmm2, dword [ebp-0x34]
	movaps xmm0, xmm4
	mov edx, [ebp-0x6c]
	mov eax, edi
	movss [ebp-0xc8], xmm4
	movss [ebp-0xd8], xmm5
	movss [ebp-0xe8], xmm6
	movss [ebp-0xf8], xmm7
	call BG_CalculateWeaponPosition_GunRecoil_SingleAngle
	mov esi, eax
	movss xmm1, dword [ebx+0x4a8]
	movss xmm5, dword [ebp-0xd8]
	movss [esp+0x4], xmm5
	movss xmm6, dword [ebp-0xe8]
	movss [esp], xmm6
	movss xmm7, dword [ebp-0xf8]
	movaps xmm3, xmm7
	movss xmm2, dword [ebp-0x34]
	movss xmm4, dword [ebp-0xc8]
	movaps xmm0, xmm4
	mov edx, [ebp-0x70]
	mov eax, [ebp-0x68]
	call BG_CalculateWeaponPosition_GunRecoil_SingleAngle
	test eax, eax
	movss xmm5, dword [ebp-0xd8]
	movss xmm6, dword [ebp-0xe8]
	movss xmm7, dword [ebp-0xf8]
	jz BG_CalculateWeaponAngles_580
	test esi, esi
	jnz BG_CalculateWeaponAngles_590
BG_CalculateWeaponAngles_580:
	movss xmm0, dword [ebp-0x38]
	pxor xmm1, xmm1
	ucomiss xmm0, xmm1
	jbe BG_CalculateWeaponAngles_590
	movaps xmm4, xmm0
BG_CalculateWeaponAngles_570:
	ucomiss xmm4, [_float_0_00500000]
	ja BG_CalculateWeaponAngles_600
	mov dword [ebp-0x38], 0x0
	jmp BG_CalculateWeaponAngles_610
BG_CalculateWeaponAngles_240:
	test al, 0x4
	jz BG_CalculateWeaponAngles_620
	movss xmm4, dword [ecx+0x49c]
	jmp BG_CalculateWeaponAngles_630
BG_CalculateWeaponAngles_230:
	movss xmm5, dword [ecx+0x490]
	pxor xmm0, xmm0
	ucomiss xmm5, xmm0
	jnz BG_CalculateWeaponAngles_640
	jp BG_CalculateWeaponAngles_640
	movss xmm5, dword [_float_80_00000000]
	movss xmm3, dword [_float_1_00000000]
	jmp BG_CalculateWeaponAngles_650
BG_CalculateWeaponAngles_220:
	mov eax, [edx+0xe8]
	jmp BG_CalculateWeaponAngles_660
BG_CalculateWeaponAngles_40:
	mov eax, [edx+0xe8]
	mov esi, [eax*4+bg_weaponDefs]
	mov ecx, [edx+0xb0]
	mov eax, ecx
	shr eax, 0x3
	and al, 0x1
	jnz BG_CalculateWeaponAngles_670
BG_CalculateWeaponAngles_50:
	test cl, 0x4
	jnz BG_CalculateWeaponAngles_680
	movss xmm3, dword [esi+0x2b0]
	jmp BG_CalculateWeaponAngles_690
BG_CalculateWeaponAngles_20:
	mov eax, [edx+0xe8]
	jmp BG_CalculateWeaponAngles_700
BG_CalculateWeaponAngles_200:
	mov eax, [ebp+0x8]
	add eax, 0xc
	mov edi, [ebp+0xc]
	movss xmm0, dword [edi]
	mov edx, [ebp+0x8]
	addss xmm0, [edx+0xc]
	movss [edi], xmm0
	movss xmm0, dword [edi+0x4]
	addss xmm0, [eax+0x4]
	mov ebx, [ebp-0x60]
	movss [ebx], xmm0
	movss xmm0, dword [edi+0x8]
	addss xmm0, [eax+0x8]
	mov edi, [ebp-0x64]
	movss [edi], xmm0
	mov eax, [ebp+0x8]
	mov edx, [eax]
	jmp BG_CalculateWeaponAngles_710
BG_CalculateWeaponAngles_320:
	ucomiss xmm4, xmm2
	ja BG_CalculateWeaponAngles_720
	mov ebx, [ebp+0x8]
	movss xmm1, dword [ebx+0x8]
	movaps xmm0, xmm1
	mulss xmm0, [_float_0_50000000]
	subss xmm2, xmm0
	movss [ebx+0x18], xmm2
	ucomiss xmm4, xmm2
	jbe BG_CalculateWeaponAngles_730
	movss [ebx+0x18], xmm4
	mov ebx, [ebp+0x8]
	jmp BG_CalculateWeaponAngles_740
BG_CalculateWeaponAngles_210:
	jp BG_CalculateWeaponAngles_710
	mulss xmm1, [_float__2_00000000]
	addss xmm1, [_float_1_00000000]
	mov eax, [ebp+0x8]
	add eax, 0xc
	movaps xmm0, xmm1
	mov edx, [ebp+0x8]
	mulss xmm0, [edx+0xc]
	mov ebx, [ebp+0xc]
	addss xmm0, [ebx]
	movss [ebx], xmm0
	movaps xmm0, xmm1
	mulss xmm0, [eax+0x4]
	addss xmm0, [ebx+0x4]
	mov edi, [ebp-0x60]
	movss [edi], xmm0
	mulss xmm1, [eax+0x8]
	addss xmm1, [ebx+0x8]
	mov eax, [ebp-0x64]
	movss [eax], xmm1
	mov ebx, [ebp+0x8]
	mov edx, [ebx]
	jmp BG_CalculateWeaponAngles_710
BG_CalculateWeaponAngles_680:
	movss xmm3, dword [esi+0x2b4]
	jmp BG_CalculateWeaponAngles_690
BG_CalculateWeaponAngles_640:
	movss xmm3, dword [ecx+0x498]
	jmp BG_CalculateWeaponAngles_650
BG_CalculateWeaponAngles_620:
	movss xmm4, dword [_float_1_00000000]
	jmp BG_CalculateWeaponAngles_630
BG_CalculateWeaponAngles_590:
	mov ebx, [ebp+0x8]
BG_CalculateWeaponAngles_860:
	mov edx, [ebp+0xc]
	movss xmm0, dword [edx]
	addss xmm0, [ebx+0x2c]
	movss [edx], xmm0
	mov eax, [ebp-0x60]
	movss xmm0, dword [eax]
	addss xmm0, [edi+0x4]
	movss [eax], xmm0
	mov edx, [ebp-0x64]
	movss xmm0, dword [edx]
	addss xmm0, [edi+0x8]
	movss [edx], xmm0
BG_CalculateWeaponAngles_550:
	mov ebx, [ebp+0xc]
	movss xmm0, dword [ebx]
	mov edi, [ebp+0x8]
	subss xmm0, [edi+0x44]
	mulss xmm0, [_float_0_00277778]
	movss xmm2, dword [_float_0_50000000]
	movaps xmm1, xmm0
	addss xmm1, xmm2
	movss [esp], xmm1
	movss [ebp-0x88], xmm0
	movss [ebp-0xa8], xmm2
	call floorf
	fstp dword [ebp-0x58]
	movss xmm0, dword [ebp-0x88]
	subss xmm0, [ebp-0x58]
	mulss xmm0, [_float_360_00000000]
	movss [ebx], xmm0
	mov eax, [ebp-0x60]
	movss xmm0, dword [eax]
	subss xmm0, [edi+0x48]
	mulss xmm0, [_float_0_00277778]
	movss xmm2, dword [ebp-0xa8]
	addss xmm2, xmm0
	movss [esp], xmm2
	movss [ebp-0x88], xmm0
	call floorf
	fstp dword [ebp-0x5c]
	movss xmm0, dword [ebp-0x88]
	subss xmm0, [ebp-0x5c]
	mulss xmm0, [_float_360_00000000]
	mov edx, [ebp-0x60]
	movss [edx], xmm0
	add esp, 0x10c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_CalculateWeaponAngles_460:
	movss xmm0, dword [ebp-0x30]
	jmp BG_CalculateWeaponAngles_750
BG_CalculateWeaponAngles_470:
	mov edx, [ebx]
	test byte [edx+0x10], 0x2
	jz BG_CalculateWeaponAngles_760
BG_CalculateWeaponAngles_540:
	mov eax, [edx+0xe0]
	jmp BG_CalculateWeaponAngles_770
BG_CalculateWeaponAngles_340:
	mov eax, [ebx+0xe8]
	jmp BG_CalculateWeaponAngles_780
BG_CalculateWeaponAngles_10:
	movss [esp], xmm0
	call GetLeanFraction
	fstp dword [ebp-0xfc]
	movss xmm0, dword [ebp-0xfc]
	mulss xmm0, [_float__2_00000000]
	mov eax, [ebp+0xc]
	addss xmm0, [eax+0x8]
	movss [eax+0x8], xmm0
	jmp BG_CalculateWeaponAngles_790
BG_CalculateWeaponAngles_410:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateWeaponAngles_800
BG_CalculateWeaponAngles_350:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm2, xmm0
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateWeaponAngles_810
BG_CalculateWeaponAngles_380:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateWeaponAngles_820
BG_CalculateWeaponAngles_370:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateWeaponAngles_810
BG_CalculateWeaponAngles_400:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateWeaponAngles_820
BG_CalculateWeaponAngles_430:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateWeaponAngles_800
BG_CalculateWeaponAngles_480:
	mov eax, [edx+0xe8]
	jmp BG_CalculateWeaponAngles_830
BG_CalculateWeaponAngles_390:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0xc]
	jmp BG_CalculateWeaponAngles_820
BG_CalculateWeaponAngles_420:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm3, xmm2
	mulss xmm3, [eax+0xc]
	jmp BG_CalculateWeaponAngles_800
BG_CalculateWeaponAngles_360:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movss xmm2, dword [ebp-0x44]
	mulss xmm2, [eax+0x10]
	jmp BG_CalculateWeaponAngles_810
BG_CalculateWeaponAngles_720:
	movss xmm1, dword [eax+0x8]
	movaps xmm0, xmm1
	mulss xmm0, [_float_0_50000000]
	addss xmm0, xmm2
	movss [eax+0x18], xmm0
	ucomiss xmm0, xmm4
	jbe BG_CalculateWeaponAngles_730
	movss [eax+0x18], xmm4
	mov ebx, [ebp+0x8]
	jmp BG_CalculateWeaponAngles_740
BG_CalculateWeaponAngles_80:
	lea eax, [esi+0x288]
	movaps xmm0, xmm1
	mulss xmm0, [esi+0x288]
BG_CalculateWeaponAngles_100:
	movss [ebp-0x24], xmm0
	movaps xmm0, xmm1
	mulss xmm0, [eax+0x4]
	movss [ebp-0x20], xmm0
	mulss xmm1, [eax+0x8]
	movss [ebp-0x1c], xmm1
	jmp BG_CalculateWeaponAngles_840
BG_CalculateWeaponAngles_510:
	mulss xmm1, [_float__0_75000000]
	addss xmm1, [_float_1_00000000]
	mulss xmm2, xmm1
	mov ebx, [ebp+0x8]
	mov eax, [ebx+0x1c]
	sub eax, ecx
	cvtsi2ss xmm0, eax
	ucomiss xmm3, xmm0
	jbe BG_CalculateWeaponAngles_850
BG_CalculateWeaponAngles_520:
	divss xmm0, xmm3
	movss [esp], xmm0
	movss [ebp-0xa8], xmm2
	call GetLeanFraction
	fstp dword [ebp-0xfc]
	movss xmm1, dword [ebp-0xfc]
	movss xmm2, dword [ebp-0xa8]
	mulss xmm1, xmm2
	movaps xmm0, xmm1
	mulss xmm0, [ebx+0x24]
	movss xmm3, dword [_float_0_50000000]
	mulss xmm0, xmm3
	mov edi, [ebp+0xc]
	addss xmm0, [edi]
	movss [edi], xmm0
	movaps xmm2, xmm1
	mulss xmm2, [ebx+0x28]
	mov eax, [ebp-0x60]
	movss xmm0, dword [eax]
	subss xmm0, xmm2
	movss [eax], xmm0
	mulss xmm1, [ebx+0x28]
	mulss xmm1, xmm3
	mov edx, [ebp-0x64]
	addss xmm1, [edx]
	movss [edx], xmm1
	mov edx, [ebx]
	jmp BG_CalculateWeaponAngles_530
BG_CalculateWeaponAngles_730:
	mov ebx, [ebp+0x8]
	jmp BG_CalculateWeaponAngles_740
BG_CalculateWeaponAngles_560:
	lea edi, [eax+0x2c]
	mov ebx, eax
	jmp BG_CalculateWeaponAngles_860
BG_CalculateWeaponAngles_90:
	lea eax, [esi+0x240]
	movaps xmm0, xmm1
	mulss xmm0, [esi+0x240]
	jmp BG_CalculateWeaponAngles_100
BG_CalculateWeaponAngles_70:
	movaps xmm0, xmm1
	subss xmm0, [_float_1_00000000]
	jmp BG_CalculateWeaponAngles_870


;BG_GetWeaponIndexForName(char const*, void (*)(unsigned int))
BG_GetWeaponIndexForName:
BG_GetWeaponIndexForName_270:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x3c
	mov edi, [ebp+0x8]
	cmp byte [edi], 0x0
	jnz BG_GetWeaponIndexForName_10
BG_GetWeaponIndexForName_20:
	mov dword [ebp-0x28], 0x0
	mov eax, [ebp-0x28]
BG_GetWeaponIndexForName_220:
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetWeaponIndexForName_10:
	mov dword [esp+0x4], _cstring_none
	mov [esp], edi
	call Q_stricmp
	test eax, eax
	jz BG_GetWeaponIndexForName_20
	mov eax, [bg_lastParsedWeaponIndex]
	test eax, eax
	jnz BG_GetWeaponIndexForName_30
BG_GetWeaponIndexForName_200:
	%ifdef OL_SERVER
	; All C1 definitions, including equipment first granted after map load,
	; come from mounted native assets. fs_game must not route them through
	; the retained IW3 loose-text weapon reader.
	mov eax, BG_LoadWeaponDef_FastFile
	jmp BG_GetWeaponIndexForName_130
	%endif
	mov eax, fs_gameDirVar
	mov eax, [eax]
	mov eax, [eax+0xc]
	cmp byte [eax], 0x0
	jz BG_GetWeaponIndexForName_40
BG_GetWeaponIndexForName_120:
	mov eax, BG_LoadWeaponDef_LoadObj
BG_GetWeaponIndexForName_130:
	mov [esp], edi
	call eax
	mov [ebp-0x30], eax
	test eax, eax
	jz BG_GetWeaponIndexForName_50
	mov eax, useFastFile
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jnz BG_GetWeaponIndexForName_60
BG_GetWeaponIndexForName_140:
	%ifdef OL_SERVER
	; The table and the three pools below append without a bound; C1 sizes
	; all four at 700 (0x41F570) but ours keep 128 per pool, matching
	; playerState ammo[128]/ammoclip[128]. Entry 128 overwrote the next
	; pool's counter. Refuse the definition first (returns 0, unknown weapon).
	extern AdmitWeaponPools
	mov eax, [ebp-0x30]
	mov [esp], eax
	call AdmitWeaponPools
	test eax, eax
	jz BG_GetWeaponIndexForName_20
	%endif
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	mov [ebp-0x2c], eax
	mov [bg_lastParsedWeaponIndex], eax
	mov [ebp-0x28], eax
	mov edx, [ebp-0x30]
	mov [eax*4+bg_weaponDefs], edx
	mov esi, [bg_numAmmoTypes]
	test esi, esi
	jnz BG_GetWeaponIndexForName_70
	xor eax, eax
	xor edx, edx
BG_GetWeaponIndexForName_180:
	mov ecx, [ebp-0x30]
	mov [eax*4+bg_weapAmmoTypes], ecx
	mov [ecx+0x328], edx
	lea eax, [esi+0x1]
	mov [bg_numAmmoTypes], eax
BG_GetWeaponIndexForName_300:
	mov eax, [ebp-0x2c]
	call BG_SetupSharedAmmoIndexes
	mov eax, [ebp-0x2c]
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x1c], eax
	mov esi, [bg_numWeapClips]
	test esi, esi
	jnz BG_GetWeaponIndexForName_80
	xor eax, eax
	xor edx, edx
BG_GetWeaponIndexForName_260:
	mov ecx, [ebp-0x1c]
	mov [eax*4+bg_weapClips], ecx
	mov [ecx+0x330], edx
	lea eax, [esi+0x1]
	mov [bg_numWeapClips], eax
BG_GetWeaponIndexForName_310:
	xor edx, edx
	mov eax, [ebp-0x2c]
	shl eax, 0x2
	add eax, bg_itemlist
BG_GetWeaponIndexForName_90:
	mov dword [eax], 0x1
	add edx, 0x1
	add eax, ITEM_STRIDE*4
	cmp edx, ITEM_MODELS
	jnz BG_GetWeaponIndexForName_90
	mov eax, [ebp-0x2c]
	mov ebx, [eax*4+bg_weaponDefs]
	mov dword [ebx+0x540], 0x0
	mov eax, [ebx+0x53c]
	cmp byte [eax], 0x0
	jnz BG_GetWeaponIndexForName_100
BG_GetWeaponIndexForName_290:
	mov eax, [ebp+0xc]
	test eax, eax
	jz BG_GetWeaponIndexForName_110
	mov ecx, [ebp-0x2c]
	mov [esp], ecx
	call dword [ebp+0xc]
	mov eax, [ebp-0x28]
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetWeaponIndexForName_40:
	mov eax, useFastFile
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jz BG_GetWeaponIndexForName_120
	mov eax, BG_LoadWeaponDef_FastFile
	jmp BG_GetWeaponIndexForName_130
BG_GetWeaponIndexForName_60:
	mov [esp+0x4], edi
	mov dword [esp], 0x17
	call DB_FindXAssetHeaderReal
	mov [esp+0x4], edi
	mov dword [esp], 0x17
	call DB_IsXAssetDefault
	test al, al
	jz BG_GetWeaponIndexForName_140
	jmp BG_GetWeaponIndexForName_20
BG_GetWeaponIndexForName_70:
	mov eax, [edx+0x324]
	mov [ebp-0x24], eax
	xor edi, edi
	mov ebx, bg_weapAmmoTypes
	mov edx, eax
	jmp BG_GetWeaponIndexForName_150
BG_GetWeaponIndexForName_170:
	mov edx, [ebp-0x24]
BG_GetWeaponIndexForName_150:
	mov eax, [ebx]
	mov [esp+0x4], edx
	mov eax, [eax+0x324]
	mov [esp], eax
	call strcmp
	test eax, eax
	jz BG_GetWeaponIndexForName_160
	add edi, 0x1
	add ebx, 0x4
	cmp esi, edi
	jnz BG_GetWeaponIndexForName_170
	mov edx, esi
	mov eax, esi
	jmp BG_GetWeaponIndexForName_180
BG_GetWeaponIndexForName_30:
	mov esi, 0x1
	mov ebx, bg_weaponDefs
	jmp BG_GetWeaponIndexForName_190
BG_GetWeaponIndexForName_210:
	add esi, 0x1
	add ebx, 0x4
	cmp esi, [bg_lastParsedWeaponIndex]
	ja BG_GetWeaponIndexForName_200
BG_GetWeaponIndexForName_190:
	mov eax, [ebx+0x4]
	mov eax, [eax]
	mov [esp+0x4], eax
	mov [esp], edi
	call Q_stricmp
	test eax, eax
	jnz BG_GetWeaponIndexForName_210
	test esi, esi
	jz BG_GetWeaponIndexForName_200
	mov [ebp-0x28], esi
	mov eax, esi
	jmp BG_GetWeaponIndexForName_220
BG_GetWeaponIndexForName_80:
	mov ecx, [eax+0x32c]
	mov [ebp-0x20], ecx
	xor edi, edi
	mov ebx, bg_weapClips
	mov edx, ecx
	jmp BG_GetWeaponIndexForName_230
BG_GetWeaponIndexForName_250:
	mov edx, [ebp-0x20]
BG_GetWeaponIndexForName_230:
	mov eax, [ebx]
	mov [esp+0x4], edx
	mov eax, [eax+0x32c]
	mov [esp], eax
	call strcmp
	test eax, eax
	jz BG_GetWeaponIndexForName_240
	add edi, 0x1
	add ebx, 0x4
	cmp esi, edi
	jnz BG_GetWeaponIndexForName_250
	mov edx, esi
	mov eax, esi
	jmp BG_GetWeaponIndexForName_260
BG_GetWeaponIndexForName_100:
	mov edx, [ebp+0xc]
	mov [esp+0x4], edx
	mov [esp], eax
	call BG_GetWeaponIndexForName_270
	mov esi, eax
	test eax, eax
	jz BG_GetWeaponIndexForName_280
BG_GetWeaponIndexForName_320:
	mov [ebx+0x540], esi
	jmp BG_GetWeaponIndexForName_290
BG_GetWeaponIndexForName_110:
	mov eax, [ebp-0x28]
	jmp BG_GetWeaponIndexForName_220
BG_GetWeaponIndexForName_50:
	mov [esp+0x8], edi
	mov dword [esp+0x4], _cstring_couldnt_find_wea
	mov dword [esp], 0x11
	call Com_DPrintf
	mov dword [ebp-0x28], 0x0
	mov eax, [ebp-0x28]
	jmp BG_GetWeaponIndexForName_220
BG_GetWeaponIndexForName_160:
	mov ecx, [ebp-0x30]
	mov [ecx+0x328], edi
	jmp BG_GetWeaponIndexForName_300
BG_GetWeaponIndexForName_240:
	mov edx, [ebp-0x1c]
	mov [edx+0x330], edi
	jmp BG_GetWeaponIndexForName_310
BG_GetWeaponIndexForName_280:
	mov eax, [ebx]
	mov [esp+0xc], eax
	mov eax, [ebx+0x53c]
	mov [esp+0x8], eax
	mov dword [esp+0x4], _cstring_could_not_find_a
	mov dword [esp], 0x2
	call Com_Error
	jmp BG_GetWeaponIndexForName_320


;BG_FindWeaponIndexForName(char const*)
BG_FindWeaponIndexForName:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x1c
	mov edi, [ebp+0x8]
	test edi, edi
	jz BG_FindWeaponIndexForName_10
	mov ebx, [bg_lastParsedWeaponIndex]
	test ebx, ebx
	jnz BG_FindWeaponIndexForName_20
BG_FindWeaponIndexForName_10:
	xor eax, eax
BG_FindWeaponIndexForName_50:
	add esp, 0x1c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_FindWeaponIndexForName_20:
	mov esi, 0x1
	mov ebx, bg_weaponDefs
BG_FindWeaponIndexForName_40:
	mov eax, [ebx+0x4]
	mov eax, [eax]
	mov [esp+0x4], eax
	mov [esp], edi
	call Q_stricmp
	test eax, eax
	jz BG_FindWeaponIndexForName_30
	add esi, 0x1
	add ebx, 0x4
	cmp [bg_lastParsedWeaponIndex], esi
	jae BG_FindWeaponIndexForName_40
	jmp BG_FindWeaponIndexForName_10
BG_FindWeaponIndexForName_30:
	mov eax, esi
	jmp BG_FindWeaponIndexForName_50
	nop


;BG_GetHorizontalBobFactor(playerState_s const*, float, float, float)
BG_GetHorizontalBobFactor:
	push ebp
	mov ebp, esp
	sub esp, 0x48
	mov edx, [ebp+0x8]
	movss xmm0, dword [ebp+0x10]
	mov eax, [edx+0x114]
	cmp eax, 0xb
	jz BG_GetHorizontalBobFactor_10
	cmp eax, 0x28
	jz BG_GetHorizontalBobFactor_20
%ifdef OL_SERVER
	test dword [edx+0xc], 0x4000 ; C1 PMF_SPRINTING
	jz BG_GetHorizontalBobFactor_30
%else
	cmp word [edx+0xc], 0x0
	jns BG_GetHorizontalBobFactor_30
%endif
	mov eax, bg_bobAmplitudeSprinting
	mov eax, [eax]
	movaps xmm1, xmm0
	mulss xmm1, [eax+0xc]
BG_GetHorizontalBobFactor_40:
	movss xmm0, dword [ebp+0x14]
	minss xmm0, xmm1
	mov eax, [ebp+0xc]
	mov [esp], eax
	movss [ebp-0x28], xmm0
	call sinf
	fstp dword [ebp-0xc]
	movss xmm1, dword [ebp-0x28]
	mulss xmm1, [ebp-0xc]
	movss [ebp-0x2c], xmm1
	fld dword [ebp-0x2c]
	leave
	ret
BG_GetHorizontalBobFactor_30:
	mov eax, bg_bobAmplitudeStanding
	mov eax, [eax]
	movaps xmm1, xmm0
	mulss xmm1, [eax+0xc]
	jmp BG_GetHorizontalBobFactor_40
BG_GetHorizontalBobFactor_10:
	mov eax, bg_bobAmplitudeProne
	mov eax, [eax]
	movaps xmm1, xmm0
	mulss xmm1, [eax+0xc]
	jmp BG_GetHorizontalBobFactor_40
BG_GetHorizontalBobFactor_20:
	mov eax, bg_bobAmplitudeDucked
	mov eax, [eax]
	movaps xmm1, xmm0
	mulss xmm1, [eax+0xc]
	jmp BG_GetHorizontalBobFactor_40
	nop


;BG_ShutdownWeaponDefFiles()
BG_ShutdownWeaponDefFiles:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov eax, fs_gameDirVar
	mov eax, [eax]
	mov eax, [eax+0xc]
	cmp byte [eax], 0x0
	jnz BG_ShutdownWeaponDefFiles_10
BG_ShutdownWeaponDefFiles_20:
	mov dword [bg_lastParsedWeaponIndex], 0x0
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_ShutdownWeaponDefFiles_10:
	call BG_ClearSurfaceTypeSounds
	mov eax, [bg_lastParsedWeaponIndex]
	test eax, eax
	jz BG_ShutdownWeaponDefFiles_20
	mov dword [ebp-0x20], 0x1
	mov dword [ebp-0x1c], bg_weaponDefs
BG_ShutdownWeaponDefFiles_110:
	mov eax, [ebp-0x1c]
	mov edi, [eax+0x4]
	mov ebx, edi
	xor esi, esi
	jmp BG_ShutdownWeaponDefFiles_30
BG_ShutdownWeaponDefFiles_50:
	add esi, 0x1
	add ebx, 0x2
	cmp esi, 0x8
	jz BG_ShutdownWeaponDefFiles_40
BG_ShutdownWeaponDefFiles_30:
	movzx eax, word [ebx+0xd8]
	test ax, ax
	jz BG_ShutdownWeaponDefFiles_50
	movzx eax, ax
	mov [esp], eax
	call SL_RemoveRefToString
	add esi, 0x1
	add ebx, 0x2
	cmp esi, 0x8
	jnz BG_ShutdownWeaponDefFiles_30
BG_ShutdownWeaponDefFiles_40:
	mov ebx, edi
	mov si, 0x10
	jmp BG_ShutdownWeaponDefFiles_60
BG_ShutdownWeaponDefFiles_90:
	movzx eax, word [ebx+0x108]
	test ax, ax
	jnz BG_ShutdownWeaponDefFiles_70
BG_ShutdownWeaponDefFiles_100:
	add ebx, 0x2
	sub esi, 0x1
	jz BG_ShutdownWeaponDefFiles_80
BG_ShutdownWeaponDefFiles_60:
	movzx eax, word [ebx+0xe8]
	test ax, ax
	jz BG_ShutdownWeaponDefFiles_90
	movzx eax, ax
	mov [esp], eax
	call SL_RemoveRefToString
	movzx eax, word [ebx+0x108]
	test ax, ax
	jz BG_ShutdownWeaponDefFiles_100
BG_ShutdownWeaponDefFiles_70:
	movzx eax, ax
	mov [esp], eax
	call SL_RemoveRefToString
	add ebx, 0x2
	sub esi, 0x1
	jnz BG_ShutdownWeaponDefFiles_60
BG_ShutdownWeaponDefFiles_80:
	add dword [ebp-0x20], 0x1
	add dword [ebp-0x1c], 0x4
	mov eax, [ebp-0x20]
	cmp eax, [bg_lastParsedWeaponIndex]
	jbe BG_ShutdownWeaponDefFiles_110
	mov dword [bg_lastParsedWeaponIndex], 0x0
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret


;PM_UpdateAimDownSightFlag(pmove_t*, pml_t*)
PM_UpdateAimDownSightFlag:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x3c
	mov edi, [ebp+0x8]
	mov esi, [edi]
	mov ebx, [esi+0x10]
	test bl, 0x2
	jz PM_UpdateAimDownSightFlag_10
	mov eax, [esi+0xe0]
PM_UpdateAimDownSightFlag_140:
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x20], eax
	mov edx, [esi+0xc]
	and edx, 0xffffffef
	mov [esi+0xc], edx
%ifdef OL_SERVER
	; C1 BG_PlayerCanAds 0x418140..0x418154 answers "no ADS" first of all for
	; an akimbo player off a turret (ps+0x35c == 1, eFlags & 0xc00 clear).
	; The same rule as the C1 pmove port, for commands the retained Pmove runs.
	extern WeaponStateBlocksAds
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov [esp], esi
	call WeaponStateBlocksAds
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_UpdateAimDownSightFlag_100
%endif
	cmp dword [esi+0x4], 0x8
	ja PM_UpdateAimDownSightFlag_20
	mov ecx, [esi+0x4]
	mov eax, 0x1
	shl eax, cl
	test eax, 0x1bc
	jz PM_UpdateAimDownSightFlag_30
PM_UpdateAimDownSightFlag_100:
	mov byte [ebp-0x29], 0x0
PM_UpdateAimDownSightFlag_130:
	mov eax, [edi+0x8]
	mov ebx, eax
	shr ebx, 0xb
	and ebx, 0x1
	test al, 0x2
	jz PM_UpdateAimDownSightFlag_40
	mov ecx, [ebp-0x20]
	mov ecx, [ecx+0x438]
	test ecx, ecx
	jz PM_UpdateAimDownSightFlag_50
	test ah, 0x20
	jz PM_UpdateAimDownSightFlag_50
PM_UpdateAimDownSightFlag_40:
	test ebx, ebx
	jz PM_UpdateAimDownSightFlag_60
	cmp byte [ebp-0x29], 0x0
	jnz PM_UpdateAimDownSightFlag_70
PM_UpdateAimDownSightFlag_60:
	and dl, 0x10
	jz PM_UpdateAimDownSightFlag_80
	mov dword [esp+0x8], 0x1
	mov dword [esp+0xc], 0x0
	mov dword [esp+0x4], 0x7
	mov eax, [esi+0xdc]
	mov [esp], eax
	call BG_SetConditionValue
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_UpdateAimDownSightFlag_30:
	test al, 0x2
	jnz PM_UpdateAimDownSightFlag_90
PM_UpdateAimDownSightFlag_20:
	test byte [esi+0x14], 0x4
	jz PM_UpdateAimDownSightFlag_100
	test bl, 0x2
	jz PM_UpdateAimDownSightFlag_110
	mov eax, [esi+0xe0]
PM_UpdateAimDownSightFlag_170:
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x1c], eax
	mov eax, [eax+0x4ec]
	test eax, eax
	jz PM_UpdateAimDownSightFlag_100
	mov ecx, [esi+0xec]
	lea eax, [ecx-0xf]
	cmp eax, 0x5
	jbe PM_UpdateAimDownSightFlag_100
	lea eax, [ecx-0xc]
	cmp eax, 0x2
	jbe PM_UpdateAimDownSightFlag_100
	lea eax, [ecx-0x1]
	cmp eax, 0x3
	jbe PM_UpdateAimDownSightFlag_100
	lea eax, [ecx-0x19]
	cmp eax, 0x1
	jbe PM_UpdateAimDownSightFlag_100
	test dword [esi+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz PM_UpdateAimDownSightFlag_100
	and bl, 0x20
	jnz PM_UpdateAimDownSightFlag_100
	mov ecx, [ebp-0x1c]
	mov eax, [ecx+0x3f8]
	test eax, eax
	jz PM_UpdateAimDownSightFlag_120
	mov eax, [esi+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov ebx, [esi+eax*4+0x35c]
	test ebx, ebx
	jz PM_UpdateAimDownSightFlag_100
PM_UpdateAimDownSightFlag_120:
	mov byte [ebp-0x29], 0x1
	jmp PM_UpdateAimDownSightFlag_130
PM_UpdateAimDownSightFlag_10:
	mov eax, [esi+0xe8]
	jmp PM_UpdateAimDownSightFlag_140
PM_UpdateAimDownSightFlag_80:
	mov dword [esp+0x8], 0x0
	mov dword [esp+0xc], 0x0
	mov dword [esp+0x4], 0x7
	mov eax, [esi+0xdc]
	mov [esp], eax
	call BG_SetConditionValue
	add esp, 0x3c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_UpdateAimDownSightFlag_70:
	mov ecx, edx
	test dl, 0x1
	jz PM_UpdateAimDownSightFlag_150
	mov eax, [esi+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x438]
	test eax, eax
	jz PM_UpdateAimDownSightFlag_160
	movss xmm0, dword [esi+0xf4]
	ucomiss xmm0, [_float_0_00000000]
	jbe PM_UpdateAimDownSightFlag_160
PM_UpdateAimDownSightFlag_150:
	mov eax, edx
	or eax, 0x10
	mov edx, eax
	mov [esi+0xc], eax
	jmp PM_UpdateAimDownSightFlag_60
PM_UpdateAimDownSightFlag_50:
	mov dword [esp+0x4], 0xe
	mov [esp], esi
	call PM_AddEvent
	mov edx, [esi+0xc]
	and edx, 0xffffffef
	mov [esi+0xc], edx
	mov byte [ebp-0x29], 0x0
	jmp PM_UpdateAimDownSightFlag_40
PM_UpdateAimDownSightFlag_110:
	mov eax, [esi+0xe8]
	jmp PM_UpdateAimDownSightFlag_170
PM_UpdateAimDownSightFlag_90:
	mov eax, [ebp+0xc]
	mov eax, [eax+0x34]
	test eax, eax
	jnz PM_UpdateAimDownSightFlag_100
	jmp PM_UpdateAimDownSightFlag_20
PM_UpdateAimDownSightFlag_160:
	test byte [edi+0x29], 0x8
	jnz PM_UpdateAimDownSightFlag_180
PM_UpdateAimDownSightFlag_190:
	mov edx, ecx
	or edx, 0x210
	mov [esi+0xc], edx
	jmp PM_UpdateAimDownSightFlag_60
PM_UpdateAimDownSightFlag_180:
	cmp word [edi+0x1a], 0x0
	jnz PM_UpdateAimDownSightFlag_60
	jmp PM_UpdateAimDownSightFlag_190


;PM_UpdateAimDownSightLerp(pmove_t*, pml_t*)
PM_UpdateAimDownSightLerp:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov esi, [ebp+0x8]
	mov ebx, [esi]
	test byte [ebx+0x10], 0x2
	jz PM_UpdateAimDownSightLerp_10
	mov eax, [ebx+0xe0]
PM_UpdateAimDownSightLerp_50:
	mov edx, [eax*4+bg_weaponDefs]
	mov eax, player_scopeExitOnDamage
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jz PM_UpdateAimDownSightLerp_20
	mov ecx, [ebx+0x144]
	test ecx, ecx
	jnz PM_UpdateAimDownSightLerp_30
PM_UpdateAimDownSightLerp_20:
	mov eax, [edx+0x4ec]
	test eax, eax
	jnz PM_UpdateAimDownSightLerp_40
PM_UpdateAimDownSightLerp_60:
	mov dword [ebx+0xf4], 0x0
	mov dword [ebx+0xf8], 0x0
PM_UpdateAimDownSightLerp_170:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
PM_UpdateAimDownSightLerp_10:
	mov eax, [ebx+0xe8]
	jmp PM_UpdateAimDownSightLerp_50
PM_UpdateAimDownSightLerp_40:
	test dword [ebx+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz PM_UpdateAimDownSightLerp_60
	mov eax, [edx+0x530]
	test eax, eax
	jnz PM_UpdateAimDownSightLerp_70
	cmp dword [ebx+0xec], 0x7
	jz PM_UpdateAimDownSightLerp_80
PM_UpdateAimDownSightLerp_180:
	mov eax, [edx+0x4f0]
	test eax, eax
	jnz PM_UpdateAimDownSightLerp_90
	cmp dword [ebx+0xec], 0x6
	jz PM_UpdateAimDownSightLerp_100
PM_UpdateAimDownSightLerp_90:
	test byte [ebx+0xc], 0x10
	jz PM_UpdateAimDownSightLerp_100
	mov ecx, 0x1
PM_UpdateAimDownSightLerp_190:
	mov eax, [edx+0x504]
	test eax, eax
	jz PM_UpdateAimDownSightLerp_110
	mov eax, [ebx+0x40]
	test eax, eax
	jz PM_UpdateAimDownSightLerp_110
	cmp dword [ebx+0xec], 0x5
	mov eax, 0x1
	cmovz ecx, eax
PM_UpdateAimDownSightLerp_110:
	movss xmm4, dword [_float_1_00000000]
	ucomiss xmm4, [ebx+0xf4]
	jnz PM_UpdateAimDownSightLerp_120
	jp PM_UpdateAimDownSightLerp_120
	test cl, cl
	jnz PM_UpdateAimDownSightLerp_120
	mov eax, player_adsExitDelay
	mov eax, [eax]
	mov eax, [eax+0xc]
	test eax, eax
	jle PM_UpdateAimDownSightLerp_120
	mov ecx, [ebx+0xf8]
	test ecx, ecx
	jnz PM_UpdateAimDownSightLerp_130
	add eax, [esi+0x4]
	mov [ebx+0xf8], eax
PM_UpdateAimDownSightLerp_230:
	cmp eax, [esi+0x4]
	jg PM_UpdateAimDownSightLerp_140
	mov dword [ebx+0xf8], 0x0
PM_UpdateAimDownSightLerp_200:
	movss xmm1, dword [ebx+0xf4]
	pxor xmm3, xmm3
	ucomiss xmm1, xmm3
	jz PM_UpdateAimDownSightLerp_150
PM_UpdateAimDownSightLerp_220:
	mov eax, [ebp+0xc]
	cvtsi2ss xmm0, dword [eax+0x28]
	mulss xmm0, [edx+0x7fc]
	subss xmm1, xmm0
	movss [ebx+0xf4], xmm1
	movaps xmm2, xmm1
	jmp PM_UpdateAimDownSightLerp_160
PM_UpdateAimDownSightLerp_30:
	mov eax, [edx+0x438]
	test eax, eax
	jz PM_UpdateAimDownSightLerp_20
	mov dword [esp+0x4], 0xe
	mov [esp], ebx
	call PM_AddEvent
	and dword [ebx+0xc], 0xffffffef
	mov dword [ebx+0xf4], 0x0
	mov dword [ebx+0xf8], 0x0
	jmp PM_UpdateAimDownSightLerp_170
PM_UpdateAimDownSightLerp_70:
	mov ecx, [ebx+0xec]
	lea eax, [ecx-0x7]
	cmp eax, 0x3
	jbe PM_UpdateAimDownSightLerp_100
	cmp ecx, 0xb
	jnz PM_UpdateAimDownSightLerp_180
PM_UpdateAimDownSightLerp_80:
	mov eax, [ebx+0x3c]
	sub eax, [edx+0x794]
	test eax, eax
	jle PM_UpdateAimDownSightLerp_180
PM_UpdateAimDownSightLerp_100:
	xor ecx, ecx
	jmp PM_UpdateAimDownSightLerp_190
PM_UpdateAimDownSightLerp_120:
	mov dword [ebx+0xf8], 0x0
	test cl, cl
	jz PM_UpdateAimDownSightLerp_200
PM_UpdateAimDownSightLerp_140:
	movss xmm1, dword [ebx+0xf4]
	ucomiss xmm1, xmm4
	jp PM_UpdateAimDownSightLerp_210
	jz PM_UpdateAimDownSightLerp_170
PM_UpdateAimDownSightLerp_210:
	mov eax, [ebp+0xc]
	cvtsi2ss xmm0, dword [eax+0x28]
	mulss xmm0, [edx+0x7f8]
	addss xmm0, xmm1
	movss [ebx+0xf4], xmm0
	pxor xmm3, xmm3
	movaps xmm2, xmm0
PM_UpdateAimDownSightLerp_160:
	movaps xmm0, xmm2
	subss xmm0, xmm4
	movaps xmm1, xmm3
	cmpss xmm1, xmm0, 0x6
	movaps xmm0, xmm2
	andps xmm0, xmm1
	andnps xmm1, xmm4
	orps xmm1, xmm0
	movaps xmm0, xmm3
	subss xmm0, xmm2
	movaps xmm2, xmm3
	cmpss xmm2, xmm0, 0x6
	movaps xmm0, xmm2
	andps xmm1, xmm2
	andnps xmm0, xmm3
	orps xmm0, xmm1
	movss [ebx+0xf4], xmm0
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
PM_UpdateAimDownSightLerp_150:
	jnp PM_UpdateAimDownSightLerp_170
	jmp PM_UpdateAimDownSightLerp_220
PM_UpdateAimDownSightLerp_130:
	mov eax, [ebx+0xf8]
	jmp PM_UpdateAimDownSightLerp_230


;BG_GetFirstEquippedOffhand(playerState_s const*, int)
BG_GetFirstEquippedOffhand:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x8
	mov esi, [bg_lastParsedWeaponIndex]
	add esi, 0x1
	cmp esi, 0x1
	jle BG_GetFirstEquippedOffhand_10
	mov edx, 0x1
	mov ebx, bg_weaponDefs
	jmp BG_GetFirstEquippedOffhand_20
BG_GetFirstEquippedOffhand_30:
	add edx, 0x1
	add ebx, 0x4
	cmp esi, edx
	jz BG_GetFirstEquippedOffhand_10
BG_GetFirstEquippedOffhand_20:
	mov eax, [ebx+0x4]
	mov ecx, [ebp+0xc]
	cmp [eax+0x144], ecx
	jnz BG_GetFirstEquippedOffhand_30
	mov edi, edx
	sar edi, 0x5
	mov [ebp-0x10], edi
	mov ecx, edx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, edi, eax, WB_OWNED
	jz BG_GetFirstEquippedOffhand_30
	mov eax, edx
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetFirstEquippedOffhand_10:
	xor edx, edx
	mov eax, edx
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;BG_GetViewmodelWeaponIndex(playerState_s const*)
BG_GetViewmodelWeaponIndex:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	test byte [eax+0x10], 0x2
	jz BG_GetViewmodelWeaponIndex_10
	mov eax, [eax+0xe0]
	pop ebp
	ret
BG_GetViewmodelWeaponIndex_10:
	mov eax, [eax+0xe8]
	pop ebp
	ret


;BG_AssertOffhandIndexOrNone(int)
BG_AssertOffhandIndexOrNone:
	push ebp
	mov ebp, esp
	pop ebp
	ret
	nop


;BG_GetFirstAvailableOffhand(playerState_s const*, int)
BG_GetFirstAvailableOffhand:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0xc
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	mov [ebp-0x18], eax
	sub eax, 0x1
	jle BG_GetFirstAvailableOffhand_10
	mov ebx, 0x1
	mov esi, bg_weaponDefs
	jmp BG_GetFirstAvailableOffhand_20
BG_GetFirstAvailableOffhand_30:
	add ebx, 0x1
	add esi, 0x4
	cmp [ebp-0x18], ebx
	jz BG_GetFirstAvailableOffhand_10
BG_GetFirstAvailableOffhand_20:
	mov edx, [esi+0x4]
	mov ecx, [ebp+0xc]
	cmp [edx+0x144], ecx
	jnz BG_GetFirstAvailableOffhand_30
	mov edi, ebx
	sar edi, 0x5
	mov [ebp-0x10], edi
	mov ecx, ebx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, edi, eax, WB_OWNED
	jz BG_GetFirstAvailableOffhand_30
	mov eax, [ecx+0x4c]
	test eax, eax
	jg BG_GetFirstAvailableOffhand_40
	mov eax, [edx+0x328]
	mov edx, [edx+0x330]
	mov eax, [ecx+eax*4+0x15c]
	add eax, [ecx+edx*4+0x35c]
	test eax, eax
	jle BG_GetFirstAvailableOffhand_30
BG_GetFirstAvailableOffhand_40:
	mov eax, ebx
	add esp, 0xc
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_GetFirstAvailableOffhand_10:
	xor ebx, ebx
	mov eax, ebx
	add esp, 0xc
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;BG_LoadPenetrationDepthTable()
BG_LoadPenetrationDepthTable:
	push ebp
	mov ebp, esp
	push ebx
	sub esp, 0x2014
	cmp byte [penetrationDepthTableLoaded], 0x0
	jnz BG_LoadPenetrationDepthTable_10
	lea eax, [ebp-0x2008]
	mov [esp+0xc], eax
	mov dword [esp+0x8], _cstring_bullet_pen_table
	mov dword [esp+0x4], _cstring_bullet_penetrati1
	mov dword [esp], _cstring_infobullet_penet
	call Com_LoadInfoString
	mov ebx, eax
	mov dword [esp+0x8], 0x1d0
	mov dword [esp+0x4], 0x0
	mov dword [esp], penetrationDepthTable
	call Com_Memset
	mov ecx, ebx
	mov edx, penetrationDepthTable+0x74
	mov eax, _cstring_small
	call BG_ParsePenetrationDepthTable
	mov ecx, ebx
	mov edx, penetrationDepthTable+0xe8
	mov eax, _cstring_medium
	call BG_ParsePenetrationDepthTable
	mov ecx, ebx
	mov edx, penetrationDepthTable+0x15c
	mov eax, _cstring_large
	call BG_ParsePenetrationDepthTable
	mov byte [penetrationDepthTableLoaded], 0x1
BG_LoadPenetrationDepthTable_10:
	add esp, 0x2014
	pop ebx
	pop ebp
	ret


;BG_PlayerHasCompatibleWeapon(playerState_s const*, unsigned int)
BG_PlayerHasCompatibleWeapon:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x4
	mov eax, [ebp+0xc]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x328]
	mov [ebp-0x10], eax
	mov edi, [bg_lastParsedWeaponIndex]
	add edi, 0x1
	cmp edi, 0x1
	jle BG_PlayerHasCompatibleWeapon_10
	mov ebx, 0x1
	mov esi, bg_weaponDefs
	jmp BG_PlayerHasCompatibleWeapon_20
BG_PlayerHasCompatibleWeapon_30:
	add ebx, 0x1
	add esi, 0x4
	cmp edi, ebx
	jz BG_PlayerHasCompatibleWeapon_10
BG_PlayerHasCompatibleWeapon_20:
	mov eax, [esi+0x4]
	mov edx, [ebp-0x10]
	cmp edx, [eax+0x328]
	jnz BG_PlayerHasCompatibleWeapon_30
	mov edx, ebx
	sar edx, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, [ebp+0x8]
	WB_TEST ecx, edx, eax, WB_OWNED
	jz BG_PlayerHasCompatibleWeapon_30
	mov eax, 0x1
	jmp BG_PlayerHasCompatibleWeapon_40
BG_PlayerHasCompatibleWeapon_10:
	xor eax, eax
BG_PlayerHasCompatibleWeapon_40:
	add esp, 0x4
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret


;BG_GetSurfacePenetrationDepth(WeaponDef const*, int)
BG_GetSurfacePenetrationDepth:
	push ebp
	mov ebp, esp
	mov ecx, [ebp+0xc]
	test ecx, ecx
	jz BG_GetSurfacePenetrationDepth_10
	mov eax, [ebp+0x8]
	mov edx, [eax+0x134]
	lea eax, [edx*8]
	sub eax, edx
	lea eax, [edx+eax*4]
	add eax, ecx
	fld dword [eax*4+penetrationDepthTable]
	pop ebp
	ret
BG_GetSurfacePenetrationDepth_10:
	fldz
	pop ebp
	ret


;BG_PlayerWeaponsFull_Primaries(playerState_s const*)
BG_PlayerWeaponsFull_Primaries:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x8
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	mov [ebp-0x14], eax
	sub eax, 0x1
	jg BG_PlayerWeaponsFull_Primaries_10
	xor edi, edi
	xor eax, eax
	cmp edi, 0x1
	setg al
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_PlayerWeaponsFull_Primaries_10:
	mov edx, 0x1
	xor edi, edi
	mov ebx, bg_weaponDefs
BG_PlayerWeaponsFull_Primaries_30:
	mov eax, [ebx+0x4]
	mov eax, [eax+0x13c]
	test eax, eax
	jnz BG_PlayerWeaponsFull_Primaries_20
	mov esi, edx
	sar esi, 0x5
	mov [ebp-0x10], esi
	mov ecx, edx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, eax
	mov eax, [ebp+0x8]
	WB_ANDREG ecx, eax, esi, WB_OWNED
	cmp ecx, 0x1
	sbb edi, 0xffffffff
BG_PlayerWeaponsFull_Primaries_20:
	add edx, 0x1
	add ebx, 0x4
	cmp [ebp-0x14], edx
	jnz BG_PlayerWeaponsFull_Primaries_30
	xor eax, eax
	cmp edi, 0x1
	setg al
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;PM_InteruptWeaponWithProneMove(playerState_s*)
PM_InteruptWeaponWithProneMove:
	push ebp
	mov ebp, esp
	mov ecx, [ebp+0x8]
	mov edx, [ecx+0xec]
	cmp edx, 0x4
	jbe PM_InteruptWeaponWithProneMove_10
	cmp edx, 0x7
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0x9
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0xb
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0xa
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0x8
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0x6
	jz PM_InteruptWeaponWithProneMove_10
	cmp edx, 0x5
	jz PM_InteruptWeaponWithProneMove_20
	lea eax, [edx-0xc]
	cmp eax, 0x2
	jbe PM_InteruptWeaponWithProneMove_20
	lea eax, [edx-0xf]
	cmp eax, 0x5
	jbe PM_InteruptWeaponWithProneMove_20
	lea eax, [edx-0x19]
	cmp eax, 0x1
	jbe PM_InteruptWeaponWithProneMove_20
	and dword [ecx+0x10], 0xfffffffd
	and dword [ecx+0xc], 0xfffffdff
	mov dword [ecx+0x3c], 0x0
	mov dword [ecx+0x40], 0x0
	mov dword [ecx+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [ecx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ecx+0x4], 0x6
%endif
	jg PM_InteruptWeaponWithProneMove_10
	mov eax, [ecx+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [ecx+0x624], eax
	mov eax, 0x1
	pop ebp
	ret
PM_InteruptWeaponWithProneMove_10:
	mov eax, 0x1
	pop ebp
	ret
PM_InteruptWeaponWithProneMove_20:
	xor eax, eax
	pop ebp
	ret
	nop


;BG_CalculateWeaponPosition_Sway(playerState_s const*, float*, float*, float*, float, int)
BG_CalculateWeaponPosition_Sway:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x5c
	mov esi, [ebp+0x8]
	mov edi, [ebp+0x10]
	movss xmm7, dword [ebp+0x18]
	mov edx, [ebp+0x1c]
	movss xmm5, dword [esi+0xf4]
	test edx, edx
	jz BG_CalculateWeaponPosition_Sway_10
	test byte [esi+0x10], 0x2
	jz BG_CalculateWeaponPosition_Sway_20
	mov eax, [esi+0xe0]
	mov eax, [eax*4+bg_weaponDefs]
	cvtsi2ss xmm0, edx
	mulss xmm0, [_float_0_00100000]
	movss [ebp-0x3c], xmm0
	mov edx, [eax+0x4ec]
	test edx, edx
	jz BG_CalculateWeaponPosition_Sway_30
BG_CalculateWeaponPosition_Sway_50:
	ucomiss xmm5, [_float_0_00000000]
	jbe BG_CalculateWeaponPosition_Sway_40
	mov ebx, [eax+0x438]
	test ebx, ebx
	jnz BG_CalculateWeaponPosition_Sway_10
BG_CalculateWeaponPosition_Sway_40:
	movss xmm0, dword [eax+0x4ac]
	movss xmm6, dword [eax+0x4c8]
	subss xmm6, xmm0
	mulss xmm6, xmm5
	addss xmm6, xmm0
	movss xmm0, dword [eax+0x4b0]
	movss xmm1, dword [eax+0x4cc]
	subss xmm1, xmm0
	mulss xmm1, xmm5
	addss xmm0, xmm1
	movss [ebp-0x40], xmm0
	movss xmm0, dword [eax+0x4b4]
	movss xmm4, dword [eax+0x4d0]
	subss xmm4, xmm0
	mulss xmm4, xmm5
	addss xmm4, xmm0
	movss xmm0, dword [eax+0x4b8]
	movss xmm3, dword [eax+0x4d4]
	subss xmm3, xmm0
	mulss xmm3, xmm5
	addss xmm3, xmm0
	movss xmm0, dword [eax+0x4bc]
	movss xmm2, dword [eax+0x4d8]
	subss xmm2, xmm0
	mulss xmm2, xmm5
	addss xmm2, xmm0
	movss xmm0, dword [eax+0x4c0]
	movss xmm1, dword [eax+0x4dc]
	subss xmm1, xmm0
	mulss xmm1, xmm5
	addss xmm1, xmm0
BG_CalculateWeaponPosition_Sway_60:
	mulss xmm4, xmm7
	movss [ebp-0x2c], xmm4
	mulss xmm3, xmm7
	movss [ebp-0x30], xmm3
	mulss xmm2, xmm7
	movss [ebp-0x34], xmm2
	mulss xmm1, xmm7
	movss [ebp-0x38], xmm1
	lea ebx, [esi+0x108]
	lea eax, [ebp-0x24]
	mov [esp+0x8], eax
	mov eax, [ebp+0xc]
	mov [esp+0x4], eax
	mov [esp], ebx
	movss [ebp-0x58], xmm6
	call AnglesSubtract
	movss xmm1, dword [ebp-0x3c]
	mulss xmm1, [_float_60_00000000]
	movss xmm0, dword [_float_1_00000000]
	divss xmm0, xmm1
	movaps xmm3, xmm0
	mulss xmm3, [ebp-0x24]
	movaps xmm5, xmm0
	mulss xmm5, [ebp-0x20]
	mulss xmm0, [ebp-0x1c]
	movss [ebp-0x1c], xmm0
	movss xmm6, dword [ebp-0x58]
	movaps xmm4, xmm6
	xorps xmm4, [_data16_80000000]
	movaps xmm0, xmm3
	subss xmm0, xmm6
	pxor xmm2, xmm2
	movaps xmm1, xmm2
	cmpss xmm1, xmm0, 0x6
	movaps xmm0, xmm3
	andps xmm0, xmm1
	andnps xmm1, xmm6
	orps xmm1, xmm0
	movaps xmm0, xmm4
	subss xmm0, xmm3
	movaps xmm3, xmm2
	cmpss xmm3, xmm0, 0x6
	movaps xmm0, xmm3
	andps xmm1, xmm3
	andnps xmm0, xmm4
	orps xmm0, xmm1
	movss [ebp-0x24], xmm0
	movaps xmm0, xmm5
	subss xmm0, xmm6
	movaps xmm1, xmm2
	cmpss xmm1, xmm0, 0x2
	movaps xmm0, xmm1
	andps xmm6, xmm1
	andnps xmm0, xmm5
	orps xmm0, xmm6
	movaps xmm1, xmm4
	subss xmm1, xmm5
	cmpss xmm2, xmm1, 0x6
	andps xmm0, xmm2
	movaps xmm6, xmm2
	andnps xmm6, xmm4
	orps xmm6, xmm0
	movss [ebp-0x20], xmm6
	movss xmm3, dword [ebp-0x38]
	mulss xmm3, [ebp-0x24]
	movss [ebp-0x38], xmm3
	movss xmm0, dword [ebp-0x3c]
	movss [esp+0xc], xmm0
	movss xmm1, dword [ebp-0x40]
	movss [esp+0x8], xmm1
	mov eax, [edi+0x4]
	mov [esp+0x4], eax
	mulss xmm6, [ebp-0x34]
	movss [esp], xmm6
	call DiffTrack
	fstp dword [edi+0x4]
	movss xmm3, dword [ebp-0x3c]
	movss [esp+0xc], xmm3
	movss xmm0, dword [ebp-0x40]
	movss [esp+0x8], xmm0
	mov eax, [edi+0x8]
	mov [esp+0x4], eax
	movss xmm1, dword [ebp-0x38]
	movss [esp], xmm1
	call DiffTrack
	fstp dword [edi+0x8]
	movss xmm0, dword [ebp-0x2c]
	mulss xmm0, [ebp-0x24]
	movss [ebp-0x24], xmm0
	movss xmm3, dword [ebp-0x30]
	mulss xmm3, [ebp-0x20]
	movss [ebp-0x20], xmm3
	movss xmm1, dword [ebp-0x3c]
	movss [esp+0xc], xmm1
	movss xmm3, dword [ebp-0x40]
	movss [esp+0x8], xmm3
	mov edx, [ebp+0x14]
	mov eax, [edx]
	mov [esp+0x4], eax
	movss [esp], xmm0
;	call Profile_Begin
	call DiffTrackAngle
;	call Profile_End
	mov eax, [ebp+0x14]
	fstp dword [eax]
	movss xmm0, dword [ebp-0x3c]
	movss [esp+0xc], xmm0
	movss xmm1, dword [ebp-0x40]
	movss [esp+0x8], xmm1
	mov edx, [ebp+0x14]
	mov eax, [edx+0x4]
	mov [esp+0x4], eax
	mov eax, [ebp-0x20]
	mov [esp], eax
	call DiffTrackAngle
	mov eax, [ebp+0x14]
	fstp dword [eax+0x4]
	mov eax, [esi+0x108]
	mov edx, [ebp+0xc]
	mov [edx], eax
	mov eax, [ebx+0x4]
	mov [edx+0x4], eax
	mov eax, [ebx+0x8]
	mov [edx+0x8], eax
BG_CalculateWeaponPosition_Sway_10:
	add esp, 0x5c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_CalculateWeaponPosition_Sway_20:
	mov eax, [esi+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	cvtsi2ss xmm0, edx
	mulss xmm0, [_float_0_00100000]
	movss [ebp-0x3c], xmm0
	mov edx, [eax+0x4ec]
	test edx, edx
	jnz BG_CalculateWeaponPosition_Sway_50
BG_CalculateWeaponPosition_Sway_30:
	movss xmm6, dword [eax+0x4ac]
	movss xmm3, dword [eax+0x4b0]
	movss [ebp-0x40], xmm3
	movss xmm4, dword [eax+0x4b4]
	movss xmm3, dword [eax+0x4b8]
	movss xmm2, dword [eax+0x4bc]
	movss xmm1, dword [eax+0x4c0]
	jmp BG_CalculateWeaponPosition_Sway_60


;BG_PlayerWeaponCountPrimaryTypes(playerState_s const*)
BG_PlayerWeaponCountPrimaryTypes:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x8
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	mov [ebp-0x14], eax
	sub eax, 0x1
	jg BG_PlayerWeaponCountPrimaryTypes_10
	xor edi, edi
	mov eax, edi
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
BG_PlayerWeaponCountPrimaryTypes_10:
	mov edx, 0x1
	xor edi, edi
	mov ebx, bg_weaponDefs
BG_PlayerWeaponCountPrimaryTypes_30:
	mov eax, [ebx+0x4]
	mov eax, [eax+0x13c]
	test eax, eax
	jnz BG_PlayerWeaponCountPrimaryTypes_20
	mov esi, edx
	sar esi, 0x5
	mov [ebp-0x10], esi
	mov ecx, edx
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	mov ecx, eax
	mov eax, [ebp+0x8]
	WB_ANDREG ecx, eax, esi, WB_OWNED
	cmp ecx, 0x1
	sbb edi, 0xffffffff
BG_PlayerWeaponCountPrimaryTypes_20:
	add edx, 0x1
	add ebx, 0x4
	cmp [ebp-0x14], edx
	jnz BG_PlayerWeaponCountPrimaryTypes_30
	mov eax, edi
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;PM_Weapon(pmove_t*, pml_t*)
PM_Weapon:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x7c
	mov eax, [ebp+0x8]
	mov [ebp-0x5c], eax
	mov esi, [ebp+0xc]
	mov edi, [eax]
%ifdef OL_SERVER
	; Script ammo/grants can occur between movement commands. Refresh concrete
	; selections before the retained machine asks the CoDOL slot consumer.
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov [esp], edi
	call WeaponStateRefreshOffhands
	mov esp, [esp+0xc]
	popad
%endif
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 PM_LASTSTAND remains weapon-active
%else
	cmp dword [edi+0x4], 0x6
%endif
	jle PM_Weapon_10
	mov dword [edi+0xe8], 0x0
PM_Weapon_20:
%ifdef OL_SERVER
	; Run the CoDOL second weapon hand for this frame once the retained
	; machine has finished, the way C1's PM_Weapon runs its per-hand machine
	; once per hand at 0x41dc36..0x41dc51. Hand 0 mirrors ps; hand 1 keeps its
	; own timers, magazine, rechamber and predictable events. Every exit of
	; PM_Weapon reaches this label; [ebp-0x5c] holds pmove_t from entry and
	; [ebp+0xc] is the pml_t parameter, which carries this frame's msec.
	extern WeaponStateUpdate
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov eax, [ebp+0xc]
	mov [esp+0x4], eax
	mov eax, [ebp-0x5c]
	mov [esp], eax
	call WeaponStateUpdate
	mov esp, esi
	popad
%endif
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
PM_Weapon_10:
	test byte [edi+0xd], 0x4
	jnz PM_Weapon_20
	test dword [edi+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz PM_Weapon_20
%ifdef OL_SERVER
	; C1 PM_Weapon advances the spin-up charge (0x4183e0 at 0x41dbf1) here,
	; past the same pm_type / PMF 0x400 / turret exits and before the hand
	; gates, so this frame's fire gate sees this frame's charge.
	extern WeaponStateSpinUpFrame
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov [esp+0x4], esi
	mov eax, [ebp-0x5c]
	mov [esp], eax
	call WeaponStateSpinUpFrame
	mov esp, [esp+0xc]
	popad
%endif
%ifndef OL_SERVER
	mov [esp+0x4], esi
	mov edx, [ebp-0x5c]
	mov [esp], edx
	call PM_UpdateAimDownSightLerp
%endif
	mov ecx, [ebp-0x5c]
	mov ebx, [ecx]
	mov edx, [ebx+0x10]
	test dl, 0x2
	jz PM_Weapon_30
	mov eax, [ebx+0xe0]
PM_Weapon_130:
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x54], eax
	mov eax, player_breath_hold_time
	mov eax, [eax]
	movss xmm1, dword [_float_1000_00000000]
	movss xmm0, dword [eax+0xc]
	mulss xmm0, xmm1
	cvttss2si ecx, xmm0
	mov eax, player_breath_gasp_time
	mov eax, [eax]
	movss xmm0, dword [eax+0xc]
	mulss xmm0, xmm1
	cvttss2si eax, xmm0
	mov [ebp-0x50], eax
%ifdef OL_SERVER
	; IW3 perk bit 0x10 / perk_extraBreath. C1's extra breath is ability 13
	; modExtraBreathTime in PM_UpdateHoldBreath 0x4188bb, implemented by
	; pmove_c1_breath.c, which also restores the breath words this
	; retained block writes. The perk bit is never set for a C1 player.
	jmp PM_Weapon_40
%else
	test byte [ebx+0x5fc], 0x10
	jz PM_Weapon_40
%endif
	mov eax, perk_extraBreath
	mov eax, [eax]
	movaps xmm0, xmm1
	mulss xmm0, [eax+0xc]
	cvttss2si eax, xmm0
	add ecx, eax
PM_Weapon_40:
	test ecx, ecx
	jle PM_Weapon_50
	movss xmm2, dword [_float_1_00000000]
	ucomiss xmm2, [ebx+0xf4]
	jnz PM_Weapon_60
	jp PM_Weapon_60
	mov eax, [ebp-0x54]
	cmp dword [eax+0x438], 0x0
	jnz PM_Weapon_70
PM_Weapon_60:
	and edx, 0xfffffffb
	mov [ebx+0x10], edx
PM_Weapon_810:
	test dl, 0x4
	jz PM_Weapon_80
	mov eax, [ebx+0x5d8]
	add eax, [esi+0x28]
	mov [ebx+0x5d8], eax
PM_Weapon_780:
	mov eax, [ebx+0x5d8]
	test eax, eax
	js PM_Weapon_90
PM_Weapon_1450:
	test dl, 0x4
	jz PM_Weapon_100
	cmp ecx, [ebx+0x5d8]
	jl PM_Weapon_110
	mov eax, player_breath_hold_lerp
	mov eax, [eax]
	mov edx, [eax+0xc]
	movss xmm1, dword [_float__1_00000000]
	jmp PM_Weapon_120
PM_Weapon_30:
	mov eax, [ebx+0xe8]
	jmp PM_Weapon_130
PM_Weapon_100:
	add ecx, [ebp-0x50]
PM_Weapon_800:
	mov eax, player_breath_gasp_lerp
	mov eax, [eax]
	mov edx, [eax+0xc]
	cvtsi2ss xmm1, dword [ebx+0x5d8]
	cvtsi2ss xmm0, ecx
	divss xmm1, xmm0
	mov eax, player_breath_gasp_scale
	mov eax, [eax]
	movss xmm0, dword [eax+0xc]
	subss xmm0, xmm2
	mulss xmm1, xmm0
	addss xmm1, xmm2
	subss xmm1, xmm2
PM_Weapon_120:
	mov eax, [esi+0x24]
	mov [esp+0xc], eax
	mov [esp+0x8], edx
	mov eax, [ebx+0x5d4]
	mov [esp+0x4], eax
	mulss xmm1, [ebx+0xf4]
	addss xmm1, xmm2
	movss [esp], xmm1
	call DiffTrack
	fstp dword [ebx+0x5d4]
PM_Weapon_1220:
	test byte [edi+0x10], 0x2
	jz PM_Weapon_140
	mov ebx, [edi+0xe0]
PM_Weapon_370:
	mov edx, [ebx*4+bg_weaponDefs]
	cmp dword [edx+0x12c], 0x1
	jz PM_Weapon_150
PM_Weapon_360:
	mov edx, [ebp-0x5c]
	mov ebx, [edx]
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x140]
	test eax, eax
	jz PM_Weapon_160
	sub eax, 0x1
	jz PM_Weapon_160
%ifdef OL_SERVER
	; C1 arms the fresh trigger-pull latch for BURST WEAPONS ONLY. 0x41ab10:
	;   0041ab23  mov edx, [edx+0x350]   fireType
	;   0041ab29  cmp edx, 2 / jl 0x41ab72
	;   0041ab2e  cmp edx, 4 / jg 0x41ab72
	;   0041ab6c  or dword [eax+0x27c], edx
	; The retained bracket above only drops 0 and 1, so a double barrel (5)
	; and a spin-up (6) latched weapFlags 0x100 that C1 would never set. EAX
	; is fireType-1 here, so C1's 2..4 is 1..3.
	cmp eax, 0x3
	ja PM_Weapon_160
%endif
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [edx+0x8], 0x1
%endif
	jnz PM_Weapon_170
PM_Weapon_160:
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov [ebp-0x44], eax
	mov eax, [ebx+0x50]
	test eax, eax
	jle PM_Weapon_180
	sub eax, [esi+0x28]
	mov edx, 0x0
	cmovs eax, edx
	mov [ebx+0x50], eax
PM_Weapon_180:
%ifdef OL_SERVER
	; C1 41ab80: per-player ability values scale phase/delay clocks.
	; The restriction timer above deliberately uses unscaled frame msec.
	extern WeaponFrameMsec
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov eax, [esi+0x28]
	mov [esp+0x4], eax
	mov [esp], ebx
	call WeaponFrameMsec
	mov esp, [esp+0xc]
	mov [esp+0x14], eax
	popad
	mov eax, [ebx+0x3c]
	jmp PM_Weapon_350
%endif
	mov edx, [ebx+0xec]
	cmp edx, 0x7
	jz PM_Weapon_190
	cmp edx, 0x9
	jz PM_Weapon_190
	cmp edx, 0xb
	jz PM_Weapon_190
	cmp edx, 0xa
	jz PM_Weapon_190
	cmp edx, 0x8
	jz PM_Weapon_190
PM_Weapon_330:
	lea eax, [edx-0x5]
	cmp eax, 0x1
	jbe PM_Weapon_200
PM_Weapon_1490:
	mov edx, [esi+0x28]
	mov eax, [ebx+0x3c]
PM_Weapon_350:
	test eax, eax
	jz PM_Weapon_210
	sub eax, edx
	mov [ebx+0x3c], eax
	test eax, eax
	jle PM_Weapon_220
PM_Weapon_210:
	mov eax, [ebx+0x40]
	test eax, eax
	jz PM_Weapon_230
	sub eax, edx
	mov [ebx+0x40], eax
	test eax, eax
	jle PM_Weapon_240
PM_Weapon_230:
	mov dword [ebp-0x58], 0x0
PM_Weapon_1520:
	mov ecx, [edi+0xe8]
	test ecx, ecx
	jz PM_Weapon_250
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x140]
	test eax, eax
	jz PM_Weapon_250
	mov edx, [edi+0xf0]
	test edx, edx
	jz PM_Weapon_250
%ifdef OL_SERVER
	; C1 0x41dc56 skips the change/offhand/melee block only while 0x41aa70
	; says the burst is still owed, and that helper answers no once the
	; magazine is empty (0x418ec0). Without this an M93 emptied mid-burst
	; stayed in the firing state forever -- EV_NOAMMO and weaponTime += 500
	; every half second -- so the out-of-ammo switch C1 had already
	; predicted never started here and the client looped put-away/raise.
	BURST_CLIP_SPENT edi
	jnz PM_Weapon_250
%endif
	cmp eax, 0x2
	jz PM_Weapon_260
	jg PM_Weapon_270
	sub eax, 0x1
	jnz PM_Weapon_280
PM_Weapon_250:
	mov edx, [ebp-0x5c]
	mov ebx, [edx]
	test byte [edx+0x2a], 0x4
	jnz PM_Weapon_290
	test byte [edx+0xa], 0x4
	jz PM_Weapon_300
	mov eax, [ebx+0x10]
	test al, 0x40
	jz PM_Weapon_310
	and eax, 0xffffffbf
	mov [ebx+0x10], eax
	mov dword [esp+0x4], 0x41
	mov [esp], ebx
	call PM_AddEvent
	mov ecx, [ebp-0x5c]
	mov ebx, [ecx]
	mov eax, ecx
	jmp PM_Weapon_320
PM_Weapon_190:
%ifdef OL_SERVER
	; IW3 perk bit 4 / perk_weapReloadMultiplier. C1 scales reload time with
	; ability 1 modReloadSpeedScale per frame (0x41abb6), which is
	; WeaponFrameMsec; C1 has no such perk bit or dvar.
	jmp PM_Weapon_330
%else
	test byte [ebx+0x5fc], 0x4
	jz PM_Weapon_330
%endif
	mov eax, perk_weapReloadMultiplier
	mov eax, [eax]
	movss xmm1, dword [eax+0xc]
	ucomiss xmm1, [_float_0_00000000]
	jnz PM_Weapon_340
	jp PM_Weapon_340
PM_Weapon_1510:
	mov edx, [ebx+0x40]
	mov eax, [ebx+0x3c]
	cmp edx, eax
	jg PM_Weapon_350
	mov edx, eax
	jmp PM_Weapon_350
PM_Weapon_140:
	mov ebx, [edi+0xe8]
	test ebx, ebx
	jz PM_Weapon_360
	jmp PM_Weapon_370
PM_Weapon_290:
	mov eax, edx
PM_Weapon_320:
	CMD_WEAPON_ISZERO eax
	jz PM_Weapon_380
	mov edx, [ebx+0xec]
	lea eax, [edx-0x5]
	cmp eax, 0x1
	jbe PM_Weapon_390
	lea eax, [edx-0xc]
	cmp eax, 0x2
	jbe PM_Weapon_390
	lea eax, [edx-0x1]
	cmp eax, 0x3
	jbe PM_Weapon_390
	lea eax, [edx-0xf]
	cmp eax, 0x5
	jbe PM_Weapon_390
	lea eax, [edx-0x19]
	cmp eax, 0x1
	jbe PM_Weapon_390
%ifdef OL_SERVER
	; C1 0x41d46d..0x41d475: no sprint phase from a dive phase (31..33).
	lea eax, [edx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_390
	; C1 0x41d421..0x41d43e: nor while the second (akimbo left) hand is
	; firing, rechambering or in a melee. Entry and exit both wait.
	extern WeaponStateSecondHandHoldsSprint
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateSecondHandHoldsSprint
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_390
%endif
%ifdef OL_SERVER
	; C1 PMF_SPRINTING is 0x4000. IW3 used the sign bit of the low word,
	; so its signed test never entered the sprint-in/loop weapon states for C1.
	test dword [ebx+0xc], 0x4000
	jz PM_Weapon_400
%else
	cmp word [ebx+0xc], 0x0
	jns PM_Weapon_400
%endif
	lea eax, [edx-0x16]
	cmp eax, 0x2
	jbe PM_Weapon_390
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov dword [ebx+0xec], 0x16
	mov eax, [eax+0x3c8]
	mov [ebx+0x3c], eax
	mov dword [ebx+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_390
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x17
	mov [ebx+0x624], eax
	mov eax, [ebp-0x5c]
	jmp PM_Weapon_380
PM_Weapon_390:
	mov eax, [ebp-0x5c]
PM_Weapon_380:
%ifdef OL_SERVER
	; C1 PM_Weapon runs its dive check 0x41d4b0 right after the sprint check
	; and before the offhand check (0x41dc91..0x41dc9e). EAX is the pmove.
	extern WeaponStateDiveCheck
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp], eax
	call WeaponStateDiveCheck
	mov esp, [esp+0xc]
	popad
%endif
	call PM_Weapon_CheckForOffHand
	mov edx, [ebp-0x5c]
	mov ebx, [edx]
	mov edx, [ebx+0xec]
	lea eax, [edx-0xc]
	cmp eax, 0x8
	jbe PM_Weapon_410
	lea eax, [edx-0x19]
	cmp eax, 0x1
	jbe PM_Weapon_410
%ifdef OL_SERVER
	; C1 PM_Weapon_CheckForChange 0x41afeb..0x41aff3: no switch in 31..33.
	lea eax, [edx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_410
%endif
	mov ecx, [ebx+0x3c]
	test ecx, ecx
	jz PM_Weapon_420
	cmp edx, 0x7
	jz PM_Weapon_420
	cmp edx, 0x9
	jz PM_Weapon_420
	cmp edx, 0xb
	jz PM_Weapon_420
	cmp edx, 0xa
	jz PM_Weapon_420
	cmp edx, 0x8
	jz PM_Weapon_420
	cmp edx, 0x6
	jz PM_Weapon_420
	lea eax, [edx-0x5]
	cmp eax, 0x1
	jbe PM_Weapon_410
	mov eax, [ebx+0x40]
	test eax, eax
	jz PM_Weapon_420
PM_Weapon_410:
	mov esi, [ebp-0x5c]
	mov [esp], esi
	call PM_Weapon_CheckForReload
	mov ebx, [esi]
	mov eax, [ebx+0xe8]
	mov ecx, [eax*4+bg_weaponDefs]
	mov edx, [ebx+0xec]
	lea eax, [edx-0xc]
	cmp eax, 0x8
	jbe PM_Weapon_430
	lea eax, [edx-0x19]
	cmp eax, 0x1
	jbe PM_Weapon_430
%ifdef OL_SERVER
	; No melee from a dive phase (native 28..30, C1 31..33).
	lea eax, [edx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_430
	; C1's melee check opens with pm_flags & 0x8000000 (0x41c07a, jne
	; 0x41c1ad): allowMelee( false ) (C1 player method 0x51b040) sets it.
	test dword [ebx+0xc], 0x8000000
	jnz PM_Weapon_430
%endif
	mov eax, [ecx+0x354]
	test eax, eax
	jz PM_Weapon_430
	mov eax, [ebp-0x58]
	test eax, eax
	jnz PM_Weapon_430
	mov eax, [ebx+0x40]
	test eax, eax
	jz PM_Weapon_440
	cmp edx, 0x7
	jz PM_Weapon_440
	cmp edx, 0x9
	jz PM_Weapon_440
	cmp edx, 0xb
	jz PM_Weapon_440
	cmp edx, 0xa
	jz PM_Weapon_440
	cmp edx, 0x8
	jnz PM_Weapon_430
PM_Weapon_440:
	mov eax, [ebp-0x5c]
	test byte [eax+0x8], 0x4
	jz PM_Weapon_430
	test byte [eax+0x28], 0x4
	jnz PM_Weapon_430
	movss xmm0, dword [ebx+0xf4]
	ucomiss xmm0, [_float_0_00000000]
	jbe PM_Weapon_450
%ifdef OL_SERVER
	; C1's melee check 0x41c123..0x41c139 refuses a melee while
	; fWeaponPosFrac > 0 on a weapon whose def+0x694 (overlayReticle) is set:
	;   0041c123  fldz / fcomp [ebx+0x348] / test ah,5 / jp 0x41c13b
	;   0041c132  cmp dword [ecx+0x694],0 / jne 0x41c1ad  (no melee)
	; The retained test reads IW3's overlayReticle at +0x438, which the
	; composed-weapon converter never fills, so every scoped weapon meleed on
	; the server while aiming. On a variable-zoom scope the melee key is the
	; zoom key (PM_UpdateVariableZoom 0x418480): C1 zoomed while the server
	; ran a melee that dropped the sight and reset the zoom. Read C1's byte
	; through the same resolver BG_CanHoldBreath's port uses.
	extern WeaponScopeOverlayC1
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov eax, [ebx+0xe8]
	mov [esp], eax
	call WeaponScopeOverlayC1
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_430
%else
	mov esi, [ecx+0x438]
	test esi, esi
	jnz PM_Weapon_430
%endif
PM_Weapon_450:
	lea eax, [edx-0x1]
	cmp eax, 0x3
	jbe PM_Weapon_430
%ifdef OL_SERVER
	; The three rules above -- no raise/drop (states 1..4), no pending weapon
	; delay outside the reload family, and none of the melee/offhand states --
	; are exactly what C1's melee check applies to every hand at 0x41c14e,
	; 0x41c171 and, through the max() of the per-hand results passed at
	; 0x41dcc3, at 0x41c104. Apply them to the second hand as well.
	extern WeaponStateSecondHandBlocksMelee
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateSecondHandBlocksMelee
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_430
%endif
%ifdef OL_SERVER
	; C1's melee check queues EV_RESET_ADS before the charge start and the
	; melee begin when the ADS key is held rather than toggled (command bit
	; 0x20000000): 0x41c185 test [cmd+8], 0x20000000 / 0x41c190 call 0x4183a0,
	; then 0x41c198 PM_MeleeChargeStart and 0x41c1a5 0x41bd30 (EV_MELEE_SWIPE).
	; Without it C1's predicted queue ran one event ahead of the server's on
	; every melee out of the sight. pm->cmd.buttons carries the C1 word.
	extern MeleeResetAdsC1
	mov eax, [ebp-0x5c]
	mov [esp], eax
	call MeleeResetAdsC1
%endif
	mov edx, [ebp-0x5c]
	mov [esp], edx
	call PM_MeleeChargeStart
	mov eax, [ebx+0xe8]
	mov edx, [eax*4+bg_weaponDefs]
%ifdef OL_SERVER
	; C1 0x41bd87 selects the charge timing pair on PMF_MELEE_CHARGE, which
	; CoDOL keeps at 0x10000 (C1 0x415c4e). Testing IW3's 0x20000 here made
	; every charged lunge run on the ordinary melee delay, so the server
	; raised EV_FIRE_MELEE on a different command than the client predicted.
	test byte [ebx+0xe], 0x1
%else
	test byte [ebx+0xe], 0x2
%endif
	jz PM_Weapon_460
	mov eax, [edx+0x70]
	test eax, eax
	jz PM_Weapon_470
	cmp byte [eax], 0x0
	jz PM_Weapon_470
	mov ecx, [edx+0x384]
	test ecx, ecx
	jle PM_Weapon_470
	mov eax, 0x1
PM_Weapon_2230:
	test eax, eax
	jz PM_Weapon_460
	mov eax, [edx+0x384]
	mov [ebx+0x3c], eax
	mov eax, [edx+0x364]
	mov [ebx+0x40], eax
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_480
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x9
	mov [ebx+0x624], eax
PM_Weapon_480:
	mov eax, 0x1
PM_Weapon_2160:
	mov edx, [edx+0x304]
	test edx, edx
	jz PM_Weapon_490
	test al, al
	jz PM_Weapon_500
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x13
	mov [esp], ebx
	call BG_AnimScriptEvent
PM_Weapon_2220:
	mov dword [ebx+0xec], 0xc
	mov dword [esp+0x4], 0x1e
	mov [esp], ebx
	call PM_AddEvent
	mov [esp], ebx
	call PM_SetProneMovementOverride
	mov ecx, [ebp-0x5c]
	mov ebx, [ecx]
PM_Weapon_430:
%ifdef OL_SERVER
	; C1 PM_Weapon calls its offhand cancel check 0x41ce70 once per frame,
	; after the melee and detonation checks and before the hand machines
	; (0x41dcd8). EBX is the playerState; [ebp-0x5c] the pmove.
	extern WeaponStateOffhandCancelCheck
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov eax, [ebp-0x5c]
	mov [esp], eax
	call WeaponStateOffhandCancelCheck
	mov esp, esi
	popad
%endif
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_510
	mov edx, [eax*4+bg_weaponDefs]
	cmp dword [edx+0x12c], 0x1
	jz PM_Weapon_520
PM_Weapon_510:
	mov edx, [ebx+0xec]
PM_Weapon_1480:
	cmp edx, 0x10
	jz PM_Weapon_530
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_540
	mov eax, [eax*4+bg_weaponDefs]
	cmp dword [eax+0x12c], 0x1
	jz PM_Weapon_550
PM_Weapon_540:
	mov ecx, [edi+0xe8]
PM_Weapon_280:
	mov esi, [ecx*4+bg_weaponDefs]
	mov edx, [edi+0xec]
	lea eax, [edx-0xf]
	cmp eax, 0x5
	jbe PM_Weapon_560
	mov ebx, [esi+0x4e8]
	test ebx, ebx
	jz PM_Weapon_560
	mov eax, ecx
	sar eax, 0x5
	lea ebx, [eax*4]
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST_B edi, ebx, eax, WB_RECHAMBER
%ifdef OL_SERVER
	; C1 41911f..41913c: the bolt latch is already cleared when its
	; shorter delay expires. Once BOTH timers reach zero, finish the
	; rechamber animation even though that inventory latch is absent.
	jz PM_Weapon_RechamberFinished
%else
	jz PM_Weapon_560
%endif
	cmp edx, 0x6
	jz PM_Weapon_570
PM_Weapon_1770:
	mov eax, [edi+0x3c]
	test eax, eax
	jz PM_Weapon_580
	lea eax, [edx-0x5]
	cmp eax, 0x1
	jbe PM_Weapon_560
	lea eax, [edx-0xc]
	cmp eax, 0x2
	jbe PM_Weapon_560
	mov eax, [edi+0x40]
	test eax, eax
	jnz PM_Weapon_560
PM_Weapon_580:
	cmp edx, 0x6
	jz PM_Weapon_590
	test edx, edx
	jnz PM_Weapon_560
	movss xmm0, dword [edi+0xf4]
	ucomiss xmm0, [_float_0_75000000]
	jbe PM_Weapon_600
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_610
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x7
	mov [edi+0x624], eax
PM_Weapon_610:
	mov dword [edi+0xec], 0x6
%ifdef OL_SERVER
	; C1 4190a4..4190c3 selects +5b0 for BOTH akimbo hands, not just
	; hand1. Keep all live registers; return the duration in saved EAX.
	extern WeaponRechamberTime
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp], edi
	call WeaponRechamberTime
	mov esp, [esp+0xc]
	mov [esp+0x1c], eax
	popad
%else
	mov eax, [esi+0x370]
%endif
	mov [edi+0x3c], eax
	mov eax, [esi+0x374]
	test eax, eax
	jz PM_Weapon_620
%ifdef OL_SERVER
	; C1 4190d3 compares the bolt delay to the SELECTED duration.
	cmp eax, [edi+0x3c]
%else
	cmp eax, [esi+0x370]
%endif
	jge PM_Weapon_620
	mov [edi+0x40], eax
PM_Weapon_1960:
	mov dword [esp+0x4], 0x1c
	mov [esp], edi
	call PM_AddEvent
PM_Weapon_560:
	mov edx, [edi+0xc]
	test dl, 0x1
	jz PM_Weapon_630
	mov esi, [ebp-0x5c]
	cmp word [esi+0x1a], 0x0
	jnz PM_Weapon_640
PM_Weapon_630:
	mov eax, [edi+0xec]
	sub eax, 0xc
	cmp eax, 0x2
	ja PM_Weapon_650
PM_Weapon_790:
	mov dword [edi+0x628], 0x437f0000
PM_Weapon_650:
	mov esi, [ebp-0x58]
	test esi, esi
	jnz PM_Weapon_660
	mov ebx, [edi+0x3c]
	test ebx, ebx
	jnz PM_Weapon_20
	mov ecx, [edi+0x40]
	test ecx, ecx
	jnz PM_Weapon_20
PM_Weapon_660:
	mov eax, [edi+0xec]
%ifdef OL_SERVER
	; One more entry than IW3's 0..26, for the offhand detonation phase, and
	; the three dolphin-dive phases 28..30 after it.
	cmp eax, 0x1e
%else
	cmp eax, 0x1a
%endif
	ja PM_Weapon_670
	jmp dword [eax*4+PM_Weapon_jumptab_0]
PM_Weapon_420:
	mov [esp], ebx
	call Mantle_IsWeaponInactive
	test al, al
	jnz PM_Weapon_680
	mov edx, [ebx+0xc]
	test dl, 0x8
	jnz PM_Weapon_680
	mov ecx, [ebx+0x10]
	test cl, cl
	jns PM_Weapon_690
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_410
PM_Weapon_1930:
	xor ecx, ecx
	xor edx, edx
	mov eax, ebx
	call PM_BeginWeaponChange
	jmp PM_Weapon_410
PM_Weapon_670:
	mov esi, [edi+0xe8]
	test esi, esi
	jz PM_Weapon_20
	mov ecx, [ebp-0x5c]
	mov esi, [ecx]
	mov eax, [esi+0xe8]
	mov [ebp-0x24], eax
	mov ecx, [eax*4+bg_weaponDefs]
	mov eax, [ebp-0x5c]
	mov eax, [eax+0x8]
	mov [ebp-0x28], eax
	mov eax, [ecx+0x12c]
	mov [ebp-0x20], eax
	sub eax, 0x1
	jz PM_Weapon_700
PM_Weapon_830:
	mov eax, 0x1
%ifdef OL_SERVER
	; C1's hand gate 0x41b240 opens hand 0 on `mask & pm->cmd.buttons`
	; (0x41b270..0x41b275) with the mask 0x41b1f0 resolves for that hand, and
	; for an akimbo weapon hand 0's mask is the second trigger 0x100000, not
	; bit 0. The C1 pmove hands PM_Weapon the filtered C1 word in
	; pm->cmd.buttons, so only the mask differs. With bit 0 here the attack
	; button fired hand 0 as well as hand 1 while the second trigger fired
	; nothing, and hand 0's pin (which asks for 0x100000) never engaged, so a
	; semi-automatic akimbo gun emptied hand 0 at its fire rate. ESI is the
	; playerState (PM_Weapon_670); only EAX changes.
	pushad
	mov edx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], edx
	mov [esp], esi
	call WeaponStateHandZeroMask
	mov esp, [esp+0xc]
	mov [esp+0x1c], eax
	popad
%endif
PM_Weapon_840:
	test [ebp-0x28], eax
	setnz bl
%ifdef OL_SERVER
	; C1 fires a non-akimbo double barrel from EITHER trigger. Its fire
	; dispatch calls 0x41bb20 at 0x41daf7, before the hand gate 0x41b240 at
	; 0x41db03, and 0x41bb20 reads:
	;   0041bb2a  cmp [esi+0x35c], 1 / je   akimbo leaves
	;   0041bb41  cmp [ecx+0x350], 5 / jne  double barrel only
	;   0041bb5d  test ecx, 0x100000 / je   the second trigger
	;   0041bb8f  xor eax, eax / call 0x41b930   fire hand 0
	; The retained gate above only ever sees the native fire bit: the wire
	; command is masked with 0xff3f in ToEngineUsercmd, so C1's raw
	; 0x100000 never reaches pm->cmd.buttons and the right trigger could not
	; open it. ESI is the playerState from PM_Weapon_670.
	extern WeaponStateSecondTriggerFires
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], esi
	call WeaponStateSecondTriggerFires
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jz PM_Weapon_NoSecondTrigger
	mov bl, 0x1
PM_Weapon_NoSecondTrigger:
%endif
	mov eax, [ecx+0x5b8]
	test eax, eax
	jz PM_Weapon_710
	cmp dword [esi+0x70], 0x7ff
	mov eax, 0x0
	cmovz ebx, eax
PM_Weapon_710:
%ifdef OL_SERVER
	; C1 hand gate 0x41b240, fire type 6 (0x41b2e0..0x41b301): the trigger
	; opens the hand only at a full spin-up charge. ESI is the playerState;
	; only BL changes.
	extern WeaponStateSpinUpWithholds
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov [esp], esi
	call WeaponStateSpinUpWithholds
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jz PM_Weapon_SpunUp
	xor bl, bl
PM_Weapon_SpunUp:
%endif
	mov eax, [ebp-0x58]
	test eax, eax
	jnz PM_Weapon_720
	mov eax, [ebp-0x24]
	test eax, eax
	jz PM_Weapon_730
	mov eax, [ecx+0x140]
	mov [ebp-0x6c], eax
	test eax, eax
	jz PM_Weapon_730
	mov eax, [esi+0xf0]
	mov [ebp-0x1c], eax
	test eax, eax
	jz PM_Weapon_730
	cmp dword [ebp-0x6c], 0x2
	jz PM_Weapon_740
	jg PM_Weapon_750
	cmp dword [ebp-0x6c], 0x1
	jz PM_Weapon_730
PM_Weapon_720:
	mov eax, 0x1
PM_Weapon_1190:
	test bl, bl
	jnz PM_Weapon_760
	test al, al
	jnz PM_Weapon_760
	cmp dword [esi+0xec], 0x5
	jz PM_Weapon_770
PM_Weapon_1980:
	mov dword [esi+0xec], 0x0
	jmp PM_Weapon_20
PM_Weapon_80:
	mov eax, [ebx+0x5d8]
	sub eax, [esi+0x28]
	mov [ebx+0x5d8], eax
	jmp PM_Weapon_780
PM_Weapon_640:
	movss xmm0, dword [_float_1_00000000]
	ucomiss xmm0, [edi+0xf4]
	jnz PM_Weapon_790
	jnp PM_Weapon_630
	jmp PM_Weapon_790
PM_Weapon_300:
	mov eax, [ebp-0x5c]
	jmp PM_Weapon_320
PM_Weapon_110:
	add ecx, [ebp-0x50]
	mov [ebx+0x5d8], ecx
	and edx, 0xfffffffb
	mov [ebx+0x10], edx
	jmp PM_Weapon_800
PM_Weapon_70:
	cmp dword [eax+0x130], 0x9
	jz PM_Weapon_60
	mov eax, [ebp-0x5c]
	test byte [eax+0x9], 0x20
	jz PM_Weapon_60
	mov eax, [ebx+0x5d8]
	test eax, eax
	jnz PM_Weapon_810
	or edx, 0x4
	mov [ebx+0x10], edx
	jmp PM_Weapon_810
PM_Weapon_170:
%ifdef OL_SERVER
	; C1 0x41ab10 takes the edge against the old command's trigger of THIS
	; hand: 0x100000 for an akimbo hand 0 (0x41ab59), bit 0 otherwise
	; (0x41ab67). EDX is the pmove and EBX the playerState here.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateHandZeroMask
	mov esp, [esp+0xc]
	; EDX is caller-clobbered; reload the pmove from pushad's slot.
	mov edx, [esp+0x14]
	test [edx+0x28], eax
	popad
%else
	test byte [edx+0x28], 0x1
%endif
	jnz PM_Weapon_160
	or dword [ebx+0x10], 0x100
	mov ebx, [edx]
	jmp PM_Weapon_160
PM_Weapon_680:
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_410
	mov ecx, 0x1
	xor edx, edx
	mov eax, ebx
	call PM_BeginWeaponChange
	jmp PM_Weapon_410
PM_Weapon_340:
	cvtsi2ss xmm0, dword [esi+0x28]
	divss xmm0, xmm1
	addss xmm0, [_float_0_50000000]
	movss [esp], xmm0
	call floorf
	fstp dword [ebp-0x60]
	cvttss2si edx, [ebp-0x60]
	mov eax, [ebx+0x3c]
	jmp PM_Weapon_350
PM_Weapon_550:
	cmp edx, 0x5
	jnz PM_Weapon_540
	mov esi, [eax+0x5b4]
	test esi, esi
	jz PM_Weapon_540
	mov ecx, [ebp-0x5c]
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [ecx+0x8], 0x1
%endif
	jnz PM_Weapon_540
	and dword [ebx+0x10], 0xfffffffd
	and dword [ebx+0xc], 0xfffffdff
	mov dword [ebx+0x3c], 0x0
	mov dword [ebx+0x40], 0x0
	mov dword [ebx+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_820
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [ebx+0x624], eax
PM_Weapon_820:
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_540
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1
	mov [ebx+0x624], eax
	mov ecx, [edi+0xe8]
	jmp PM_Weapon_280
PM_Weapon_700:
	mov ebx, [ecx+0x5a8]
	test ebx, ebx
	jz PM_Weapon_830
	mov eax, 0x80000
	jmp PM_Weapon_840
PM_Weapon_1070:
	and dword [edi+0x10], 0xfffffffd
	and dh, 0xfd
	mov [edi+0xc], edx
	mov dword [edi+0x3c], 0x0
	mov dword [edi+0x40], 0x0
%ifdef OL_SERVER
	; The path above falls through to PM_Weapon_850 without the clear below.
	jmp PM_Weapon_850
	; C1 0x41d82b -> 0x41a0c0: a finished raise (states 1 and 2 only) clears
	; the weapFlags 0x10000 an immediate switch set (0x5110c7), ending the run
	; of quick changes it forces in PM_BeginWeaponChange.
PM_Weapon_RaiseDone:
	and dword [edi+0x10], 0xfffeffff
%endif
PM_Weapon_850:
	mov dword [edi+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_20
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [edi+0x624], eax
	jmp PM_Weapon_20
PM_Weapon_2420:
	mov eax, [edi+0x3c]
	test eax, eax
	jz PM_Weapon_850
	jmp PM_Weapon_20
PM_Weapon_2410:
	mov dword [edi+0xec], 0x17
	mov dword [edi+0x3c], 0x0
	mov dword [edi+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_20
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x18
	mov [edi+0x624], eax
	jmp PM_Weapon_20
PM_Weapon_2310:
	mov ecx, [ebp-0x5c]
	mov ebx, [ecx]
	mov ecx, [ebx+0xe8]
	mov edi, [ecx*4+bg_weaponDefs]
	mov edx, [ebp-0x58]
	test edx, edx
	jnz PM_Weapon_860
	mov eax, [ebx+0x3c]
	test eax, eax
	jnz PM_Weapon_20
PM_Weapon_2120:
	mov eax, [edi+0x530]
	test eax, eax
	jz PM_Weapon_870
	mov esi, [ebp-0x5c]
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [esi+0x8], 0x1
%endif
	jz PM_Weapon_870
	mov dword [ebx+0xec], 0x8
PM_Weapon_870:
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov eax, 0xfffffffe
	rol eax, cl
	WB_AND ebx, edx, eax, WB_RECHAMBER
	mov eax, [edi+0x530]
	test eax, eax
	jz PM_Weapon_880
	cmp dword [ebx+0xec], 0x8
	jz PM_Weapon_890
	mov eax, [ebx+0xe8]
	mov edx, [eax*4+bg_weaponDefs]
	mov ecx, [edx+0x330]
	mov eax, [edx+0x328]
	mov eax, [ebx+eax*4+0x15c]
	test eax, eax
	jz PM_Weapon_890
	mov esi, [ebx+ecx*4+0x35c]
	mov ecx, [edx+0x338]
	cmp esi, ecx
	jge PM_Weapon_890
	mov eax, [edx+0x52c]
	test eax, eax
	jz PM_Weapon_900
	mov eax, [edx+0x534]
	test eax, eax
	jz PM_Weapon_910
	cmp ecx, eax
	jg PM_Weapon_920
PM_Weapon_910:
	test esi, esi
	jz PM_Weapon_900
PM_Weapon_890:
	mov esi, [edi+0x3a0]
	test esi, esi
	jz PM_Weapon_880
	mov dword [ebx+0xec], 0xb
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_930
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x10
	mov [ebx+0x624], eax
PM_Weapon_930:
	mov eax, [edi+0x3a0]
PM_Weapon_1060:
	mov [ebx+0x3c], eax
%ifdef OL_SERVER
	; C1 runs every hand's machine after hand 0's reload start; the retained
	; tail call to PM_AddEvent left PM_Weapon without reaching PM_Weapon_20,
	; so hand 1 and the published hand/shot records skipped that frame.
	mov dword [esp+0x4], 0x12
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_Weapon_20
%else
	mov dword [ebp+0xc], 0x12
	mov [ebp+0x8], ebx
%endif
PM_Weapon_1090:
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_AddEvent
PM_Weapon_2370:
	mov eax, [ebp-0x5c]
	mov ebx, [eax]
	mov [esp+0x8], ebx
	mov eax, [ebx+0xe0]
	mov [esp+0x4], eax
	mov dword [esp], 0x21
	call BG_AddPredictableEventToPlayerstate
	cmp dword [ebx+0x48], 0x7ff
	jnz PM_Weapon_940
	mov eax, [ebx+0xe0]
	mov eax, [eax*4+bg_weaponDefs]
	mov ecx, [eax+0x330]
	mov eax, [eax+0x328]
	mov edx, [ebx+ecx*4+0x35c]
	mov eax, [ebx+eax*4+0x15c]
	add eax, edx
	test eax, eax
	jz PM_Weapon_950
	mov eax, player_sustainAmmo
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jnz PM_Weapon_940
	test edx, edx
	jle PM_Weapon_960
	lea eax, [edx-0x1]
PM_Weapon_2240:
	mov [ebx+ecx*4+0x35c], eax
PM_Weapon_940:
	mov dword [ebx+0xec], 0x13
	or dword [ebx+0x10], 0x2
	jmp PM_Weapon_20
PM_Weapon_2380:
	mov eax, [ebp-0x5c]
	mov edx, [eax]
	mov eax, [edx+0xe0]
	mov ecx, [eax*4+bg_weaponDefs]
	mov eax, [ecx+0x5b4]
	test eax, eax
	jnz PM_Weapon_970
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandHold 0x41c5f0 tests the raw CoDOL buttons 0x1c000
	; in cmd and oldcmd (0x41c627/0x41c630), not the 0xc000 the 0xff3f
	; native command keeps, and ends a +0xbec weapon on the cancel button
	; 0x400000 (0x41c638). 0 throws, 1 holds, 2 already ended the offhand.
	; EDX is the playerState and ECX its offhand definition for
	; PM_Weapon_970; both survive through pushad.
	extern WeaponStateOffhandHold
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov eax, [ebp-0x5c]
	mov [esp+0x4], eax
	mov [esp], edx
	call WeaponStateOffhandHold
	mov esp, esi
	mov [esp+0x1c], eax
	popad
	test eax, eax
	jz PM_Weapon_970
	cmp eax, 0x1
	jnz PM_Weapon_20
%else
	mov esi, [ebp-0x5c]
	test word [esi+0x28], 0xc000
	jz PM_Weapon_970
	test word [esi+0x8], 0xc000
	jz PM_Weapon_970
%endif
	mov dword [edx+0x40], 0x1
	jmp PM_Weapon_20
PM_Weapon_2390:
	mov eax, [edi+0xe8]
	test eax, eax
	jnz PM_Weapon_980
	mov dword [edi+0x3c], 0x0
	mov dword [edi+0x40], 0x1
PM_Weapon_2010:
	mov dword [edi+0x4c], 0x0
	mov dword [edi+0x48], 0x7ff
	mov dword [edi+0xec], 0x14
	and dword [edi+0x10], 0xfffffffd
	and dh, 0xfd
	mov [edi+0xc], edx
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandEnd 0x41c961: EV_OFFHAND_END_NOTIFY.
	CALL_PS WeaponStateOffhandEndNotify, edi
%endif
	jmp PM_Weapon_20
PM_Weapon_2320:
	mov eax, [ebp-0x5c]
	mov ebx, [eax]
	mov ecx, [ebx+0xe8]
	mov ecx, [ecx*4+bg_weaponDefs]
	mov [ebp-0x3c], ecx
	mov eax, [ebp-0x58]
	test eax, eax
	jnz PM_Weapon_990
PM_Weapon_2130:
	mov eax, [ebx+0x3c]
	test eax, eax
	jnz PM_Weapon_20
	mov edx, [ebp-0x3c]
	mov eax, [edx+0x530]
	test eax, eax
	jz PM_Weapon_1000
	mov ecx, [ebp-0x5c]
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [ecx+0x8], 0x1
%endif
	jz PM_Weapon_1000
	mov dword [ebx+0xec], 0xa
PM_Weapon_1000:
	cmp dword [ebx+0xec], 0xa
	jz PM_Weapon_1010
	mov ecx, [ebx+0xe8]
PM_Weapon_2190:
	mov edx, [ecx*4+bg_weaponDefs]
	mov esi, [edx+0x330]
	mov eax, [edx+0x328]
	mov eax, [ebx+eax*4+0x15c]
	test eax, eax
	jz PM_Weapon_1020
	mov edi, [ebx+esi*4+0x35c]
	mov esi, [edx+0x338]
	cmp edi, esi
	jge PM_Weapon_1020
	mov eax, [edx+0x52c]
	test eax, eax
	jz PM_Weapon_900
	mov eax, [edx+0x534]
	test eax, eax
	jz PM_Weapon_1030
	cmp esi, eax
	jg PM_Weapon_1040
PM_Weapon_1030:
	test edi, edi
	jz PM_Weapon_900
PM_Weapon_1020:
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov eax, 0xfffffffe
	rol eax, cl
	WB_AND ebx, edx, eax, WB_RECHAMBER
	mov esi, [ebp-0x3c]
	mov ecx, [esi+0x3a0]
	test ecx, ecx
	jz PM_Weapon_880
	mov dword [ebx+0xec], 0xb
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_1050
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x10
	mov [ebx+0x624], eax
PM_Weapon_1050:
	mov edx, [ebp-0x3c]
	mov eax, [edx+0x3a0]
	jmp PM_Weapon_1060
PM_Weapon_2340:
	mov eax, [edi+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov ecx, [eax+0x304]
	test ecx, ecx
	jz PM_Weapon_1070
	mov dword [edi+0xec], 0xe
	mov eax, [eax+0x3b8]
	mov [edi+0x3c], eax
	mov dword [edi+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_1080
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x14
	mov [edi+0x624], eax
PM_Weapon_1080:
%ifdef OL_SERVER
	mov [esp], edi
	call PM_SetProneMovementOverride
	jmp PM_Weapon_20
%else
	mov [ebp+0x8], edi
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_SetProneMovementOverride
%endif
PM_Weapon_2350:
	mov eax, edi
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_Weapon_OffHandPrepare
PM_Weapon_2360:
	mov dword [edi+0xec], 0x12
	mov dword [edi+0x3c], 0x0
	mov dword [edi+0x40], 0x0
	or dword [edi+0x10], 0x2
	cmp dword [edi+0x48], 0x7ff
	jnz PM_Weapon_20
	mov eax, [edi+0xe0]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x3ec]
	mov [edi+0x44], eax
%ifdef OL_SERVER
	; C1 0x41c564..0x41c590: ability 39 adds whole seconds to the fuse the
	; hold just started, only when this is not a throwback.
	extern WeaponOffhandFuseBonus
	CALL_PS WeaponOffhandFuseBonus, edi
%endif
	jmp PM_Weapon_20
PM_Weapon_2400:
	mov eax, [ebp-0x58]
	test eax, eax
	jz PM_Weapon_1070
	mov eax, [edi+0xe8]
	test eax, eax
	jz PM_Weapon_1070
%ifdef OL_SERVER
	mov [esp+0x8], edi
	mov [esp+0x4], eax
	mov dword [esp], 0x3f
	call BG_AddPredictableEventToPlayerstate
	jmp PM_Weapon_20
%else
	mov dword [ebp+0xc], 0x3f
	mov [ebp+0x8], edi
	jmp PM_Weapon_1090
%endif
%ifdef OL_SERVER
; The offhand detonation phase, modelled on C1's own handler for it at
; 0x41d8b5 (jump table 0x41d998 entry 19, i.e. state 20) and on the retained
; arm directly above, which is the same shape for the main weapon:
;   0x41d8b7  runs for hand 0 only -- the retained machine is hand 0
;   0x41d8bb  when the hand produced nothing this pass, end the offhand
;             (0x41d88b calls 0x41c860, C1's offhand end)
;   0x41d8c6  otherwise queue the predicted event EV_DETONATE, which the
;             retained enum already carries as 63 and the CoDOL map already
;             translates to 102
; The third argument C1 passes to that dispatcher is result[hand] from the
; earlier PM_Weapon_Hand loop, and the two predicates are the same one, not a
; resemblance. PM_Weapon_Hand ends at 0x41af6d with: read weaponDelay; return 0
; (0x41adbd `xor eax,eax`) when it is already zero; subtract msec, clamp at
; zero, store; return 0 when the result is still non-zero; otherwise return 1
; (0x41af97). PM_Weapon_210 here does exactly that -- PM_Weapon_230 writes
; [ebp-0x58] = 0 when the delay was zero or is still running, and
; PM_Weapon_240 writes 1 after clamping it to zero -- so [ebp-0x58] is
; "weaponDelay was non-zero and reached zero this frame" in both engines.
; EDI is the playerState.
; The end sequence is the retained one from PM_Weapon_2010 with EDX loaded
; here instead of inherited, since that arm is entered with it already held.
PM_Weapon_OffHandDetonate:
	mov eax, [ebp-0x58]
	test eax, eax
	jz PM_Weapon_OffHandDetonate_10
	mov eax, [edi+0xe0]
	test eax, eax
	jz PM_Weapon_OffHandDetonate_10
	mov [esp+0x8], edi
	mov [esp+0x4], eax
	mov dword [esp], 0x3f
	call BG_AddPredictableEventToPlayerstate
	jmp PM_Weapon_20
PM_Weapon_OffHandDetonate_10:
	mov dword [edi+0x3c], 0x0
	mov dword [edi+0x40], 0x1
	mov dword [edi+0x4c], 0x0
	mov dword [edi+0x48], 0x7ff
	mov dword [edi+0xec], 0x14
	and dword [edi+0x10], 0xfffffffd
	and dword [edi+0xc], 0xfffffdff
	; C1 0x41d88b ends the offhand detonation through 0x41c860 as well.
	CALL_PS WeaponStateOffhandEndNotify, edi
	jmp PM_Weapon_20
; Dolphin-dive weapon phases. Dive-in (28) expired: C1 0x41d955 -> 0x41d350
; enters the loop. Loop (29), dispatched every frame because its timers stay
; zero: C1 0x41d967 -> 0x41d2a0 leaves once the dive slides or ends. Both
; return 1 in C1, so neither falls through to the fire path this frame. EDI is
; the playerState.
	extern WeaponStateDiveInDone
	extern WeaponStateDiveLoop
PM_Weapon_DiveIn:
	mov [esp], edi
	call WeaponStateDiveInDone
	jmp PM_Weapon_20
PM_Weapon_DiveLoop:
	mov [esp], edi
	call WeaponStateDiveLoop
	jmp PM_Weapon_20
%endif
PM_Weapon_2300:
	cmp eax, 0x4
	setz byte [ebp-0x35]
	mov eax, [ebp-0x5c]
	mov ebx, [eax]
	mov [esp], ebx
	call Mantle_IsWeaponInactive
	test al, al
	jnz PM_Weapon_1100
	test byte [ebx+0xc], 0x8
	jnz PM_Weapon_1100
	mov edx, [ebp-0x5c]
	CMD_WEAPON esi, edx, eax
	mov eax, esi
	sar eax, 0x5
	mov ecx, esi
	and ecx, 0x1f
	mov edi, 0x1
	shl edi, cl
	WB_LOAD edx, ebx, eax, WB_OWNED
	test edx, edi
	jz PM_Weapon_1100
	cmp byte [ebx+0x10], 0x0
	js PM_Weapon_1100
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	cmp eax, esi
	ja PM_Weapon_1110
PM_Weapon_1100:
	xor esi, esi
	mov edi, 0x1
	WB_LOAD edx, ebx, 0, WB_OWNED
PM_Weapon_1110:
	test edx, edi
	jnz PM_Weapon_1120
	xor esi, esi
	xor eax, eax
PM_Weapon_2020:
	mov ecx, [ebx+0xe8]
	mov [ebp-0x30], ecx
	movzx ecx, al
	mov [ebx+0xe8], ecx
	mov eax, [ecx*4+bg_weaponDefs]
	mov [ebp-0x34], eax
%ifdef OL_SERVER
	; The one place the retained machine assigns ps->weapon. IW4 0x576437 and
	; C1 0x419cdf store the new weapon and its owning primary together here;
	; resolve the CoDOL cache with the weapon actually being left ([ebp-0x30]),
	; which is the context those engines receive from their caller.
	extern WeaponStateSelectWeapon
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov eax, [ebx+0xe8]
	mov [esp+0x8], eax
	mov eax, [ebp-0x30]
	mov [esp+0x4], eax
	mov [esp], ebx
	call WeaponStateSelectWeapon
	mov esp, esi
	popad
%endif
	cmp esi, [ebp-0x30]
	jz PM_Weapon_880
	mov eax, ecx
	sar eax, 0x5
	shl eax, 0x2
	and ecx, 0x1f
	mov edx, 0x1
	shl edx, cl
	xor ecx, ecx
	WB_TEST_B ebx, eax, edx, WB_OLD
	setz cl
	WB_OR_B ebx, eax, edx, WB_OLD
%ifndef OL_SERVER
	; C1 PM_FinishWeaponChange has no pm_flags test here: the alternate raise
	; follows the finishing state alone (0x419df6). A sprint already kept
	; PM_BeginWeaponChange from starting an alternate drop (0x41985c).
	cmp word [ebx+0xc], 0x0
	js PM_Weapon_1130
%endif
	mov eax, [ebp-0x30]
	test eax, eax
	jz PM_Weapon_1130
	test esi, esi
	jz PM_Weapon_1130
%ifdef OL_SERVER
	; C1 raises with the alternate animation exactly when the change began as
	; an alternate drop: PM_Weapon passes the finishing weaponstate (0x41d80c)
	; and PM_FinishWeaponChange tests it for 5 (0x419df6). The retained drop
	; keeps states 3/4 and marks the alternate drop with animation 17, which
	; the snapshot already publishes as C1 state 5. Re-testing the old weapon's
	; altWeaponIndex missed the rifle raise after leaving a launcher. EBX is the
	; playerState; ECX (first-raise flag) and ESI survive through pushad.
	extern WeaponChangeWasAlternate
	pushad
	mov edx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], edx
	mov [esp], ebx
	call WeaponChangeWasAlternate
	mov esp, [esp+0xc]
	mov [esp+0x1c], eax
	popad
	test eax, eax
	jnz PM_Weapon_1140
%else
	mov edx, [ebp-0x30]
	mov eax, [edx*4+bg_weaponDefs]
	cmp esi, [eax+0x540]
	jz PM_Weapon_1140
%endif
PM_Weapon_1130:
	mov dword [ebp-0x2c], 0x0
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov eax, [ebx+eax*4+0x35c]
	test eax, eax
	jnz PM_Weapon_1150
	mov esi, [ebp-0x34]
	mov edi, [esi+0x3c0]
	mov esi, 0x16
PM_Weapon_2100:
	mov eax, [ebp-0x30]
	test eax, eax
	jz PM_Weapon_1160
	test ecx, ecx
	jz PM_Weapon_1170
	mov dword [esp+0x4], 0x16
	mov [esp], ebx
	call PM_AddEvent
PM_Weapon_2200:
	mov dword [esp+0xc], 0x0
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x7
	mov [esp], ebx
	call BG_AnimScriptEvent
PM_Weapon_1160:
	movss xmm0, dword [_float_255_00000000]
PM_Weapon_2270:
	cmp dword [ebp-0x2c], 0x1
	sbb eax, eax
	add eax, 0x2
	mov [ebx+0xec], eax
	mov [ebx+0x3c], edi
	movss [ebx+0x628], xmm0
	mov [esp], ebx
	call PM_SetProneMovementOverride
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_1180
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, esi
	mov [ebx+0x624], eax
PM_Weapon_1180:
	mov edx, [ebp-0x34]
	mov eax, [edx+0x128]
	mov [esp+0x8], eax
	mov dword [esp+0x4], 0x0
	mov eax, [ebx+0xdc]
	mov [esp], eax
	call BG_SetConditionBit
	mov ecx, [ebp-0x34]
	mov eax, [ecx+0x130]
	mov [esp+0x8], eax
	mov dword [esp+0x4], 0x1
	mov eax, [ebx+0xdc]
	mov [esp], eax
	call BG_SetConditionBit
	mov eax, [ebp-0x30]
	test eax, eax
	jz PM_Weapon_20
	mov edx, [ebp-0x30]
	sar edx, 0x5
	mov ecx, [ebp-0x30]
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_OWNED
	jz PM_Weapon_20
	mov esi, [ebp-0x30]
	mov edx, [esi*4+bg_weaponDefs]
	mov edi, [edx+0x500]
	test edi, edi
	jz PM_Weapon_20
	mov eax, [edx+0x330]
	mov ecx, [ebx+eax*4+0x35c]
	test ecx, ecx
	jnz PM_Weapon_20
	mov eax, [edx+0x328]
	mov eax, [ebx+eax*4+0x15c]
	test eax, eax
	jnz PM_Weapon_20
	mov eax, [edx+0x5a8]
	test eax, eax
	jnz PM_Weapon_20
	mov dword [esp+0x8], 0x0
	mov [esp+0x4], esi
	mov [esp], ebx
	call BG_TakePlayerWeapon
	jmp PM_Weapon_20
PM_Weapon_2330:
	mov dword [edi+0xec], 0xd
	mov dword [esp+0x4], 0x1f
	mov [esp], edi
	call PM_AddEvent
	jmp PM_Weapon_1080
PM_Weapon_740:
	cmp dword [ebp-0x1c], 0x1
	jbe PM_Weapon_720
PM_Weapon_730:
	xor eax, eax
	jmp PM_Weapon_1190
PM_Weapon_530:
	mov eax, [ebx+0xe0]
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x5b4]
	test eax, eax
	jz PM_Weapon_540
	mov edx, [ebp-0x5c]
	test word [edx+0x8], 0xc000
	jnz PM_Weapon_540
	mov eax, [ebx+0xe8]
	test eax, eax
	jnz PM_Weapon_1200
	mov dword [ebx+0x3c], 0x0
	mov dword [ebx+0x40], 0x1
PM_Weapon_1780:
	mov dword [ebx+0x4c], 0x0
	mov dword [ebx+0x48], 0x7ff
	mov dword [ebx+0xec], 0x14
	and dword [ebx+0x10], 0xfffffffd
	and dword [ebx+0xc], 0xfffffdff
%ifdef OL_SERVER
	; The held-pin release ends the offhand the same way (C1 0x41ce8b ->
	; 0x41c860), so it notifies as well.
	CALL_PS WeaponStateOffhandEndNotify, ebx
%endif
	mov ecx, [edi+0xe8]
	jmp PM_Weapon_280
PM_Weapon_150:
	mov eax, [edi+0x44]
	test eax, eax
	jle PM_Weapon_360
	mov edx, [edx+0x4fc]
	test edx, edx
	jz PM_Weapon_360
	sub eax, [esi+0x28]
	mov [edi+0x44], eax
	test eax, eax
	jg PM_Weapon_360
	mov dword [edi+0x44], 0xffffffff
	mov [esp+0x8], edi
	mov eax, [edi+0xe0]
	mov [esp+0x4], eax
	mov dword [esp], 0x3e
	call BG_AddPredictableEventToPlayerstate
	cmp dword [edi+0x48], 0x7ff
	jnz PM_Weapon_20
	mov eax, player_sustainAmmo
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jnz PM_Weapon_20
	mov eax, [ebx*4+bg_weaponDefs]
	mov edx, [eax+0x330]
	mov eax, [edi+edx*4+0x35c]
	test eax, eax
	jle PM_Weapon_1210
	sub eax, 0x1
PM_Weapon_2250:
	mov [edi+edx*4+0x35c], eax
	jmp PM_Weapon_20
PM_Weapon_50:
	and edx, 0xfffffffb
	mov [ebx+0x10], edx
	mov dword [ebx+0x5d4], 0x3f800000
	mov dword [ebx+0x5d8], 0x0
	jmp PM_Weapon_1220
PM_Weapon_760:
	mov eax, [ebp-0x58]
	test eax, eax
	jz PM_Weapon_1230
	mov eax, [ebp-0x24]
	test eax, eax
	jz PM_Weapon_1230
	cmp dword [ebp-0x20], 0x1
	jz PM_Weapon_1240
PM_Weapon_1230:
	and dh, 0x8
	jnz PM_Weapon_20

	mov ecx, [edi+0xe8]
	mov ebx, [ecx*4+bg_weaponDefs]
	mov eax, [ebx+0x330]
	mov esi, [edi+eax*4+0x35c]
	test esi, esi
	jle PM_Weapon_1250
	cmp dword [ebx+0x12c], 0x1
	jz PM_Weapon_1260
	mov eax, [ebx+0x35c]
	mov [edi+0x40], eax
	mov eax, [ebx+0x36c]
	mov [edi+0x3c], eax
	mov eax, [ebx+0x504]
	test eax, eax
	jz PM_Weapon_1270
%ifdef OL_SERVER
	; C1 0x41b4db..0x41b52f: delay = ftol((1 - posFrac) * (float)(1/len)),
	; the scale first divided by value10 (modADSSpeedScale) when ability 10
	; is set. EDI is the playerState, EBX the WeaponDef.
	pushad
	mov esi, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp], edi
	mov eax, [ebx+0x7f8]
	mov [esp+0x4], eax
	mov eax, [edi+0xf4]
	mov [esp+0x8], eax
	call WeaponAdsFireOnlyDelay
	mov [edi+0x40], eax
	mov esp, esi
	popad
%else
	movss xmm0, dword [_float_1_00000000]
	movaps xmm1, xmm0
	subss xmm1, [edi+0xf4]
	divss xmm0, dword [ebx+0x7f8]
	mulss xmm1, xmm0
	cvttss2si eax, xmm1
	mov [edi+0x40], eax
%endif
PM_Weapon_1270:
	mov eax, [ebx+0x4e8]
	test eax, eax
	jz PM_Weapon_1280
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_OR edi, edx, eax, WB_RECHAMBER
PM_Weapon_1280:
	cmp dword [edi+0xec], 0x5
	jz PM_Weapon_1290
	movss xmm0, dword [edi+0xf4]
	ucomiss xmm0, [_float_1_00000000]
	jb PM_Weapon_1300
	mov eax, [ebx+0x36c]
	imul eax, [ebx+0x6d8]
	add eax, [ebx+0x35c]
	mov [edi+0x50], eax
PM_Weapon_1290:
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x2
	mov [esp], edi
	call BG_AnimScriptEvent
PM_Weapon_1900:
	mov dword [edi+0xec], 0x5
	mov [esp], edi
	call PM_SetProneMovementOverride
	mov eax, [ebx+0x140]
	test eax, eax
	jz PM_Weapon_1310
	mov eax, [edi+0xf0]
	test eax, eax
	jnz PM_Weapon_1320
	and dword [edi+0x10], 0xfffffeff
PM_Weapon_1320:
	mov edx, [edi+0xf0]
	add edx, 0x1
	mov eax, 0x4
	cmp edx, 0x5
	cmovb eax, edx
	mov [edi+0xf0], eax
PM_Weapon_1310:
	mov eax, [edi+0x40]
	test eax, eax
	jnz PM_Weapon_20
%ifdef OL_SERVER
	extern WeaponLockFireEvent
	mov [esp+0x4], ebx
	mov [esp], edi
	call WeaponLockFireEvent
	test eax, eax
	jz PM_Weapon_LockAllowed
	mov [esp+0x8], edi
	mov dword [esp+0x4], 0
	mov [esp], eax
	call BG_AddPredictableEventToPlayerstate
	jmp PM_Weapon_20
PM_Weapon_LockAllowed:
%endif
	mov ecx, [edi+0xe8]
	mov eax, [ecx*4+bg_weaponDefs]
	mov esi, [eax+0x330]
	mov edx, [edi+esi*4+0x35c]
	cmp edx, 0xffffffff
	jz PM_Weapon_1330
	test dword [edi+0xb0], PLAYER_EFLAG_TURRET_MASK
	jnz PM_Weapon_1330
	mov eax, player_sustainAmmo
	mov eax, [eax]
	cmp byte [eax+0xc], 0x0
	jnz PM_Weapon_1330
%ifdef OL_SERVER
	; C1 0x41e3cf: an unlimited-ammo definition (def+0x560; minigun_zm) never
	; spends a round. ECX is still the weapon index loaded above.
	extern WeaponUnlimitedAmmo
	pushad
	mov eax, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	mov [esp+0xc], eax
	mov eax, [ecx*4+bg_weaponDefs]
	mov [esp], eax
	call WeaponUnlimitedAmmo
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_1330
%endif
	test edx, edx
	jle PM_Weapon_1340
	lea eax, [edx-0x1]
PM_Weapon_2210:
	mov [edi+esi*4+0x35c], eax
PM_Weapon_1330:
	cmp dword [ebx+0x12c], 0x1
	jz PM_Weapon_1350
PM_Weapon_1950:
	movss xmm0, dword [edi+0xf4]
	ucomiss xmm0, [_float_0_75000000]
	jbe PM_Weapon_1360
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	cmp dword [edi+eax*4+0x35c], 0x1
	sbb edx, edx
	not edx
	add edx, 0x6
PM_Weapon_1800:
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_1370
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or edx, eax
	mov [edi+0x624], edx
PM_Weapon_1370:
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov esi, [edi+eax*4+0x35c]
%ifdef OL_SERVER
	; Both arms below queue this hand's fire event. Record which hand owns it
	; and whether the shot emptied the magazine -- ESI is the post-shot count,
	; the same value the retained code is about to branch on. The record is
	; read twice: in queue order by BG_WeaponFireRecoil, which is handed an
	; event with no hand argument while C1 keeps one kick-restriction window
	; per hand at ps+0x274 + hand*0x1c, and by ring slot by the snapshot
	; producer, which needs C1's EV_SV_FIRE_WEAPON 43/44/45/46 selection.
	extern WeaponStateRecordShot
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	xor eax, eax
	test esi, esi
	setz al
	mov [esp+0x8], eax
	mov dword [esp+0x4], 0x0
	mov [esp], edi
	call WeaponStateRecordShot
	mov esp, [esp+0xc]
	popad
%endif
	test esi, esi
	jnz PM_Weapon_1380
	mov dword [esp+0x4], 0x1b
	mov [esp], edi
	call PM_AddEvent
PM_Weapon_1790:
%ifdef OL_SERVER
	; Both triggers down at once puts BOTH barrels out in the same pmove
	; frame: C1's dispatch calls 0x41bb20 (which fires hand 0 on 0x100000)
	; and then the hand gate 0x41b240 fires hand 0 again on bit 0, so
	; 0x41b930 runs twice off one command. 0x41bb6a..0x41bb80 additionally
	; sets ps+0x344 bit 0x200 when the magazine held more than one round,
	; which is the doubled kick BG_WeaponFireRecoil reads at 0x00422407.
	; The retained machine fires at most once per frame, so the second round
	; is taken here, right after the first one's event was queued. EDI is the
	; playerState.
	extern WeaponStateDoubleBarrelSecondShot
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], edi
	call WeaponStateDoubleBarrelSecondShot
	mov esp, [esp+0xc]
	popad
%endif
	mov edx, [edi+0x10]
	test dl, 0x2
	jz PM_Weapon_1390
	mov eax, [edi+0xe0]
	mov ecx, [edi+0xe8]
PM_Weapon_1810:
	mov eax, [eax*4+bg_weaponDefs]
	movss xmm2, dword [_float_1_00000000]
	ucomiss xmm2, [edi+0xf4]
	jnz PM_Weapon_1400
	jp PM_Weapon_1400
	mov ebx, [eax+0x438]
	test ebx, ebx
	jz PM_Weapon_1400
	cmp dword [eax+0x130], 0x9
	jz PM_Weapon_1400
	mov eax, player_breath_hold_time
	mov eax, [eax]
	movss xmm0, dword [_float_1000_00000000]
	movss xmm1, dword [eax+0xc]
	mulss xmm1, xmm0
	cvttss2si ebx, xmm1
	mov eax, player_breath_fire_delay
	mov eax, [eax]
	mulss xmm0, [eax+0xc]
	cvttss2si esi, xmm0
	mov eax, [edi+0x5d8]
	cmp ebx, eax
	jle PM_Weapon_1410
	lea eax, [esi+eax]
	cmp ebx, eax
	cmovge ebx, eax
	mov [edi+0x5d8], ebx
PM_Weapon_1410:
	and edx, 0xfffffffb
	mov [edi+0x10], edx
PM_Weapon_1400:
	mov eax, [ecx*4+bg_weaponDefs]
	ucomiss xmm2, [edi+0xf4]
	jp PM_Weapon_1420
	jz PM_Weapon_1430
PM_Weapon_1420:
	movss xmm1, dword [_float_255_00000000]
	movss xmm0, dword [eax+0x46c]
	mulss xmm0, xmm1
	addss xmm0, [edi+0x628]
	movss [edi+0x628], xmm0
	ucomiss xmm0, xmm1
	jbe PM_Weapon_1440
	movss [edi+0x628], xmm1
PM_Weapon_1440:
	mov edx, [ecx*4+bg_weaponDefs]
PM_Weapon_1820:
	mov eax, [edx+0x330]
	mov esi, [edi+eax*4+0x35c]
	test esi, esi
	jnz PM_Weapon_20
	mov eax, [edx+0x328]
	mov ebx, [edi+eax*4+0x15c]
	test ebx, ebx
	jnz PM_Weapon_20
	mov ecx, [edx+0x5a8]
	test ecx, ecx
	jnz PM_Weapon_20
%ifdef OL_SERVER
	; EV_NOAMMO through the frame's single exit (see PM_Weapon_1060): the
	; tail call skipped WeaponStateUpdate, so the shot record of the
	; last round was published one command late and the first snapshot
	; carried EV_FIRE_WEAPON_LASTSHOT 31 where C1 predicts 44.
	mov dword [esp+0x4], 0xb
	mov [esp], edi
	call PM_AddEvent
	jmp PM_Weapon_20
%else
	mov dword [ebp+0xc], 0xb
	mov [ebp+0x8], edi
	jmp PM_Weapon_1090
%endif
PM_Weapon_90:
	mov dword [ebx+0x5d8], 0x0
	jmp PM_Weapon_1450
PM_Weapon_520:
	mov eax, [edx+0x5a8]
	test eax, eax
	jz PM_Weapon_510
	mov ecx, [ebx+0xec]
	cmp ecx, 0x15
	jz PM_Weapon_1460
	cmp ecx, 0x7
	jz PM_Weapon_510
	cmp ecx, 0x9
	jz PM_Weapon_510
	cmp ecx, 0xb
	jz PM_Weapon_510
	cmp ecx, 0xa
	jz PM_Weapon_510
	cmp ecx, 0x8
	jz PM_Weapon_510
	lea eax, [ecx-0x5]
	cmp eax, 0x1
	jbe PM_Weapon_510
	lea eax, [ecx-0xc]
	cmp eax, 0x2
	jbe PM_Weapon_510
	lea eax, [ecx-0x1]
	cmp eax, 0x3
	jbe PM_Weapon_510
	lea eax, [ecx-0xf]
	cmp eax, 0x5
	jbe PM_Weapon_510
%ifdef OL_SERVER
	; C1 0x41ce1f..0x41ce27 excludes 16..21 from the main weapon's detonation;
	; the retained 15..20 above is that set minus the offhand detonation.
	cmp ecx, 0x1b
	jz PM_Weapon_510
	; C1 0x41ce33..0x41ce3b: nor from the dive phases 31..33.
	lea eax, [ecx-0x1c]
	cmp eax, 0x2
	jbe PM_Weapon_510
%endif
	lea eax, [ecx-0x19]
	cmp eax, 0x1
	jbe PM_Weapon_510
	mov esi, [ebp-0x5c]
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [esi+0x8], 0x1
%endif
	jz PM_Weapon_510
	mov dword [ebx+0xec], 0x15
	mov eax, [edx+0x37c]
	mov [ebx+0x3c], eax
	mov eax, [edx+0x368]
	mov [ebx+0x40], eax
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_1470
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1b
	mov [ebx+0x624], eax
	mov ebx, [esi]
	mov edx, [ebx+0xec]
	jmp PM_Weapon_1480
PM_Weapon_200:
	lea eax, [edx-0xc]
	cmp eax, 0x2
	jbe PM_Weapon_1490
%ifdef OL_SERVER
	; IW3 perk bit 8 / perk_weapRateMultiplier. C1's rate of fire is ability
	; 23 modRateOfFire scaling the fire/rechamber clock (0x41ac92), which is
	; WeaponFrameMsec; C1 has no such perk bit or dvar.
	jmp PM_Weapon_1490
%else
	test byte [ebx+0x5fc], 0x8
	jz PM_Weapon_1490
%endif
	mov eax, perk_weapRateMultiplier
	mov eax, [eax]
	movss xmm1, dword [eax+0xc]
	ucomiss xmm1, [_float_0_00000000]
	jnz PM_Weapon_1500
	jnp PM_Weapon_1510
PM_Weapon_1500:
	cvtsi2ss xmm0, dword [esi+0x28]
	divss xmm0, xmm1
	addss xmm0, [_float_0_50000000]
	movss [esp], xmm0
	call floorf
	fstp dword [ebp-0x64]
	cvttss2si edx, [ebp-0x64]
	mov eax, [ebx+0x3c]
	jmp PM_Weapon_350
PM_Weapon_310:
	or eax, 0x40
	mov [ebx+0x10], eax
	mov dword [esp+0x4], 0x40
	mov [esp], ebx
	call PM_AddEvent
	mov esi, [ebp-0x5c]
	mov ebx, [esi]
	mov eax, esi
	jmp PM_Weapon_320
PM_Weapon_240:
	mov dword [ebx+0x40], 0x0
	mov dword [ebp-0x58], 0x1
	jmp PM_Weapon_1520
PM_Weapon_220:
	mov ecx, [ebx+0xec]
	mov [ebp-0x70], ecx
	cmp ecx, 0x5
	jz PM_Weapon_1530
PM_Weapon_1830:
	mov eax, [ebx+0x10]
	shr eax, 0x8
	mov ecx, eax
	and cl, 0x1
	mov [ebp-0x49], cl
	jnz PM_Weapon_1540
	mov ecx, [ebp-0x44]
	mov eax, [ecx+0x140]
	cmp eax, 0x2
	jz PM_Weapon_1550
	jg PM_Weapon_1560
	sub eax, 0x1
	jz PM_Weapon_1570
PM_Weapon_1540:
	mov byte [ebp-0x4a], 0x0
PM_Weapon_1690:
	mov esi, [ebp-0x44]
	cmp dword [esi+0x12c], 0x1
	jz PM_Weapon_1580
PM_Weapon_1670:
	xor esi, esi
PM_Weapon_1680:
%ifdef OL_SERVER
	; C1 0x41ae7b leaves the hold for states 16..21; the retained 15..20 below
	; is that set minus the offhand detonation, so the new phase joins it.
	cmp dword [ebp-0x70], 0x1b
	jz PM_Weapon_1590
%endif
	; The hold C1 writes at 0x41aed5 is this chain, not a shortcut past it:
	; PM_Weapon_1600 tests the request and the ps/pm weapon match that C1
	; tests at 0x41ae9f and 0x41aeae, and PM_Weapon_1620 tests the magazine
	; that C1 reads through 0x40f220 before storing the weapon time. The
	; unconditional CoDOL hold that used to sit here skipped all three and is
	; withdrawn; its weapon term now feeds PM_Weapon_1580 below, which is
	; where the retained code builds the same term.
	mov eax, [ebp-0x70]
	sub eax, 0xf
	cmp eax, 0x5
	jbe PM_Weapon_1590
	cmp byte [ebp-0x4a], 0x0
	jnz PM_Weapon_1600
	mov eax, esi
	test al, al
	jz PM_Weapon_1590
PM_Weapon_1600:
	mov ecx, [ebp-0x5c]
%ifdef OL_SERVER
	; The pin's own request term, not the plain hand request. C1 builds the
	; two separately: 0x41ae32 keeps `buttons & mask(0x41b1f0)` in EDI, then
	; 0x41ae73 `and ecx, 0x100000 / or eax, ecx` adds the second trigger to
	; EAX for a non-akimbo double barrel, and it is EAX that 0x41ae9f gates
	; the pin on while EDI is what 0x41af3d tests before clearing the shot
	; counter. PM_Weapon_1590 below is the EDI site and stays plain; this is
	; the EAX site, so the right trigger pins the weapon it just fired
	; instead of leaving the gate open every frame it is held.
	extern WeaponStatePinRequest
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStatePinRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [ecx+0x8], 0x1
%endif
	jz PM_Weapon_1610
	mov esi, [ebx+0xe8]
	CMD_WEAPON eax, ecx, edx
	cmp esi, eax
	jz PM_Weapon_1620
PM_Weapon_1700:
	cmp byte [ebp-0x49], 0x0
	jz PM_Weapon_1630
PM_Weapon_1610:
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_1640
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x140]
	test eax, eax
	jz PM_Weapon_1640
	mov ecx, [ebx+0xf0]
	test ecx, ecx
	jz PM_Weapon_1640
%ifdef OL_SERVER
	; C1 0x41af4d..0x41af59: the shot counter clears once 0x41aa70 reports
	; nothing owed, an empty magazine included.
	BURST_CLIP_SPENT ebx
	jnz PM_Weapon_1640
%endif
	cmp eax, 0x2
	jz PM_Weapon_1650
	jg PM_Weapon_1660
	sub eax, 0x1
	jz PM_Weapon_1640
PM_Weapon_1630:
	mov dword [ebx+0x3c], 0x0
	jmp PM_Weapon_210
PM_Weapon_1580:
	mov eax, [esi+0x5b4]
	test eax, eax
	jnz PM_Weapon_1580_set
%ifdef OL_SERVER
	; C1 builds this same term at 0x41adf9..0x41ae15 -- weapon type grenade
	; and a serialized definition byte -- but from WeaponDef +0xbe4, which is
	; not the retained +0x5b4 read above: in the shipped zones +0xbe4 is
	; carried by the briefcase-bomb plant weapons. Take either, so a weapon
	; that only the CoDOL asset marks still reaches the retained hold.
	; EBX is the playerState; ESI is this weapon's definition and is restored.
	extern WeaponStateWeaponHoldsFire
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateWeaponHoldsFire
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_1580_set
%endif
	jmp PM_Weapon_1670
PM_Weapon_1580_set:
	mov esi, 0x1
	jmp PM_Weapon_1680
PM_Weapon_1570:
	mov eax, [ebx+0xf0]
	test eax, eax
	jz PM_Weapon_1540
	mov byte [ebp-0x4a], 0x1
	jmp PM_Weapon_1690
PM_Weapon_1590:
	mov esi, [ebp-0x5c]
%ifdef OL_SERVER
	; The same resolved hand-0 request as above rather than raw bit 0.
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateFireRequest
	mov esp, [esp+0xc]
	test eax, eax
	popad
%else
	test byte [esi+0x8], 0x1
%endif
	jnz PM_Weapon_1700
	jmp PM_Weapon_1610
PM_Weapon_1650:
	cmp ecx, 0x1
	jbe PM_Weapon_1630
PM_Weapon_1640:
	mov dword [ebx+0xf0], 0x0
	jmp PM_Weapon_1630
PM_Weapon_270:
	cmp eax, 0x3
	jz PM_Weapon_1710
	cmp eax, 0x4
	jnz PM_Weapon_280
	cmp edx, 0x3
	jbe PM_Weapon_280
	jmp PM_Weapon_250
PM_Weapon_690:
	mov esi, [ebx+0xe8]
	mov eax, [ebp-0x5c]
	CMD_WEAPON eax, eax, edx
	mov [ebp-0x6c], eax
	mov [ebp-0x40], eax
	cmp esi, eax
	jz PM_Weapon_1720
%ifdef OL_SERVER
	; Restrict requested switches; forced mantle/global-disable transitions above
	; and completion of an existing raise/drop remain in the retained code.
	test dword [ebx+0x10], 0x800
	jnz PM_Weapon_410
	; C1 asks its burst-continuation helper 0x41aa70 for every hand at
	; 0x41dc56/0x41dc66 and skips this whole weapon-change block while either
	; one is still inside a burst. The retained machine already declines its
	; own hand through the states above; decline for the second hand too.
	extern WeaponStateSecondHandBursting
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov [esp], ebx
	call WeaponStateSecondHandBursting
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_410
%endif
	and dh, 0xc
	jz PM_Weapon_1730
	test esi, esi
	jnz PM_Weapon_1740
PM_Weapon_1730:
	cmp dword [ebp-0x6c], 0x0
	jz PM_Weapon_1750
	mov eax, [bg_lastParsedWeaponIndex]
	add eax, 0x1
	cmp [ebp-0x40], eax
	jae PM_Weapon_1760
	mov edx, [ebp-0x40]
	sar edx, 0x5
	mov ecx, [ebp-0x40]
	and ecx, 0x1f
	mov eax, 0x1
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_OWNED
	jz PM_Weapon_1760
PM_Weapon_1750:
	mov ecx, [ebx+0x5ec]
	shr ecx, 0x4
	and ecx, 0x1
	mov edx, [ebp-0x40]
	mov eax, ebx
	call PM_BeginWeaponChange
	jmp PM_Weapon_410
PM_Weapon_260:
	cmp edx, 0x1
	ja PM_Weapon_250
	jmp PM_Weapon_280
PM_Weapon_570:
	mov ecx, [ebp-0x58]
	test ecx, ecx
	jz PM_Weapon_1770
	not eax
	WB_AND_B edi, ebx, eax, WB_RECHAMBER
	mov dword [esp+0x4], 0x1d
	mov [esp], edi
	call PM_AddEvent
	mov eax, [edi+0x3c]
	test eax, eax
	jnz PM_Weapon_20
	mov edx, [edi+0xec]
	jmp PM_Weapon_580
PM_Weapon_1200:
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x3b8]
	mov [ebx+0x3c], eax
%ifdef OL_SERVER
	; The same C1 PM_Weapon_OffHandEnd 0x41c8f8 division.
	CALL_PS WeaponOffhandPhaseTime, ebx
%endif
	mov dword [ebx+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_1780
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x14
	mov [ebx+0x624], eax
	jmp PM_Weapon_1780
PM_Weapon_1380:
	mov dword [esp+0x4], 0x1a
	mov [esp], edi
	call PM_AddEvent
	jmp PM_Weapon_1790
PM_Weapon_1360:
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	cmp dword [edi+eax*4+0x35c], 0x1
	sbb edx, edx
	not edx
	add edx, 0x3
	jmp PM_Weapon_1800
PM_Weapon_1390:
	mov ecx, [edi+0xe8]
	mov eax, ecx
	jmp PM_Weapon_1810
PM_Weapon_1430:
	mov edx, eax
	jmp PM_Weapon_1820
PM_Weapon_1530:
	mov eax, [ebx+0xe8]
	test eax, eax
	jz PM_Weapon_1830
	mov eax, [eax*4+bg_weaponDefs]
	mov esi, [eax+0x140]
	lea eax, [esi-0x2]
	cmp eax, 0x2
	ja PM_Weapon_1830
	mov [ebp-0x6c], esi
	test esi, esi
	jz PM_Weapon_1840
	mov eax, [ebx+0xf0]
	mov [ebp-0x48], eax
	test eax, eax
	jz PM_Weapon_1840
%ifdef OL_SERVER
	; C1 0x41ad62..0x41adb7: a burst weapon's firing phase ends in the 200 ms
	; wait and the ready state whenever 0x41aa70 reports nothing owed, which
	; includes a magazine emptied before the burst quota was reached.
	BURST_CLIP_SPENT ebx
	jnz PM_Weapon_1840
%endif
	cmp esi, 0x2
	jz PM_Weapon_1850
	jg PM_Weapon_1860
	sub esi, 0x1
	jnz PM_Weapon_1830
PM_Weapon_1840:
	mov eax, player_burstFireCooldown
	mov eax, [eax]
	movss xmm0, dword [eax+0xc]
	ucomiss xmm0, [_float_0_00000000]
	jp PM_Weapon_1870
	jnz PM_Weapon_1870
	mov dword [ebx+0x3c], 0x1
PM_Weapon_2290:
	mov eax, [ebx+0x624]
	test eax, 0xfffffdff
	jz PM_Weapon_1880
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_1880
	and eax, 0x200
	xor ah, 0x2
	mov [ebx+0x624], eax
PM_Weapon_1880:
	mov dword [ebx+0xec], 0x0
	mov dword [ebp-0x58], 0x0
	jmp PM_Weapon_1520
PM_Weapon_1260:
	mov eax, [ebp-0x58]
	test eax, eax
	jnz PM_Weapon_1290
	mov eax, [ebx+0x3ec]
	mov [edi+0x44], eax
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_1890
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1a
	mov [edi+0x624], eax
PM_Weapon_1890:
	mov [esp+0x8], edi
	mov eax, [edi+0xe8]
	mov [esp+0x4], eax
	mov dword [esp], 0x19
	call BG_AddPredictableEventToPlayerstate
	mov eax, [ebx+0x378]
	mov [edi+0x40], eax
	mov dword [edi+0x3c], 0x0
	jmp PM_Weapon_1900
PM_Weapon_1250:
	mov eax, [ebx+0x328]
	mov edx, [edi+eax*4+0x15c]
	test edx, edx
	setg al
	movzx eax, al
	cmp dword [ebx+0x12c], 0x1
	jz PM_Weapon_1910
	test eax, eax
	jz PM_Weapon_1920
PM_Weapon_2030:
	mov eax, edi
%ifdef OL_SERVER
	call PM_BeginWeaponReload
	jmp PM_Weapon_20
%else
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_BeginWeaponReload
%endif
PM_Weapon_1760:
	test esi, esi
	jz PM_Weapon_410
PM_Weapon_1740:
	mov edx, esi
	sar edx, 0x5
	and esi, 0x1f
	mov eax, 0x1
	mov ecx, esi
	shl eax, cl
	WB_TEST ebx, edx, eax, WB_OWNED
	jnz PM_Weapon_410
	jmp PM_Weapon_1930
PM_Weapon_590:
	mov eax, [edi+0x624]
	test eax, 0xfffffdff
	jz PM_Weapon_1940
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_1940
	and eax, 0x200
	xor ah, 0x2
	mov [edi+0x624], eax
PM_Weapon_1940:
	mov dword [edi+0xec], 0x0
	jmp PM_Weapon_560
%ifdef OL_SERVER
PM_Weapon_RechamberFinished:
	cmp edx, 0x6
	jnz PM_Weapon_560
	cmp dword [edi+0x3c], 0x0
	jnz PM_Weapon_560
	cmp dword [edi+0x40], 0x0
	jnz PM_Weapon_560
	jmp PM_Weapon_590
%endif
PM_Weapon_1350:
	mov eax, [ebx+0x36c]
	mov [edi+0x3c], eax
	jmp PM_Weapon_1950
PM_Weapon_620:
	mov dword [edi+0x40], 0x1
	jmp PM_Weapon_1960
PM_Weapon_1460:
	mov edx, ecx
	jmp PM_Weapon_1480
PM_Weapon_1560:
	cmp eax, 0x3
	jz PM_Weapon_1970
	cmp eax, 0x4
	jz PM_Weapon_1560_burst4
%ifdef OL_SERVER
	; C1's quota helper 0x41a9d0 dispatches fireType-1 through the table at
	; 0x41aa4c, whose six entries are 0x41a9e9, 0x41aa04, 0x41aa1b, 0x41aa32,
	; 0x41a9e9, 0x41aa00 -- so type 5 (double barrel) shares the single-shot
	; arm and is spent at one shot, and type 6 (spin-up) takes the arm that
	; never reports spent, as does type 0 through the `ja` at 0x41a9e0.
	; The retained switch stops at 4, which left both new types in the
	; full-auto arm and a double barrel never latching.
	cmp eax, 0x5
	jnz PM_Weapon_1540
	cmp dword [ebx+0xf0], 0x1
	jb PM_Weapon_1540
	; C1 0x41ae37..0x41ae6c: a double barrel whose *other* trigger is asking
	; clears the quota flag instead of latching it, so the barrel that did
	; not take the last shot still comes out on this magazine.
	; PM_Weapon_1540 stores 0 and falls through to the same PM_Weapon_1690.
	extern WeaponStateDoubleBarrelReleasesPin
	pushad
	mov ecx, esp
	and esp, 0xfffffff0
	sub esp, 0x10
	; ECX is caller-clobbered; keep the saved stack in our unused slot.
	mov [esp+0xc], ecx
	mov dword [esp+0x4], 0x0
	mov [esp], ebx
	call WeaponStateDoubleBarrelReleasesPin
	mov esp, [esp+0xc]
	test eax, eax
	popad
	jnz PM_Weapon_1540
	mov byte [ebp-0x4a], 0x1
	jmp PM_Weapon_1690
%else
	jmp PM_Weapon_1540
%endif
PM_Weapon_1560_burst4:
	cmp dword [ebx+0xf0], 0x3
	jbe PM_Weapon_1540
	mov byte [ebp-0x4a], 0x1
	jmp PM_Weapon_1690
PM_Weapon_880:
	mov dword [ebx+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_20
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [ebx+0x624], eax
	jmp PM_Weapon_20
PM_Weapon_400:
	lea eax, [edx-0x16]
	cmp eax, 0x1
	ja PM_Weapon_390
	mov eax, [ebx+0xe8]
	mov eax, [eax*4+bg_weaponDefs]
	mov dword [ebx+0xec], 0x18
	mov eax, [eax+0x3d0]
	mov [ebx+0x3c], eax
%ifdef OL_SERVER
	; C1 0x41d178..0x41d1af (state 25): weaponTime /= value11
	; (modWeaponRecoveryTimeScale).
	CALL_PS WeaponSprintOutTime, ebx
%endif
	mov dword [ebx+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_390
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x19
	mov [ebx+0x624], eax
	mov eax, [ebp-0x5c]
	jmp PM_Weapon_380
PM_Weapon_770:
	mov eax, [esi+0x624]
	test eax, 0xfffffdff
	jz PM_Weapon_1980
%ifdef OL_SERVER
	cmp dword [esi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [esi+0x4], 0x6
%endif
	jg PM_Weapon_1980
	and eax, 0x200
	xor ah, 0x2
	mov [esi+0x624], eax
	jmp PM_Weapon_1980
PM_Weapon_1550:
	cmp dword [ebx+0xf0], 0x1
	jbe PM_Weapon_1540
	mov byte [ebp-0x4a], 0x1
	jmp PM_Weapon_1690
PM_Weapon_970:
	mov dword [edx+0xec], 0x11
	mov eax, [ecx+0x36c]
	mov [edx+0x3c], eax
	mov eax, [ecx+0x35c]
	mov [edx+0x40], eax
%ifdef OL_SERVER
	; C1 0x41c66d..0x41c6ec (state 19): both timers /= value28.
	CALL_PS WeaponOffhandThrowTimes, edx
%endif
	or dword [edx+0x10], 0x2
%ifdef OL_SERVER
	cmp dword [edx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edx+0x4], 0x6
%endif
	jg PM_Weapon_1990
	mov eax, [edx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x2
	mov [edx+0x624], eax
PM_Weapon_1990:
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x2
	mov [esp], edx
	call BG_AnimScriptEvent
	jmp PM_Weapon_20
PM_Weapon_1240:
	mov eax, [ecx+0x5b4]
	test eax, eax
	jnz PM_Weapon_1230
	cmp dword [ecx+0x5a8], 0x1
	sbb eax, eax
	and eax, 0xfff80001
	add eax, 0x80000
	test [ebp-0x28], eax
	jz PM_Weapon_1230
	mov dword [esi+0x40], 0x1
	jmp PM_Weapon_20
PM_Weapon_1710:
	cmp edx, 0x2
	ja PM_Weapon_250
	jmp PM_Weapon_280
PM_Weapon_600:
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_610
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x4
	mov [edi+0x624], eax
	jmp PM_Weapon_610
PM_Weapon_1720:
	mov eax, [ebx+0xec]
	sub eax, 0x3
	cmp eax, 0x1
	ja PM_Weapon_1760
	and ecx, 0xfffffffd
	mov [ebx+0x10], ecx
	and dh, 0xfd
	mov [ebx+0xc], edx
	mov dword [ebx+0x3c], 0x0
	mov dword [ebx+0x40], 0x0
	mov dword [ebx+0xec], 0x0
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_2000
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	mov [ebx+0x624], eax
PM_Weapon_2000:
%ifdef OL_SERVER
	; C1 0x41b18a..0x41b1b8 readies every hand and gives each animation 1.
	extern WeaponStateCancelChange
	CALL_PS WeaponStateCancelChange, ebx
%endif
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_410
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x1
	mov [ebx+0x624], eax
	jmp PM_Weapon_410
PM_Weapon_1300:
	mov eax, [ebx+0x36c]
	imul eax, [ebx+0x720]
	add eax, [ebx+0x35c]
	mov [edi+0x50], eax
	jmp PM_Weapon_1290
PM_Weapon_980:
	mov eax, [eax*4+bg_weaponDefs]
	mov eax, [eax+0x3b8]
	mov [edi+0x3c], eax
%ifdef OL_SERVER
	; C1 PM_Weapon_OffHandEnd 0x41c8e3..0x41c925 (state 21): weaponTime
	; /= value28. The no-weapon arm (C1 0x41c877) is not divided.
	CALL_PS WeaponOffhandPhaseTime, edi
%endif
	mov dword [edi+0x40], 0x0
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_2010
	mov eax, [edi+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x14
	mov [edi+0x624], eax
	jmp PM_Weapon_2010
PM_Weapon_1120:
	mov eax, esi
	jmp PM_Weapon_2020
PM_Weapon_1910:
	test eax, eax
	jnz PM_Weapon_2030
PM_Weapon_2080:
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov eax, 0xfffffffe
	rol eax, cl
	WB_AND edi, edx, eax, WB_RECHAMBER
	mov eax, [edi+0x624]
	test eax, 0xfffffdff
	jnz PM_Weapon_2040
PM_Weapon_2140:
	cmp dword [ebx+0x12c], 0x1
	jz PM_Weapon_20
	add dword [edi+0x3c], 0x1f4
	jmp PM_Weapon_20
PM_Weapon_1620:
	mov eax, [esi*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov esi, [ebx+eax*4+0x35c]
	test esi, esi
	jz PM_Weapon_1700
	mov dword [ebx+0x3c], 0x1
	cmp dword [ebp-0x70], 0x7
	jz PM_Weapon_2050
	cmp dword [ebp-0x70], 0x9
	jz PM_Weapon_2050
	cmp dword [ebp-0x70], 0xb
	jz PM_Weapon_2050
	cmp dword [ebp-0x70], 0xa
	jz PM_Weapon_2050
	cmp dword [ebp-0x70], 0x8
	jz PM_Weapon_2050
	cmp dword [ebp-0x70], 0x6
	jz PM_Weapon_2060
	mov eax, [ebp-0x70]
	sub eax, 0x5
	cmp eax, 0x1
	jbe PM_Weapon_2060
	mov eax, [ebp-0x70]
	sub eax, 0xc
	cmp eax, 0x2
	ja PM_Weapon_210
PM_Weapon_2060:
	mov eax, [ebx+0x624]
	test eax, 0xfffffdff
	jz PM_Weapon_2070
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_2070
	and eax, 0x200
	xor ah, 0x2
	mov [ebx+0x624], eax
PM_Weapon_2070:
	mov dword [ebx+0xec], 0x0
	jmp PM_Weapon_210
PM_Weapon_1920:
	mov dword [esp+0x4], 0xb
	mov [esp], edi
	call PM_AddEvent
	mov ecx, [edi+0xe8]
	jmp PM_Weapon_2080
PM_Weapon_1150:
	test ecx, ecx
	jz PM_Weapon_2090
	mov eax, [ebp-0x34]
	mov edi, [eax+0x3bc]
	mov esi, 0xc
	jmp PM_Weapon_2100
PM_Weapon_750:
%ifdef OL_SERVER
	; C1 0x41aa70 owes no burst round to a spin-up weapon (0x41aaa7); the
	; retained arms below would report one for every shot already taken,
	; firing it without the trigger or a full spin-up charge.
	cmp dword [ebp-0x6c], 0x6
	jz PM_Weapon_730
	; Nor to a double barrel: its quota arm 0x41a9e9 is spent at one shot,
	; and 0x41aa70 only asks once a shot has been taken (0x41aab5), so for
	; type 5 the answer is always "not owed". The retained arms below fell
	; through to "owed" for every type past 4. The trigger pin (0x41aed5)
	; already kept this from firing a measured extra round, but the term
	; itself now matches C1.
	cmp dword [ebp-0x6c], 0x5
	jz PM_Weapon_730
%endif
	cmp dword [ebp-0x6c], 0x3
	jz PM_Weapon_2110
	cmp dword [ebp-0x6c], 0x4
	jnz PM_Weapon_720
	cmp dword [ebp-0x1c], 0x3
	jbe PM_Weapon_720
	xor eax, eax
	jmp PM_Weapon_1190
PM_Weapon_1970:
	cmp dword [ebx+0xf0], 0x2
	jbe PM_Weapon_1540
	mov byte [ebp-0x4a], 0x1
	jmp PM_Weapon_1690
PM_Weapon_860:
	mov eax, ebx
	call PM_Weapon_ReloadDelayedAction
	mov eax, [ebx+0x3c]
	test eax, eax
	jnz PM_Weapon_20
	mov ecx, [ebx+0xe8]
	jmp PM_Weapon_2120
PM_Weapon_990:
	mov eax, ebx
	call PM_Weapon_ReloadDelayedAction
	jmp PM_Weapon_2130
PM_Weapon_2040:
%ifdef OL_SERVER
	cmp dword [edi+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [edi+0x4], 0x6
%endif
	jg PM_Weapon_2140
	and eax, 0x200
	xor ah, 0x2
	mov [edi+0x624], eax
	jmp PM_Weapon_2140
PM_Weapon_460:
	mov eax, [edx+0x380]
	mov [ebx+0x3c], eax
	mov eax, [edx+0x360]
	mov [ebx+0x40], eax
%ifdef OL_SERVER
	cmp dword [ebx+0x4], 0x7 ; C1 animates below PM_DEAD 8, last stand included
%else
	cmp dword [ebx+0x4], 0x6
%endif
	jg PM_Weapon_2150
	mov eax, [ebx+0x624]
	and eax, 0x200
	xor ah, 0x2
	or eax, 0x8
	mov [ebx+0x624], eax
PM_Weapon_2150:
	xor eax, eax
	jmp PM_Weapon_2160
PM_Weapon_1660:
	cmp eax, 0x3
	jz PM_Weapon_2170
%ifdef OL_SERVER
	; Releasing the trigger clears the hand's shot counter unless the hand is
	; still inside a burst. C1 0x41af47..0x41af59 asks the generic helper
	; 0x41aa70 and stores 0 into the counter at ps+0x280 when it says no; that
	; helper leaves on fireType 0 and 6 (0x41aaa3/0x41aaa7) and, for fireType
	; 5, reports "spent at one round" through the table at 0x41aa4c, so a
	; double barrel and a spin-up both RESET here. The retained switch only
	; knows 0..4 and dropped both into the "keep the counter" arm, which left
	; a Ranger permanently spent after its first round once PM_Weapon_1560
	; started latching fireType 5 at all.
	cmp eax, 0x5
	jae PM_Weapon_1640
%endif
	cmp eax, 0x4
	jnz PM_Weapon_1630
	cmp ecx, 0x3
	jbe PM_Weapon_1630
	jmp PM_Weapon_1640
PM_Weapon_2090:
	cmp byte [ebp-0x35], 0x0
	jz PM_Weapon_2180
	mov edx, [ebp-0x34]
	mov edi, [edx+0x3b8]
	mov esi, 0x14
	jmp PM_Weapon_2100
PM_Weapon_1010:
	mov ecx, [ebx+0xe8]
	mov eax, [ecx*4+bg_weaponDefs]
	mov eax, [eax+0x330]
	mov eax, [ebx+eax*4+0x35c]
	test eax, eax
	jnz PM_Weapon_1020
	jmp PM_Weapon_2190
PM_Weapon_1170:
	mov dword [esp+0x4], 0x15
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_Weapon_2200
PM_Weapon_1340:
	xor eax, eax
	jmp PM_Weapon_2210
PM_Weapon_2050:
	mov dword [ebx+0x3c], 0x0
	mov dword [ebx+0xf0], 0x0
	jmp PM_Weapon_210
PM_Weapon_950:
	mov dword [esp+0x4], 0xd
	mov [esp], ebx
	call PM_AddEvent
	jmp PM_Weapon_940
PM_Weapon_500:
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x12
	mov [esp], ebx
	call BG_AnimScriptEvent
	jmp PM_Weapon_2220
PM_Weapon_920:
	sub ecx, esi
	cmp eax, ecx
	jg PM_Weapon_890
PM_Weapon_900:
	mov eax, ebx
	add esp, 0x7c
	pop ebx
	pop esi
	pop edi
	pop ebp
	jmp PM_SetReloadingState
PM_Weapon_2110:
	cmp dword [ebp-0x1c], 0x2
	jbe PM_Weapon_720
	xor eax, eax
	jmp PM_Weapon_1190
PM_Weapon_470:
	xor eax, eax
	jmp PM_Weapon_2230
PM_Weapon_2180:
	mov esi, [ebp-0x34]
	mov edi, [esi+0x3a8]
	mov esi, 0xb
	jmp PM_Weapon_2100
PM_Weapon_490:
	mov dword [esp+0xc], 0x1
	mov dword [esp+0x8], 0x0
	mov dword [esp+0x4], 0x11
	mov [esp], ebx
	call BG_AnimScriptEvent
	jmp PM_Weapon_2220
PM_Weapon_2170:
	cmp ecx, 0x2
	ja PM_Weapon_1640
	jmp PM_Weapon_1630
PM_Weapon_960:
	xor eax, eax
	jmp PM_Weapon_2240
PM_Weapon_1040:
	sub esi, edi
	cmp eax, esi
	jg PM_Weapon_1020
	jmp PM_Weapon_900
PM_Weapon_1210:
	xor eax, eax
	jmp PM_Weapon_2250
PM_Weapon_1470:
	mov eax, [ebp-0x5c]
	mov ebx, [eax]
	mov edx, [ebx+0xec]
	jmp PM_Weapon_1480
PM_Weapon_1140:
	mov dword [ebp-0x2c], 0x1
	mov ecx, [ebp-0x34]
	mov edi, [ecx+0x3b0]
	movss xmm0, dword [ebx+0x628]
	ucomiss xmm0, [_float_128_00000000]
	jae PM_Weapon_2260
	jp PM_Weapon_2260
	movss xmm0, dword [_float_128_00000000]
	mov esi, 0x12
	jmp PM_Weapon_2270
PM_Weapon_1860:
	cmp esi, 0x3
	jz PM_Weapon_2280
	cmp esi, 0x4
	jnz PM_Weapon_1830
	cmp dword [ebp-0x48], 0x3
	jbe PM_Weapon_1830
	jmp PM_Weapon_1840
PM_Weapon_1870:
	mulss xmm0, [_float_1000_00000000]
	addss xmm0, [_float_0_50000000]
	movss [esp], xmm0
	call floorf
	fstp dword [ebp-0x68]
	cvttss2si eax, [ebp-0x68]
	mov [ebx+0x3c], eax
	jmp PM_Weapon_2290
PM_Weapon_1850:
	cmp dword [ebp-0x48], 0x1
	ja PM_Weapon_1840
	jmp PM_Weapon_1830
PM_Weapon_2260:
	mov esi, 0x12
	jmp PM_Weapon_2270
PM_Weapon_2280:
	cmp dword [ebp-0x48], 0x2
	ja PM_Weapon_1840
	jmp PM_Weapon_1830
	nop
	
	
PM_Weapon_jumptab_0:
	dd PM_Weapon_670
%ifdef OL_SERVER
	dd PM_Weapon_RaiseDone
	dd PM_Weapon_RaiseDone
%else
	dd PM_Weapon_850
	dd PM_Weapon_850
%endif
	dd PM_Weapon_2300
	dd PM_Weapon_2300
	dd PM_Weapon_670
	dd PM_Weapon_670
	dd PM_Weapon_2310
	dd PM_Weapon_2310
	dd PM_Weapon_2320
	dd PM_Weapon_2320
	dd PM_Weapon_850
	dd PM_Weapon_2330
	dd PM_Weapon_2340
	dd PM_Weapon_1070
	dd PM_Weapon_2350
	dd PM_Weapon_2360
	dd PM_Weapon_2370
	dd PM_Weapon_2380
	dd PM_Weapon_2390
	dd PM_Weapon_1070
	dd PM_Weapon_2400
	dd PM_Weapon_2410
	dd PM_Weapon_20
	dd PM_Weapon_1070
	dd PM_Weapon_2420
	dd PM_Weapon_2420
%ifdef OL_SERVER
	dd PM_Weapon_OffHandDetonate
	; C1 jump table 0x41d998: dive-in expiry 31 -> loop (0x41d955),
	; loop 32 -> leave on slide/end (0x41d967), dive-out 33 -> ready
	; (0x41d7f6 -> 0x41c1c0, the retained PM_Weapon_1070 below).
	dd PM_Weapon_DiveIn
	dd PM_Weapon_DiveLoop
	dd PM_Weapon_1070
%endif


;Initialized global or static variables of bg_weapons:
SECTION .data
_ZZ24PM_Weapon_CheckForReloadP7pmove_tE35MY_RELOADSTART_INTERUPT_IGNORE_FRAC: dd 0x3ecccccd, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
bg_lastParsedWeaponIndex: dd 0x0


;Initialized constant data of bg_weapons:
SECTION .rdata


;Zero initialized global or static variables of bg_weapons:
SECTION .bss
bg_numAmmoTypes: resb 0x20
bg_weapAmmoTypes: resb 0x200
bg_numSharedAmmoCaps: resb 0x20
bg_sharedAmmoCaps: resb 0x200
bg_numWeapClips: resb 0x20
bg_weapClips: resb 0x200
penetrationDepthTableLoaded: resb 0x20
; CoDOL: C1 holds 700 definition pointers (0x0150D720, memset 0xAF0); the
; retained table kept IW3's 128+1. One extra dword keeps the retained
; lastParsed+1 sentinel read in bounds. WEAPON_SLOTS states 700.
bg_weaponDefs: resb 0xaf4
penetrationDepthTable: resb 0x1e0


;All cstrings:
SECTION .rdata
_cstring_s_s:		db "%s_%s",0
_cstring_bullet_penetrati:		db 15h,"Bullet penetration table param name [%s_%s] is to long.",0ah,0
_cstring_error_parsing_bu:		db 15h,"Error parsing bullet penetration table [%s].",0ah,0
_cstring_mp:		db "mp",0
_cstring_defaultweapon_mp:		db "defaultweapon_mp",0
_cstring_bg_loadweapondef:		db 15h,"BG_LoadWeaponDef: Could not find default weapon",0
_cstring_s_s1:		db "%s: %s",0ah,0
_cstring_shared_ammo_cap_:		db 15h,"Shared ammo cap mismatch for ",22h,"%s",22h," shared ammo cap: ",27h,"%s",22h," set it to %i, but ",22h,"%s",22h," already set it to %i.",0ah,0
_cstring_s_only_grenades_:		db "[%s] Only grenades are currently supported for off hand use",0ah,0
_cstring_s_no_offhand_cla:		db "[%s] No offhand class set",0ah,0
_cstring_none:		db "none",0
_cstring_couldnt_find_wea:		db "Couldn",27h,"t find weapon ",22h,"%s",22h,0ah,0
_cstring_could_not_find_a:		db 15h,"could not find altWeapon ",27h,"%s",27h," for weapon ",27h,"%s",27h,0
_cstring_bullet_pen_table:		db "BULLET_PEN_TABLE",0
_cstring_bullet_penetrati1:		db "bullet penetration table",0
_cstring_infobullet_penet:		db "info/bullet_penetration_mp",0
_cstring_small:		db "small",0
_cstring_medium:		db "medium",0
_cstring_large:		db "large",0



;All constant floats and doubles:
SECTION .rdata
_float_0_25000000:		dd 0x3e800000	; 0.25
_float_1_00000000:		dd 0x3f800000	; 1
_float_255_00000000:		dd 0x437f0000	; 255
_double_3_14159265:		dq 0x400921fb54442d18	; 3.14159
_double_6_28318531:		dq 0x401921fb54442d18	; 6.28319
_float_0_00000000:		dd 0x0	; 0
_float_0_12500000:		dd 0x3e000000	; 0.125
_float_8_00000000:		dd 0x41000000	; 8
_float_0_01000000:		dd 0x3c23d70a	; 0.01
_float__0_50000000:		dd 0xbf000000	; -0.5
_float_40_00000000:		dd 0x42200000	; 40
_float_20_00000000:		dd 0x41a00000	; 20
_float_11_00000000:		dd 0x41300000	; 11
_float_29_00000000:		dd 0x41e80000	; 29
_float_100_00000000:		dd 0x42c80000	; 100
_float__400_00000000:		dd 0xc3c80000	; -400
_double_0_78539816:		dq 0x3fe921fb54442d18	; 0.785398
_float_0_16000000:		dd 0x3e23d70a	; 0.16
_float_10_00000000:		dd 0x41200000	; 10
_float_4_00000000:		dd 0x40800000	; 4
_double_1_57079633:		dq 0x3ff921fb54442d18	; 1.5708
_float_0_20000000:		dd 0x3e4ccccd	; 0.2
_float_0_75000000:		dd 0x3f400000	; 0.75
_double_0_47123892:		dq 0x3fde28c7460698c7	; 0.471239
_float_1_50000000:		dd 0x3fc00000	; 1.5
_float_45_00000000:		dd 0x42340000	; 45
_float_80_00000000:		dd 0x42a00000	; 80
_float_0_50000000:		dd 0x3f000000	; 0.5
_float_1000_00000000:		dd 0x447a0000	; 1000
_float_0_00070000:		dd 0x3a378034	; 0.0007
_float_0_00100000:		dd 0x3a83126f	; 0.001
_float__255_00000000:		dd 0xc37f0000	; -255
_float_0_00549316:		dd 0x3bb40000	; 0.00549316
_float_0_00277778:		dd 0x3b360b61	; 0.00277778
_float_360_00000000:		dd 0x43b40000	; 360
_float_1_27999997:		dd 0x3fa3d70a	; 1.28
_float__1_00000000:		dd 0xbf800000	; -1
_float_0_10000000:		dd 0x3dcccccd	; 0.1
_float__0_10000000:		dd 0xbdcccccd	; -0.1
_float_0_00050000:		dd 0x3a03126f	; 0.0005
_float_400_00000000:		dd 0x43c80000	; 400
_float_0_00500000:		dd 0x3ba3d70a	; 0.005
_float__2_00000000:		dd 0xc0000000	; -2
_float__0_75000000:		dd 0xbf400000	; -0.75
_float_60_00000000:		dd 0x42700000	; 60
_float_128_00000000:		dd 0x43000000	; 128

align   16,db 0
_data16_7fffffff:		dd 0x7fffffff, 0x0, 0x0, 0x0	; DQWORD
_data16_80000000:		dd 0x80000000, 0x0, 0x0, 0x0	; DQWORD
