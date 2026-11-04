import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
const source=fs.readFileSync(new URL('../src/Components/Modules/ZWNet.cpp',import.meta.url),'utf8');
test('playlist art uses the existing material loader asynchronously and guards obsolete menu sessions',()=>{
  const loader=source.slice(source.indexOf('std::string ZWNet::ResolvePlaylistImage('),source.indexOf('void ZWNet::PublishPlaylistCatalog('));
  assert.match(loader,/EnqueueAsync\(/);assert.match(loader,/Scheduler::Pipeline::MAIN/);
  assert.match(loader,/generation != playlistCatalog.generation/);
  assert.match(loader,/!isActive \|\| isInGame \|\| !isPlaylistSelectorOpen/);
  assert.match(loader,/Materials::CreateNewsMaterialFromImageBytes/);
  assert.match(loader,/images.size\(\) >= 8/);assert.match(loader,/Materials::Delete\(material, true\)/);
  assert.match(loader,/SHA256::Compute\(bytes\)/);assert.match(loader,/request.CancelDownload\(\)/);
  const publish=source.slice(source.indexOf('void ZWNet::PublishPlaylistCatalog('),source.indexOf('void ZWNet::UpdateVoteDvars('));
  assert.match(publish,/ResolvePlaylistImage\(active->imageUrl\)/);
  assert.match(publish,/ResolveVoteMapImage\(active->mapId, active->mapImage\)/);
});
test('the existing selected-playlist population is fed by both catalog and status refreshes',()=>{
  assert.match(source,/entry.playerPopulation.Observe\(Utils::ReadOnlineMetric\(row, "player_count"\)/);
  assert.match(source,/active->playerPopulation.Current\(Utils::OnlineMetric::Clock::now\(\)\)/);
  assert.match(source, /zwnet_catalog_selected_population/);
  assert.match(source, /PLAYLIST PLAYERS UNAVAILABLE/);
});
const game=process.env.ZW3_GAME_ROOT;
test('shipped Find Match hover and focus use the same population label and preview',{skip:!game},()=>{
  const menu=fs.readFileSync(game+'/zw3/core/ui_mp/popup_zwnet_playlists.menu','utf8');
  const row=menu.split('\n').find(line=>line.includes('mouseEnter')&&line.includes('ZWNET_HighlightPlaylist'));
  assert.match(row,/onFocus[^\n]*ZWNET_HighlightPlaylist/);
  assert.equal((menu.match(/exp text\(dvarString\("zwnet_catalog_selected_population"\)\)/g)||[]).length,1);
  assert.match(menu,/exp material\(dvarString\("zwnet_catalog_selected_image"\)\)/);
});
