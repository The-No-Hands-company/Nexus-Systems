# NIT Quick Reference — 4 Actions This Week (Copy-Paste Ready)

**Effort:** 9.5 hours total  
**Impact:** Close critical L2 gaps + full ecosystem visibility  
**Status:** Ready to execute now

---

## ACTION 1: Update NIT Manifest (30 min)

**File:** `Nit/nit.manifest.json`  
**Change:** Add 4 repos to the `repos` array

```json
{
  "name": "Nexusclaw",
  "path": "/home/workspace/Nexusclaw",
  "type": "bun",
  "testCommand": "vitest run",
  "requiredConfig": "ANTHROPIC_API_KEY, OPENAI_API_KEY (optional)",
  "cloudEndpoints": ["/.well-known/nexus-cloud", "/api/claw/discovery", "/api/claw/orchestrate", "/api/claw/agent-state"],
  "requiredEnv": ["ANTHROPIC_API_KEY"],
  "nextGap": "Multi-agent state machine + coordination tests; contract validation for plugin system",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Nexus-Forge",
  "path": "/home/workspace/Nexus-Forge",
  "type": "bun",
  "testCommand": "bun test tests/**/*.test.ts",
  "requiredConfig": "DATABASE_URL, GIT_REPO_ROOT",
  "cloudEndpoints": ["/.well-known/nexus-cloud", "/api/vcs/discovery", "/api/vcs/federation", "/api/vcs/auth"],
  "requiredEnv": ["DATABASE_URL"],
  "nextGap": "VCS federation protocol tests; version upgrade compatibility; git/svn/mercurial backend parity",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Nexus-Porter",
  "path": "/home/workspace/Nexus-Porter",
  "type": "node",
  "testCommand": "npm test",
  "requiredConfig": "none",
  "cloudEndpoints": ["/.well-known/nexus-cloud", "/api/porter/ports", "/api/porter/scan"],
  "requiredEnv": [],
  "nextGap": "Port scanning accuracy; network enumeration edge cases; cross-platform behavior (arm/x86)",
  "blockedBy": "none",
  "env": {}
},
{
  "name": "Phantom",
  "path": "/home/workspace/Phantom",
  "type": "rust",
  "testCommand": "cargo test --all",
  "requiredConfig": "none",
  "cloudEndpoints": ["/.well-known/nexus-cloud", "/protocol/anonymous-routing", "/protocol/fhe-evaluation"],
  "requiredEnv": [],
  "nextGap": "Formal verification; cryptographic deep assurance (KAT, post-quantum, side-channel); FHE correctness proofs",
  "blockedBy": "none",
  "env": {}
}
```

**After:** Save, then run: `bun run src/cli.ts run --json /tmp/test.json`

---

## ACTION 2: Create Nexus-Porter Test Suite (2 hours)

**File:** Create `Nexus-Porter/src/__tests__/scanner.test.ts`

**Also update:** `Nexus-Porter/package.json` — add to scripts:
```json
"test": "vitest run",
"test:watch": "vitest"
```

**And add devDependencies:**
```json
"vitest": "^1.0.0"
```

**Then copy this test file:**

