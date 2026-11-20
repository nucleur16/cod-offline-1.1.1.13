;Imports of dobj_utils:
	extern sinf
	extern cosf
	extern XModelNumBones
	extern R_GetBaseLodDist
	extern XModelGetLodRampType
	extern R_GetAdjustedLodDist
	extern XModelGetLodForDist
	extern vec3_origin
	extern SL_ConvertToString

;Exports of dobj_utils:
	global DObjGetModel
	global DObjNumBones
	global DObjSetTrans
	global DObjClearSkel
	global DObjSetAngles
	global DObjCreateSkel
	global DObjSkelExists
	global DObjGetBoneInfo
	global DObjGetSetBones
	global DObjSetLocalTag
	global DObjGetNumModels
	global I_dmaGetDObjSkel
	global DObjGetSurfaceData
	global DObjGetHidePartBits
	global DObjGetAllocSkelSize
	global DObjGetRootBoneCount
	global DObjGetRotTransArray
	global DObjSetRotTransIndex
	global DObjSetControlTagAngles
	global DObjSetLocalTagInternal
	global DObjSetSkelRotTransIndex
	global DObjSkelAreBonesUpToDate
	global DObjCompleteHierarchyBits


SECTION .text


;DObjGetModel(DObj_s const*, int)
DObjGetModel:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov edx, [eax+0xa0]
	mov eax, [ebp+0xc]
	mov eax, [edx+eax*4]
	pop ebp
	ret
	nop


;DObjNumBones(DObj_s const*)
DObjNumBones:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	movzx eax, byte [eax+0xa]
	pop ebp
	ret
	add [eax], al


;DObjSetTrans(DObjAnimMat*, float const*)
DObjSetTrans:
	push ebp
	mov ebp, esp
	push ebx
	mov edx, [ebp+0x8]
	mov ecx, [ebp+0xc]
	mov dword [edx+0x1c], 0x0
	lea ebx, [edx+0x10]
	mov eax, [ecx]
	mov [edx+0x10], eax
	mov eax, [ecx+0x4]
	mov [ebx+0x4], eax
	mov eax, [ecx+0x8]
	mov [ebx+0x8], eax
	pop ebx
	pop ebp
	ret


;DObjClearSkel(DObj_s const*)
DObjClearSkel:
	push ebp
	mov ebp, esp
	push edi
	mov edi, [ebp+0x8]
	add edi, 0x14
	cld
	mov ecx, 0x18
	xor eax, eax
	rep stosd
	pop edi
	pop ebp
	ret
	nop


;DObjSetAngles(DObjAnimMat*, float const*)
DObjSetAngles:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x50
	mov esi, [ebp+0x8]
	mov ebx, [ebp+0xc]
	cvtss2sd xmm0, [ebx+0x4]
	mulsd xmm0, [_double_0_00872665]
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x14], xmm0
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0x18]
	movss xmm0, dword [ebp-0x14]
	movss [esp], xmm0
	call cosf
	fstp dword [ebp-0x1c]
	cvtss2sd xmm0, [ebx]
	mulsd xmm0, [_double_0_00872665]
	cvtsd2ss xmm0, xmm0
	movss [ebp-0x10], xmm0
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0x20]
	movss xmm0, dword [ebp-0x10]
	movss [esp], xmm0
	call cosf
	fstp dword [ebp-0x24]
	cvtss2sd xmm0, [ebx+0x8]
	mulsd xmm0, [_double_0_00872665]
	cvtsd2ss xmm0, xmm0
	movss [ebp-0xc], xmm0
	movss [esp], xmm0
	call sinf
	fstp dword [ebp-0x3c]
	movss xmm4, dword [ebp-0x3c]
	movss xmm0, dword [ebp-0xc]
	movss [esp], xmm0
	movss [ebp-0x38], xmm4
	call cosf
	fstp dword [ebp-0x3c]
	movss xmm3, dword [ebp-0x3c]
	movss xmm2, dword [ebp-0x20]
	xorps xmm2, [_data16_80000000]
	mulss xmm2, [ebp-0x18]
	movss xmm7, dword [ebp-0x20]
	mulss xmm7, [ebp-0x1c]
	movss xmm5, dword [ebp-0x18]
	mulss xmm5, [ebp-0x24]
	movss xmm6, dword [ebp-0x1c]
	mulss xmm6, [ebp-0x24]
	movss xmm4, dword [ebp-0x38]
	movaps xmm0, xmm4
	mulss xmm0, xmm6
	movaps xmm1, xmm3
	mulss xmm1, xmm2
	addss xmm0, xmm1
	movss [esi], xmm0
	movaps xmm0, xmm3
	mulss xmm0, xmm7
	movaps xmm1, xmm4
	mulss xmm1, xmm5
	addss xmm0, xmm1
	movss [esi+0x4], xmm0
	mulss xmm7, xmm4
	mulss xmm5, xmm3
	subss xmm5, xmm7
	movss [esi+0x8], xmm5
	mulss xmm6, xmm3
	mulss xmm2, xmm4
	subss xmm6, xmm2
	movss [esi+0xc], xmm6
	add esp, 0x50
	pop ebx
	pop esi
	pop ebp
	ret
	nop


