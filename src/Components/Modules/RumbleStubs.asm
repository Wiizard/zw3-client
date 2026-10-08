EXTERN Rumble_MeleeRumble:PROC
EXTERN Rumble_PlayNoteMappedRumbleAliases:PROC
EXTERN Rumble_MeleeTargetIsClient:QWORD
EXTERN Rumble_MeleeTargetIsNotClient:QWORD
EXTERN Rumble_NoteSoundMapTest:QWORD

.code

MeleeRumbleStub PROC
	mov rcx, rbx
	mov rdx, r15
	sub rsp, 20h
	call Rumble_MeleeRumble
	add rsp, 20h

	cmp qword ptr [rbx+158h], 0
	je targetIsNotClient
	jmp qword ptr [Rumble_MeleeTargetIsClient]

targetIsNotClient:
	jmp qword ptr [Rumble_MeleeTargetIsNotClient]
MeleeRumbleStub ENDP

PlayNoteMappedSoundAliasesStub PROC
	mov rax, qword ptr [rsp+0C0h]
	mov rdx, qword ptr [r14+rax]
	mov ecx, ebp
	mov r8, rdi
	sub rsp, 20h
	call Rumble_PlayNoteMappedRumbleAliases
	add rsp, 20h

	mov rax, qword ptr [rsp+0C0h]
	mov rcx, qword ptr [r14+rax]
	jmp qword ptr [Rumble_NoteSoundMapTest]
PlayNoteMappedSoundAliasesStub ENDP

END
