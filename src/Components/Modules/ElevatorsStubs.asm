EXTERN Elevators_bg_elevators:QWORD

.code

StartSolidStub PROC
	mov rax, qword ptr [Elevators_bg_elevators]
	cmp dword ptr [rax + 10h], 2
	movzx eax, byte ptr [rdi + 2Dh]
	jne done

	xor eax, eax

done:
	test al, al
	ret
StartSolidStub ENDP

GroundStartSolidStub PROC
	mov rax, qword ptr [Elevators_bg_elevators]
	cmp dword ptr [rax + 10h], 1
	movzx eax, byte ptr [rdi + 2Dh]
	jl done

	xor eax, eax

done:
	test al, al
	ret
GroundStartSolidStub ENDP

TraceStub PROC FRAME
	push rbx
	.pushreg rbx
	sub rsp, 30h
	.allocstack 30h
	.endprolog

	mov rbx, rcx
	mov eax, dword ptr [rsp + 60h]
	mov dword ptr [rsp + 20h], eax
	mov eax, dword ptr [rsp + 68h]
	mov dword ptr [rsp + 28h], eax
	call qword ptr [rbp + r10 * 8]

	mov rax, qword ptr [Elevators_bg_elevators]
	cmp dword ptr [rax + 10h], 2
	jne done

	mov byte ptr [rbx + 2Ch], 0

done:
	add rsp, 30h
	pop rbx
	ret
TraceStub ENDP

END
