import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';

const gameRoot = process.env.ZW3_GAME_ROOT;
const readMenu = path => readFileSync(`${gameRoot}/zw3/core/ui_mp/${path}`, 'utf8');
const source = readFileSync(new URL('../src/Components/Modules/ZWNet.cpp', import.meta.url), 'utf8');

test('Quit enters the menu actually labelled Play Online and rebuilds its online party', { skip: !gameRoot }, () => {
  const returning = source.slice(source.indexOf('if (explicitLeave)'), source.indexOf('void ZWNet::ScheduleReturnToIdleMenu('));
  const branch = returning.slice(0, returning.indexOf('\n\t\t\treturn;'));
  const target = /Menus_OpenByName\(Game::uiContext, "([^"]+)"\)/.exec(branch)?.[1];
  assert.ok(target);
  const menu = readMenu(`${target}.menu`);
  assert.match(menu, /text "PLAY ONLINE"/);
  assert.match(menu, /"FIND MATCH"/);
  assert.match(branch, /"popup_zwnet_connecting"/);
  assert.match(readMenu('popup_zwnet_connecting.menu'), /uiScript "ZWNET_ConnectOnline"/);
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
