#!/usr/bin/env bash
# Nexus Systems — API Contract Validation
# Tests that every app's Systems API payload is valid against Nexus-Cloud schema
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
APPS="$ROOT/apps"
RESULTS="/tmp/contract-test-results.txt"
>"$RESULTS"

cleanup() { jobs -p | xargs -r kill 2>/dev/null; wait 2>/dev/null; }
trap cleanup EXIT

echo "━━━ Contract Validation — Testing all app payloads against Nexus-Cloud ━━━"

# ── Start Nexus-Cloud ──────────────────────────────────────────────

cd "$APPS/Nexus-Cloud"
NEXUS_CLOUD_API_KEY=change-me PORT=8787 bun run src/index.ts > /tmp/contract-cloud.log 2>&1 &
for i in $(seq 1 20); do nc -z localhost 8787 2>/dev/null && break; sleep 1; done
echo "✓ Cloud running"

# ── Test every app's contract ──────────────────────────────────────

TOTAL=0
PASS=0
FAIL=0

# Use Python to import contracts and test against Cloud
python3 << 'PYEOF'
import json, subprocess, sys, os
from pathlib import Path

apps_dir = Path('/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/apps')
cloud_url = 'http://localhost:8787'
api_key = 'change-me'

total = 0
passed = 0
failed = 0

for app_dir in sorted(apps_dir.iterdir()):
    if not app_dir.is_dir(): continue
    if app_dir.name.startswith('.'): continue
    
    # Check for contracts.ts
    contracts_file = app_dir / 'src' / 'contracts.ts'
    if not contracts_file.exists(): continue
    
    # Read the contract to extract registration payload
    content = contracts_file.read_text()
    
    # Extract port
    import re
    port_match = re.search(r'defaultPort:\s*(\d+)', content)
    if not port_match: continue
    port = port_match.group(1)
    
    # Extract app ID and name
    id_match = re.search(r'id:\s*"([^"]+)"', content)
    name_match = re.search(r'name:\s*"([^"]+)"', content)
    desc_match = re.search(r'description:\s*"([^"]+)"', content)
    if not all([id_match, name_match]): continue
    
    app_id = id_match.group(1)
    app_name = name_match.group(1)
    description = desc_match.group(1) if desc_match else ""
    
    # Build payload (matching what buildSystemsApiRegistrationPayload produces)
    payload = {
        'id': app_id,
        'name': app_name,
        'description': description,
        'mode': 'orchestrated',
        'exposed': False,
        'health': 'healthy',
        'upstreamUrl': f'http://localhost:{port}',
        'capabilities': [],
        'metadata': {'version': 'v1', 'defaultPort': int(port)},
    }
    
    total += 1
    
    # POST to Cloud
    try:
        import urllib.request
        req = urllib.request.Request(
            f'{cloud_url}/api/v1/tools',
            data=json.dumps(payload).encode(),
            headers={
                'content-type': 'application/json',
                'x-api-key': api_key,
            },
            method='POST',
        )
        resp = urllib.request.urlopen(req, timeout=5)
        if resp.status == 201:
            print(f'  ✓ {app_name} ({app_id})')
            passed += 1
        else:
            print(f'  ✗ {app_name}: HTTP {resp.status}')
            failed += 1
    except Exception as e:
        print(f'  ✗ {app_name}: {e}')
        failed += 1

print(f'\n━━━ Results ━━━')
print(f'  Total: {total}')
print(f'  Pass:  {passed}')
print(f'  Fail:  {failed}')

sys.exit(0 if failed == 0 else 1)
PYEOF

EXIT=$?
echo ""
[ $EXIT -eq 0 ] && echo "✓ All contracts valid" || echo "✗ Some contracts failed"
exit $EXIT
