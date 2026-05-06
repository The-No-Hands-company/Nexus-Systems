"""
Random number generation for NexusLang.
Enhanced random utilities beyond math module.
"""

import random
from typing import List, Any
from ...runtime.runtime import Runtime


def random_int(min_val: int, max_val: int) -> int:
    """Generate random integer in range [min, max] inclusive."""
    return random.randint(min_val, max_val)


def random_float(min_val: float = 0.0, max_val: float = 1.0) -> float:
    """Generate random float in range [min, max)."""
    return random.uniform(min_val, max_val)


def random_choice(items: List[Any]) -> Any:
    """Choose random item from list."""
    if not items:
        raise ValueError("Cannot choose from empty list")
    return random.choice(items)


def random_choices(items: List[Any], k: int = 1) -> List[Any]:
    """Choose k random items from list (with replacement)."""
    if not items:
        raise ValueError("Cannot choose from empty list")
    return random.choices(items, k=k)


def random_sample(items: List[Any], k: int) -> List[Any]:
    """Choose k unique random items from list (without replacement)."""
    if not items:
        raise ValueError("Cannot sample from empty list")
    if k > len(items):
        raise ValueError(f"Sample size {k} larger than population {len(items)}")
    return random.sample(items, k)


def shuffle_list(items: List[Any]) -> List[Any]:
    """Shuffle list randomly (returns new list)."""
    result = items.copy()
    random.shuffle(result)
    return result


def random_bool(probability: float = 0.5) -> bool:
    """Generate random boolean with given probability of True."""
    return random.random() < probability


def random_gauss(mu: float = 0.0, sigma: float = 1.0) -> float:
    """Generate random number from Gaussian (normal) distribution."""
    return random.gauss(mu, sigma)


def random_exponential(lambd: float = 1.0) -> float:
    """Generate random number from exponential distribution."""
    return random.expovariate(lambd)


def set_random_seed(seed: int) -> None:
    """Set random seed for reproducibility."""
    random.seed(seed)


def get_random_state() -> Any:
    """Get current random generator state."""
    return random.getstate()


def set_random_state(state: Any) -> None:
    """Set random generator state."""
    random.setstate(state)


def register_random_functions(runtime: Runtime) -> None:
    """Register random functions with the runtime."""
    runtime.register_function("random_int", random_int)
    runtime.register_function("random_float", random_float)
    runtime.register_function("random_choice", random_choice)
    runtime.register_function("random_choices", random_choices)
    runtime.register_function("random_sample", random_sample)
    runtime.register_function("shuffle_list", shuffle_list)
    runtime.register_function("random_bool", random_bool)
    runtime.register_function("random_gauss", random_gauss)
    runtime.register_function("random_exponential", random_exponential)
    runtime.register_function("set_random_seed", set_random_seed)
    runtime.register_function("get_random_state", get_random_state)
    runtime.register_function("set_random_state", set_random_state)
    
    # Aliases
    runtime.register_function("randint", random_int)
    runtime.register_function("randfloat", random_float)
    runtime.register_function("choice", random_choice)
    runtime.register_function("sample", random_sample)
    runtime.register_function("shuffle", shuffle_list)
