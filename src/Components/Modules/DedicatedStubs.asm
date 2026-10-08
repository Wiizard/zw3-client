EXTERN Dedicated_ComClampMsec:PROC
EXTERN Com_Frame_SvRunningSlot:QWORD

.code

Com_ClampMsec_Stub PROC FRAME
	push rcx
	.pushreg rcx
	sub rsp, 20h
	.allocstack 20h
	.endprolog

	call Dedicated_ComClampMsec

	add rsp, 20h
	pop rcx
	mov rax, qword ptr [Com_Frame_SvRunningSlot]
	mov rax, qword ptr [rax]
	ret
Com_ClampMsec_Stub ENDP

END
