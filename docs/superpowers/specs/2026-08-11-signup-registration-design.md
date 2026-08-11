# Hybrid Signup / Registration Design
Date: 2026-08-11
Author: Nexus Systems (design by Copilot)

Summary

Implement a hybrid identity model: Nexus-Auth (OAuth2/OIDC) acts as the primary Identity Provider (IdP) for UX and session management; each user account is associated with a Phantom DID for cryptographic identity and decentralized mapping. This allows smooth onboarding while enabling DID-backed features later.

Goals

- Single signup flow for all apps via Nexus-Auth.
- Create or attach a Phantom DID to each account at signup.
- Central DID↔user mapping service for lookups by apps.
- Token flows remain standard OAuth2/OIDC; tokens include DID in claims when available.
- Support linking existing DIDs to accounts and migration tools.

Architecture

Components:
- Nexus-Auth: existing Bun service (port 4310). Handles OAuth2/OIDC flows, user profile DB, session tokens.
- DID Mapper microservice: lightweight service (Bun/Node) that stores mappings {did -> user_id} and exposes lookup APIs.
- User Profile DB: Nexus-Auth-managed user table extended with phantom_did field and did_metadata.
- SDKs / Middleware: small libs for apps to validate tokens, fetch DID mapping, and cache results.
- Cloud Registry: Nexus-Cloud stores discovery metadata and service endpoints.

Data model (sketch)

User table (users):
- id (uuid)
- email (nullable)
- username
- password_hash (if local auth enabled)
- phantom_did (nullable)
- did_metadata (json nullable)
- created_at, updated_at

did_mappings table (did_mapper):
- did (primary key)
- user_id (indexed)
- created_at, linked_at, source

Signup flow (happy path)

1. User initiates signup via any app → redirects to Nexus-Auth (OAuth2 Authorization Code + PKCE).
2. Nexus-Auth completes registration (email verify / social login) and creates user record.
3. Nexus-Auth checks DID Mapper for existing DID (optional) OR creates a new Phantom DID using the Phantom SDK (server-side key generation) depending on policy.
4. Nexus-Auth stores phantom_did on user profile and writes did_mappings entry.
5. Nexus-Auth issues ID/Access tokens; ID token contains claim `phantom_did`.
6. Apps validate tokens locally; if they need DID-resolved data, they call DID Mapper API (authenticated service-to-service) or use cached mapping.

Token & Claims

- ID token (OIDC) includes claim `phantom_did` when present.
- Access tokens remain opaque or JWT per existing Nexus-Auth behavior; introspection endpoint returns `phantom_did` if needed.

Linking existing DID

- Provide UI in Nexus-Auth account settings: user proves DID ownership by signing a nonce/challenge with their DID key and POSTing proof to Nexus-Auth.
- Nexus-Auth verifies signature using Phantom SDK, then writes mapping and updates profile.

Error handling & retries

- DID Mapper transient failures: Nexus-Auth uses retry (exponential backoff) and a local write-ahead log to reconcile mappings if mapping write fails after account creation.
- Duplicate DID mapping: enforce uniqueness on did_mappings.deduplicate by rejecting conflicting links and providing admin/manual merge tooling.

Security considerations

- Protect DID Mapper with mTLS or API key; limit write operations to Nexus-Auth.
- Store DID private keys per policy (prefer server-managed ephemeral keys or HSM-backed keys); users who self-manage keys must be able to link them via proof-of-possession flows.
- Audit logs for mapping operations and account linking/unlinking.

Testing

- Unit tests for Nexus-Auth: token claims, DID attach/detach, link-proof verification.
- Integration tests: end-to-end signup flow with DID creation and mapping verification using local Phantom SDK or test doubles.
- E2E: run in local ecosystem (`scripts/ecosystem-local.sh`) validating app token introspection and DID-based features.

Migration & Rollout

- Backfill existing users by running a migration job that creates Phantom DIDs for accounts without phantom_did (rate-limited, idempotent).
- Start with Nexus-Auth generating DIDs server-side; later expose DID-first flow if needed.

Next steps

1. Commit this spec to `docs/superpowers/specs/` (done).
2. Implement DID Mapper service scaffold (small API + storage).
3. Add phantom_did field and migration in Nexus-Auth user model.
4. Update Nexus-Auth token issuance to include `phantom_did` claim.
5. Write SDK middleware and integration tests.

Notes

- This design chooses central UX first (Nexus-Auth) to minimize disruption across 89 apps while enabling decentralized identity features via Phantom DID.
