import { describe, it, expect, beforeEach } from 'bun:test';
import { createMapping, getUserByDid, getDidByUserId, deleteMapping, clearStore } from '../src/db';
import { createServer } from '../src/index';

beforeEach(() => {
  clearStore();
});

describe('DID Mapper DB', () => {
  it('createMapping and getUserByDid', async () => {
    const m = { did: 'did:example:123', user_id: 'user-1', created_at: new Date().toISOString() };
    await createMapping(m);
    const got = await getUserByDid(m.did);
    expect(got).not.toBeNull();
    expect(got?.user_id).toBe('user-1');
    const byUser = await getDidByUserId('user-1');
    expect(byUser?.did).toBe(m.did);
  });

  it('http smoke test', async () => {
    process.env.X_API_KEY = 'local-test-key';
    const server = await new Promise((res) => {
      const s = createServer();
      s.listen(4001, () => res(s));
    });
    try {
      const resp = await fetch('http://127.0.0.1:4001/v1/dids', { method: 'POST', headers: { 'content-type': 'application/json', 'x-api-key': 'local-test-key' }, body: JSON.stringify({ did: 'did:example:abc', user_id: 'user-http' }) });
      expect(resp.status).toBe(201);
      const data = await resp.json();
      expect(data.did).toBe('did:example:abc');

      const getResp = await fetch('http://127.0.0.1:4001/v1/dids/did:example:abc');
      expect(getResp.status).toBe(200);
      const getData = await getResp.json();
      expect(getData.user_id).toBe('user-http');
    } finally {
      // @ts-ignore
      server.close();
    }
  }, 30_000);
});
