EXTERN ClientSlots_snapshotDue:BYTE
EXTERN ClientSlots_clientMaskHigh:DWORD
EXTERN ClientSlots_snapshotMaskHigh:DWORD
EXTERN ClientSlots_baselineMaskHigh:DWORD
EXTERN ClientSlots_snapshotRingBegin:QWORD
EXTERN ClientSlots_snapshotRingEnd:QWORD
EXTERN ClientSlots_baselineBegin:QWORD
EXTERN ClientSlots_baselineEnd:QWORD
EXTERN ClientSlots_serverClientCount:QWORD
EXTERN ClientSlots_clientIndexBits:BYTE
EXTERN ClientSlots_wideClientBits:BYTE
EXTERN ClientSlots_buildItemClientMaskBody:QWORD
EXTERN ClientSlots_sessionGetXuid:QWORD
EXTERN ClientSlots_sessionIsRegistered:QWORD
EXTERN ClientSlots_partyRemovePlayer:QWORD
EXTERN ClientSlots_partyVoiceBits:QWORD
EXTERN ClientSlots_setConfigstring:QWORD
EXTERN ClientSlots_partyMemberAddr:QWORD
EXTERN ClientSlots_sessionRegister:QWORD
EXTERN ClientSlots_clientInMyParty:QWORD
EXTERN ClientSlots_playerMuted:QWORD
EXTERN ClientSlots_playerTalking:QWORD
EXTERN ClientSlots_scriptChildCounts:QWORD
EXTERN ClientSlots_scriptChild0Begin:DWORD
EXTERN ClientSlots_allocVariableBody:QWORD
EXTERN ClientSlots_initVariablesBody:QWORD
EXTERN ClientSlots_childPoolExhaustedBody:QWORD

.code

ClientSlots_SnapshotDueAddress PROC
	lea rcx, ClientSlots_snapshotDue
	ret
ClientSlots_SnapshotDueAddress ENDP

ClientSlots_SnapshotDueSet PROC FRAME
	push rax
	.pushreg rax
	.endprolog

	lea rax, ClientSlots_snapshotDue
	mov byte ptr [rax+rsi], 1
	pop rax
	ret
ClientSlots_SnapshotDueSet ENDP

ClientSlots_SnapshotDueTest PROC FRAME
	push rcx
	.pushreg rcx
	.endprolog

	lea rcx, ClientSlots_snapshotDue
	cmp byte ptr [rcx+rax], 0
	pop rcx
	ret
ClientSlots_SnapshotDueTest ENDP


MASK_HIGH_WORDS EQU 3

SetMaskRow MACRO row, value
	maskWord = 0
	REPT MASK_HIGH_WORDS
	mov dword ptr [row+maskWord*4], value
	maskWord = maskWord + 1
	ENDM
ENDM

AndMaskRow MACRO value, row
	maskWord = 0
	REPT MASK_HIGH_WORDS
	and value, dword ptr [row+maskWord*4]
	maskWord = maskWord + 1
	ENDM
ENDM

ClientSlots_MaskLoad_1401627BA PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskLoad_1401627BA_high
	mov eax, dword ptr [rsi+0FCh]
	jmp ClientSlots_MaskLoad_1401627BA_done
ClientSlots_MaskLoad_1401627BA_high:
	movsxd r11, dword ptr [rsi]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rdx
	shl r11, 2
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_1401627BA_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_1401627BA ENDP

ClientSlots_MaskLoad_14018215E PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskLoad_14018215E_high
	mov eax, dword ptr [r15+0FCh]
	jmp ClientSlots_MaskLoad_14018215E_done
ClientSlots_MaskLoad_14018215E_high:
	movsxd r11, dword ptr [r15]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rdx
	shl r11, 2
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_14018215E_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_14018215E ENDP

ClientSlots_MaskLoad_140197A13 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskLoad_140197A13_high
	mov eax, dword ptr [r13+0FCh]
	jmp ClientSlots_MaskLoad_140197A13_done
ClientSlots_MaskLoad_140197A13_high:
	movsxd r11, dword ptr [r13]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_140197A13_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_140197A13 ENDP

