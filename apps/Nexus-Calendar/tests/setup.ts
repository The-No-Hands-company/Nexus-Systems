// Package tests must never register their disposable servers in a running
// default Cloud registry. Cloud integration tests enable only their own fixture.
process.env.NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION = "false";
