EXTERN ClanTags_UserinfoChanged:PROC
EXTERN ClanTags_GetClanTagWithName:PROC
EXTERN ClanTags_ScoreboardNameNext:QWORD

.code

ClientUserinfoChangedStub PROC
	mov r8d, ebx
	jmp ClanTags_UserinfoChanged
ClientUserinfoChangedStub ENDP

ClientConnectUserinfoStub PROC
	mov r8d, r12d
	jmp ClanTags_UserinfoChanged
ClientConnectUserinfoStub ENDP

ScoreboardNameStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov ecx, dword ptr [r15]
	lea rdx, [rbx + 0Ch]
	call ClanTags_GetClanTagWithName
	mov rdx, rax

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [ClanTags_ScoreboardNameNext]
ScoreboardNameStub ENDP

END
