EXTERN db_hashTableHighDelta:QWORD
EXTERN db_hashTableRebased:QWORD
EXTERN g_assetEntryPoolMoved:QWORD

.code

XModelKeyStub PROC
	mov rdx, r13
	and edx, 1
	shl rdx, 16h
	mov r8, r10
	shl r8, 18h
	or rdx, r8
	mov r8, 0FE1FFFFFFFBF0000h
	and rcx, r8
	ret
XModelKeyStub ENDP

BModelProbeStub PROC
	shr rax, 10h
	mov r8b, 72h
	movzx ecx, al
	and ecx, 3Fh
	ret
BModelProbeStub ENDP

SkinnedProbeStub PROC
	shr rsi, 10h
	mov edx, 1
	movzx ecx, sil
	and ecx, 3Fh
	ret
SkinnedProbeStub ENDP

RigidSkinnedProbeStub PROC
	shr rbx, 10h
	movzx edx, bl
	and edx, 3Fh
	ret
RigidSkinnedProbeStub ENDP

RigidProbeStub PROC
	shr rax, 10h
	movzx edx, al
	and edx, 3Fh
	ret
RigidProbeStub ENDP

WorldSlotStub PROC
	shl eax, 0Dh
	shr edx, 2
	or eax, edx
	ret
WorldSlotStub ENDP

DwordSlotStub PROC
	shr r9d, 2
	and r9d, 0FFFh
	ret
DwordSlotStub ENDP

SModelSlotStub PROC
	shl eax, 0Dh
	shr r8d, 2
	or eax, r8d
	ret
SModelSlotStub ENDP

SetupWorldIndexStub PROC
	shr rax, 1Ch
	and eax, 3FFFh
	shr r8, 2Dh
	mov ecx, eax
	ret
SetupWorldIndexStub ENDP

SetupSlotStub PROC
	shl edx, 0Dh
	shr ecx, 2
	or edx, ecx
	ret
SetupSlotStub ENDP

SetupSModelIndexStub PROC
	and eax, 3FFFh
	movzx ecx, ax
	ret
SetupSModelIndexStub ENDP

SetupDwordSlotStub PROC
	shl edx, 0Ch
	shr r9d, 2
	and r9d, esi
	ret
SetupDwordSlotStub ENDP

SetupWordSlotStub PROC
	movzx eax, cx
	and edx, 3Fh
	shr eax, 2
	and eax, esi
	ret
SetupWordSlotStub ENDP

SunShadowStreamStub PROC
	mov eax, r8d
	shl eax, 2
	lock xadd dword ptr [rdx + 41964h], eax
	mov r8d, eax
	ret
SunShadowStreamStub ENDP


BucketRdiEcxStub PROC
	mov rcx, qword ptr [db_hashTableHighDelta]
	add rcx, rdi
	movzx ecx, byte ptr [rcx+rax*2+150B520h]
	shl ecx, 16
	mov cx, word ptr [rdi+rax*2+150B520h]
	test ecx, ecx
	ret
BucketRdiEcxStub ENDP

HashRbxEcxStub PROC
	movzx ecx, byte ptr [rbx+12h+4]
	shl ecx, 16
	mov cx, word ptr [rbx+12h]
	test ecx, ecx
	ret
HashRbxEcxStub ENDP

BucketR14EcxStub PROC
	mov rcx, qword ptr [db_hashTableHighDelta]
	add rcx, r14
	movzx ecx, byte ptr [rcx+rax*2+150B520h]
	shl ecx, 16
	mov cx, word ptr [r14+rax*2+150B520h]
	test ecx, ecx
	ret
BucketR14EcxStub ENDP

OverrideRbxEaxStub PROC
	movzx eax, byte ptr [rbx+14h+3]
	shl eax, 16
	mov ax, word ptr [rbx+14h]
	test eax, eax
	ret
OverrideRbxEaxStub ENDP

OverrideWalkRbxRcxStub PROC
	movzx eax, byte ptr [rbx+rcx*8+17h]
	shl eax, 16
	mov ax, word ptr [rbx+rcx*8+14h]
	lea rbx, [rbx+rcx*8]
	test eax, eax
	ret
OverrideWalkRbxRcxStub ENDP

BucketR8EcxStub PROC
	mov rcx, qword ptr [db_hashTableHighDelta]
	add rcx, r8
	movzx ecx, byte ptr [rcx+rax*2+150B520h]
	shl ecx, 16
	mov cx, word ptr [r8+rax*2+150B520h]
	test ecx, ecx
	ret
