#!/usr/bin/env python3
import os

SRC = 'src/utility'
INC = 'include/nexus/utility'

simplified = 0
for root, dirs, files in os.walk(SRC):
    for fname in files:
        if not fname.endswith('.cpp'):
            continue
        cpp_path = os.path.join(root, fname)
        with open(cpp_path) as f:
            cpp = f.read()

        if 'instance()' not in cpp or 'enabled_' not in cpp:
            continue

        rel = os.path.relpath(cpp_path, SRC)
        cat = os.path.dirname(rel)
        name = os.path.splitext(os.path.basename(rel))[0]
        h_path = os.path.join(INC, cat, name + '.h')

        if not os.path.exists(h_path):
            continue
        h_size = os.path.getsize(h_path)

        if h_size > 1000:
            with open(h_path) as f:
                h = f.read()
            if 'instance()' not in h and 'enabled_' not in h:
                new_cpp = '// Header-only utility (no additional implementation needed)\n#include "nexus/utility/' + cat + '/' + name + '.h"\n'
                if cpp != new_cpp:
                    with open(cpp_path, 'w') as f:
                        f.write(new_cpp)
                    print('  SIMPLIFIED: ' + cat + '/' + name + '.cpp')
                    simplified += 1

print('Simplified ' + str(simplified) + ' files')
