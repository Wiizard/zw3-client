import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');
const inputGate = source.slice(source.indexOf('static bool CanActivatePlaylistSelection('), source.indexOf('static std::string SafePlaylistText('));
const slot = source.slice(source.indexOf('void ZWNet::ActivatePlaylistSlot('), source.indexOf('void ZWNet::ActivateQuickPlay('));
const quickPlay = source.slice(source.indexOf('void ZWNet::ActivateQuickPlay('), source.indexOf('void ZWNet::ActivatePlaylist('));
const activation = source.slice(source.indexOf('void ZWNet::ActivatePlaylist('), source.indexOf('void ZWNet::ChangePlaylistPage('));
const start = source.slice(source.indexOf('void ZWNet::StartQuickPlay('), source.indexOf('std::optional<nlohmann::json> ZWNet::ApplyPartyVisibility('));
const cancellation = source.slice(source.indexOf('void ZWNet::CancelPendingJoinPreparation('), source.indexOf('static bool IsCurrentJoinPreview('));

test('both input actions retain visible-menu, opening-click, leader and duplicate guards', () => {
  assert.match(inputGate, /Menu_IsVisible\(Game::uiContext, menu\)/);
  assert.match(inputGate, /isPlaylistSelectionStarting/);
  assert.match(inputGate, /isPlaylistSearchStarting \|\| isSearching \|\| isInGame/);
  assert.match(inputGate, /PlaylistClockMilliseconds\(\) - playlistSelectorOpenedAtMs < 200/);
  assert.match(inputGate, /if \(!isLocalPartyLeader\)/);
  assert.match(slot, /CanActivatePlaylistSelection\(\)/);
  assert.match(quickPlay, /CanActivatePlaylistSelection\(\)/);
  assert.match(activation, /CanActivatePlaylistSelection\(\)/);
});

test('Quick Play uses all verified catalog entries and the existing authoritative party count', () => {
  assert.match(quickPlay, /Dvar::Var\("zwnet_lobby_member_count"\)\.Get<int>\(\)/);
  assert.match(quickPlay, /for \(const auto& entry : playlistCatalog\.entries\)/);
  assert.match(quickPlay, /std::ranges::all_of\(entry\.requiredContent/);
  assert.match(quickPlay, /std::ranges::find\(entry\.verifiedContent, contentId\)/);
  assert.match(quickPlay, /Utils::SelectQuickPlayPlaylist\(candidates, playlistCatalog\.isLoaded, playlistCatalog\.isStale/);
  assert.doesNotMatch(quickPlay, /playlistCatalog\.page|ActivatePlaylistSlot\(|Request\(/);
  assert.match(quickPlay, /No compatible public playlist for your party/);
});

test('explicit selection and Quick Play share ID plus revision activation and one request pipeline', () => {
  assert.match(slot, /ActivatePlaylist\(playlistId, playlistRevision, false\)/);
  assert.match(quickPlay, /ActivatePlaylist\(playlistId, playlistRevision, true\)/);
  assert.match(activation, /entry\.id == playlistId && entry\.revision == playlistRevision/);
  assert.match(activation, /!playlistCatalog\.isLoaded \|\| playlistCatalog\.isStale/);
  assert.match(activation, /isQuickPlay && entry\.audience != "PUBLIC"/);
  assert.match(activation, /isPlaylistSelectionStarting\.exchange\(true\)/);
  assert.match(activation, /const auto generation = playlistSelectorGeneration\.load\(\)/);
  assert.match(activation, /StartQuickPlay\(playlistId, playlistRevision, generation, isQuickPlay\)/);
  assert.match(source, /UIScript::Add\("ZWNET_AutoQuickPlay"[\s\S]*?ActivateQuickPlay\(\)/);
  assert.match(source, /Command::Add\("zwnet_autoquickplay", \[\]\s*\{\s*ActivateQuickPlay\(\);\s*\}\);/);
});

test('legacy and new private menus share registered feedback and the standard selector-opening action is unchanged', () => {
  const feedback = source.slice(source.indexOf('static void SetPlaylistActionError('), source.indexOf('static bool CanActivatePlaylistSelection('));
  assert.match(feedback, /Dvar::Var\("zwnet_catalog_action_error"\)\.Set\(error\)/);
  assert.match(feedback, /Dvar::Var\("zwnet_quickplay_error"\)\.Set\(error\)/);
  assert.match(source, /Dvar::Register\("zwnet_quickplay_error", "", Game::DVAR_NONE/);
  const opening = source.slice(source.indexOf('Command::Add("zwnet_quickplay"'), source.indexOf('Command::Add("zwnet_autoquickplay"'));
  assert.match(opening, /Command::Execute\("openmenu popup_zwnet_playlists", false\)/);
  assert.doesNotMatch(opening, /StartQuickPlay\(|ActivateQuickPlay\(/);
});

test('queued Quick Play revalidates public audience and preserves backend party, content and revision validation', () => {
  assert.match(start, /!isQuickPlay \|\| selected->audience == "PUBLIC"/);
  assert.match(start, /selected->availability == "AVAILABLE"/);
  assert.match(start, /JsonString\(\*party, "leader_id"\) != playerId/);
  assert.match(start, /"\/content"/);
  assert.match(start, /"\/select-playlist"/);
  assert.match(start, /"playlist_revision", playlistRevision/);
  assert.equal((start.match(/Request\("POST", "\/zwnet\/matchmaking\/search"/g) || []).length, 1);
  for (const error of ['PLAYLIST_ACCESS_DENIED', 'PARTY_TOO_LARGE', 'PARTY_CONTENT_MISSING', 'PLAYLIST_NO_CAPACITY']) {
    assert.ok(start.includes(error), `${error} handling must remain in the reused flow`);
  }
});

test('input-thread cancellation invalidates queued playlist activation before worker cleanup', () => {
  assert.match(cancellation, /\+\+playlistSelectorGeneration/);
  for (const command of ['Command::Add("zwnet_cancel"', 'UIScript::Add("ZWNET_CancelMatchmaking"']) {
    const handler = source.slice(source.indexOf(command), source.indexOf(command) + 500);
    assert.ok(handler.indexOf('CancelPendingJoinPreparation()') < handler.indexOf('EnqueueAsync('));
  }
  assert.match(start, /if \(!IsCurrentPlaylistActivation\(selectorGeneration\)\)/);
  const searchResponse = start.slice(start.indexOf('const auto result = Request("POST", "/zwnet/matchmaking/search"'));
  assert.ok(searchResponse.indexOf('!IsCurrentPlaylistActivation(selectorGeneration)') < searchResponse.indexOf('accept(joinedState)'));
  assert.match(searchResponse, /if \(isQueued\)[\s\S]*?Request\("POST", "\/zwnet\/matchmaking\/cancel"\)/);
});
