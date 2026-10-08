EXTERN PlayerMovement_bg_bounces:QWORD
EXTERN PlayerMovement_bg_bouncesAllAngles:QWORD

EXTERN PlayerMovement_StepSlideMoveSlopeTest:QWORD
EXTERN PlayerMovement_StepSlideMoveProject:QWORD
EXTERN PlayerMovement_StepSlideMoveRestore:QWORD
EXTERN PlayerMovement_ProjectVelocityScale:QWORD
EXTERN PlayerMovement_ProjectVelocityDone:QWORD

EXTERN PlayerMovement_bg_disableLandingSlowdown:QWORD
EXTERN PlayerMovement_bg_bunnyHopAuto:QWORD

EXTERN PlayerMovement_CrashLandScaleNext:QWORD
EXTERN PlayerMovement_CrashLandScaleDone:QWORD
EXTERN PlayerMovement_JumpCheckHeldBranch:QWORD
EXTERN PlayerMovement_JumpCheckJump:QWORD

EXTERN PlayerMovement_ApplyRocketJump:PROC
EXTERN PlayerMovement_G_FireRocket:QWORD

EXTERN PlayerMovement_bg_ladderFixedInput:QWORD

EXTERN PlayerMovement_bg_sprintIgnoreRepress:QWORD
EXTERN PlayerMovement_SprintRepressEnd:QWORD
EXTERN PlayerMovement_SprintRepressDone:QWORD

EXTERN PlayerMovement_bg_dive:QWORD
EXTERN PlayerMovement_bg_omnimovementDive:QWORD
EXTERN PlayerMovement_BackDiveScale:QWORD

EXTERN PlayerMovement_bg_omnimovement:QWORD
EXTERN PlayerMovement_KeyMoveSprintBlock:QWORD
EXTERN PlayerMovement_KeyMoveSprintDone:QWORD
EXTERN PlayerMovement_WalkMoveSprintScale:QWORD
EXTERN PlayerMovement_WalkMoveJumpCheck:QWORD
EXTERN PlayerMovement_player_backSpeedScale:QWORD
EXTERN PlayerMovement_MaxSpeedBackDiagonal:QWORD
EXTERN PlayerMovement_MaxSpeedBackPure:QWORD
EXTERN PlayerMovement_MaxSpeedDone:QWORD

.code

StepSlideMoveStub PROC
	mov rax, qword ptr [PlayerMovement_bg_bounces]
	cmp dword ptr [rax + 10h], 0
	jle stock

	mov rax, qword ptr [PlayerMovement_bg_bouncesAllAngles]
	cmp dword ptr [rax + 10h], 2
	je project

	jmp qword ptr [PlayerMovement_StepSlideMoveSlopeTest]

project:
	jmp qword ptr [PlayerMovement_StepSlideMoveProject]

stock:
	test r13d, r13d
	jnz restore

	jmp qword ptr [PlayerMovement_StepSlideMoveSlopeTest]

restore:
	jmp qword ptr [PlayerMovement_StepSlideMoveRestore]
StepSlideMoveStub ENDP

ProjectVelocityStub PROC
	mov rax, qword ptr [PlayerMovement_bg_bouncesAllAngles]
	cmp dword ptr [rax + 10h], 0
	jne project

	comiss xmm10, xmm9
	jbe done

project:
	jmp qword ptr [PlayerMovement_ProjectVelocityScale]

done:
	jmp qword ptr [PlayerMovement_ProjectVelocityDone]
ProjectVelocityStub ENDP

CrashLandScaleStub PROC
	mov rax, qword ptr [PlayerMovement_bg_disableLandingSlowdown]
	cmp byte ptr [rax + 10h], 0
	jne skip

	movss xmm0, dword ptr [rdi + 28h]
	jmp qword ptr [PlayerMovement_CrashLandScaleNext]

skip:
	jmp qword ptr [PlayerMovement_CrashLandScaleDone]
CrashLandScaleStub ENDP

JumpCheckStub PROC
	mov rcx, qword ptr [PlayerMovement_bg_bunnyHopAuto]
	cmp byte ptr [rcx + 10h], 1
	je autoHop

	test dword ptr [rdi + 34h], 400h
	jmp qword ptr [PlayerMovement_JumpCheckHeldBranch]

autoHop:
	jmp qword ptr [PlayerMovement_JumpCheckJump]
JumpCheckStub ENDP

RocketFireStub PROC FRAME
	sub rsp, 48h
	.allocstack 48h
	.endprolog

	mov rax, qword ptr [rsp + 70h]
	mov qword ptr [rsp + 20h], rax
	mov rax, qword ptr [rsp + 78h]
	mov qword ptr [rsp + 28h], rax
	mov rax, qword ptr [rsp + 80h]
	mov qword ptr [rsp + 30h], rax
	call qword ptr [PlayerMovement_G_FireRocket]
	mov qword ptr [rsp + 38h], rax

	mov rcx, rbx
	lea rdx, [rbp - 80h]
	call PlayerMovement_ApplyRocketJump

	mov rax, qword ptr [rsp + 38h]
	add rsp, 48h
	ret