;DObjCreateSkel(DObj_s const*, char*, int)
DObjCreateSkel:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov edx, [ebp+0xc]
	mov [eax+0x78], edx
	mov edx, [ebp+0x10]
	mov [eax+0x74], edx
	pop ebp
	ret


;DObjSkelExists(DObj_s const*, int)
DObjSkelExists:
	push ebp
	mov ebp, esp
	push edi
	mov edx, [ebp+0x8]
	mov eax, [ebp+0xc]
	cmp [edx+0x74], eax
	jz DObjSkelExists_10
	lea edi, [edx+0x14]
	cld
	mov ecx, 0x1a
	xor eax, eax
	rep stosd
	pop edi
	pop ebp
	ret
DObjSkelExists_10:
	xor eax, eax
	cmp dword [edx+0x78], 0x0
	setnz al
	pop edi
	pop ebp
	ret
	nop


;DObjGetBoneInfo(DObj_s const*, XBoneInfo**)
DObjGetBoneInfo:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x8
	mov ebx, [ebp+0xc]
	mov eax, [ebp+0x8]
	mov eax, [eax+0xa0]
	mov [ebp-0x10], eax
	mov edx, [ebp+0x8]
	cmp byte [edx+0x9], 0x0
	jz DObjGetBoneInfo_10
	mov dword [ebp-0x14], 0x0
DObjGetBoneInfo_40:
	mov eax, [ebp-0x14]
	mov edx, [ebp-0x10]
	mov edi, [edx+eax*4]
	movzx esi, byte [edi+0x4]
	test esi, esi
	jle DObjGetBoneInfo_20
	xor ecx, ecx
	xor edx, edx
DObjGetBoneInfo_30:
	mov eax, edx
	add eax, [edi+0xa4]
	mov [ebx], eax
	add ecx, 0x1
	add ebx, 0x4
	add edx, 0x28
	cmp esi, ecx
	jnz DObjGetBoneInfo_30
DObjGetBoneInfo_20:
	add dword [ebp-0x14], 0x1
	mov edx, [ebp+0x8]
	movzx eax, byte [edx+0x9]
	cmp [ebp-0x14], eax
	jl DObjGetBoneInfo_40
DObjGetBoneInfo_10:
	add esp, 0x8
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;DObjGetSetBones(DObj_s const*, int*)
DObjGetSetBones:
	push ebp
	mov ebp, esp
	mov edx, [ebp+0x8]
	mov ecx, [ebp+0xc]
	mov eax, [edx+0x14]
	mov [ecx], eax
	mov eax, [edx+0x18]
	mov [ecx+0x4], eax
	mov eax, [edx+0x1c]
	mov [ecx+0x8], eax
	mov eax, [edx+0x20]
	mov [ecx+0xc], eax
	mov eax, [edx+0x24]
	mov [ecx+0x10], eax
	mov eax, [edx+0x28]
	mov [ecx+0x14], eax
	mov eax, [edx+0x2c]
	mov [ecx+0x18], eax
	mov eax, [edx+0x30]
	mov [ecx+0x1c], eax
	pop ebp
	ret


