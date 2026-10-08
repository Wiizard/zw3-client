EXTERN Voice_UI_Mute_player:PROC
EXTERN Voice_UIRunMenuScriptExit:QWORD

.code

UI_MutePlayerStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov ecx, eax
	mov edx, edi
	call Voice_UI_Mute_player

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [Voice_UIRunMenuScriptExit]
UI_MutePlayerStub ENDP

END
