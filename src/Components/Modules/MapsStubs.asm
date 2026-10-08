EXTERN Maps_TriggerReconnectForMap:PROC

EXTERN Maps_NewMapFlag:QWORD
EXTERN Maps_NewMapBodyNext:QWORD
EXTERN Maps_NewMapReturnTrue:QWORD

.code

LoadingNewMapStub PROC FRAME
	push rax
	.pushreg rax
	sub rsp, 20h
	.allocstack 20h
	.endprolog

	mov rcx, r12
	lea rdx, [rbp - 30h]
	call Maps_TriggerReconnectForMap
	mov ecx, eax

	add rsp, 20h
	pop rax

	test ecx, ecx
	jnz reconnecting

	xor ecx, ecx
	mov r8, qword ptr [Maps_NewMapFlag]
	mov byte ptr [r8], 1

	lea rsp, [rsp + 8]
	jmp qword ptr [Maps_NewMapBodyNext]

reconnecting:
	lea rsp, [rsp + 8]
	jmp qword ptr [Maps_NewMapReturnTrue]
LoadingNewMapStub ENDP

END