;DObjSetLocalTag(DObj_s const*, int*, unsigned int, float const*, float const*)
DObjSetLocalTag:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov edx, [ebp+0x10]
	cmp edx, 0xfd
	ja DObjSetLocalTag_10
	mov esi, edx
	sar esi, 0x5
	mov ecx, edx
	and ecx, 0x1f
	mov ebx, 0x80000000
	shr ebx, cl
	mov eax, [ebp+0xc]
	test [eax+esi*4], ebx
	jnz DObjSetLocalTag_20
DObjSetLocalTag_10:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetLocalTag_20:
	mov ecx, [ebp+0x8]
	add ecx, 0x14
	mov eax, [ecx+esi*4]
	test eax, ebx
	jnz DObjSetLocalTag_10
	or eax, ebx
	mov [ecx+esi*4], eax
	mov ecx, [ebp+0x8]
	mov eax, [ecx+0x78]
	test eax, eax
	jz DObjSetLocalTag_10
	shl edx, 0x5
	lea ebx, [eax+edx]
	mov ecx, [ebp+0x18]
	test ecx, ecx
	jz DObjSetLocalTag_30
	mov eax, [ebp+0x18]
	mov [esp+0x4], eax
	mov [esp], ebx
	call DObjSetAngles
DObjSetLocalTag_40:
	mov dword [ebx+0x1c], 0x0
	lea edx, [ebx+0x10]
	mov ecx, [ebp+0x14]
	mov eax, [ecx]
	mov [ebx+0x10], eax
	mov eax, [ecx+0x4]
	mov [edx+0x4], eax
	mov eax, [ecx+0x8]
	mov [edx+0x8], eax
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetLocalTag_30:
	xor eax, eax
	mov [ebx], eax
	mov [ebx+0x4], eax
	mov [ebx+0x8], eax
	mov dword [ebx+0xc], 0x3f800000
	jmp DObjSetLocalTag_40


;DObjGetNumModels(DObj_s const*)
DObjGetNumModels:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	movzx eax, byte [eax+0x9]
	pop ebp
	ret


;I_dmaGetDObjSkel(DObj_s const*)
I_dmaGetDObjSkel:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax+0x78]
	pop ebp
	ret
	nop


;DObjGetSurfaceData(DObj_s const*, float const*, float, char*)
DObjGetSurfaceData:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0x2c
	mov edi, [ebp+0x8]
	movzx eax, byte [edi+0x9]
	mov [ebp-0x20], eax
	mov eax, [ebp+0xc]
	mov [esp], eax
	call R_GetBaseLodDist
	fstp dword [ebp-0x1c]
	movss xmm0, dword [_float_1_00000000]
	divss xmm0, dword [ebp+0x10]
	mulss xmm0, [ebp-0x1c]
	movss [ebp-0x1c], xmm0
	mov eax, [ebp-0x20]
	test eax, eax
	jg DObjGetSurfaceData_10
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
DObjGetSurfaceData_10:
	xor esi, esi
DObjGetSurfaceData_20:
	lea ebx, [esi*4]
	mov eax, [edi+0xa0]
	mov eax, [eax+ebx]
	mov [esp], eax
	call XModelGetLodRampType
	mov [esp+0x4], eax
	movss xmm0, dword [ebp-0x1c]
	movss [esp], xmm0
	call R_GetAdjustedLodDist
	fstp dword [esp+0x4]
	mov eax, [edi+0xa0]
	mov eax, [eax+ebx]
	mov [esp], eax
	call XModelGetLodForDist
	mov edx, [ebp+0x14]
	mov [edx+esi], al
	add esi, 0x1
	cmp [ebp-0x20], esi
	jnz DObjGetSurfaceData_20
	add esp, 0x2c
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
	nop


;DObjGetHidePartBits(DObj_s const*, unsigned int*)
DObjGetHidePartBits:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov ecx, [ebp+0xc]
	lea edx, [eax+0x80]
	mov eax, [eax+0x80]
	mov [ecx], eax
	mov eax, [edx+0x4]
	mov [ecx+0x4], eax
	mov eax, [edx+0x8]
	mov [ecx+0x8], eax
	mov eax, [edx+0xc]
	mov [ecx+0xc], eax
	mov eax, [edx+0x10]
	mov [ecx+0x10], eax
	mov eax, [edx+0x14]
	mov [ecx+0x14], eax
	mov eax, [edx+0x18]
	mov [ecx+0x18], eax
	mov eax, [edx+0x1c]
	mov [ecx+0x1c], eax
	pop ebp
	ret
	nop


