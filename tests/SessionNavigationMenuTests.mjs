import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const gameRoot = process.env.ZW3_GAME_ROOT;
const readMenu = path => readFileSync(`${gameRoot}/zw3/core/ui_mp/${path}`, 'utf8');
const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');

test('leaving matchmaking connects to online services before opening Play Online', { skip: !gameRoot }, () => {
  const returning = source.slice(source.indexOf('void ZWNet::ReturnToIdleMenu('), source.indexOf('void ZWNet::ScheduleReturnToIdleMenu('));
  const explicitLeave = returning.slice(returning.indexOf('if (explicitLeave)'), returning.indexOf('\n\t\t\treturn;'));
  assert.match(explicitLeave, /Menus_OpenByName\(Game::uiContext, "pregame_loaderror"\);\s+Game::Menus_OpenByName\(Game::uiContext, "popup_zwnet_connecting"\)/);
  assert.doesNotMatch(explicitLeave, /Menus_OpenByName\(Game::uiContext, "zwnet_matchmaking"\)/);
  const popup = readMenu('popup_zwnet_connecting.menu');
  assert.match(popup, /text "CONNECTING TO ONLINE SERVICES"/);
  assert.match(popup, /uiScript "ZWNET_ConnectOnline"/);
  const completed = source.slice(source.indexOf('void ZWNet::CompleteOnlineEntry('), source.indexOf('void ZWNet::AbandonOnlineSession('));
  assert.match(completed, /Menus_CloseRequest\(Game::uiContext, connecting\)[\s\S]+Menus_OpenByName\(Game::uiContext, "zwnet_matchmaking"\)/);
  assert.match(readMenu('zwnet_matchmaking.menu'), /text "PLAY ONLINE"/);
});

test('leaving the native Private Match lobby goes to Play Online rather than Main Menu', { skip: !gameRoot }, () => {
  const menu = readMenu('menu_xboxlive_privatelobby.menu');
  const leave = menu.slice(menu.indexOf('#define LEAVE_PRIVATE_LOBBY'), menu.indexOf('/* bot logic */'));
  assert.match(leave, /LEAVE_LOBBY_ACTION\( "zwnet_matchmaking" \)/);
  assert.match(leave, /open "popup_zwnet_connecting"/);
  assert.doesNotMatch(leave, /pregame_loaderror|zwnet_private_return_online/);
});

test('both pause menus reserve restart and Save & Quit for hosts and route guest Quit through cleanup', { skip: !gameRoot }, () => {
  for (const path of ['scriptmenus/class.menu', 'scriptmenus/team_marinesopfor.menu']) {
    const menu = readMenu(path);
    assert.match(menu, /BUTTON_SELECT_HOST\( btn_restart,/);
    assert.match(menu, /BUTTON_SELECT_HOST\( btn_savequit_host,[^\n]*uiScript "SaveQuitToPrivateLobby"/);
    assert.match(menu, /BUTTON_SELECT_CLIENT\( btn_quit_client,[^\n]*uiScript "ZWNET_QuitToMatchmaking"/);
    assert.doesNotMatch(menu, /exec "disconnect"/);
  }
});