ClientSlots_MaskLoad_140197A8B PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskLoad_140197A8B_high
	mov eax, dword ptr [rax+0FCh]
	jmp ClientSlots_MaskLoad_140197A8B_done
ClientSlots_MaskLoad_140197A8B_high:
	movsxd r11, dword ptr [rax]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_140197A8B_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_140197A8B ENDP

ClientSlots_MaskLoad_1401A3419 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskLoad_1401A3419_high
	mov eax, dword ptr [rbp+0FCh]
	jmp ClientSlots_MaskLoad_1401A3419_done
ClientSlots_MaskLoad_1401A3419_high:
	movsxd r11, dword ptr [rbp]
	imul r11, r11, MASK_HIGH_WORDS * 4
	add r11, rdx
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_1401A3419_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_1401A3419 ENDP

ClientSlots_MaskLoad_1401A34BE PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskLoad_1401A34BE_high
	mov eax, dword ptr [rbx+0FCh]
	jmp ClientSlots_MaskLoad_1401A34BE_done
ClientSlots_MaskLoad_1401A34BE_high:
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_1401A34BE_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_1401A34BE ENDP

ClientSlots_MaskLoad_1401A3717 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	.endprolog

	cmp rcx, rbx
	jnz ClientSlots_MaskLoad_1401A3717_high
	mov eax, dword ptr [rcx+0FCh]
	jmp ClientSlots_MaskLoad_1401A3717_done
ClientSlots_MaskLoad_1401A3717_high:
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS * 4
	add r11, rcx
	sub r11, rbx
	lea rax, ClientSlots_clientMaskHigh
	mov eax, dword ptr [rax+r11-4]
ClientSlots_MaskLoad_1401A3717_done:
	pop r11
	popfq
	ret
ClientSlots_MaskLoad_1401A3717 ENDP

ClientSlots_MaskStore_1401627C4 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskStore_1401627C4_high
	mov dword ptr [rsi+0FCh], eax
	jmp ClientSlots_MaskStore_1401627C4_done
ClientSlots_MaskStore_1401627C4_high:
	movsxd r11, dword ptr [rsi]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rdx
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_1401627C4_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_1401627C4 ENDP

ClientSlots_MaskStore_140182169 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskStore_140182169_high
	mov dword ptr [r15+0FCh], eax
	jmp ClientSlots_MaskStore_140182169_done
ClientSlots_MaskStore_140182169_high:
	movsxd r11, dword ptr [r15]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rdx
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_140182169_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_140182169 ENDP

ClientSlots_MaskStore_140197A1E PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskStore_140197A1E_high
	mov dword ptr [r13+0FCh], eax
	jmp ClientSlots_MaskStore_140197A1E_done
ClientSlots_MaskStore_140197A1E_high:
	movsxd r11, dword ptr [r13]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_140197A1E_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_140197A1E ENDP

ClientSlots_MaskStore_140197A95 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskStore_140197A95_high
	mov dword ptr [r8+0FCh], eax
	jmp ClientSlots_MaskStore_140197A95_done
ClientSlots_MaskStore_140197A95_high:
	movsxd r11, dword ptr [r8]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_140197A95_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_140197A95 ENDP

ClientSlots_MaskStore_1401A3424 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rdx, rdx
	jnz ClientSlots_MaskStore_1401A3424_high
	mov dword ptr [rbp+0FCh], eax
	jmp ClientSlots_MaskStore_1401A3424_done
ClientSlots_MaskStore_1401A3424_high:
	movsxd r11, dword ptr [rbp]
	imul r11, r11, MASK_HIGH_WORDS * 4
	add r11, rdx
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_1401A3424_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_1401A3424 ENDP

ClientSlots_MaskStore_1401A34C8 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rcx, rcx
	jnz ClientSlots_MaskStore_1401A34C8_high
	mov dword ptr [rbx+0FCh], eax
	jmp ClientSlots_MaskStore_1401A34C8_done
