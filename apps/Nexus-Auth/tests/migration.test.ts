import { strict as assert } from 'assert';
import { readFileSync, unlinkSync, existsSync } from 'fs';

// This test implements a smoke check for the migration by applying the SQL
// migration to a temporary sqlite database and asserting the users table has
// the phantom_did column.

Bun.test('migration adds phantom_did column', async () => {
  const migrationPath = new URL('../db/migrations/2026-08-11-add-phantom-did.sql', import.meta.url).pathname;
  assert.ok(existsSync(migrationPath), `Migration file not found at ${migrationPath}`);

  const sql = readFileSync(migrationPath, 'utf8');

  const tmpDb = new URL('./.migration_test.db', import.meta.url).pathname;
  // Remove any existing DB
  try { if (existsSync(tmpDb)) unlinkSync(tmpDb); } catch (e) {}

  // Apply migration using sqlite3 CLI
  const run = Bun.spawnSync([
    'sqlite3',
    tmpDb,
  ], {
    stdin: sql,
    stdout: 'pipe',
    stderr: 'pipe',
  });

  if (run.exitCode !== 0) {
    throw new Error(`Failed to apply migration: ${run.stderr?.toString()}`);
  }

  // Query table info for users
  const info = Bun.spawnSync(['sqlite3', tmpDb, "PRAGMA table_info('users');"], { stdout: 'pipe', stderr: 'pipe' });
  if (info.exitCode !== 0) {
    throw new Error(`Failed to query DB: ${info.stderr?.toString()}`);
  }

  const out = info.stdout?.toString() || '';
  // PRAGMA table_info outputs rows with column name as the 2nd field, tab-separated.
  // Check whether the column name appears anywhere in output.
  assert.ok(out.includes('phantom_did'), `phantom_did not found in table schema:\n${out}`);

  // Cleanup
  try { unlinkSync(tmpDb); } catch (e) {}
});