BucketR8EcxStub ENDP

HashRdxEcxStub PROC
	movzx ecx, byte ptr [rdx+12h+4]
	shl ecx, 16
	mov cx, word ptr [rdx+12h]
	test ecx, ecx
	ret
HashRdxEcxStub ENDP

CreateDefaultLinkStub PROC
	pushfq
	mov rax, qword ptr [db_hashTableHighDelta]
	add rax, r14
	movzx eax, byte ptr [rax+r8]
	shl eax, 16
	mov ax, word ptr [r8+r14]
	mov word ptr [rbp+12h], ax
	shr eax, 16
	mov byte ptr [rbp+16h], al
	popfq
	ret
CreateDefaultLinkStub ENDP

CreateDefaultHeadStub PROC
	mov word ptr [r8+r14], dx
	pushfq
	push rdx
	push rax
	mov rax, qword ptr [db_hashTableHighDelta]
	add rax, r14
	shr rdx, 16
	mov byte ptr [rax+r8], dl
	pop rax
	pop rdx
	popfq
	ret
CreateDefaultHeadStub ENDP

BucketRaxEsiStub PROC
	mov rsi, qword ptr [db_hashTableHighDelta]
	movzx esi, byte ptr [rax+rsi]
	shl esi, 16
	mov si, word ptr [rax]
	test esi, esi
	ret
BucketRaxEsiStub ENDP

HashRbxEsiStub PROC
	movzx esi, byte ptr [rbx+12h+4]
	shl esi, 16
	mov si, word ptr [rbx+12h]
	test esi, esi
	ret
HashRbxEsiStub ENDP

AddHashLinkStub PROC
	pushfq
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r8+rax]
	shl eax, 16
	mov ax, word ptr [r8]
	mov word ptr [r14+12h], ax
	shr eax, 16
	mov byte ptr [r14+16h], al
	popfq
	ret
AddHashLinkStub ENDP

AddHeadStub PROC
	mov word ptr [r8], dx
	pushfq
	push rdx
	push rax
	mov rax, qword ptr [db_hashTableHighDelta]
	shr rdx, 16
	mov byte ptr [r8+rax], dl
	pop rax
	pop rdx
	popfq
	mov rax, qword ptr [r12]
	ret
AddHeadStub ENDP

AddOverrideCopyStub PROC
	pushfq
	movzx eax, byte ptr [rbx+17h]
	shl eax, 16
	mov ax, word ptr [rbx+14h]
	mov word ptr [r14+14h], ax
	shr eax, 16
	mov byte ptr [r14+17h], al
	popfq
	ret
AddOverrideCopyStub ENDP

AddOverrideHeadStub PROC
	add rdx, rax
	mov word ptr [rbx+14h], dx
	pushfq
	push rdx
	shr rdx, 16
	mov byte ptr [rbx+14h+3], dl
	pop rdx
	popfq
	ret
AddOverrideHeadStub ENDP

AllocClearLinksStub PROC
	mov dword ptr [rdi+12h], eax
	mov word ptr [rdi+16h], 0
	mov rax, rdi
	ret
AllocClearLinksStub ENDP

BucketR13RaxEcxStub PROC
	mov rcx, qword ptr [db_hashTableHighDelta]
	add rcx, r13
	movzx ecx, byte ptr [rcx+rax*2]
	shl ecx, 16
	mov cx, word ptr [r13+rax*2]
	test ecx, ecx
	ret
BucketR13RaxEcxStub ENDP

HashR12EcxStub PROC
	movzx ecx, byte ptr [r12+12h+4]
	shl ecx, 16
	mov cx, word ptr [r12+12h]
	test ecx, ecx
	ret
HashR12EcxStub ENDP

LinkOverrideCopyStub PROC
	pushfq
	movzx eax, byte ptr [r12+17h]
	shl eax, 16
	mov ax, word ptr [r12+14h]
	mov word ptr [rbx+14h], ax
	shr eax, 16
	mov byte ptr [rbx+17h], al
	popfq
	ret
LinkOverrideCopyStub ENDP

LinkOverrideHeadStub PROC
	mov word ptr [r12+14h], dx
	pushfq
	push rdx
	shr rdx, 16
	mov byte ptr [r12+14h+3], dl
	pop rdx
	popfq
	ret
