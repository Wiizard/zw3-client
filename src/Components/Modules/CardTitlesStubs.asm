EXTERN CardTitles_GetPlayerCardClientInfo:PROC
EXTERN CardTitles_GetPlayerCardClientInfoNext:QWORD

.code

GetPlayerCardClientInfoStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov ecx, dword ptr [rbx + 4]
	mov rdx, rbx
	call CardTitles_GetPlayerCardClientInfo

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [CardTitles_GetPlayerCardClientInfoNext]
GetPlayerCardClientInfoStub ENDP

END
