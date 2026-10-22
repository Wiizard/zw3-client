import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/Threading.cpp', import.meta.url), 'utf8').replace(/\r\n/g, '\n');
const wait = source.slice(source.indexOf('static void NetSleep('), source.indexOf('int Threading_ComFrameWait(int minMsec)\n\t{'));
const server = source.slice(source.indexOf('static int SV_CheckLoadGame_Hk()'), source.indexOf('Threading::Threading()'));
const client = source.slice(source.indexOf('int Threading_ComFrameWait(int minMsec)\n\t{'), source.indexOf('static int SV_CheckLoadGame_Hk()'));

test('dedicated pacing waits passively and never dispatches packets or script events', () => {
  assert.match(server, /NetSleep\(50 - residual, false\);/);
  assert.doesNotMatch(server, /NetSleep\(50 - residual\);|Com_ServerPacketEvent\)\)\(\)|Com_EventLoop\)\)\(\)/);
  assert.match(wait, /const bool dispatchEvents = true/);
  const guard = wait.indexOf('if (readyCount <= 0 || !dispatchEvents)');
  const nativeDispatch = wait.indexOf('const auto* const runningDvar');
  assert.ok(guard > 0 && nativeDispatch > guard);
  assert.match(wait.slice(guard, nativeDispatch), /return;/);
});

test('passive waits retain the existing socket wakeup, invalid-socket fallback and original pacing interval', () => {
  assert.match(wait, /select\(0, &readSet, nullptr, nullptr, &timeout\)/);
  assert.match(wait, /socketHandle == 0 \|\| socketHandle == INVALID_SOCKET/);
  assert.match(wait, /Sleep\(static_cast<DWORD>\(waitMs\)\);/);
  assert.match(server, /if \(residual < 50\)/);
  assert.match(server, /const int isRestarting = reinterpret_cast<int\(\*\)\(\)>/);
  assert.match(server, /return isRestarting;/);
});

test('client frame waits and the native server-frame hook target remain unchanged', () => {
  assert.match(client, /NetSleep\(remainingMs - 2\);/);
  assert.match(client, /NetSleep\(0\);/);
  assert.doesNotMatch(client, /NetSleep\([^;]*, false\)/);
  assert.match(source, /SV_Frame_SV_CheckLoadGameCall = 0x14023C557;/);
  assert.match(source, /BranchesTo\(SV_Frame_SV_CheckLoadGameCall, SV_CheckLoadGame, HOOK_CALL\)/);
});
