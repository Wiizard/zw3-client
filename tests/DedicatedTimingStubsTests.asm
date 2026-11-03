EXTERN Dedicated_FrameMsec_Stub:PROC
EXTERN Dedicated_FrameMsec_Resume:QWORD

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

END
