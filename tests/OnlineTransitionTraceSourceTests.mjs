import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');
const helper = source.slice(source.indexOf('static void TraceOnlineTransition('), source.indexOf('static Utils::OnlineMetric onlinePlayerMetric;'));

test('transition diagnostics are opt-in and bounded without private session values', () => {
  assert.match(helper, /if \(!Flags::HasFlag\("log"\)\) return;/);
  assert.ok(helper.indexOf('Flags::HasFlag') < helper.indexOf('Game::CL_GetLocalClientConnectionState'));
  assert.match(helper, /hasMatch = !currentMatchId\.empty\(\);/);
  assert.match(helper, /std::ranges::find\(knownStates, uiState\).*uiState = "OTHER";/);
  assert.match(helper, /nativeState >= Game::CA_DISCONNECTED && nativeState <= Game::CA_ACTIVE/);
  const logged = helper.slice(helper.indexOf('Logger::Print('));
  for (const forbidden of ['currentMatchId', 'currentPlayerId', 'currentPartyId', 'accessToken', 'refreshToken', 'endpoint', 'params->', 'Request(']) {
    // endpoint_join is a boolean label, never the endpoint value.
    assert.ok(!logged.replaceAll('endpoint_join', '').includes(forbidden), `${forbidden} must not be logged`);
  }
});

test('UI transition sources are fixed labels recorded before state invalidation', () => {
  for (const [sourceName, event] of [
    ['Command::Add("zwnet_cancel"', 'command_cancel'],
    ['UIScript::Add("ZWNET_CancelMatchmaking"', 'ui_cancel_matchmaking'],
    ['UIScript::Add("ZWNET_QuitToMatchmaking"', 'ui_quit_to_matchmaking'],
    ['UIScript::Add("ZWNET_CloseOnlineSession"', 'ui_close_online_session'],
  ]) {
    const handler = source.slice(source.indexOf(sourceName), source.indexOf(sourceName) + 260);
    assert.ok(handler.indexOf(`TraceOnlineTransition("${event}")`) > 0);
    assert.ok(handler.indexOf(`TraceOnlineTransition("${event}")`) < handler.indexOf('CancelPendingJoinPreparation()'));
  }
});

test('disconnect and cgame traces preserve the pre-transition snapshot and existing callback order', () => {
  const disconnected = source.slice(source.indexOf('Events::OnCLDisconnected('), source.indexOf('Events::OnCGameInit('));
  assert.ok(disconnected.indexOf('TraceOnlineTransition(wasConnected') < disconnected.indexOf('isInGame = false'));
  assert.ok(disconnected.indexOf('isServerJoinTransition.exchange(false)') < disconnected.indexOf('HandleServerDisconnect(isTerminal, wasMatchmaking, nativeError)'));
  const initialized = source.slice(source.indexOf('Events::OnCGameInit('), source.indexOf('Scheduler::OnGameInitialized(', source.indexOf('Events::OnCGameInit(')));
  assert.ok(initialized.indexOf('TraceOnlineTransition("cgame_init_begin")') < initialized.indexOf('CancelRelayHandshake()'));
  assert.ok(initialized.indexOf('isInGame = hasMatch') < initialized.indexOf('TraceOnlineTransition("cgame_init_assigned")'));
  assert.ok(initialized.indexOf('TraceOnlineTransition("cgame_init_assigned")') < initialized.indexOf('SetState("IN_MATCH")'));
});