RocketFireStub ENDP

LadderClimbRateStub PROC
	movaps xmm7, xmm8

	mov rax, qword ptr [PlayerMovement_bg_ladderFixedInput]
	cmp byte ptr [rax + 10h], 0
	jne done

	minss xmm7, xmm0

done:
	ret
LadderClimbRateStub ENDP

SprintRepressStub PROC
	mov rax, qword ptr [PlayerMovement_bg_sprintIgnoreRepress]
	cmp byte ptr [rax + 10h], 1
	je skip

	mov r8, rsi
	mov rdx, rdi
	jmp qword ptr [PlayerMovement_SprintRepressEnd]

skip:
	jmp qword ptr [PlayerMovement_SprintRepressDone]
SprintRepressStub ENDP

DivePerkTestStub PROC
	mov r10, qword ptr [PlayerMovement_bg_dive]
	cmp byte ptr [r10 + 10h], 0
	jne skipPerk

	test dword ptr [rbx + 428h], 100000h
	ret

skipPerk:
	test r10, r10
	ret
DivePerkTestStub ENDP

BackDiveScaleStub PROC
	mov r10, qword ptr [PlayerMovement_bg_omnimovementDive]
	cmp byte ptr [r10 + 10h], 0
	jne done

	mov r10, qword ptr [PlayerMovement_BackDiveScale]
	mulss xmm2, dword ptr [r10]

done:
	ret
BackDiveScaleStub ENDP

KeyMoveSprintBitStub PROC
	mov rax, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [rax + 10h], 0
	jne sprintBlock

	cmp byte ptr [r10 + 4Ch], 0
	jnz skip

sprintBlock:
	jmp qword ptr [PlayerMovement_KeyMoveSprintBlock]

skip:
	jmp qword ptr [PlayerMovement_KeyMoveSprintDone]
KeyMoveSprintBitStub ENDP

SprintIntentStub PROC
	movsx ecx, byte ptr [rdi + 22h]

	mov r10, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [r10 + 10h], 0
	je compare

	mov r10d, ecx
	neg r10d
	cmovs r10d, ecx

	movsx r11d, byte ptr [rdi + 23h]
	mov ecx, r11d
	neg ecx
	cmovs ecx, r11d

	cmp ecx, r10d
	cmovl ecx, r10d

compare:
	cmp ecx, dword ptr [rax + 10h]
	ret
SprintIntentStub ENDP

WalkMoveSprintStrafeStub PROC
	mov rcx, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [rcx + 10h], 0
	jne jumpCheck

	bt eax, 0Eh
	jnb jumpCheck

	jmp qword ptr [PlayerMovement_WalkMoveSprintScale]

jumpCheck:
	jmp qword ptr [PlayerMovement_WalkMoveJumpCheck]
WalkMoveSprintStrafeStub ENDP

MaxSpeedBackDiagonalStub PROC
	test ebp, ebp
	jz stock

	mov rax, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [rax + 10h], 0
	jne skip

stock:
	test cl, cl
	jns skip

	mov rax, qword ptr [PlayerMovement_player_backSpeedScale]
	mov rax, qword ptr [rax]
	jmp qword ptr [PlayerMovement_MaxSpeedBackDiagonal]

skip:
	jmp qword ptr [PlayerMovement_MaxSpeedDone]
MaxSpeedBackDiagonalStub ENDP

MaxSpeedBackPureStub PROC
	test ebp, ebp
	jz stock

	mov rax, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [rax + 10h], 0
	jne skip

stock:
	test cl, cl
	jns skip

	mov rax, qword ptr [PlayerMovement_player_backSpeedScale]
	mov rax, qword ptr [rax]
	jmp qword ptr [PlayerMovement_MaxSpeedBackPure]

skip:
	jmp qword ptr [PlayerMovement_MaxSpeedDone]
MaxSpeedBackPureStub ENDP

MovementDirClampStub PROC
	mov r8d, 5Ah

	mov rax, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [rax + 10h], 0
	je clamp

	mov r8d, 7Fh

clamp:
	cmp ecx, r8d
	jle notAbove

	mov ecx, r8d

notAbove:
	neg r8d
	cmp ecx, r8d
	jge done

	mov ecx, r8d

done:
	ret
MovementDirClampStub ENDP

StrafeConditionStub PROC
	mov rax, qword ptr [rsi]
	mov edx, 7

	mov r10, qword ptr [PlayerMovement_bg_omnimovement]
	cmp byte ptr [r10 + 10h], 0
	je done

	test dword ptr [rax + 0Ch], 4000h
	jz done

	xor r8d, r8d

done:
	ret
StrafeConditionStub ENDP

END
