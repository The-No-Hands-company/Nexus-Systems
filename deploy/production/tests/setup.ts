/**
 * Keeps the test run off the network and off production's ports.
 *
 * CLOUD_URL points at a port nothing listens on, so getRoutes() fails its
 * fetch and falls back to its cache exactly as it does when Cloud is down —
 * tests then seed the cache explicitly rather than depending on a live Cloud.
 */
process.env.CLOUD_URL = "http://127.0.0.1:1";
process.env.PROXY_PORT = "0";
process.env.POLL_INTERVAL_MS = "0";
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:1";
