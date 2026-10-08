EXTERN Network_MSG_WriteByte:PROC

.code

SV_WriteGameState_MSG_WriteByte PROC
	mov r8, qword ptr [rsp+68h]
	jmp Network_MSG_WriteByte
SV_WriteGameState_MSG_WriteByte ENDP

SV_WriteGameState_MSG_WriteByteTail PROC
	mov r8, qword ptr [rsp+8]
	jmp Network_MSG_WriteByte
SV_WriteGameState_MSG_WriteByteTail ENDP

MSG_WriteByte_ClientInRbx PROC
	mov r8, rbx
	jmp Network_MSG_WriteByte
MSG_WriteByte_ClientInRbx ENDP

MSG_WriteByte_ClientInRsi PROC
	mov r8, rsi
	jmp Network_MSG_WriteByte
MSG_WriteByte_ClientInRsi ENDP

MSG_WriteByte_ClientInRdi PROC
	mov r8, rdi
	jmp Network_MSG_WriteByte
MSG_WriteByte_ClientInRdi ENDP

MSG_WriteByte_ClientInR13 PROC
	mov r8, r13
	jmp Network_MSG_WriteByte
MSG_WriteByte_ClientInR13 ENDP

END
