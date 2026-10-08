EXTERN Theatre_StoreBaseline:PROC
EXTERN Theatre_AdjustTimeDelta:PROC

EXTERN Theatre_clcDemowaiting:QWORD
EXTERN Theatre_clcDemoplaying:QWORD
EXTERN Theatre_clNewSnapshots:QWORD
EXTERN Theatre_ParseSnapshotNext:QWORD
EXTERN Theatre_AdjustTimeDeltaNext:QWORD
EXTERN Theatre_AdjustTimeDeltaSkip:QWORD
EXTERN Theatre_UISetActiveMenuReturn:QWORD

.code

BaselineStoreStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov rsi, rbx
	mov rax, qword ptr [Theatre_clcDemowaiting]
	mov dword ptr [rax], ebx

	mov rcx, rbp
	call Theatre_StoreBaseline

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [Theatre_ParseSnapshotNext]
BaselineStoreStub ENDP

AdjustTimeDeltaStub PROC FRAME
	push rcx
	.pushreg rcx
	push r8
	.pushreg r8
	push r9
	.pushreg r9
	push r10
	.pushreg r10
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	call Theatre_AdjustTimeDelta

	add rsp, 28h
	pop r10
	pop r9
	pop r8
	pop rcx

	test al, al
	jnz skip

	mov r11d, r9d
	mov rax, qword ptr [Theatre_clNewSnapshots]
	mov dword ptr [rax], r15d

	lea rsp, [rsp + 8]
	jmp qword ptr [Theatre_AdjustTimeDeltaNext]

skip:
	lea rsp, [rsp + 8]
	jmp qword ptr [Theatre_AdjustTimeDeltaSkip]
AdjustTimeDeltaStub ENDP

UISetActiveMenuStub PROC
	mov rax, qword ptr [Theatre_clcDemoplaying]
	cmp dword ptr [rax], 0
	jnz playing

	mov edx, 10h
	ret

playing:
	lea rsp, [rsp + 8]
	jmp qword ptr [Theatre_UISetActiveMenuReturn]
UISetActiveMenuStub ENDP

END
