EXTERN Threading_ComFrameWait:PROC
EXTERN Com_Frame_WaitResume:QWORD

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

END