ClientSlots_MaskStore_1401A34C8_high:
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rcx
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_1401A34C8_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_1401A34C8 ENDP

ClientSlots_MaskStore_1401A3720 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	cmp rcx, rbx
	jnz ClientSlots_MaskStore_1401A3720_high
	mov dword ptr [rcx+0FCh], eax
	jmp ClientSlots_MaskStore_1401A3720_done
ClientSlots_MaskStore_1401A3720_high:
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS * 4
	add r11, rcx
	sub r11, rbx
	lea r10, ClientSlots_clientMaskHigh
	mov dword ptr [r10+r11-4], eax
ClientSlots_MaskStore_1401A3720_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskStore_1401A3720 ENDP

ClientSlots_MaskAnd_1401629BD PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	test rax, rax
	jnz ClientSlots_MaskAnd_1401629BD_high
	and dword ptr [rbx+0FCh], esi
	jmp ClientSlots_MaskAnd_1401629BD_done
ClientSlots_MaskAnd_1401629BD_high:
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS
	add r11, rax
	shl r11, 2
	lea r10, ClientSlots_clientMaskHigh
	and dword ptr [r10+r11-4], esi
ClientSlots_MaskAnd_1401629BD_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskAnd_1401629BD ENDP

ClientSlots_MaskHideAll_1401629B3 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	mov dword ptr [rbx+0FCh], 0FFFFFFFFh
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	SetMaskRow r10, 0FFFFFFFFh
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskHideAll_1401629B3 ENDP

ClientSlots_MaskHideAll_140197A69 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	mov dword ptr [rax+0FCh], 0FFFFFFFFh
	movsxd r11, dword ptr [rax]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	SetMaskRow r10, 0FFFFFFFFh
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskHideAll_140197A69 ENDP

ClientSlots_MaskHideAll_1401A33C9 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	mov dword ptr [rbp+0FCh], 0FFFFFFFFh
	movsxd r11, dword ptr [rbp]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	SetMaskRow r10, 0FFFFFFFFh
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskHideAll_1401A33C9 ENDP

ClientSlots_MaskHideAll_1401A34A7 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	mov dword ptr [rbx+0FCh], 0FFFFFFFFh
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	SetMaskRow r10, 0FFFFFFFFh
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskHideAll_1401A34A7 ENDP

ClientSlots_MaskHideAll_1401A3672 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	mov dword ptr [rbx+0FCh], 0FFFFFFFFh
	movsxd r11, dword ptr [rbx]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	SetMaskRow r10, 0FFFFFFFFh
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskHideAll_1401A3672 ENDP

ClientSlots_MaskOnlyFor_140167BD7 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	push rcx
	.pushreg rcx
	.endprolog

	movzx ecx, bl
	movsxd r11, dword ptr [rax]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r10, r11
	mov dword ptr [rax+0FCh], 0FFFFFFFFh
	SetMaskRow r10, 0FFFFFFFFh
	mov r11d, ecx
	shr r11d, 5
	and ecx, 1Fh
	test r11d, r11d
	jnz ClientSlots_MaskOnlyFor_140167BD7_high
	lea r10, [rax+0FCh]
	jmp ClientSlots_MaskOnlyFor_140167BD7_clear
ClientSlots_MaskOnlyFor_140167BD7_high:
	lea r10, [r10+r11*4-4]
ClientSlots_MaskOnlyFor_140167BD7_clear:
	mov r11d, 1
	shl r11d, cl
	not r11d
	mov dword ptr [r10], r11d
	pop rcx
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskOnlyFor_140167BD7 ENDP

ClientSlots_MaskHiddenTest_14023F34E PROC FRAME
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	movsxd r11, dword ptr [r9]
	imul r11, r11, MASK_HIGH_WORDS * 4
	lea r10, ClientSlots_clientMaskHigh
	add r11, r10
	mov r10d, dword ptr [r9+0FCh]
	AndMaskRow r10d, r11
	cmp r10d, 0FFFFFFFFh
	pop r10
	pop r11
	ret
ClientSlots_MaskHiddenTest_14023F34E ENDP

