EXTERN Threading_ComFrameWait:PROC
EXTERN Com_Frame_WaitResume:QWORD
EXTERN Com_Frame_DedicatedMsecResume:QWORD
EXTERN SV_DedicatedFrameRun:QWORD
EXTERN SV_DedicatedFrameSkip:QWORD
EXTERN SV_DedicatedTime:QWORD
EXTERN SV_DedicatedTargetTime:QWORD

.code

Com_Frame_WaitStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov ecx, ebx
	call Threading_ComFrameWait

	mov ecx, eax
	add rsp, 30h
	jmp qword ptr [Com_Frame_WaitResume]
Com_Frame_WaitStub ENDP

; Enter after the native elapsed-time clamp and CL_ModifyMsec. Preserve XMM6
; (elapsed seconds), supply R9D (simulation milliseconds), and initialize the
; fractional timescale accumulator just as the skipped native filter does.
Com_Frame_DedicatedMsecStub PROC
	mov r9d, eax
	xorps xmm3, xmm3
	lea rsp, [rsp + 8]
	jmp qword ptr [Com_Frame_DedicatedMsecResume]
Com_Frame_DedicatedMsecStub ENDP

SV_DedicatedFrameBudgetStub PROC FRAME
	push rax
	.pushreg rax
	push rcx
	.pushreg rcx
	.endprolog
	test r14d, r14d
	jz noFrame
	mov rax, qword ptr [SV_DedicatedTime]
	mov eax, dword ptr [rax]
	mov rcx, qword ptr [SV_DedicatedTargetTime]
	; Map startup uses worker frames before SV_Frame initializes its target.
	; Those frames also complete the main/worker script-VM hand-off.
	cmp dword ptr [rcx], 0
	je runFrame
	cmp eax, dword ptr [rcx]
	jge noFrame
runFrame:
	pop rcx
	pop rax
	lea rsp, [rsp + 8]
	jmp qword ptr [SV_DedicatedFrameRun]
noFrame:
	pop rcx
	pop rax
	lea rsp, [rsp + 8]
	jmp qword ptr [SV_DedicatedFrameSkip]
SV_DedicatedFrameBudgetStub ENDP

END
