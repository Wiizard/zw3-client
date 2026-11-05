import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {test} from 'node:test';
const read=file=>readFileSync(new URL('../'+file,import.meta.url),'utf8');
const threading=read('src/Components/Modules/Threading.cpp');
const dedicated=read('src/Components/Modules/Dedicated.cpp');
const asm=read('src/Components/Modules/DedicatedStubs.asm');
const network=read('src/Components/Modules/Network.cpp');
test('dedicated pacing does not sleep or dispatch VM events inside SV_Frame',()=>{
 assert.doesNotMatch(threading,/SV_CheckLoadGame_Hk|serverFrameWaitHook|NetSleep\(50 - residual/);
 assert.match(threading,/if \(Dedicated::IsEnabled\(\)\)[\s\S]*?return;/);
});
test('dedicated raw elapsed time rejoins native timescale processing with rollback and byte guards',()=>{
 assert.match(dedicated,/Com_Frame_ModifyMsecCall = 0x1401F4468/);
 assert.match(dedicated,/Com_Frame_SlowMotion = 0x1401F4603/);
 assert.match(dedicated,/BranchesTo\(Com_Frame_ModifyMsecCall, CL_ModifyMsec, HOOK_CALL\)/);
 assert.match(dedicated,/frameMsecHook\.Uninstall\(\)/);
 assert.match(asm,/mov r9d, ecx\s+mov ebx, ecx\s+xorps xmm3, xmm3\s+add rsp, 8\s+jmp qword ptr \[Dedicated_FrameMsec_Resume\]/);
 assert.match(dedicated,/BranchesTo\(Com_Frame_ServerFrameCall, SV_Frame, HOOK_CALL\)/);
 assert.match(dedicated,/DedicatedTimeBudget::Reconcile\(\*Game::svs_time, target, residual\)/);
 assert.match(dedicated,/serverBudgetHook\.Uninstall\(\)/);
});
test('snapshot scheduling keeps the native sender and only adjusts active rate-limited dedicated snapshots',()=>{
 assert.match(network,/if \(!Dedicated::IsEnabled\(\)\) return;/);
 assert.match(network,/BranchesTo\(SV_SendClientSnapshot_SendMessageCall, SV_SendMessageToClient, HOOK_CALL\)/);
 assert.match(network,/active && client->header.state == Game::CS_ACTIVE && rateDelayed/);
 assert.match(network,/before == \*Game::svs_time/);
 assert.match(network,/SnapshotRatePolicy::Deadline\(before, previous, client->nextSnapshotTime\)/);
});
