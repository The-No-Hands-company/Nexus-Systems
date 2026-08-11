import { describe, expect, it, beforeAll } from 'bun:test';
import { createPhantomSDK } from '../../../packages/phantom-sdk/src/index';
import * as phantomUtils from '../../../packages/phantom-utils/src/verify';
import { handleRequest } from '../src/server';
import { createUser, getUser } from '../src/users';
import { createSession } from '../src/sessions';

// Integration test: end-to-end link DID flow using the Phantom SDK mock (no stub)
// - Generates an identity via the Phantom SDK mock
// - Signs a nonce with the identity's handle (real sign/verify via mock)
// - Monkeypatches verifyDidSignature to call the mock SDK verify for that DID
// - Mocks global fetch to assert DID Mapper POST payload and header

let sdk: any;
let identity: any;

beforeAll(async () => {
  sdk = await createPhantomSDK();
  identity = await sdk.generateIdentity('test-link-did');
});

describe('POST /api/v1/account/link-did', () => {
  it('verifies signature, posts to DID Mapper, and updates user phantom_did', async () => {
    // Prepare nonce and signature
    const nonce = 'test-nonce-' + Date.now();
    const sig = await sdk.sign(identity.handle, new TextEncoder().encode(nonce));

    // Inject test verifier to use the mock SDK's verify for our DID
    (phantomUtils as any).__test_set_verifier(async (did: string, message: string, signature: string) => {
      if (did === identity.did) {
        return await sdk.verify(identity.handle, new TextEncoder().encode(message), signature);
      }
      return false;
    });

    // Mock fetch to capture DID Mapper POST
    const calls: any[] = [];
    const originalFetch = globalThis.fetch;
    (globalThis as any).fetch = async (input: any, init?: any) => {
      calls.push({ input, init });
      return new Response('', { status: 201 });
    };

    try {
      // Create a user and session
      const user = createUser({ username: 'linktester', email: 'link@test', password: 'pw' });
      const session = createSession({ userId: user.id, ipAddress: '127.0.0.1', userAgent: 'bun-test' });

      // Make request to the handler
      const req = new Request('http://localhost/api/v1/account/link-did', {
        method: 'POST',
        headers: { Authorization: `Bearer ${session.token}`, 'content-type': 'application/json' },
        body: JSON.stringify({ did: identity.did, signature: sig, nonce }),
      });

      const res = await handleRequest(req);
      expect(res.status).toBe(200);

      // Assert fetch (DID Mapper) was called with /v1/dids and JSON body contains did and user_id
      expect(calls.length).toBeGreaterThan(0);
      const call = calls[0];
      const calledUrl = String(call.input);
      expect(calledUrl.includes('/v1/dids')).toBe(true);
      const bodyText = call.init.body;
      const parsed = typeof bodyText === 'string' ? JSON.parse(bodyText) : bodyText;
      expect(parsed.did).toBe(identity.did);
      expect(parsed.user_id).toBeDefined();

      // Confirm user phantom_did updated
      const updated = getUser(user.id);
      expect(updated).toBeDefined();
      expect((updated as any).phantom_did).toBe(identity.did);
    } finally {
      // restore test verifier and fetch
      (phantomUtils as any).__test_set_verifier(null);
      (globalThis as any).fetch = originalFetch;
    }
  });
});
