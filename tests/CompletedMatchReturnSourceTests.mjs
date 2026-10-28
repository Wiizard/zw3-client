import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';

const source=readFileSync(new URL('../src/Components/Modules/ZWNet.cpp',import.meta.url),'utf8');
const method=(start,end)=>source.slice(source.indexOf(start),source.indexOf(end));

test('an expected Game Over return cannot fall through to a transient disconnect or another connection',()=>{
  const handler=method('void ZWNet::HandleServerDisconnect(', 'bool ZWNet::BeginManagedReconnect(');
  const expected=handler.slice(handler.indexOf('if (wasMatchmaking && !isTerminal && isCompletedMatchReturnPending)'),handler.indexOf('if (wasMatchmaking && !isTerminal && !nativeError.hasNativeError'));
  assert.match(expected,/isSearching = true/);
  assert.match(expected,/SetState\("POST_MATCH"\)/);
  assert.match(expected,/ReturnToMatchmakingLobby\(\);\s*return;/);
  assert.doesNotMatch(expected,/CloseOnlineSession|ConnectMatch|BeginManagedReconnect/);
  const poll=method('void ZWNet::UpdateMatchmaking()', 'void ZWNet::InitializeDvars()');
  assert.ok(poll.indexOf('if (isCompletedMatchReturnPending)')<poll.indexOf('const auto transitionGeneration'));
  assert.match(poll,/if \(isCompletedMatchReturnPending\)\s*\{\s*ReturnToMatchmakingLobby\(\);\s*return;/);
  const completed=source.slice(source.indexOf('if (!isActive || !isInGame || !Dvar::Var("zwnet_match_return")'));
  assert.ok(completed.indexOf('isCompletedMatchReturnPending = true')<completed.indexOf('Command::Execute("disconnect"'));
});

test('lobby return waits for authoritative completion and revalidates cancellation after requests',()=>{
  const returning=method('bool ZWNet::ReturnToMatchmakingLobby(', 'void ZWNet::ReturnToIdleMenu(');
  assert.match(returning,/IsCompletedMatchmakingState\(JsonString\(\*status, "state"\)\)/);
  assert.doesNotMatch(returning,/sleep_for|attempt < 7/);
  assert.ok((returning.match(/generation != joinTransitionGeneration/g)||[]).length>=4);
  assert.ok(returning.indexOf('IsCompletedMatchmakingState')<returning.indexOf('currentMatchId.clear()'));
  assert.match(returning,/isCompletedMatchReturnPending = false/);
  assert.match(returning,/IsConfirmedAssignedMatchEnd/);
  assert.match(returning,/ResetManagedRoute\(\);\s*generation = joinTransitionGeneration.load\(\)/);
  assert.match(returning,/Menus_CloseAll/);
  const cancellation=method('void ZWNet::CancelPendingJoinPreparation()', 'static bool IsCurrentJoinPreview(');
  assert.match(cancellation,/isCompletedMatchReturnPending = false/);
});

test('explicit X cancellation releases assignment while retaining the party and reports failed cleanup',()=>{
  const cancel=method('void ZWNet::CancelMatchmaking()', 'void ZWNet::CloseOnlineSession(');
  assert.match(cancel,/"terminal", true/);
  assert.match(cancel,/"preserve_party", true/);
  assert.ok(cancel.indexOf('result->contains("error")')<cancel.indexOf('currentMatchId.clear()'));
  assert.doesNotMatch(cancel,/CloseOnlineSession\(false, false\)/);
  assert.match(cancel,/ResetManagedRoute\(\);\s*generation = joinTransitionGeneration.load\(\)/);
});

test('completed lobby return waits for rebuilt frontend menus instead of racing native UI initialization',()=>{
  const returning=method('bool ZWNet::ReturnToMatchmakingLobby(', 'void ZWNet::ReturnToIdleMenu(');
  assert.match(returning,/Scheduler::Schedule\(\[generation, frontendInitialized = false\]/);
  assert.match(returning,/CL_GetLocalClientConnectionState\(0\) != Game::CA_DISCONNECTED/);
  assert.match(returning,/"main_text", "pregame_loaderror"/);
  assert.match(returning,/frontendInitialized = frontendInitialized \|\| frontendVisible/);
  assert.ok(returning.indexOf('if (!frontendInitialized) return false')<returning.indexOf('Menus_CloseAll'));
  assert.match(returning,/return visible;/);
  assert.doesNotMatch(returning,/MAIN, 500ms/);
});

test('the private runtime script publishes the real server end before notifying clients',{skip:!process.env.ZW3_GAME_ROOT},()=>{
  const script=readFileSync(process.env.ZW3_GAME_ROOT+'/zw3/core/maps/mp/gametypes/_gamelogic.gsc','utf8');
  const notify=script.indexOf('setClientDvar( "zwnet_match_return", "1" )');
  assert.ok(notify>0);
  const begin=script.lastIndexOf('if ( getDvar( "zwnet_match_id" ) != "" )',notify);
  const branch=script.slice(begin,notify);
  assert.match(branch,/setDvar\( "zwnet_match_ended", "1" \)/);
  assert.match(branch,/ZWNET_MATCH_COMPLETE/);
  assert.doesNotMatch(branch,/\bwait\s*\(/);
});

test('the installed V11 menu uses the existing elapsed timer and displays post-match return',{skip:!process.env.ZW3_GAME_ROOT},()=>{
  const menu=readFileSync(process.env.ZW3_GAME_ROOT+'/zw3/core/ui_mp/zwnet_matchmaking.menu','utf8');
  assert.match(menu,/exp text\(dvarString\("zwnet_search_elapsed_text"\)\)/);
  assert.doesNotMatch(menu,/milliseconds\(\) - dvarInt\("zwnet_search_active"\)/);
  assert.match(menu,/"POST_MATCH"/);
});
