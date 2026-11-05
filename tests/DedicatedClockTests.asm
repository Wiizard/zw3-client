EXTERN Com_Frame_DedicatedMsecStub:PROC
EXTERN SV_DedicatedFrameBudgetStub:PROC

.code

; Exercise the production hook with the register contract at RVA 0x1F446D.
DedicatedClockTest_Run PROC FRAME
	push rbx
	.pushreg rbx
	sub rsp, 30h
	.allocstack 30h
	movaps xmmword ptr [rsp + 20h], xmm6
	.savexmm128 xmm6, 20h
	.endprolog
	mov rbx, r9
	mov eax, ecx
	movaps xmm6, xmm1
	movaps xmm3, xmm2
	call Com_Frame_DedicatedMsecStub
	int 3
DedicatedClockTest_Resume LABEL PROC
	mov dword ptr [rbx], r9d
	movss dword ptr [rbx + 4], xmm6
	movss dword ptr [rbx + 8], xmm3
	movaps xmm6, xmmword ptr [rsp + 20h]
	add rsp, 30h
	pop rbx
	ret
DedicatedClockTest_Run ENDP

PUBLIC DedicatedClockTest_Resume

DedicatedClockTest_Worker PROC FRAME
	push r14
	.pushreg r14
	sub rsp, 20h
	.allocstack 20h
	.endprolog
	mov r14d, ecx
	mov rax, 123456789ABCDEF0h
	call SV_DedicatedFrameBudgetStub
	int 3
DedicatedClockTest_FrameRun LABEL PROC
	mov rdx, 123456789ABCDEF0h
	cmp rax, rdx
	jne badRegisters
	cmp ecx, r14d
	jne badRegisters
	mov eax, 1
	jmp workerDone
DedicatedClockTest_FrameSkip LABEL PROC
	mov rdx, 123456789ABCDEF0h
	cmp rax, rdx
	jne badRegisters
	cmp ecx, r14d
	jne badRegisters
	xor eax, eax
	jmp workerDone
badRegisters:
	mov eax, 2
workerDone:
	add rsp, 20h
	pop r14
	ret
DedicatedClockTest_Worker ENDP
PUBLIC DedicatedClockTest_FrameRun
PUBLIC DedicatedClockTest_FrameSkip
END