LinkOverrideHeadStub ENDP

BucketR13EaxStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r13+rax]
	shl eax, 16
	mov ax, word ptr [r13]
	test eax, eax
	ret
BucketR13EaxStub ENDP

OverrideR11RsiEaxStub PROC
	movzx eax, byte ptr [r11+rsi+14h+3]
	shl eax, 16
	mov ax, word ptr [r11+rsi+14h]
	test eax, eax
	ret
OverrideR11RsiEaxStub ENDP

HashRbxEaxStub PROC
	movzx eax, byte ptr [rbx+12h+4]
	shl eax, 16
	mov ax, word ptr [rbx+12h]
	test eax, eax
	ret
HashRbxEaxStub ENDP

UnloadBucketStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r12+rax]
	shl eax, 16
	mov ax, word ptr [r12]
	mov rbx, r12
	test eax, eax
	ret
UnloadBucketStub ENDP

OverrideRdiEaxStub PROC
	movzx eax, byte ptr [rdi+14h+3]
	shl eax, 16
	mov ax, word ptr [rdi+14h]
	test eax, eax
	ret
OverrideRdiEaxStub ENDP

UnloadUnlinkHeadStub PROC
	pushfq
	movzx eax, byte ptr [rdi+12h+4]
	shl eax, 16
	mov ax, word ptr [rdi+12h]
	mov word ptr [rbx], ax
	push rcx
	mov rcx, rbx
	sub rcx, qword ptr [db_hashTableRebased]
	cmp rcx, 74000
	jae UnloadUnlinkHead_entry
	mov rcx, qword ptr [db_hashTableHighDelta]
	jmp UnloadUnlinkHead_done
UnloadUnlinkHead_entry:
	mov rcx, 4
UnloadUnlinkHead_done:
	shr eax, 16
	mov byte ptr [rbx+rcx], al
	pop rcx
	popfq
	ret
UnloadUnlinkHeadStub ENDP

UnloadPromoteStub PROC
	pushfq
	movzx eax, byte ptr [rbx+17h]
	shl eax, 16
	mov ax, word ptr [rbx+14h]
	mov word ptr [rdi+14h], ax
	shr eax, 16
	mov byte ptr [rdi+17h], al
	popfq
	ret
UnloadPromoteStub ENDP

UnloadOverridesStub PROC
	movzx eax, byte ptr [rdi+14h+3]
	shl eax, 16
	mov ax, word ptr [rdi+14h]
	lea rsi, [rdi+14h]
	test eax, eax
	ret
UnloadOverridesStub ENDP

UnloadUnlinkOverrideStub PROC
	pushfq
	movzx eax, byte ptr [rbx+17h]
	shl eax, 16
	mov ax, word ptr [rbx+14h]
	mov word ptr [rsi], ax
	shr eax, 16
	mov byte ptr [rsi+3], al
	popfq
	ret
UnloadUnlinkOverrideStub ENDP

OverrideSlotRsiStub PROC
	movzx eax, byte ptr [rsi+3]
	shl eax, 16
	mov ax, word ptr [rsi]
	test eax, eax
	ret
OverrideSlotRsiStub ENDP

UnloadHashSlotStub PROC
	push rcx
	mov rcx, rbx
	sub rcx, qword ptr [db_hashTableRebased]
	cmp rcx, 74000
	jae UnloadHashSlot_entry
	mov rcx, qword ptr [db_hashTableHighDelta]
	jmp UnloadHashSlot_done
UnloadHashSlot_entry:
	mov rcx, 4
UnloadHashSlot_done:
	movzx eax, byte ptr [rbx+rcx]
	pop rcx
	shl eax, 16
	mov ax, word ptr [rbx]
	test eax, eax
	ret
UnloadHashSlotStub ENDP

BucketRdiEaxStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [rdi+rax]
	shl eax, 16
	mov ax, word ptr [rdi]
	test eax, eax
	ret
BucketRdiEaxStub ENDP

LoadOverridePoolStub PROC
	push rcx
	mov rcx, qword ptr [g_assetEntryPoolMoved]
	add rcx, r11
	movzx eax, byte ptr [rcx+17h]
	shl eax, 16
	mov ax, word ptr [rcx+14h]
	pop rcx
	test eax, eax
	ret
LoadOverridePoolStub ENDP

BucketRdxEaxStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [rdx+rax]
	shl eax, 16
	mov ax, word ptr [rdx]
	test eax, eax
	ret
