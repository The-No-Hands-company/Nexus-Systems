status: completed
commit: 69bedde6
one-line-test-summary: All Nexus-Auth tests passed (48 tests)
blockers: none

notes:
- Added phantom_did field to User type and createUser API to allow setting a Phantom DID on user creation.
- Updated OIDC ID token issuance (apps/Nexus-Auth/src/oidc.ts) to include phantom_did claim from the user record when present.
- Added unit test apps/Nexus-Auth/tests/token.test.ts verifying the claim is present in decoded ID token payload.
