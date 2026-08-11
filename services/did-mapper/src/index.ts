import http from 'http';
import { getUserByDid, getDidByUserId, createMapping, deleteMapping } from './db';

export function createServer() {
  const server = http.createServer(async (req, res) => {
    const host = req.headers.host || '127.0.0.1:4001';
    const url = new URL(req.url || '', `http://${host}`);
    const method = req.method || 'GET';
    res.setHeader('content-type', 'application/json');
    try {
      // GET /v1/dids/:did
      if (method === 'GET' && url.pathname.startsWith('/v1/dids/')) {
        const did = decodeURIComponent(url.pathname.replace('/v1/dids/', ''));
        const m = await getUserByDid(did);
        if (!m) {
          res.statusCode = 404;
          res.end(JSON.stringify({ error: 'not found' }));
          return;
        }
        res.end(JSON.stringify(m));
        return;
      }

      // GET /v1/users/:user_id/did
      if (method === 'GET' && url.pathname.startsWith('/v1/users/')) {
        const parts = url.pathname.split('/');
        if (parts.length === 5 && parts[4] === 'did') {
          const user_id = parts[3];
          const m = await getDidByUserId(user_id);
          if (!m) {
            res.statusCode = 404;
            res.end(JSON.stringify({ error: 'not found' }));
            return;
          }
          res.end(JSON.stringify(m));
          return;
        }
      }

      // POST /v1/dids and DELETE /v1/dids/:did require auth guard (placeholder)
      if ((method === 'POST' && url.pathname === '/v1/dids') || (method === 'DELETE' && url.pathname.startsWith('/v1/dids/'))) {
        // Minimal auth guard: check X-API-KEY env var. Replace with mTLS in production.
        const apiKeyHeader = (req.headers['x-api-key'] || req.headers['X-API-KEY']) as string | string[] | undefined;
        const apiKey = Array.isArray(apiKeyHeader) ? apiKeyHeader[0] : apiKeyHeader;
        if (apiKey !== process.env.X_API_KEY) {
          res.statusCode = 401;
          res.end(JSON.stringify({ error: 'unauthorized' }));
          return;
        }
      }

      // POST /v1/dids
      if (method === 'POST' && url.pathname === '/v1/dids') {
        let body = '';
        for await (const chunk of req) {
          body += chunk;
        }
        const obj = JSON.parse(body || '{}');
        if (!obj.did || !obj.user_id) {
          res.statusCode = 400;
          res.end(JSON.stringify({ error: 'invalid' }));
          return;
        }
        const mapping = { did: obj.did, user_id: obj.user_id, created_at: new Date().toISOString() };
        await createMapping(mapping);
        res.statusCode = 201;
        res.end(JSON.stringify(mapping));
        return;
      }

      // DELETE /v1/dids/:did
      if (method === 'DELETE' && url.pathname.startsWith('/v1/dids/')) {
        const did = decodeURIComponent(url.pathname.replace('/v1/dids/', ''));
        const ok = await deleteMapping(did);
        if (!ok) {
          res.statusCode = 404;
          res.end(JSON.stringify({ error: 'not found' }));
          return;
        }
        res.end(JSON.stringify({ ok: true }));
        return;
      }

      res.statusCode = 404;
      res.end(JSON.stringify({ error: 'not found' }));
    } catch (err: any) {
      res.statusCode = 500;
      res.end(JSON.stringify({ error: err?.message || String(err) }));
    }
  });
  return server;
}

export function start(port = 4001) {
  const s = createServer();
  return new Promise((resolve) => s.listen(port, () => resolve(s)));
}

export function stop(server: http.Server) {
  return new Promise((resolve, reject) => server.close((err) => (err ? reject(err) : resolve(undefined))));
}
