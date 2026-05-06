"""
NLPL Standard Library - Data Processing Module

Provides functions for data analysis, statistics, and data transformation.

This module demonstrates NLPL's capability for data science and analytics
alongside all other programming domains.
"""

from typing import Any, Dict, List, Union
from nexuslang.runtime.runtime import Runtime
import statistics


def calculate_mean(values: List[float]) -> float:
    """Calculate arithmetic mean of values."""
    if not values:
        return 0.0
    return sum(values) / len(values)


def calculate_median(values: List[float]) -> float:
    """Calculate median of values."""
    if not values:
        return 0.0
    return statistics.median(values)


def calculate_std_dev(values: List[float]) -> float:
    """Calculate standard deviation."""
    if len(values) < 2:
        return 0.0
    return statistics.stdev(values)


def calculate_variance(values: List[float]) -> float:
    """Calculate variance."""
    if len(values) < 2:
        return 0.0
    return statistics.variance(values)


def find_outliers(values: List[float], threshold: float = 2.0) -> List[float]:
    """Find outliers using standard deviation method."""
    if len(values) < 3:
        return []
    
    mean = calculate_mean(values)
    std_dev = calculate_std_dev(values)
    
    outliers = []
    for value in values:
        z_score = abs((value - mean) / std_dev) if std_dev > 0 else 0
        if z_score > threshold:
            outliers.append(value)
    
    return outliers


def normalize_data(values: List[float]) -> List[float]:
    """Normalize values to 0-1 range."""
    if not values:
        return []
    
    min_val = min(values)
    max_val = max(values)
    
    if min_val == max_val:
        return [0.5] * len(values)
    
    return [(v - min_val) / (max_val - min_val) for v in values]


def register_data_functions(runtime: Runtime) -> None:
    """Register data processing functions with the NexusLang runtime."""
    
    # Statistical functions
    runtime.register_function("calculate_mean", calculate_mean)
    runtime.register_function("calculate_median", calculate_median)
    runtime.register_function("calculate_std_dev", calculate_std_dev)
    runtime.register_function("calculate_variance", calculate_variance)
    
    # Data analysis
    runtime.register_function("find_outliers", find_outliers)
    runtime.register_function("normalize_data", normalize_data)


__all__ = [
    'calculate_mean',
    'calculate_median',
    'calculate_std_dev',
    'calculate_variance',
    'find_outliers',
    'normalize_data',
    'register_data_functions'
]
