import { test, expect } from 'bun:test';
import { runBackfill, UserRecord } from '../backfill-dids';

// Mock DB client
function makeMockDb(initial: UserRecord[]) {
  const users = initial.map(u => ({ ...u }));
  return {
    async getUsersWithoutDid() {
      // return copies
      return users.filter(u => !u.phantom_did).map(u => ({ ...u }));
    },
    async updateUserDid(id: string, did: string) {
      const u = users.find(x => x.id === id);
      if (!u) throw new Error('user not found ' + id);
      u.phantom_did = did;
    },
    // expose for assertions
    _dump() {
      return users.map(u => ({ ...u }));
    }
  };
}

test('backfill creates did mappings and updates users', async () => {
  const initial = [
    { id: 'user-1', email: 'a@example.com', phantom_did: null },
    { id: 'user-2', email: 'b@example.com', phantom_did: null },
    { id: 'user-3', email: 'c@example.com', phantom_did: 'did:phantom:existing' }
  ];

  const db = makeMockDb(initial);

  // capture POSTs to DID Mapper
  const posts: Array<{ url: string; body: any; headers: Record<string,string> }> = [];
  const realFetch = globalThis.fetch;
  globalThis.fetch = async (input: any, init?: any) => {
    const url = String(input);
    const bodyText = init?.body ? String(init.body) : '';
    let parsed = null;
    try { parsed = JSON.parse(bodyText); } catch {};
    posts.push({ url, body: parsed, headers: init?.headers || {} });
    return new Response('', { status: 201 });
  };

  const res = await runBackfill({ db, didMapperUrl: 'http://localhost:4001', rate: 1000, didGenerator: (u) => `did:phantom:test:${u.id}` });

  // restore fetch
  globalThis.fetch = realFetch;

  expect(res.processed).toBe(2);
  // two posts
  expect(posts.length).toBe(2);
  expect(posts[0].body).toEqual({ did: 'did:phantom:test:user-1', user_id: 'user-1' });
  expect(posts[1].body).toEqual({ did: 'did:phantom:test:user-2', user_id: 'user-2' });

  const final = db._dump();
  const u1 = final.find(u => u.id === 'user-1');
  const u2 = final.find(u => u.id === 'user-2');
  const u3 = final.find(u => u.id === 'user-3');
  expect(u1?.phantom_did).toBe('did:phantom:test:user-1');
  expect(u2?.phantom_did).toBe('did:phantom:test:user-2');
  expect(u3?.phantom_did).toBe('did:phantom:existing');
});
