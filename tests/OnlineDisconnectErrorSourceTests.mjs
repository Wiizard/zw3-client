import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');
const returning = source.slice(source.indexOf('void ZWNet::ReturnToIdleMenu('), source.indexOf('void ZWNet::ScheduleReturnToIdleMenu('));
const scheduled = source.slice(source.indexOf('void ZWNet::ScheduleReturnToIdleMenu('), source.indexOf('void ZWNet::HandleServerDisconnect('));
const disconnected = source.slice(source.indexOf('Events::OnCLDisconnected('), source.indexOf('Events::OnCGameInit('));
const handling = source.slice(source.indexOf('void ZWNet::HandleServerDisconnect('), source.indexOf('bool ZWNet::BeginManagedReconnect('));

test('native error snapshot is captured in the callback and copied before asynchronous cleanup', () => {
  assert.ok(disconnected.indexOf('Events::CurrentNativeDisconnectError()') < disconnected.indexOf('isInGame.exchange(false)'));
  assert.match(disconnected, /EnqueueAsync\(\[isTerminal, wasMatchmaking, nativeError, disconnectGeneration, wasConnected, wasInGame\]/);
  assert.match(disconnected, /HandleServerDisconnect\(isTerminal, wasMatchmaking, nativeError, wasInGame\)/);
  assert.match(disconnected, /isServerJoinTransition\.exchange\(false\) && !nativeError\.hasNativeError/);
  assert.doesNotMatch(disconnected, /nativeError\.(message|title)/);
});

test('completed-match duplicate callbacks cannot treat a delayed managed DVAR as a live assignment', () => {
  assert.match(source, /#include "Utils\/ManagedDisconnect\.hpp"/);
  assert.match(disconnected, /Utils::ShouldHandleManagedDisconnect\(hasMatch, isManaged, wasConnected\)/);
  assert.doesNotMatch(disconnected, /wasMatchmaking = hasMatch \|\| isManaged/);
  const queued = disconnected.slice(disconnected.indexOf('EnqueueAsync([isTerminal, wasMatchmaking'));
  assert.match(queued, /std::lock_guard _\(stateMutex\);\s*hasCurrentMatch = !currentMatchId\.empty\(\);/);
  assert.ok(queued.indexOf('ShouldHandleManagedDisconnect(hasCurrentMatch, wasMatchmaking, wasConnected)') < queued.indexOf('HandleServerDisconnect(isTerminal, wasMatchmaking, nativeError, wasInGame)'));
});

test('native error cannot be mistaken for a completed match, while normal completion remains unchanged', () => {
  assert.match(handling, /wasMatchmaking && !isTerminal && !nativeError\.hasNativeError && ReturnToMatchmakingLobby\(\)/);
  assert.match(handling, /wasInGame && nativeError\.hasNativeError && ReturnToMatchmakingLobby\(true\)/);
  assert.ok(handling.indexOf('CloseOnlineSession(false, isTerminal)') < handling.indexOf('ScheduleReturnToIdleMenu(std::move(nativeError))'));
  assert.match(disconnected, /else if \(isPrivateMatchClient\)/);
});

test('error return restores its snapshot after menu changes and never opens the automatic connect popup', () => {
  const branchStart = returning.indexOf('if (nativeError.hasNativeError)');
  const branchEnd = returning.indexOf('\n\t\t\treturn;', branchStart);
  const errorBranch = returning.slice(branchStart, branchEnd);
  assert.ok(errorBranch.indexOf('Menus_CloseRequest') < errorBranch.indexOf('Menus_OpenByName(Game::uiContext, "pregame_loaderror")'));
  assert.ok(errorBranch.indexOf('Menus_OpenByName(Game::uiContext, "pregame_loaderror")') < errorBranch.indexOf('Dvar::Var("com_errorMessage").Set('));
  assert.ok(errorBranch.indexOf('Dvar::Var("com_errorTitle").Set(') < errorBranch.indexOf('Menus_OpenByName(Game::uiContext, "error_popmenu_lobby")'));
  assert.doesNotMatch(errorBranch, /Menus_OpenByName\(Game::uiContext, "popup_zwnet_connecting"\)|BeginOnlineEntry\(/);
  assert.match(returning.slice(branchEnd), /Menus_OpenByName\(Game::uiContext, "popup_zwnet_connecting"\)/);
});

test('superseded errors cannot clean up a new join or reopen an old error dialog', () => {
  assert.match(disconnected, /if \(joinTransitionGeneration != disconnectGeneration\) return;/);
  assert.match(scheduled, /nativeError = std::move\(nativeError\)/);
  assert.ok(scheduled.indexOf('nativeError.hasNativeError && joinTransitionGeneration != returnGeneration') < scheduled.indexOf('CL_IsCgameInitialized'));
  assert.match(scheduled, /ReturnToIdleMenu\(nativeError\)/);
});
