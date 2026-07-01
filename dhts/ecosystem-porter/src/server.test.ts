import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { createServer } from './server.js';

type JsonResponse = {
  status: number;
  body: unknown;
};

function requestJson(port: number, path: string, method = 'GET'): Promise<JsonResponse> {
  return new Promise((resolve, reject) => {
    const req = http.request(
      {
        host: '127.0.0.1',
        port,
        path,
        method,
      },
      (res) => {
        let data = '';
        res.on('data', (chunk) => {
          data += chunk;
        });
        res.on('end', () => {
          try {
            resolve({
              status: res.statusCode ?? 0,
              body: JSON.parse(data),
            });
          } catch (err) {
            reject(err);
          }
        });
      },
    );

    req.on('error', reject);
    req.end();
  });
}

test('GET /health returns ok payload', async () => {
  const server = createServer(0);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', () => resolve()));

  try {
    const address = server.address();
    assert.ok(address && typeof address === 'object');
    const port = address.port;

    const response = await requestJson(port, '/health');
    assert.equal(response.status, 200);

    const body = response.body as { ok: boolean; port: number; ts: string };
    assert.equal(body.ok, true);
    assert.equal(typeof body.ts, 'string');
    assert.equal(body.port, 0);
  } finally {
    await new Promise<void>((resolve, reject) => server.close((err) => (err ? reject(err) : resolve())));
  }
});

test('non-GET request returns 405', async () => {
  const server = createServer(0);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', () => resolve()));

  try {
    const address = server.address();
    assert.ok(address && typeof address === 'object');
    const port = address.port;

    const response = await requestJson(port, '/health', 'POST');
    assert.equal(response.status, 405);

    const body = response.body as { error: string };
    assert.equal(body.error, 'Method not allowed');
  } finally {
    await new Promise<void>((resolve, reject) => server.close((err) => (err ? reject(err) : resolve())));
  }
});

test('invalid /available range returns 400', async () => {
  const server = createServer(0);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', () => resolve()));

  try {
    const address = server.address();
    assert.ok(address && typeof address === 'object');
    const port = address.port;

    const response = await requestJson(port, '/available?from=100&to=70000');
    assert.equal(response.status, 400);

    const body = response.body as { error: string };
    assert.equal(body.error, 'Invalid range. Use ?from=1024&to=9999');
  } finally {
    await new Promise<void>((resolve, reject) => server.close((err) => (err ? reject(err) : resolve())));
  }
});

test('GET /capabilities returns capabilities array', async () => {
  const server = createServer(0);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', () => resolve()));

  try {
    const address = server.address();
    assert.ok(address && typeof address === 'object');
    const port = address.port;

    const response = await requestJson(port, '/capabilities');
    assert.equal(response.status, 200);

    const body = response.body as { capabilities: Array<{ id: string; status: string }> };
    assert.ok(Array.isArray(body.capabilities));
    assert.ok(body.capabilities.some((c) => c.id === 'inventory-live-scan'));
  } finally {
    await new Promise<void>((resolve, reject) => server.close((err) => (err ? reject(err) : resolve())));
  }
});

test('GET /reconfigure/plan without from returns 400', async () => {
  const server = createServer(0);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', () => resolve()));

  try {
    const address = server.address();
    assert.ok(address && typeof address === 'object');
    const port = address.port;

    const response = await requestJson(port, '/reconfigure/plan');
    assert.equal(response.status, 400);

    const body = response.body as { error: string };
    assert.equal(body.error, 'Invalid from port. Use ?from=3000');
  } finally {
    await new Promise<void>((resolve, reject) => server.close((err) => (err ? reject(err) : resolve())));
  }
});