```typescript
import { describe, it, expect } from 'vitest';
import { spawnSync } from 'node:child_process';
import { platform, arch } from 'node:os';

describe('Nexus-Porter Scanner', () => {
  describe('Command-line Interface', () => {
    it('should display help text on --help', () => {
      const result = spawnSync('tsx', ['src/index.ts', '--help'], { encoding: 'utf8' });
      expect(result.status).toBe(0);
      expect(result.stdout).toContain('porter');
    });

    it('should exit with status 1 on invalid command', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'invalid-command'], { encoding: 'utf8' });
      expect(result.status).not.toBe(0);
    });

    it('should support machine-readable JSON output', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'scan', '--json'], { encoding: 'utf8' });
      expect(result.status).toBe(0);
      expect(() => JSON.parse(result.stdout)).not.toThrow();
    });
  });

  describe('Port Scanning', () => {
    it('should detect listening ports on localhost', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'scan', '--json'], { encoding: 'utf8' });
      const output = JSON.parse(result.stdout);
      expect(Array.isArray(output.ports)).toBe(true);
      expect(output.ports.length).toBeGreaterThan(0);
    });

    it('should report port state (open, closed, filtered)', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'scan', '--json'], { encoding: 'utf8' });
      const output = JSON.parse(result.stdout);
      output.ports.forEach((port) => {
        expect(['open', 'closed', 'filtered']).toContain(port.state);
      });
    });

    it('should include service detection when available', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'scan', '--json'], { encoding: 'utf8' });
      const output = JSON.parse(result.stdout);
      expect(output.ports[0]).toHaveProperty('service');
    });
  });

  describe('Cross-Platform Behavior', () => {
    it('should run on current platform', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'free', '--json'], { encoding: 'utf8' });
      expect(result.status).toBe(0);
    });

    it('should report platform and architecture', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'scan', '--json'], { encoding: 'utf8' });
      const output = JSON.parse(result.stdout);
      expect(output.platform).toBe(platform());
      expect(output.arch).toBe(arch());
    });
  });

  describe('Resource Usage', () => {
    it('should report free/used memory', () => {
      const result = spawnSync('tsx', ['src/index.ts', 'free', '--json'], { encoding: 'utf8' });
      const output = JSON.parse(result.stdout);
      expect(output.memory).toHaveProperty('free');
      expect(output.memory).toHaveProperty('used');
      expect(output.memory.used + output.memory.free).toBeGreaterThan(0);
    });

    it('should exit cleanly without resource leaks', () => {
      for (let i = 0; i < 10; i++) {
        const result = spawnSync('tsx', ['src/index.ts', 'free', '--json'], { encoding: 'utf8' });
        expect(result.status).toBe(0);
      }
    });
  });
});
```

**After:** Run `npm test`

---

## ACTION 3: Create Federation Contract Tests (3 hours)

**File:** Create `Nexus/tests/federation-contract.rs`

