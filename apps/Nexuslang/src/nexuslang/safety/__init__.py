"""
NLPL Safety and Error Handling System
======================================

Provides production-ready error handling:
- Result<T, E> type for recoverable errors
- Panic system for unrecoverable errors
- Null safety with Optional<T>
- Basic ownership and borrow checking
"""

from nexuslang.safety.result import Result, Ok, Err
from nexuslang.safety.panic import Panic, PanicHandler, set_panic_handler
from nexuslang.safety.null_safety import NullSafetyChecker
from nexuslang.safety.ownership import OwnershipTracker, MoveSemantics

__all__ = [
    'Result', 'Ok', 'Err',
    'Panic', 'PanicHandler', 'set_panic_handler',
    'NullSafetyChecker',
    'OwnershipTracker', 'MoveSemantics'
]
