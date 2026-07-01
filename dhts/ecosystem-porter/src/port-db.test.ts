import test from 'node:test';
import assert from 'node:assert/strict';
import { lookupPort } from './port-db.js';

test('lookupPort returns known metadata for ecosystem-critical ports', () => {
  const postgres = lookupPort(5432);
  assert.ok(postgres);
  assert.equal(postgres.name, 'PostgreSQL');
  assert.equal(postgres.category, 'database');

  const porter = lookupPort(2978);
  assert.ok(porter);
  assert.equal(porter.name, 'Nexus Porter');
  assert.equal(porter.category, 'dev');
});

test('lookupPort returns null for unknown ports', () => {
  assert.equal(lookupPort(65534), null);
});