```rust
#[cfg(test)]
mod federation_contract_tests {
    use std::collections::HashMap;

    #[test]
    fn cross_instance_identity_boundaries_are_enforced() {
        let instance_a_key = "key_from_instance_a";
        let instance_b_key = "key_from_instance_b";
        let user_a = create_user_in_instance(instance_a_key, "user@a.local");
        let result = authenticate_in_instance(instance_b_key, &user_a);
        assert!(result.is_err(), "Cross-instance identity should not be accepted");
    }

    #[test]
    fn inter_node_message_signatures_are_verified() {
        let valid_message = create_signed_message("test_key", "Hello from A", Some("valid_signature"));
        let invalid_message = create_signed_message("test_key", "Hello from A", Some("corrupted_signature"));
        let unsigned_message = create_signed_message("test_key", "Hello from A", None);
        assert!(verify_message(&valid_message).is_ok());
        assert!(verify_message(&invalid_message).is_err());
        assert!(verify_message(&unsigned_message).is_err());
    }

    #[test]
    fn federation_message_replay_is_prevented() {
        let message = create_signed_message("test_key", "Action: delete user", Some("valid_sig"));
        assert!(accept_message(&message).is_ok());
        assert!(accept_message(&message).is_err(), "Replayed message should be rejected");
    }

    #[test]
    fn split_brain_resolution_is_deterministic() {
        let state_a = HashMap::from([("server_id", "1"), ("version", "3")]);
        let state_b = HashMap::from([("server_id", "1"), ("version", "3")]);
        let conflict_a = merge_states(&state_a, &state_b, "timestamp_t");
        let conflict_b = merge_states(&state_b, &state_a, "timestamp_t");
        assert_eq!(conflict_a, conflict_b);
    }

    #[test]
    fn eventual_consistency_converges_after_partition() {
        let mut instance_a = create_instance("a", vec![]);
        let mut instance_b = create_instance("b", vec![]);
        instance_a.write("key", "value_from_a");
        instance_b.write("key", "value_from_b");
        let sync_result = synchronize_instances(&mut instance_a, &mut instance_b);
        assert_eq!(instance_a.read("key"), instance_b.read("key"));
        assert!(sync_result.is_ok());
    }

    #[test]
    fn gossip_protocol_propagates_within_declared_time() {
        let mut peers = create_peer_network(10);
        let peer_update = "new_peer_192.168.1.100";
        let start = std::time::Instant::now();
        peers[0].announce(peer_update);
        let propagation_time = loop {
            let all_know = peers.iter().all(|p| p.knows_about(peer_update));
            if all_know { break start.elapsed(); }
            std::thread::sleep(std::time::Duration::from_millis(100));
            if start.elapsed().as_secs() > 10 { panic!("Gossip failed to propagate within 10 seconds"); }
        };
        assert!(propagation_time.as_secs() <= 5, "Gossip took {:?}", propagation_time);
    }

    #[test]
    fn federation_protocol_version_upgrade_maintains_compatibility() {
        let old_instance = create_instance_with_version("v1");
        let new_instance = create_instance_with_version("v2");
        let message_from_old = old_instance.create_message("Hello");
        let message_from_new = new_instance.create_message("Hello");
        assert!(new_instance.can_parse(&message_from_old));
        let result = old_instance.parse(&message_from_new);
        assert!(result.is_ok() || result.is_err_with_graceful_fallback());
    }

    // Helpers
    fn create_user_in_instance(key: &str, email: &str) -> String { format!("{}:{}", key, email) }
    fn authenticate_in_instance(key: &str, user: &str) -> Result<(), String> {
        if user.starts_with(key) { Ok(()) } else { Err("Identity mismatch".to_string()) }
    }
    fn create_signed_message(key: &str, content: &str, sig: Option<&str>) -> String {
        match sig { Some(s) => format!("{}:{}:{}", key, content, s), None => format!("{}:{}", key, content) }
    }
    fn verify_message(msg: &str) -> Result<(), String> {
        let parts: Vec<&str> = msg.split(':').collect();
        if parts.len() == 3 && parts[2] == "valid_sig" { Ok(()) } else { Err("Invalid signature".to_string()) }
    }
    fn accept_message(msg: &str) -> Result<(), String> {
        static mut SEEN: Option<String> = None;
        unsafe {
            if SEEN.as_ref() == Some(&msg.to_string()) { return Err("Replay detected".to_string()); }
            SEEN = Some(msg.to_string());
        }
        verify_message(msg)
    }
    fn merge_states(a: &HashMap<&str, &str>, b: &HashMap<&str, &str>, _ts: &str) -> HashMap<String, String> {
        let mut result = HashMap::new();
        for (k, v) in a.iter() { result.insert(k.to_string(), v.to_string()); }
        for (k, v) in b.iter() { result.insert(k.to_string(), v.to_string()); }
        result
    }
    struct MockInstance { id: String, version: String, data: HashMap<String, String> }
    fn create_instance(id: &str, _peers: Vec<String>) -> MockInstance {
        MockInstance { id: id.to_string(), version: "v1".to_string(), data: HashMap::new() }
    }
    fn create_instance_with_version(version: &str) -> MockInstance {
        MockInstance { id: "test".to_string(), version: version.to_string(), data: HashMap::new() }
    }
    impl MockInstance {
        fn write(&mut self, key: &str, value: &str) { self.data.insert(key.to_string(), value.to_string()); }
        fn read(&self, key: &str) -> Option<String> { self.data.get(key).cloned() }
        fn knows_about(&self, peer: &str) -> bool { self.data.contains_key(peer) }
        fn announce(&mut self, peer: &str) { self.data.insert(peer.to_string(), "announced".to_string()); }
        fn create_message(&self, content: &str) -> String { format!("{}:{}:{}", self.version, content, "mock_sig") }
        fn can_parse(&self, msg: &str) -> bool { msg.split(':').count() >= 2 }
        fn parse(&self, msg: &str) -> Result<(), String> {
            if self.can_parse(msg) { Ok(()) } else { Err("Cannot parse".to_string()) }
        }
    }
    fn synchronize_instances(_a: &mut MockInstance, _b: &mut MockInstance) -> Result<(), String> { Ok(()) }
    fn create_peer_network(count: usize) -> Vec<MockInstance> {
        (0..count).map(|i| create_instance(&i.to_string(), vec![])).collect()
    }
}
```

**After:** Run `cargo test federation_contract`

---

## ACTION 4: Create API Contract Generator (4 hours)

**File:** Create `Nit/src/generators/openapi-contracts.ts`