;DObjGetAllocSkelSize(DObj_s const*)
DObjGetAllocSkelSize:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	movzx eax, byte [eax+0xa]
	shl eax, 0x5
	pop ebp
	ret
	nop


;DObjGetRootBoneCount(DObj_s const*)
DObjGetRootBoneCount:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax+0xa0]
	mov eax, [eax]
	movzx eax, byte [eax+0x5]
	pop ebp
	ret
	nop


;DObjGetRotTransArray(DObj_s const*)
DObjGetRotTransArray:
	push ebp
	mov ebp, esp
	mov eax, [ebp+0x8]
	mov eax, [eax+0x78]
	pop ebp
	ret
	nop


;DObjSetRotTransIndex(DObj_s const*, int const*, int)
DObjSetRotTransIndex:
	push ebp
	mov ebp, esp
	push ebx
	mov ecx, [ebp+0x10]
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov ebx, 0x80000000
	shr ebx, cl
	mov eax, [ebp+0xc]
	test [eax+edx*4], ebx
	jz DObjSetRotTransIndex_10
	mov ecx, [ebp+0x8]
	add ecx, 0x14
	mov eax, [ecx+edx*4]
	test eax, ebx
	jnz DObjSetRotTransIndex_10
	or eax, ebx
	mov [ecx+edx*4], eax
	mov eax, 0x1
	pop ebx
	pop ebp
	ret
DObjSetRotTransIndex_10:
	xor eax, eax
	pop ebx
	pop ebp
	ret
	nop


;DObjSetControlTagAngles(DObj_s const*, int*, unsigned int, float*)
DObjSetControlTagAngles:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov ebx, [ebp+0x10]
	cmp ebx, 0xfd
	ja DObjSetControlTagAngles_10
	mov eax, ebx
	sar eax, 0x5
	mov ecx, ebx
	and ecx, 0x1f
	mov esi, 0x80000000
	shr esi, cl
	mov edx, [ebp+0xc]
	test [edx+eax*4], esi
	jnz DObjSetControlTagAngles_20
DObjSetControlTagAngles_10:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetControlTagAngles_20:
	mov edx, [ebp+0x8]
	add edx, 0x14
	mov ecx, [edx+eax*4]
	test ecx, esi
	jnz DObjSetControlTagAngles_10
	or [edx+eax*4+0x20], esi
	or ecx, esi
	mov [edx+eax*4], ecx
	mov edx, [ebp+0x8]
	mov eax, [edx+0x78]
	test eax, eax
	jz DObjSetControlTagAngles_10
	shl ebx, 0x5
	lea ebx, [eax+ebx]
	mov edx, [ebp+0x14]
	test edx, edx
	jz DObjSetControlTagAngles_30
	mov eax, [ebp+0x14]
	mov [esp+0x4], eax
	mov [esp], ebx
	call DObjSetAngles
DObjSetControlTagAngles_40:
	mov dword [ebx+0x1c], 0x0
	lea ecx, [ebx+0x10]
	mov edx, vec3_origin
	mov eax, [edx]
	mov [ebx+0x10], eax
	mov eax, [edx+0x4]
	mov [ecx+0x4], eax
	mov eax, [edx+0x8]
	mov [ecx+0x8], eax
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetControlTagAngles_30:
	xor eax, eax
	mov [ebx], eax
	mov [ebx+0x4], eax
	mov [ebx+0x8], eax
	mov dword [ebx+0xc], 0x3f800000
	jmp DObjSetControlTagAngles_40


;DObjSetLocalTagInternal(DObj_s const*, float const*, float const*, int)
DObjSetLocalTagInternal:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	sub esp, 0x10
	mov esi, [ebp+0xc]
	mov ecx, [ebp+0x10]
	mov edx, [ebp+0x14]
	mov eax, [ebp+0x8]
	mov eax, [eax+0x78]
	test eax, eax
	jz DObjSetLocalTagInternal_10
	shl edx, 0x5
	lea ebx, [eax+edx]
	test ecx, ecx
	jz DObjSetLocalTagInternal_20
	mov [esp+0x4], ecx
	mov [esp], ebx
	call DObjSetAngles
