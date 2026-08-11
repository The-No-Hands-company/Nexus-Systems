import * as users from "./src/users";
import { handleRequest } from "./src/server";

const PASSWORD = "correct-horse-battery-staple";
const BASE = "http://auth.test";

async function post(path: string, body: unknown, headers: Record<string,string> = {}) {
  const req = new Request(`${BASE}${path}`, { method: 'POST', headers: { 'content-type': 'application/json', ...headers }, body: JSON.stringify(body) });
  return await handleRequest(req);
}

async function main(){
  users.clearUsers();
  users.createUser({ username: 'alice', email: 'a@x.dev', password: PASSWORD });
  const loginRes = await post('/api/v1/auth/login', { username: 'alice', password: PASSWORD });
  console.log('login status', loginRes.status);
  const loginBody = await loginRes.json();
  const token = loginBody.token;
  const user = loginBody.user;
  console.log('user id', user.id);

  // stub fetch
  const originalFetch = globalThis.fetch;
  // @ts-ignore
  globalThis.fetch = async (input:any, init?:any) => {
    const url = String(input);
    console.log('fetch called', url);
    if (url.endsWith('/v1/dids')) {
      const b = JSON.parse(init.body || '{}');
      console.log('did-mapper body', b);
      return new Response(null, { status: 201 });
    }
    return originalFetch(input, init);
  };

  const res = await post('/api/v1/account/link-did', { did: 'did:phantom:abc123', signature: 'sig', nonce: 'nonce' }, { authorization: `Bearer ${token}` });
  console.log('link-did status', res.status);
  try {
    const body = await res.json();
    console.log('link-did body', body);
  } catch (e) { console.log('no body'); }

  // query /api/v1/auth/me
  const meReq = new Request(`${BASE}/api/v1/auth/me`, { method: 'GET', headers: { authorization: `Bearer ${token}` } });
  const meRes = await handleRequest(meReq);
  console.log('me status', meRes.status);
  const meBody = await meRes.json();
  console.log('me body', meBody);

  const updated = users.getUser(user.id);
  console.log('updated user phantom_did', updated?.phantom_did);

  // restore fetch
  // @ts-ignore
  globalThis.fetch = originalFetch;
}

main().catch((e)=>{ console.error(e); process.exit(1); });
