import sys
from pathlib import Path

# Make the src/ package importable regardless of test subdirectory depth.
_SRC = str(Path(__file__).resolve().parent.parent / "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)
