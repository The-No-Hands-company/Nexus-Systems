#!/usr/bin/env python3
"""Refactor VersaTone engine: dawg::core -> dawg::engine, VoiceManager -> VoiceScheduler."""
import os, re
from pathlib import Path

ENGINE_ROOT = Path(__file__).parent.parent

REPLACEMENTS = [
    # Namespace declarations (old nested -> C++17 nested)
    (r'namespace dawg\s*\{\s*\n\s*namespace core\s*\{', 'namespace dawg::engine {', re.MULTILINE | re.DOTALL),
    # Closing namespace comments (two-line -> one-line)
    (r'\} // namespace core\s*\n\} // namespace dawg', '} // namespace dawg::engine', re.MULTILINE | re.DOTALL),
    # Include path rewrites
    (r'#include\s*[<"]dawg/core/', '#include <dawg/engine/', 0),
    # Qualified name rewrites
    (r'\bdawg::core::', 'dawg::engine::', 0),
    # Method name rename (must come before class name rename)
    (r'\bgetVoiceManager\b', 'getVoiceScheduler', 0),
    # Class name rename
    (r'\bVoiceManager\b', 'VoiceScheduler', 0),
]

EXTENSIONS = {'.h', '.hpp', '.cpp', '.cc'}

def refactor_file(path: Path) -> bool:
    original = path.read_text(encoding='utf-8')
    text = original
    for entry in REPLACEMENTS:
        pattern, replacement, flags = entry
        text = re.sub(pattern, replacement, text, flags=flags)
    if text != original:
        path.write_text(text, encoding='utf-8')
        return True
    return False

changed = []
for ext in EXTENSIONS:
    for p in ENGINE_ROOT.rglob(f'*{ext}'):
        if 'build' in p.parts:
            continue
        if refactor_file(p):
            changed.append(p)

print(f"Modified {len(changed)} files:")
for p in sorted(changed):
    print(f"  {p.relative_to(ENGINE_ROOT)}")
