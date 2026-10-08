EXTERN ClientSlots_cgameClientInfo:QWORD
EXTERN ClientSlots_cgameClientCount:DWORD
EXTERN QuickPatch_Sys_IsLANAddress:QWORD

.code

VehicleCl_ResetEntity_PlayerIndexStub PROC
	mov eax, [rbp + 158h]
	cmp eax, dword ptr ClientSlots_cgameClientCount
	jb resetValid
	xor eax, eax

resetValid:
	mov [rsi + 38h], eax
	ret
VehicleCl_ResetEntity_PlayerIndexStub ENDP

VehicleCl_ProcessEntity_PlayerIndexStub PROC
	mov eax, [rdx + 158h]
	cmp eax, dword ptr ClientSlots_cgameClientCount
	jb updateValid
	xor eax, eax

updateValid:
	mov [rbx + r12 + 6744488h], eax
	ret
VehicleCl_ProcessEntity_PlayerIndexStub ENDP

VehicleFx_PlayerIndexCheckStub PROC
	cmp eax, dword ptr ClientSlots_cgameClientCount
	jb checkValid
	test r9d, r9d
	ret

checkValid:
	imul rdx, rax, 548h
	add rdx, qword ptr ClientSlots_cgameClientInfo
	cmp r9d, [rdx + 1Ch]
	ret
VehicleFx_PlayerIndexCheckStub ENDP

SV_UserinfoChanged_LanRateStub PROC FRAME
	sub rsp, 48h
	.allocstack 48h
	.endprolog

	movups xmm0, xmmword ptr [rbp + 28h]
	movups xmmword ptr [rsp + 20h], xmm0
	mov eax, dword ptr [rbp + 38h]
	mov dword ptr [rsp + 30h], eax
	lea rcx, [rsp + 20h]
	call qword ptr [QuickPatch_Sys_IsLANAddress]

	mov rbx, -1
	test eax, eax
	lea rsp, [rsp + 48h]
	ret
SV_UserinfoChanged_LanRateStub ENDP

END
