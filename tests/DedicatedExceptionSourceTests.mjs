import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';

const source = readFileSync(new URL('../src/Components/Modules/Exception.cpp', import.meta.url), 'utf8');
const filter = source.slice(source.indexOf('LONG WINAPI Exception::ExceptionFilter('), source.indexOf('LPTOP_LEVEL_EXCEPTION_FILTER WINAPI Exception::SetUnhandledExceptionFilter_Stub('));

test('headless fatal exceptions retain the real dump before returning to native termination', () => {
  const dump = filter.indexOf('const bool wroteDump = WriteMiniDump(exceptionInfo, dumpPath);');
  const headless = filter.indexOf('if (isHeadlessServer)', dump);
  const end = filter.indexOf('\n\t\tstd::string error = description;', headless);
  assert.ok(dump >= 0 && headless > dump && end > headless);
  const branch = filter.slice(headless, end);
  assert.match(branch, /return EXCEPTION_EXECUTE_HANDLER;/);
  assert.doesNotMatch(branch, /MessageBoxA\(|GetErrorMessage\(|CopyToClipboard\(|EXCEPTION_CONTINUE_EXECUTION/);
});

test('only explicitly dedicated launches skip crash dialogs, including stack-overflow dialogs', () => {
  assert.match(filter, /const bool isHeadlessServer = Flags::HasFlag\("dedicated"\);/);
  assert.match(filter, /code == EXCEPTION_STACK_OVERFLOW && !isHeadlessServer/);
  assert.match(filter, /MessageBoxA\(nullptr, message\.data\(\), "Zombie Warfare 3"/);
});

test('fatal server output contains only the numeric exception and dump success without shared logger locks', () => {
  const branch = filter.slice(filter.indexOf('if (isHeadlessServer)'), filter.indexOf('\n\t\tstd::string error = description;'));
  assert.match(branch, /ZWNET dedicated fatal exception: code=0x00000000 dump_written=0/);
  assert.doesNotMatch(branch, /description\.c_str\(|launchParameters|DvarString\(|GetErrorMessage\(/);
  assert.match(branch, /WriteFile\(GetStdHandle\(STD_ERROR_HANDLE\)/);
  assert.doesNotMatch(branch, /Logger::|fprintf\(|fflush\(/);
});
