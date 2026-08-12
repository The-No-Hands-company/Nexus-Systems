import { it, expect } from 'bun:test';
import { readFileSync, unlinkSync, existsSync } from 'fs';

// This test implements a smoke check for the migration by applying the SQL
// migration to a temporary sqlite database and asserting the users table has
// the phantom_did column.

it('migration adds phantom_did column', () => {
  const migrationPath = new URL('../db/migrations/2026-08-11-add-phantom-did.sql', import.meta.url).pathname;
  expect(existsSync(migrationPath)).toBe(true);

  const sql = readFileSync(migrationPath, 'utf8');

  const tmpDb = new URL('./.migration_test.db', import.meta.url).pathname;
  // Remove any existing DB
  try { if (existsSync(tmpDb)) unlinkSync(tmpDb); } catch (e) {}

  // Create a minimal users table so ALTER TABLE can run in sqlite
  const create = Bun.spawnSync(['sqlite3', tmpDb, "CREATE TABLE users (id TEXT PRIMARY KEY, username TEXT);"]); 
  expect(create.exitCode).toBe(0);

  // Apply migration using sqlite3 CLI via .read to ensure multi-statement SQL is executed as a file
  const run = Bun.spawnSync(['sqlite3', tmpDb, `.read ${migrationPath}`], { stdout: 'pipe', stderr: 'pipe' });
  expect(run.exitCode).toBe(0);

  // Query table info for users
  const info = Bun.spawnSync(['sqlite3', tmpDb, "PRAGMA table_info('users');"], { stdout: 'pipe', stderr: 'pipe' });
  expect(info.exitCode).toBe(0);

  const out = info.stdout?.toString() || '';
  // PRAGMA table_info outputs rows with column name as the 2nd field, tab-separated.
  // Check whether the column name appears anywhere in output.
  expect(out).toContain('phantom_did');

  // Cleanup
  try { unlinkSync(tmpDb); } catch (e) {}
});
