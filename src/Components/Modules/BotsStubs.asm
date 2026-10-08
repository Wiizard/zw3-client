EXTERN Bots_BotUserMove:PROC
EXTERN Bots_BotUserMoveNext:QWORD

.code

SV_BotUserMove_Stub PROC FRAME
	push rax
	.pushreg rax
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
	sub rsp, 80h
	.allocstack 80h
	.endprolog

	movdqu xmmword ptr [rsp + 20h], xmm0
	movdqu xmmword ptr [rsp + 30h], xmm1
	movdqu xmmword ptr [rsp + 40h], xmm2
	movdqu xmmword ptr [rsp + 50h], xmm3
	movdqu xmmword ptr [rsp + 60h], xmm4
	movdqu xmmword ptr [rsp + 70h], xmm5

	lea rcx, [rsi - 212B8h]
	call Bots_BotUserMove
	test al, al

	movdqu xmm0, xmmword ptr [rsp + 20h]
	movdqu xmm1, xmmword ptr [rsp + 30h]
	movdqu xmm2, xmmword ptr [rsp + 40h]
	movdqu xmm3, xmmword ptr [rsp + 50h]
	movdqu xmm4, xmmword ptr [rsp + 60h]
	movdqu xmm5, xmmword ptr [rsp + 70h]

	lea rsp, [rsp + 80h]
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	pop rax
	jnz taken

	xor eax, eax
	xorps xmm0, xmm0
	ret

taken:
	lea rsp, [rsp + 8]
	jmp qword ptr [Bots_BotUserMoveNext]
SV_BotUserMove_Stub ENDP

END
