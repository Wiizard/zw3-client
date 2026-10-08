EXTERN VisionFile_LoadVisionSettingsFromBuffer:PROC

EXTERN VisionFile_ParseDone:QWORD

.code

LoadVisionSettingsFromBufferStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	mov rcx, rbx
	lea rdx, [rbp - 9]
	mov r8, r15
	call VisionFile_LoadVisionSettingsFromBuffer

	add rsp, 28h
	lea rsp, [rsp + 8]
	jmp qword ptr [VisionFile_ParseDone]
LoadVisionSettingsFromBufferStub ENDP

END