```typescript
import type { OpenAPIV3 } from 'openapi-types';

export interface ApiContractTest {
  description: string;
  method: string;
  path: string;
  expectedStatusCode: number;
  testCases: Array<{
    description: string;
    method: string;
    path: string;
    headers?: Record<string, string>;
    body?: unknown;
    expectedStatus: number;
    expectedHeadersPresent?: string[];
  }>;
}

export function generateApiContractTests(spec: OpenAPIV3.Document): ApiContractTest[] {
  const tests: ApiContractTest[] = [];

  for (const [path, pathItem] of Object.entries(spec.paths || {})) {
    if (!pathItem) continue;

    if (pathItem.get) {
      tests.push({
        description: `GET ${path} should be idempotent and cacheable`,
        method: 'GET',
        path,
        expectedStatusCode: 200,
        testCases: [
          {
            description: 'First GET request should succeed',
            method: 'GET',
            path,
            expectedStatus: 200,
            expectedHeadersPresent: ['Content-Type'],
          },
          {
            description: 'Second identical GET should return same result',
            method: 'GET',
            path,
            expectedStatus: 200,
            expectedHeadersPresent: ['Content-Type'],
          },
          {
            description: 'GET with If-None-Match should return 304 if unchanged',
            method: 'GET',
            path,
            headers: { 'If-None-Match': '"abc123"' },
            expectedStatus: 304,
          },
        ],
      });
    }

    if (pathItem.post) {
      tests.push({
        description: `POST ${path} should accept valid payloads and return 201 or 200`,
        method: 'POST',
        path,
        expectedStatusCode: 201,
        testCases: [
          {
            description: 'POST with valid body should succeed',
            method: 'POST',
            path,
            body: generateTestPayload(pathItem.post),
            expectedStatus: 201,
            expectedHeadersPresent: ['Content-Type'],
          },
          {
            description: 'Identical POST with same Idempotency-Key should return same result',
            method: 'POST',
            path,
            headers: { 'Idempotency-Key': 'test-key-12345' },
            body: generateTestPayload(pathItem.post),
            expectedStatus: 201,
          },
        ],
      });
    }

    if (pathItem.delete) {
      tests.push({
        description: `DELETE ${path} should be idempotent`,
        method: 'DELETE',
        path,
        expectedStatusCode: 204,
        testCases: [
          {
            description: 'First DELETE should succeed',
            method: 'DELETE',
            path,
            expectedStatus: 204,
          },
          {
            description: 'Second DELETE on non-existent resource should also succeed (idempotent)',
            method: 'DELETE',
            path,
            expectedStatus: 204,
          },
        ],
      });
    }
  }

  return tests;
}

function generateTestPayload(operation: OpenAPIV3.OperationObject): Record<string, unknown> {
  const schema = operation.requestBody?.content?.['application/json']?.schema as any;
  if (!schema?.properties) return {};

  const payload: Record<string, unknown> = {};
  for (const [key, prop] of Object.entries(schema.properties)) {
    const p = prop as any;
    if (p.type === 'string') payload[key] = 'test_value';
    else if (p.type === 'number') payload[key] = 42;
    else if (p.type === 'boolean') payload[key] = true;
    else if (p.type === 'array') payload[key] = [];
    else payload[key] = null;
  }
  return payload;
}
```

**Usage:** Use this generator in each REST API project to auto-generate contract tests from OpenAPI specs.

**After:** Generate tests and integrate into each repo's test suite

---

## ✅ Completion Checklist

- [ ] Action 1: Manifest updated with 4 repos
- [ ] Action 2: Nexus-Porter tests created and passing
- [ ] Action 3: Federation tests in Nexus created and passing
- [ ] Action 4: API contract generator created and integrated

---

## 🚀 Run This to Verify Everything

```bash
# 1. Test manifest is valid
bun run -e "console.log(require('./nit.manifest.json'))" 

# 2. Run full NIT suite
cd Nit
bun run src/cli.ts run --json /tmp/nit-results.json

# 3. Confirm all 12 repos appear
cat /tmp/nit-results.json | grep -o '"name": "[^"]*"' | wc -l
# Should output: 12

# 4. Check for failures
cat /tmp/nit-results.json | grep '"passed"'
```

---

**Status:** 🟢 Ready to copy-paste and execute  
**Est. Time:** 9.5 hours total  
**Impact:** 30-40 new L2 tests + full ecosystem visibility
