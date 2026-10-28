import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
const auth=readFileSync(new URL('../src/Components/Modules/Auth.cpp',import.meta.url),'utf8');
const online=readFileSync(new URL('../src/Components/Modules/ZWNet.cpp',import.meta.url),'utf8');

test('native connect defers expired preparation tickets once and never sends an unauthenticated managed packet',()=>{
  assert.doesNotMatch(auth,/managedTicketLifetime = 120s/);
  const send=auth.slice(auth.indexOf('bool Auth::SendConnectDataStub('),auth.indexOf('static bool IsSameMachineAddress('));
  assert.match(send,/needsRefresh = !managedConnectTicket.refreshPending/);
  assert.match(send,/if \(needsRefresh\) ZWNet::RefreshManagedConnectTicket\(address\)/);
  assert.ok(send.indexOf('if (waitsForRefresh) return true')<send.indexOf('Network::SendCommand'));
  assert.match(send,/zwnet_managed_session.*!hasManagedTicket\) return false/);
  assert.doesNotMatch(send,/sleep_for|Request\(|WebIO/);
});

test('admission renewal revalidates the same session and server and rejects cancelled or delayed work',()=>{
  const renew=online.slice(online.indexOf('void ZWNet::RefreshManagedConnectTicket('),online.indexOf('void ZWNet::ConnectMatch('));
  assert.match(renew,/EnqueueAsync/);
  assert.ok((renew.match(/if \(!stillCurrent\(\)\) return/g)||[]).length>=3);
  for(const key of ['match_id','session_id','instance_id','server_identity','route_type'])assert.ok(renew.includes('"'+key+'"'));
  assert.match(renew,/ticket_ttl_seconds/);
  assert.match(renew,/requestedAt \+ std::chrono::seconds\(ttl\)/);
  assert.match(renew,/expires > std::chrono::steady_clock::now\(\) \+ 2s/);
  assert.match(renew,/Party::ConnectError/);
});
