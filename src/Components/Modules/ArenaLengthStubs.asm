EXTERN ArenaLength_I_strncpyz:QWORD
EXTERN ArenaLength_I_stricmp:QWORD
EXTERN ArenaLength_Dvar_SetStringByName:QWORD
EXTERN ArenaLength_va:QWORD

.code

MapNameCopyStub PROC
	mov r8d, 20h
	jmp qword ptr [ArenaLength_I_strncpyz]
MapNameCopyStub ENDP

MapNameCompareStub PROC
	lea rdx, [rbx + 0B00h]
	jmp qword ptr [ArenaLength_I_stricmp]
MapNameCompareStub ENDP

FeederMapNameStub PROC
	lea rdx, [rax + 0B00h]
	jmp qword ptr [ArenaLength_Dvar_SetStringByName]
FeederMapNameStub ENDP

MapCommandStub PROC
	lea rdx, [rbx + 0B00h]
	jmp qword ptr [ArenaLength_va]
MapCommandStub ENDP

END
