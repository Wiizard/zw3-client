import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';

const source = fs.readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');
const update = source.slice(source.indexOf('void ZWNet::UpdateSearchTime()'), source.indexOf('void ZWNet::SetState('));
const state = source.slice(source.indexOf('void ZWNet::SetState('), source.indexOf('void ZWNet::Refresh()'));
const menu = source.slice(source.indexOf('UIScript::Add("ZWNET_SetMatchmakingMenuActive"'), source.indexOf('UIScript::Add("ZWNET_CloseOnlineSession"'));

test('the active flag remains boolean and elapsed text has its own registered display contract', () => {
  assert.match(source, /Dvar::Register\("zwnet_search_active", false, Game::DVAR_NONE/);
  assert.match(source, /Dvar::Register\("zwnet_search_elapsed_text", "0:00", Game::DVAR_NONE/);
  assert.match(state, /Dvar::Var\("zwnet_search_active"\).Set\(searching\)/);
  assert.doesNotMatch(state, /zwnet_search_active"\).Set\([^;]*(?:milliseconds|Clock::now)/);
});

test('one monotonic search stopwatch is bound to actual search states on the main thread', () => {
  assert.match(source, /#include "Utils\/MatchmakingSearchTimer.hpp"/);
  assert.match(source, /static Utils::MatchmakingSearchTimer searchTimer;/);
  assert.match(state, /const bool searching = state == "SEARCH_STARTING" \|\| state == "SEARCHING";/);
  assert.match(state, /searchTimer.SetSearching\(searching, now\)/);
  assert.match(state, /zwnet_search_elapsed_text"\).Set\(searchTimer.Text\(now\)\)/);
  assert.match(state, /Scheduler::Pipeline::MAIN/);
  assert.doesNotMatch(state, /Sleep\(|sleep_for|system_clock/);
});

test('the existing menu lifecycle gates elapsed updates and reopening republishes the same active search', () => {
  assert.match(update, /if \(!isActive \|\| !isNetworkMetricsEnabled\) return;/);
  assert.match(update, /Clock::now\(\)/);
  assert.match(update, /Get<std::string>\(\) != text/);
  assert.match(source, /Scheduler::Loop\(UpdateSearchTime, Scheduler::Pipeline::MAIN, 250ms\)/);
  assert.match(menu, /isNetworkMetricsEnabled = isEnabled;\s*UpdateSearchTime\(\);/);
  assert.doesNotMatch(menu, /searchTimer.SetSearching/);
});
