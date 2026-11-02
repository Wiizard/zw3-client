import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');
const resume = source.slice(source.indexOf('void ZWNet::ResumeParty('), source.indexOf('void ZWNet::JoinParty('));
const roster = source.slice(source.indexOf('void ZWNet::UpdateLobbyDvars('), source.indexOf('void ZWNet::ResumeParty('));

test('invited managed parties open the backend roster in idle and active matchmaking states', () => {
  assert.match(resume, /UpdateLobbyDvars\(resumedParty\)/);
  assert.match(resume, /openmenu zwnet_matchmaking/);
  assert.match(resume, /closemenu zwnet_party_lobby/);
  assert.doesNotMatch(resume, /openmenu zwnet_party_lobby|EnterLobby\(|Dvar::Var\("ui_mapname"\)/);
  assert.match(resume, /if \(isMatchmaking\)\s*\{\s*UpdateMatchmaking\(\)/);
});

test('idle party resumption invalidates old connection routes and match previews', () => {
  assert.match(resume, /CancelJoinInProgressPreview\(\)/);
  const idle = resume.slice(resume.indexOf('if (!isMatchmaking)'), resume.indexOf('isSearching = isMatchmaking'));
  for (const cleanup of ['ResetManagedRoute();', 'currentMatchId.clear();', 'currentProposalId.clear();',
    'ClearMatchDvars();', 'Dvar::Var("zwnet_managed_session").Set(false);']) {
    assert.ok(idle.includes(cleanup), `Missing idle cleanup: ${cleanup}`);
  }
  assert.doesNotMatch(resume, /Request\(/, 'resuming an invitation must not reconfigure or recreate the party');
});

test('queued roster updates are party scoped and idle uses a lobby activity label', () => {
  assert.match(roster, /currentPartyId != partyId \|\| currentPlayerId != playerId/);
  assert.match(roster, /state == "IDLE" \|\| state == "CONFIGURING"\) \? "IN LOBBY"/);
  assert.match(roster, /std::format\("zwnet_lobby_member_\{\}", i\)/);
  assert.match(roster, /Dvar::Var\(prefix \+ "_name"\)\.Set/);
  assert.match(roster, /SetDisplayText\(prefix \+ "_guid", memberId\)/);
});

test('inline friend icons resolve generated UI materials before looking in the fastfile database', () => {
  const renderer = readFileSync(new URL('../src/Components/Modules/TextRenderer.cpp', import.meta.url), 'utf8');
  const lookup = renderer.slice(renderer.indexOf('static void* FindMaterial('), renderer.indexOf('static float ColorMapAspect('));
  assert.ok(lookup.indexOf('Materials::GetRuntimeMaterial(name)') < lookup.indexOf('DB_FindXAssetEntry'));
  assert.match(lookup, /if \(auto\* const material = Materials::GetRuntimeMaterial\(name\)\)\s*\{\s*return material;/);
  assert.match(renderer, /std::strcmp\(\*techniqueSet, "2d"\) != 0/);
});
