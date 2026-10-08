EXTERN Weapon_BG_GetWeaponDef:QWORD
EXTERN Weapon_CG_SelectWeaponIndexFail:QWORD

.code

CG_SelectWeaponIndexStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	call qword ptr [Weapon_BG_GetWeaponDef]
	test rax, rax
	jz noDef

	add rsp, 28h
	ret

noDef:
	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [Weapon_CG_SelectWeaponIndexFail]
CG_SelectWeaponIndexStub ENDP

END