DObjSetLocalTagInternal_30:
	mov dword [ebx+0x1c], 0x0
	lea edx, [ebx+0x10]
	mov eax, [esi]
	mov [ebx+0x10], eax
	mov eax, [esi+0x4]
	mov [edx+0x4], eax
	mov eax, [esi+0x8]
	mov [edx+0x8], eax
DObjSetLocalTagInternal_10:
	add esp, 0x10
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetLocalTagInternal_20:
	xor eax, eax
	mov [ebx], eax
	mov [ebx+0x4], eax
	mov [ebx+0x8], eax
	mov dword [ebx+0xc], 0x3f800000
	jmp DObjSetLocalTagInternal_30


;DObjSetSkelRotTransIndex(DObj_s const*, int const*, int)
DObjSetSkelRotTransIndex:
	push ebp
	mov ebp, esp
	push esi
	push ebx
	mov ecx, [ebp+0x10]
	mov ebx, ecx
	sar ebx, 0x5
	and ecx, 0x1f
	mov esi, 0x80000000
	shr esi, cl
	mov eax, [ebp+0xc]
	test [eax+ebx*4], esi
	jz DObjSetSkelRotTransIndex_10
	mov edx, [ebp+0x8]
	add edx, 0x14
	mov eax, [edx+ebx*4]
	test eax, esi
	jnz DObjSetSkelRotTransIndex_20
	or eax, esi
	mov [edx+ebx*4], eax
	or [edx+ebx*4+0x40], esi
DObjSetSkelRotTransIndex_10:
	mov eax, 0x1
	pop ebx
	pop esi
	pop ebp
	ret
DObjSetSkelRotTransIndex_20:
	xor eax, eax
	pop ebx
	pop esi
	pop ebp
	ret
	add [eax], al


;DObjSkelAreBonesUpToDate(DObj_s const*, int*)
DObjSkelAreBonesUpToDate:
	push ebp
	mov ebp, esp
	push ebx
	mov ebx, [ebp+0xc]
	mov edx, [ebp+0x8]
	add edx, 0x14
	mov ecx, 0x1
DObjSkelAreBonesUpToDate_20:
	mov eax, [edx+0x40]
	not eax
	test [ebx+ecx*4-0x4], eax
	jnz DObjSkelAreBonesUpToDate_10
	add ecx, 0x1
	add edx, 0x4
	cmp ecx, 0x9
	jnz DObjSkelAreBonesUpToDate_20
	mov eax, 0x1
	pop ebx
	pop ebp
	ret
DObjSkelAreBonesUpToDate_10:
	xor eax, eax
	pop ebx
	pop ebp
	ret
	nop
	add [eax], al


;DObjCompleteHierarchyBits(DObj_s const*, int*)
DObjCompleteHierarchyBits:
	push ebp
	mov ebp, esp
	push edi
	push esi
	push ebx
	sub esp, 0xbc
	mov ebx, [ebp+0x8]
	movzx eax, byte [ebx+0xa]
	lea esi, [eax-0x1]
	movzx edi, byte [ebx+0x9]
	movzx eax, word [ebx+0x4]
	mov [esp], eax
	call SL_ConvertToString
	mov [ebp-0xa4], eax
	add eax, 0x20
	mov [ebp-0xac], eax
	mov ebx, [ebx+0xa0]
	mov [ebp-0xa0], ebx
	lea eax, [ebx+edi*4]
	mov [ebp-0x9c], eax
	test edi, edi
	jg DObjCompleteHierarchyBits_10
	mov dword [ebp-0xb0], 0x0
	mov dword [ebp-0xb4], 0x0
DObjCompleteHierarchyBits_130:
	mov eax, [ebp+0xc]
	or dword [eax], 0x80000000
	mov edx, [ebp-0xb0]
	mov edx, [edx+0xc]
	mov [ebp-0xa8], edx
	mov ecx, [ebp-0xb4]
	mov edi, [ebp+ecx*4-0x98]
DObjCompleteHierarchyBits_80:
	mov ebx, esi
	sub ebx, edi
	js DObjCompleteHierarchyBits_20
