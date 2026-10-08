EXTERN Renderer_BackendFrameHandler:PROC

EXTERN Renderer_SwapChainIndex:QWORD

.code

BackendFrameStub PROC FRAME
	sub rsp, 28h
	.allocstack 28h
	.endprolog

	call Renderer_BackendFrameHandler

	add rsp, 28h

	mov rax, qword ptr [Renderer_SwapChainIndex]
	movsxd rax, dword ptr [rax]
	ret
BackendFrameStub ENDP

END