ClientSlots_MaskForClient_140242864 PROC FRAME
	pushfq
	.allocstack 8
	push r11
	.pushreg r11
	push r10
	.pushreg r10
	.endprolog

	cmp edx, 32
	jae ClientSlots_MaskForClient_140242864_high
	mov eax, dword ptr [rcx+0FCh]
	jmp ClientSlots_MaskForClient_140242864_done
ClientSlots_MaskForClient_140242864_high:
	mov r10, qword ptr ClientSlots_snapshotRingBegin
	cmp rcx, r10
	jb ClientSlots_MaskForClient_140242864_baseline
	cmp rcx, qword ptr ClientSlots_snapshotRingEnd
	jae ClientSlots_MaskForClient_140242864_baseline
	mov r11, rcx
	sub r11, r10
	shr r11, 8
	lea r10, ClientSlots_snapshotMaskHigh
	jmp ClientSlots_MaskForClient_140242864_word
ClientSlots_MaskForClient_140242864_baseline:
	mov r10, qword ptr ClientSlots_baselineBegin
	cmp rcx, r10
	jb ClientSlots_MaskForClient_140242864_live
	cmp rcx, qword ptr ClientSlots_baselineEnd
	jae ClientSlots_MaskForClient_140242864_live
	mov r11, rcx
	sub r11, r10
	shr r11, 8
	lea r10, ClientSlots_baselineMaskHigh
	jmp ClientSlots_MaskForClient_140242864_word
ClientSlots_MaskForClient_140242864_live:
	movsxd r11, dword ptr [rcx]
	lea r10, ClientSlots_clientMaskHigh
ClientSlots_MaskForClient_140242864_word:
	imul r11, r11, MASK_HIGH_WORDS * 4
	add r10, r11
	mov eax, edx
	shr eax, 5
	mov eax, dword ptr [r10+rax*4-4]
ClientSlots_MaskForClient_140242864_done:
	pop r10
	pop r11
	popfq
	ret
ClientSlots_MaskForClient_140242864 ENDP


ClientSlots_ServerIndexBitsR8 PROC FRAME
	pushfq
	.allocstack 8
	push rax
	.pushreg rax
	.endprolog

	mov rax, ClientSlots_serverClientCount
	mov r8d, 5
	cmp dword ptr [rax], 18
	jle ClientSlots_ServerIndexBitsR8_done
	movzx r8d, byte ptr ClientSlots_wideClientBits
ClientSlots_ServerIndexBitsR8_done:
	pop rax
	popfq
	ret
ClientSlots_ServerIndexBitsR8 ENDP

ClientSlots_ServerIndexBitsStack PROC FRAME
	pushfq
	.allocstack 8
	push rax
	.pushreg rax
	.endprolog

	mov rax, ClientSlots_serverClientCount
	mov dword ptr [rsp+50h], 5
	cmp dword ptr [rax], 18
	jle ClientSlots_ServerIndexBitsStack_done
	movzx eax, byte ptr ClientSlots_wideClientBits
	mov dword ptr [rsp+50h], eax
ClientSlots_ServerIndexBitsStack_done:
	pop rax
	popfq
	ret
ClientSlots_ServerIndexBitsStack ENDP

ClientSlots_ServerIndexBitsEdx PROC FRAME
	pushfq
	.allocstack 8
	push rax
	.pushreg rax
	.endprolog

	mov rax, ClientSlots_serverClientCount
	mov edx, 5
	cmp dword ptr [rax], 18
	jle ClientSlots_ServerIndexBitsEdx_done
	movzx edx, byte ptr ClientSlots_wideClientBits
ClientSlots_ServerIndexBitsEdx_done:
	pop rax
	popfq
	ret
ClientSlots_ServerIndexBitsEdx ENDP

ClientSlots_ClientIndexBitsEdx PROC
	movzx edx, byte ptr ClientSlots_clientIndexBits
	ret
ClientSlots_ClientIndexBitsEdx ENDP

