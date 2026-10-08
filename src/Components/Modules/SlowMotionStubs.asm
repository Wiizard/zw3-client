EXTERN SlowMotion_Delay:DWORD
EXTERN SlowMotion_Active:QWORD

.code

SlowMotionUpdateStub PROC
	cmp dword ptr [SlowMotion_Delay], 0
	jle stock

	sub dword ptr [SlowMotion_Delay], r9d
	xor eax, eax
	ret

stock:
	mov rax, qword ptr [SlowMotion_Active]
	cmp byte ptr [rax], 0
	ret
SlowMotionUpdateStub ENDP

END
