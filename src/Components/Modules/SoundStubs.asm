EXTERN Sound_DirectSound:QWORD
EXTERN Sound_CreateBufferNext:QWORD
EXTERN Sound_CreateBufferFail:QWORD
EXTERN Sound_VoiceLoopNext:QWORD
EXTERN Sound_VoiceInitReturn:QWORD
EXTERN Sound_ParseStreamHeader:PROC
EXTERN Sound_FrontendGain:DWORD
EXTERN Sound_MasterVolume:QWORD
EXTERN Sound_MasterVolumeNext:QWORD

.code

SoundMasterVolumeStub PROC
	mulss xmm0, dword ptr [Sound_FrontendGain]
	push rax
	mov rax, qword ptr [Sound_MasterVolume]
	movss dword ptr [rax], xmm0
	pop rax
	jmp qword ptr [Sound_MasterVolumeNext]
SoundMasterVolumeStub ENDP

SND_ParseStreamHeaderStub PROC
	mov r9, rbx
	jmp Sound_ParseStreamHeader
SND_ParseStreamHeaderStub ENDP

DirectSoundCheckStub PROC
	mov rcx, qword ptr [Sound_DirectSound]
	mov rcx, qword ptr [rcx]
	test rcx, rcx
	jz fail

	cmp qword ptr [rcx], 0
	je fail

	jmp qword ptr [Sound_CreateBufferNext]

fail:
	jmp qword ptr [Sound_CreateBufferFail]
DirectSoundCheckStub ENDP

VoiceLoopStub PROC
	test rax, rax
	jz failInit

	mov qword ptr [rbx], rax
	add rbx, 8
	jmp qword ptr [Sound_VoiceLoopNext]

failInit:
	xor al, al
	jmp qword ptr [Sound_VoiceInitReturn]
VoiceLoopStub ENDP

END