BucketRdxEaxStub ENDP

ReleaseHashStub PROC
	push rcx
	lea rcx, [r9+rax*8]
	movzx eax, byte ptr [rcx+16h]
	shl eax, 16
	mov ax, word ptr [rcx+12h]
	pop rcx
	test eax, eax
	ret
ReleaseHashStub ENDP

BucketRsiEdiStub PROC
	mov rdi, qword ptr [db_hashTableHighDelta]
	movzx edi, byte ptr [rsi+rdi]
	shl edi, 16
	mov di, word ptr [rsi]
	test edi, edi
	ret
BucketRsiEdiStub ENDP

ShutdownHashStub PROC
	pushfq
	mov rdx, qword ptr [rbx+8]
	movzx edi, byte ptr [rbx+12h+4]
	shl edi, 16
	mov di, word ptr [rbx+12h]
	popfq
	ret
ShutdownHashStub ENDP

ShutdownClearBucketStub PROC
	mov word ptr [rsi], r15w
	pushfq
	push r15
	push rax
	mov rax, qword ptr [db_hashTableHighDelta]
	shr r15, 16
	mov byte ptr [rsi+rax], r15b
	pop rax
	pop r15
	popfq
	add rsi, 2
	ret
ShutdownClearBucketStub ENDP

FreeHashPoolStub PROC
	push r11
	mov r11, qword ptr [g_assetEntryPoolMoved]
	lea r11, [r11+rcx*8]
	movzx eax, byte ptr [r11+16h]
	shl eax, 16
	mov ax, word ptr [r11+12h]
	pop r11
	test eax, eax
	ret
FreeHashPoolStub ENDP

FreeBucketStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r14+rax]
	shl eax, 16
	mov ax, word ptr [r14]
	mov rdi, r14
	test eax, eax
	ret
FreeBucketStub ENDP

FreeUnlinkStub PROC
	pushfq
	movzx eax, byte ptr [rbx+12h+4]
	shl eax, 16
	mov ax, word ptr [rbx+12h]
	mov word ptr [rdi], ax
	push rcx
	mov rcx, rdi
	sub rcx, qword ptr [db_hashTableRebased]
	cmp rcx, 74000
	jae FreeUnlink_entry
	mov rcx, qword ptr [db_hashTableHighDelta]
	jmp FreeUnlink_done
FreeUnlink_entry:
	mov rcx, 4
FreeUnlink_done:
	shr eax, 16
	mov byte ptr [rdi+rcx], al
	pop rcx
	popfq
	movsxd rax, dword ptr [rbx]
	ret
FreeUnlinkStub ENDP

FreeHashSlotStub PROC
	push rcx
	mov rcx, rdi
	sub rcx, qword ptr [db_hashTableRebased]
	cmp rcx, 74000
	jae FreeHashSlot_entry
	mov rcx, qword ptr [db_hashTableHighDelta]
	jmp FreeHashSlot_done
FreeHashSlot_entry:
	mov rcx, 4
FreeHashSlot_done:
	movzx eax, byte ptr [rdi+rcx]
	pop rcx
	shl eax, 16
	mov ax, word ptr [rdi]
	test eax, eax
	ret
FreeHashSlotStub ENDP

BucketR12EaxStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r12+rax]
	shl eax, 16
	mov ax, word ptr [r12]
	test eax, eax
	ret
BucketR12EaxStub ENDP

OverrideRbxR15EaxStub PROC
	movzx eax, byte ptr [rbx+r15+14h+3]
	shl eax, 16
	mov ax, word ptr [rbx+r15+14h]
	test eax, eax
	ret
OverrideRbxR15EaxStub ENDP

HashRdiEaxStub PROC
	movzx eax, byte ptr [rdi+12h+4]
	shl eax, 16
	mov ax, word ptr [rdi+12h]
	test eax, eax
	ret
HashRdiEaxStub ENDP

BucketR8EaxStub PROC
	mov rax, qword ptr [db_hashTableHighDelta]
	movzx eax, byte ptr [r8+rax]
	shl eax, 16
	mov ax, word ptr [r8]
	test eax, eax
	ret
BucketR8EaxStub ENDP

HashRcxEaxStub PROC
	movzx eax, byte ptr [rcx+12h+4]
	shl eax, 16
	mov ax, word ptr [rcx+12h]
	test eax, eax
	ret
HashRcxEaxStub ENDP

END