DObjCompleteHierarchyBits_60:
	mov eax, esi
	sar eax, 0x5
	shl eax, 0x2
	mov ecx, esi
	and ecx, 0x1f
	mov edx, 0x80000000
	shr edx, cl
	mov ecx, [ebp+0xc]
	test [ecx+eax], edx
	jz DObjCompleteHierarchyBits_30
	mov ecx, [ebp-0xa4]
	test [ecx+eax], edx
	jnz DObjCompleteHierarchyBits_40
	mov edx, [ebp-0xb0]
	movzx eax, byte [edx+0x5]
	sub ebx, eax
	js DObjCompleteHierarchyBits_50
	mov ecx, [ebp-0xa8]
	movzx eax, byte [ecx+ebx]
	mov ecx, esi
	sub ecx, eax
DObjCompleteHierarchyBits_90:
	mov edx, ecx
	sar edx, 0x5
	and ecx, 0x1f
	mov eax, 0x80000000
	shr eax, cl
	mov ecx, [ebp+0xc]
	or [ecx+edx*4], eax
	sub esi, 0x1
	mov ebx, esi
	sub ebx, edi
	jns DObjCompleteHierarchyBits_60
DObjCompleteHierarchyBits_20:
	sub dword [ebp-0xb4], 0x1
	js DObjCompleteHierarchyBits_70
	mov eax, [ebp-0xb4]
	mov edx, [ebp-0xa0]
	mov eax, [edx+eax*4]
	mov [ebp-0xb0], eax
	mov edx, [eax+0xc]
	mov [ebp-0xa8], edx
	mov ecx, [ebp-0xb4]
	mov edi, [ebp+ecx*4-0x98]
	jmp DObjCompleteHierarchyBits_80
DObjCompleteHierarchyBits_50:
	mov edx, [ebp-0x9c]
	mov ecx, [ebp-0xb4]
	movzx eax, byte [edx+ecx]
	movzx ecx, al
	add al, 0x1
	jnz DObjCompleteHierarchyBits_90
DObjCompleteHierarchyBits_30:
	sub esi, 0x1
	jmp DObjCompleteHierarchyBits_80
DObjCompleteHierarchyBits_40:
	mov edx, [ebp-0xac]
	movzx eax, byte [edx]
	sub eax, 0x1
	cmp esi, eax
	jz DObjCompleteHierarchyBits_100
DObjCompleteHierarchyBits_110:
	add edx, 0x2
	movzx eax, byte [edx]
	sub eax, 0x1
	cmp esi, eax
	jnz DObjCompleteHierarchyBits_110
DObjCompleteHierarchyBits_100:
	movzx eax, byte [edx+0x1]
	lea ecx, [eax-0x1]
	jmp DObjCompleteHierarchyBits_90
DObjCompleteHierarchyBits_70:
	add esp, 0xbc
	pop ebx
	pop esi
	pop edi
	pop ebp
	ret
DObjCompleteHierarchyBits_10:
	mov dword [ebp-0x98], 0x0
	mov eax, [ebx]
	mov [ebp-0xb0], eax
	movzx edx, byte [eax+0x4]
	cmp esi, edx
	jl DObjCompleteHierarchyBits_120
	mov dword [ebp-0xb4], 0x0
DObjCompleteHierarchyBits_140:
	add dword [ebp-0xb4], 0x1
	cmp edi, [ebp-0xb4]
	jz DObjCompleteHierarchyBits_130
	mov ecx, [ebp-0xb4]
	mov [ebp+ecx*4-0x98], edx
	mov eax, [ebp-0xa0]
	mov eax, [eax+ecx*4]
	mov [ebp-0xb0], eax
	movzx eax, byte [eax+0x4]
	add edx, eax
	cmp esi, edx
	jge DObjCompleteHierarchyBits_140
	jmp DObjCompleteHierarchyBits_130
DObjCompleteHierarchyBits_120:
	mov dword [ebp-0xb4], 0x0
	jmp DObjCompleteHierarchyBits_130


;Zero initialized global or static variables of dobj_utils:
SECTION .bss


;Initialized global or static variables of dobj_utils:
SECTION .data


;Initialized constant data of dobj_utils:
SECTION .rdata


;All cstrings:
SECTION .rdata



;All constant floats and doubles:
SECTION .rdata
_double_0_00872665:		dq 0x3f81df46a2529d39	; 0.00872665
_float_1_00000000:		dd 0x3f800000	; 1

align   16,db 0
_data16_80000000:		dd 0x80000000, 0x0, 0x0, 0x0	; DQWORD
