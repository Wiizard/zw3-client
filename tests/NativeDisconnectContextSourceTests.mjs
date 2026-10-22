import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const source = readFileSync(new URL('../src/Components/Modules/Events.cpp', import.meta.url), 'utf8');
const header = readFileSync(new URL('../src/Components/Modules/Events.hpp', import.meta.url), 'utf8');
const hook = source.slice(source.indexOf('void Events::CL_Disconnect_Hook('), source.indexOf('Events::Events()'));

test('fresh validated native error and bounded typed text are captured before original disconnect', () => {
  assert.match(source, /canReadError = Utils::Hook::MatchesBytes\(Com_ErrorEntered/);
  assert.match(source, /dvar->type != Game::DVAR_TYPE_STRING/);
  assert.match(source, /maximumLength = 4096/);
  assert.match(source, /if \(length > maximumLength\) return \{\};/);
  assert.ok(hook.indexOf('if (errorEntered == 1)') < hook.indexOf('ReadNativeErrorText("com_errorMessage")'));
  assert.ok(hook.indexOf('ReadNativeErrorText("com_errorMessage")') < hook.indexOf('reinterpret_cast<void(*)(int)>'));
});

test('native error context is callback-scoped, thread-local, and nested scopes restore previous context', () => {
  assert.match(source, /static thread_local const Events::NativeDisconnectError\* currentNativeDisconnectError = nullptr/);
  assert.match(source, /return currentNativeDisconnectError \? \*currentNativeDisconnectError : NativeDisconnectError\{\}/);
  assert.match(hook, /previous\(currentNativeDisconnectError\).*currentNativeDisconnectError = &error/s);
  assert.match(hook, /~NativeErrorCallbackScope\(\) \{ currentNativeDisconnectError = previous; \}/);
  assert.ok(hook.indexOf('reinterpret_cast<void(*)(int)>') < hook.indexOf('nativeErrorScope(nativeError)'));
  assert.ok(hook.indexOf('if (!wasRunning)') < hook.indexOf('nativeErrorScope(nativeError)'));
  assert.ok(hook.indexOf('nativeErrorScope(nativeError)') < hook.indexOf('disconnectedTasks.Access'));
  assert.match(header, /static NativeDisconnectError CurrentNativeDisconnectError\(\)/);
});

test('diagnostics never print copied arbitrary native message or title', () => {
  const log = hook.slice(hook.indexOf('Logger::Print('), hook.indexOf('catch (...) { /* Diagnostics'));
  assert.doesNotMatch(log, /nativeError\.(message|title)/);
  assert.match(log, /ReadDisconnectReason\("com_errorMessage"\)/);
  assert.match(log, /ReadDisconnectReason\("com_errorTitle"\)/);
});