ClientSlots_BuildItemClientMask PROC FRAME
	push rcx
	.pushreg rcx
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

	movaps [rsp+20h], xmm0
	movaps [rsp+30h], xmm1
	movaps [rsp+40h], xmm2
	movaps [rsp+50h], xmm3
	movaps [rsp+60h], xmm4
	movaps [rsp+70h], xmm5
	call qword ptr ClientSlots_buildItemClientMaskBody
	movaps xmm0, [rsp+20h]
	movaps xmm1, [rsp+30h]
	movaps xmm2, [rsp+40h]
	movaps xmm3, [rsp+50h]
	movaps xmm4, [rsp+60h]
	movaps xmm5, [rsp+70h]

	add rsp, 80h
	pop r11
	pop r10
	pop r9
	pop r8
	pop rcx
	ret
ClientSlots_BuildItemClientMask ENDP

ClientSlots_GuardSessionGetXuid PROC
	cmp edx, 18
	jae ClientSlots_GuardSessionGetXuid_outside
	jmp qword ptr ClientSlots_sessionGetXuid
ClientSlots_GuardSessionGetXuid_outside:
	xor eax, eax
	ret
ClientSlots_GuardSessionGetXuid ENDP

ClientSlots_GuardSessionIsRegistered PROC
	cmp edx, 18
	jae ClientSlots_GuardSessionIsRegistered_outside
	jmp qword ptr ClientSlots_sessionIsRegistered
ClientSlots_GuardSessionIsRegistered_outside:
	xor eax, eax
	ret
ClientSlots_GuardSessionIsRegistered ENDP

ClientSlots_GuardPartyRemovePlayer PROC
	cmp edx, 18
	jae ClientSlots_GuardPartyRemovePlayer_outside
	jmp qword ptr ClientSlots_partyRemovePlayer
ClientSlots_GuardPartyRemovePlayer_outside:
	xor eax, eax
	ret
ClientSlots_GuardPartyRemovePlayer ENDP

ClientSlots_GuardPartyVoiceBits PROC
	cmp edx, 18
	jae ClientSlots_GuardPartyVoiceBits_outside
	jmp qword ptr ClientSlots_partyVoiceBits
ClientSlots_GuardPartyVoiceBits_outside:
	xor eax, eax
	ret
ClientSlots_GuardPartyVoiceBits ENDP

ClientSlots_GuardPartyMemberAddr PROC
	cmp edx, 18
	jae ClientSlots_GuardPartyMemberAddr_outside
	jmp qword ptr ClientSlots_partyMemberAddr
ClientSlots_GuardPartyMemberAddr_outside:
	xor eax, eax
	ret
ClientSlots_GuardPartyMemberAddr ENDP

ClientSlots_GuardSessionRegister PROC
	cmp dword ptr [rsp+28h], 18
	jae ClientSlots_GuardSessionRegister_outside
	jmp qword ptr ClientSlots_sessionRegister
ClientSlots_GuardSessionRegister_outside:
	xor eax, eax
	ret
ClientSlots_GuardSessionRegister ENDP

ClientSlots_GuardPlayerInfo PROC
	cmp ecx, 425 + 18
	jae ClientSlots_GuardPlayerInfo_outside
	jmp qword ptr ClientSlots_setConfigstring
ClientSlots_GuardPlayerInfo_outside:
	xor eax, eax
	ret
ClientSlots_GuardPlayerInfo ENDP

ClientSlots_GuardClientInMyParty PROC
	cmp edx, 18
	jae ClientSlots_GuardClientInMyParty_outside
	jmp qword ptr ClientSlots_clientInMyParty
ClientSlots_GuardClientInMyParty_outside:
	xor eax, eax
	ret
ClientSlots_GuardClientInMyParty ENDP

ClientSlots_GuardPlayerMuted PROC
	cmp r8d, 18
	jae ClientSlots_GuardPlayerMuted_outside
	jmp qword ptr ClientSlots_playerMuted
ClientSlots_GuardPlayerMuted_outside:
	xor eax, eax
	ret
ClientSlots_GuardPlayerMuted ENDP

