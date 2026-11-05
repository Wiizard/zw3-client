import assert from 'node:assert/strict';
import fs from 'node:fs';
import test from 'node:test';
const read=file=>fs.readFileSync(new URL('../'+file,import.meta.url),'utf8');
const source=read('src/Components/Modules/ZWNet.cpp');
const method=(start,end)=>source.slice(source.indexOf(start),source.indexOf(end,source.indexOf(start)));

test('manual Ready has no client entry points, roster fields, dvars or state presentation',()=>{
  for(const file of ['src/Components/Modules/ZWNet.cpp','src/Components/Modules/ZWNet.hpp',
    'src/Components/Modules/Discord.cpp','src/Components/Modules/Menu/Selection.hpp','src/Utils/SocialPresentation.hpp'])
    assert.doesNotMatch(read(file),/ToggleReady|IsPartyMemberReady|ClearReadyPending|zwnet_(?:all_ready|ready_pending|lobby_self_ready)|memberDvarName\(i, "_ready"|READY_CHECK|WAITING_FOR_READY/,file);
});

test('match roster, vote and winner callbacks retain the request transition and snapshot',()=>{
  const poll=method('void ZWNet::UpdateMatchmaking()', 'void ZWNet::InitializeDvars()');
  assert.match(poll,/UpdateVoteDvars\(\*status, transitionGeneration, uiSnapshot\)/);
  assert.match(poll,/UpdateMatchLobbyDvars\(\*status, transitionGeneration, uiSnapshot\)/);
  assert.match(poll,/IsCurrentMatchUi\(transitionGeneration, uiSnapshot\)/);
  const roster=method('void ZWNet::UpdateMatchLobbyDvars(', 'void ZWNet::UpdatePresence(');
  assert.ok(roster.indexOf('IsCurrentMatchUi')<roster.indexOf('ObserveMatchLobbyMembers'));
  assert.match(roster,/Scheduler::Once\(\[[^\]]*uiTransition, uiSnapshot\]/);
});

test('Quick Play creates a party only after confirmed absence and return requires an available party',()=>{
  const quick=method('void ZWNet::StartQuickPlay(', 'void ZWNet::SelectPlaylist(');
  assert.match(quick,/if \(party && party->is_null\(\)\)/);
  assert.doesNotMatch(quick,/if \(!party \|\| party->is_null\(\)\)\s*\{\s*party = Request\("POST", "\/zwnet\/parties\/create"/);
  const returning=method('bool ZWNet::ReturnToMatchmakingLobby(', 'void ZWNet::ReturnToIdleMenu(');
  assert.ok(returning.indexOf('partyState != "IDLE"')<returning.indexOf('currentMatchId.clear()'));
});

const menuRoot=process.env.ZW3_MENU_ROOT||process.env.ZW3_GAME_ROOT;
test('vote mode follows the accepted playlist and exact party revision instead of private settings',()=>{
  const start=method('void ZWNet::StartQuickPlay(', 'void ZWNet::SelectPlaylist(');
  assert.match(start,/playlistMode = selected->zombieMode/);
  assert.match(start,/zwnet_vote_zombiemode"\).Set\(playlistMode\)/);
  const publish=method('void ZWNet::PublishPlaylistCatalog()', 'void ZWNet::CancelJoinInProgressPreview(');
  const partyMode=publish.slice(publish.indexOf('std::string partyPlaylistName;'));
  assert.match(partyMode,/partySelected->revision == playlistCatalog.partySelectedRevision/);
  assert.match(partyMode,/zwnet_vote_zombiemode"\).Set\(partySelected->zombieMode\)/);
  assert.doesNotMatch(partyMode,/active->zombieMode|Dvar::Var\("zombiemode"\)/);
  assert.match(method('static void ClearMatchDvars()', 'static void ClearMemberDvars('),/zwnet_vote_zombiemode"\).Set\(""\)/);
});

test('shipped vote and winner labels use the playlist mode',{skip:!menuRoot},()=>{
  const menu=fs.readFileSync(menuRoot+'/zw3/core/ui_mp/zwnet_matchmaking.menu','utf8');
  for(const name of ['label_map_mode_vote_a','label_map_mode_vote_b','label_map_mode_winner']){
    const item=menu.split('\n').find(line=>line.includes(`name "${name}"`));
    assert.match(item,/dvarString\("zwnet_vote_zombiemode"\)/);
    assert.doesNotMatch(item,/dvarString\("zombiemode"\)/);
  }
});

test('deployed menu variants agree, remove Ready and separate thumbnails from names and vote totals',{skip:!menuRoot},()=>{
  for(const file of ['zwnet_matchmaking.menu','zwnet_party_lobby.menu']){
    const core=fs.readFileSync(menuRoot+'/zw3/core/ui_mp/'+file,'utf8');
    assert.equal(fs.readFileSync(menuRoot+'/userraw/ui_mp/'+file,'utf8'),core);
    assert.doesNotMatch(core,/ZWNET_ToggleReady|ZWNET_MM_READY|WAITING_FOR_READY|READY_CHECK|zwnet_all_ready|zwnet_lobby_self_ready|zwnet_ready_pending/);
    if(file==='zwnet_matchmaking.menu')for(const [letter,y] of [['a',284],['b',334]]){
      const image=core.split('\n').find(line=>line.includes(`name "image_map_preview_vote_${letter}"`));
      assert.ok(image.includes(`rect 22 ${y} 64 41`));
      assert.ok(core.includes(`name "label_map_name_vote_${letter}" rect 94 ${y+14} 158 25`));
      const fallback=core.split('\n').find(line=>line.includes(`name "image_map_preview_vote_${letter}_fallback"`));
      assert.match(fallback,/@ZWNET_PREVIEW_UNAVAILABLE/);assert.doesNotMatch(fallback,/exp material/);
    }
  }
});
