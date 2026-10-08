EXTERN Gamepad_ApplyMovement:PROC
EXTERN MSG_WriteDeltaUsercmdKey_Resume:QWORD
EXTERN MSG_ReadDeltaUsercmdKey_Resume:QWORD
EXTERN MSG_ReadDeltaUsercmdKey_Resume2:QWORD
EXTERN Gamepad_IN_Frame:PROC
EXTERN Gamepad_INFrameReturn:QWORD

.code

MSG_WriteDeltaUsercmdKeyStub PROC
	mov qword ptr [rsp+50h], r15

	movzx r15d, byte ptr [rsi+1Bh]
	shl r15d, 8
	movzx eax, byte ptr [rsi+1Ah]
	or r15d, eax

	movzx r12d, byte ptr [rbp+1Bh]
	shl r12d, 8
	movzx eax, byte ptr [rbp+1Ah]
	or r12d, eax

	mov ebx, 2
	jmp qword ptr [MSG_WriteDeltaUsercmdKey_Resume]
MSG_WriteDeltaUsercmdKeyStub ENDP

MSG_ReadDeltaUsercmdKeyStub PROC
	mov rcx, rsi
	mov edx, ebp
	mov r8, r14
	mov r9, rdi
	sub rsp, 20h
	call Gamepad_ApplyMovement
	add rsp, 20h
	jmp qword ptr [MSG_ReadDeltaUsercmdKey_Resume]
MSG_ReadDeltaUsercmdKeyStub ENDP

MSG_ReadDeltaUsercmdKeyStub2 PROC
	mov rcx, rsi
	mov edx, ebp
	mov r8, r14
	mov r9, rdi
	sub rsp, 20h
	call Gamepad_ApplyMovement
	add rsp, 20h
	jmp qword ptr [MSG_ReadDeltaUsercmdKey_Resume2]
MSG_ReadDeltaUsercmdKeyStub2 ENDP

INFrameMouseMoveStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	call Gamepad_IN_Frame

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [Gamepad_INFrameReturn]
INFrameMouseMoveStub ENDP

END
