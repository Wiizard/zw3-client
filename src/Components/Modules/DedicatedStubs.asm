EXTERN Dedicated_ComClampMsec:PROC
EXTERN Com_Frame_SvRunningSlot:QWORD
EXTERN Dedicated_FrameMsec_Resume:QWORD

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

; Entered at the CL_ModifyMsec CALL. ECX is the clamped elapsed milliseconds,
; XMM6 already holds its seconds value. Skip client smoothing only, then
; resume at the existing slow-motion hook with the same register contract.
Dedicated_FrameMsec_Stub PROC
	mov r9d, ecx
	mov ebx, ecx
	xorps xmm3, xmm3
	add rsp, 8
	jmp qword ptr [Dedicated_FrameMsec_Resume]
Dedicated_FrameMsec_Stub ENDP

END
