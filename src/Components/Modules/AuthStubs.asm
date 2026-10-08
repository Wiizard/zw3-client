EXTERN Auth_PrivateClientCount:PROC

.code

DirectConnectPrivateClientStub PROC FRAME
	push rcx
	.pushreg rcx
	push rdx
	.pushreg rdx
	push r8
	.pushreg r8
	push r9
	.pushreg r9
	push r10
	.pushreg r10
	push r11
	.pushreg r11
	sub rsp, 88h
	.allocstack 88h
	.endprolog

	movdqu xmmword ptr [rsp + 20h], xmm0
	movdqu xmmword ptr [rsp + 30h], xmm1
	movdqu xmmword ptr [rsp + 40h], xmm2
	movdqu xmmword ptr [rsp + 50h], xmm3
	movdqu xmmword ptr [rsp + 60h], xmm4
	movdqu xmmword ptr [rsp + 70h], xmm5

	call Auth_PrivateClientCount
	mov r14d, eax

	movdqu xmm0, xmmword ptr [rsp + 20h]
	movdqu xmm1, xmmword ptr [rsp + 30h]
	movdqu xmm2, xmmword ptr [rsp + 40h]
	movdqu xmm3, xmmword ptr [rsp + 50h]
	movdqu xmm4, xmmword ptr [rsp + 60h]
	movdqu xmm5, xmmword ptr [rsp + 70h]

	add rsp, 88h
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	ret
DirectConnectPrivateClientStub ENDP

END
