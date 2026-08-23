/**
 * Test environment preamble.
 *
 * @workspace/db throws at import time when DATABASE_URL is unset, so any test
 * whose import graph reaches it fails to load — not to fail an assertion, but
 * to collect at all.
 *
 * Values are placeholders, deliberately: these are unit tests whose modules
 * merely sit downstream of the db/storage imports and never open a connection.
 * A real-looking URL or credential here would invite a test to quietly talk to
 * something, and would put a secret in the repo. CI/local runs that DO need a
 * live database export the real values themselves (see check.sh).
 */
process.env.DATABASE_URL ??= "postgresql://127.0.0.1:1/nexus-tests-never-connect";
process.env.OBJECT_STORAGE_ENDPOINT ??= "http://127.0.0.1:9";
process.env.OBJECT_STORAGE_BUCKET ??= "test-bucket";
process.env.DEFAULT_OBJECT_STORAGE_BUCKET_ID ??= "test-bucket";
process.env.MINIO_ROOT_USER ??= "test-user";
process.env.MINIO_ROOT_PASSWORD ??= "test-password-not-a-secret";