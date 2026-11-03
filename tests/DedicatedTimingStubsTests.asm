EXTERN Dedicated_FrameMsec_Stub:PROC
EXTERN Dedicated_FrameMsec_Resume:QWORD
EXTERN Dedicated_SnapshotInterval_Stub:PROC

.code

TestDedicatedFrameMsec PROC FRAME
	push rbx
	.pushreg rbx
	push rdi
	.pushreg rdi
	sub rsp, 38h
	.allocstack 38h
	movaps [rsp+20h], xmm6
	.savexmm128 xmm6, 20h
	.endprolog
	mov edi, ecx
	mov ebx, 3
	mov r9d, 123456
	pcmpeqd xmm3, xmm3
	cvtsi2ss xmm6, ecx
	mov r10, rsp
	lea rax, resumed
	mov qword ptr [Dedicated_FrameMsec_Resume], rax
	call Dedicated_FrameMsec_Stub
	; The production stub must skip this client-smoothing continuation.
	int 3
resumed:
	mov eax, 1
	cmp rsp, r10
	jne done
	cmp ebx, edi
	jne done
	cmp r9d, edi
	jne done
	movd edx, xmm3
	test edx, edx
	jne done
	cvttss2si edx, xmm6
	cmp edx, edi
	jne done
	xor eax, eax
done:
	movaps xmm6, [rsp+20h]
	add rsp, 38h
	pop rdi
	pop rbx
	ret
TestDedicatedFrameMsec ENDP

TestDedicatedSnapshotInterval PROC FRAME
	push rsi
	.pushreg rsi
	sub rsp, 20h
	.allocstack 20h
	.endprolog
	mov esi, edx
	mov eax, ecx
	mov r9d, ecx
	mov r10, rsp
	call Dedicated_SnapshotInterval_Stub
	cmp eax, r9d
	jne failed
	cmp esi, edx
	jne failed
	cmp rsp, r10
	jne failed
	cmp ecx, r8d
	jne failed
	xor eax, eax
	jmp finished
failed:
	mov eax, 1
finished:
	add rsp, 20h
	pop rsi
	ret
TestDedicatedSnapshotInterval ENDP

END
