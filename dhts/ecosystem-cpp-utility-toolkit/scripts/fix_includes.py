#!/usr/bin/env python3
"""Auto-fix missing #includes in headers based on what std types they use."""

import os, re

INC = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug/include/nexus/utility"

NEEDS = {
    'std::unordered_map': 'unordered_map',
    'std::unordered_set': 'unordered_set', 
    'std::mutex': 'mutex',
    'std::lock_guard': 'mutex',
    'std::unique_lock': 'mutex',
    'std::condition_variable': 'condition_variable',
    'std::chrono::': 'chrono',
    'std::function<': 'functional',
    'std::atomic<': 'atomic',
    'std::filesystem::': 'filesystem',
    'std::thread': 'thread',
    'std::stacktrace': 'stacktrace',
    'std::source_location': 'source_location',
    'std::any': 'any',
    'std::optional<': 'optional',
    'std::regex': 'regex',
    'std::ofstream': 'fstream',
    'std::ifstream': 'fstream',
    'std::fstream': 'fstream',
    'std::ostream': 'ostream',
    'std::istream': 'istream',
    'std::stringstream': 'sstream',
    'std::ostringstream': 'sstream',
    'std::istringstream': 'sstream',
    'std::shared_ptr': 'memory',
    'std::unique_ptr': 'memory',
    'std::weak_ptr': 'memory',
    'std::make_shared': 'memory',
    'std::make_unique': 'memory',
}

fixed = 0
for root, dirs, files in os.walk(INC):
    for fname in files:
        if not fname.endswith('.h'):
            continue
        fpath = os.path.join(root, fname)
        with open(fpath) as f:
            content = f.read()

        existing = set()
        for line in content.split('\n'):
            m = re.match(r'\s*#include\s+<(\w+)>', line)
            if m:
                existing.add(m.group(1))

        needed_headers = set()
        for pattern, header in NEEDS.items():
            if pattern in content and header not in existing:
                needed_headers.add(header)

        # Also check for std::array, std::vector, std::list etc which are already likely included
        # Focus on the ones most commonly missing

        if needed_headers:
            lines = content.split('\n')
            last_inc = 0
            for i, line in enumerate(lines):
                if line.strip().startswith('#include'):
                    last_inc = i

            for h in sorted(needed_headers):
                lines.insert(last_inc + 1, f'#include <{h}>')
                last_inc += 1
                fixed += 1

            with open(fpath, 'w') as f:
                f.write('\n'.join(lines))

print(f"Added {fixed} missing #includes")