ClientSlots_GuardPlayerTalking PROC
	cmp r8d, 18
	jae ClientSlots_GuardPlayerTalking_outside
	jmp qword ptr ClientSlots_playerTalking
ClientSlots_GuardPlayerTalking_outside:
	xor eax, eax
	ret
ClientSlots_GuardPlayerTalking ENDP

ClientSlots_ScriptChildAddedEbx PROC FRAME
	push rax
	.pushreg rax
	push rcx
	.pushreg rcx
	.endprolog

	mov eax, ebx
	sub eax, dword ptr ClientSlots_scriptChild0Begin
	add eax, 0FFFFh
	shr eax, 16
	mov rcx, qword ptr ClientSlots_scriptChildCounts
	inc dword ptr [rcx+rax*4]

	pop rcx
	pop rax
	cmp eax, eax
	ret
ClientSlots_ScriptChildAddedEbx ENDP

ClientSlots_ScriptChildFreedEbx PROC FRAME
	push rax
	.pushreg rax
	push rcx
	.pushreg rcx
	.endprolog

	mov eax, ebx
	sub eax, dword ptr ClientSlots_scriptChild0Begin
	add eax, 0FFFFh
	shr eax, 16
	mov rcx, qword ptr ClientSlots_scriptChildCounts
	dec dword ptr [rcx+rax*4]

	pop rcx
	pop rax
	cmp eax, eax
	ret
ClientSlots_ScriptChildFreedEbx ENDP

ClientSlots_ScriptChildFreedEdi PROC FRAME
	push rax
	.pushreg rax
	push rcx
	.pushreg rcx
	.endprolog

	mov eax, edi
	sub eax, dword ptr ClientSlots_scriptChild0Begin
	add eax, 0FFFFh
	shr eax, 16
	mov rcx, qword ptr ClientSlots_scriptChildCounts
	dec dword ptr [rcx+rax*4]

	pop rcx
	pop rax
	cmp eax, eax
	ret
ClientSlots_ScriptChildFreedEdi ENDP

ClientSlots_ScriptPoolExhaustedEax PROC
	mov ecx, eax
	and rsp, -16
	sub rsp, 20h
	call qword ptr ClientSlots_childPoolExhaustedBody
	int 3
ClientSlots_ScriptPoolExhaustedEax ENDP

ClientSlots_ScriptPoolExhaustedEcx PROC
	and rsp, -16
	sub rsp, 20h
	call qword ptr ClientSlots_childPoolExhaustedBody
	int 3
ClientSlots_ScriptPoolExhaustedEcx ENDP

ClientSlots_AllocVariable PROC FRAME
	push r11
	.pushreg r11
	sub rsp, 80h
	.allocstack 80h
	.endprolog

	movaps [rsp+20h], xmm0
	movaps [rsp+30h], xmm1
	movaps [rsp+40h], xmm2
	movaps [rsp+50h], xmm3
	movaps [rsp+60h], xmm4
	movaps [rsp+70h], xmm5
	call qword ptr ClientSlots_allocVariableBody
	movaps xmm0, [rsp+20h]
	movaps xmm1, [rsp+30h]
	movaps xmm2, [rsp+40h]
	movaps xmm3, [rsp+50h]
	movaps xmm4, [rsp+60h]
	movaps xmm5, [rsp+70h]

	add rsp, 80h
	pop r11
	ret
ClientSlots_AllocVariable ENDP

ClientSlots_ScrInitVariables PROC FRAME
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

	movaps [rsp+20h], xmm0
	movaps [rsp+30h], xmm1
	movaps [rsp+40h], xmm2
	movaps [rsp+50h], xmm3
	movaps [rsp+60h], xmm4
	movaps [rsp+70h], xmm5
	call qword ptr ClientSlots_initVariablesBody
	movaps xmm0, [rsp+20h]
	movaps xmm1, [rsp+30h]
	movaps xmm2, [rsp+40h]
	movaps xmm3, [rsp+50h]
	movaps xmm4, [rsp+60h]
	movaps xmm5, [rsp+70h]

	add rsp, 88h
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
	ret
ClientSlots_ScrInitVariables ENDP

END
