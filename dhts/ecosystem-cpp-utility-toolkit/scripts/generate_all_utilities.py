#!/usr/bin/env python3
"""Generate ALL 600+ C++ utility files for the comprehensive toolkit"""

import os
from pathlib import Path

BASE_INCLUDE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/c++utilitytoolkit/include/nexus/utility"
BASE_SRC = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Active/c++utilitytoolkit/src/utility"

# Import from both scripts
exec(open('generate_utilities.py').read())
exec(open('complete_utilities_manifest.py').read())

# Combine all utilities
ALL_UTILITIES = UTILITIES + COMPLETE_UTILITIES

print(f"Total utilities to generate: {len(ALL_UTILITIES)}")
print(f"Total files (headers + sources): {len(ALL_UTILITIES) * 2}")

# Generate all
UTILITIES = ALL_UTILITIES
main()
