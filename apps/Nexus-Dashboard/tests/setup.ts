/**
 * Pins every upstream address before any test file is imported.
 *
 * src/server.ts reads its upstreams into top-level consts, so their values are
 * frozen by whichever test file imports it first — and that is decided by
 * alphabetical load order. Each file used to set only the upstreams it cared
 * about, which meant adding a test file could silently change what a different
 * file's module graph was pointed at.
 *
 * That is not hypothetical: adding tests/calendar-proxy.test.ts put a file
 * ahead of cloud-proxy.test.ts that did not set NEXUS_CLOUD_URL, so CLOUD_URL
 * froze to its production default of 127.0.0.1:8787. On a developer machine
 * that is the *live Cloud*, which answered and made the suite pass. On a CI
 * runner nothing is there, and two tests failed for a reason that had nothing
 * to do with either of them.
 *
 * Preloading removes load order from the picture entirely: the frozen values
 * are always these, whoever imports first.
 */
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:4398";
process.env.NEXUS_TERMINAL_URL = "http://127.0.0.1:4397";
process.env.NEXUS_EMAIL_URL = "http://127.0.0.1:4397";
process.env.NEXUS_CALENDAR_URL = "http://127.0.0.1:4396";
