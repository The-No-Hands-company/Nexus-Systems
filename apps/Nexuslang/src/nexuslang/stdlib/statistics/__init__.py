"""
Statistics module for NexusLang.

Provides statistical analysis functions for data processing.

Features:
- Descriptive statistics (mean, median, mode, stdev, variance)
- Distribution analysis (percentile, quartiles, range)
- Correlation and regression
- Data normalization and scaling
- Binning and histograms

Example usage in NexusLang:
    set data to [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    
    # Basic statistics
    set avg to stats_mean with data
    set mid to stats_median with data
    set sd to stats_stdev with data
    
    # Distribution analysis
    set p95 to stats_percentile with data and 95
    set qs to stats_quartiles with data
    
    # Correlation
    set x to [1, 2, 3, 4, 5]
    set y to [2, 4, 6, 8, 10]
    set corr to stats_correlation with x and y
"""

from ...runtime.runtime import Runtime
import math
from collections import Counter


def stats_mean(values):
    """
    Calculate the arithmetic mean (average).
    
    Args:
        values: List of numbers
    
    Returns:
        Mean value
    """
    if not values:
        return 0
    
    return sum(values) / len(values)


def stats_median(values):
    """
    Calculate the median (middle value).
    
    Args:
        values: List of numbers
    
    Returns:
        Median value
    """
    if not values:
        return 0
    
    sorted_values = sorted(values)
    n = len(sorted_values)
    
    if n % 2 == 0:
        # Even number of values - average of two middle values
        return (sorted_values[n // 2 - 1] + sorted_values[n // 2]) / 2
    else:
        # Odd number of values - middle value
        return sorted_values[n // 2]


def stats_mode(values):
    """
    Calculate the mode (most common value).
    
    Args:
        values: List of numbers
    
    Returns:
        Mode value (or first mode if multiple)
    """
    if not values:
        return None
    
    counter = Counter(values)
    max_count = max(counter.values())
    
    # Find all modes
    modes = [value for value, count in counter.items() if count == max_count]
    
    return modes[0] if modes else None


def stats_modes(values):
    """
    Get all mode values.
    
    Args:
        values: List of numbers
    
    Returns:
        List of all mode values
    """
    if not values:
        return []
    
    counter = Counter(values)
    max_count = max(counter.values())
    
    return [value for value, count in counter.items() if count == max_count]


def stats_variance(values, sample=True):
    """
    Calculate variance.
    
    Args:
        values: List of numbers
        sample: If True, use sample variance (n-1), else population variance (n)
    
    Returns:
        Variance
    """
    if not values:
        return 0
    
    if len(values) == 1:
        return 0
    
    mean = stats_mean(values)
    squared_diffs = [(x - mean) ** 2 for x in values]
    
    if sample:
        return sum(squared_diffs) / (len(values) - 1)
    else:
        return sum(squared_diffs) / len(values)


def stats_stdev(values, sample=True):
    """
    Calculate standard deviation.
    
    Args:
        values: List of numbers
        sample: If True, use sample stdev (n-1), else population stdev (n)
    
    Returns:
        Standard deviation
    """
    variance = stats_variance(values, sample)
    return math.sqrt(variance)


def stats_range(values):
    """
    Calculate the range (max - min).
    
    Args:
        values: List of numbers
    
    Returns:
        Range value
    """
    if not values:
        return 0
    
    return max(values) - min(values)


def stats_percentile(values, percentile):
    """
    Calculate a percentile value.
    
    Args:
        values: List of numbers
        percentile: Percentile (0-100)
    
    Returns:
        Value at the given percentile
    """
    if not values:
        return 0
    
    if percentile < 0 or percentile > 100:
        raise ValueError("Percentile must be between 0 and 100")
    
    sorted_values = sorted(values)
    n = len(sorted_values)
    
    if percentile == 0:
        return sorted_values[0]
    if percentile == 100:
        return sorted_values[-1]
    
    # Calculate index (linear interpolation)
    index = (percentile / 100) * (n - 1)
    lower_index = int(index)
    upper_index = min(lower_index + 1, n - 1)
    
    # Interpolate between values
    weight = index - lower_index
    return sorted_values[lower_index] * (1 - weight) + sorted_values[upper_index] * weight


def stats_quartiles(values):
    """
    Calculate quartiles (Q1, Q2/median, Q3).
    
    Args:
        values: List of numbers
    
    Returns:
        Dictionary with q1, q2, q3
    """
    if not values:
        return {"q1": 0, "q2": 0, "q3": 0}
    
    return {
        "q1": stats_percentile(values, 25),
        "q2": stats_percentile(values, 50),
        "q3": stats_percentile(values, 75)
    }


def stats_iqr(values):
    """
    Calculate the interquartile range (Q3 - Q1).
    
    Args:
        values: List of numbers
    
    Returns:
        IQR value
    """
    quartiles = stats_quartiles(values)
    return quartiles["q3"] - quartiles["q1"]


def stats_outliers(values, threshold=1.5):
    """
    Identify outliers using IQR method.
    
    Args:
        values: List of numbers
        threshold: IQR multiplier (default 1.5 for standard outliers)
    
    Returns:
        Dictionary with outliers list and bounds
    """
    if not values:
        return {"outliers": [], "lower_bound": 0, "upper_bound": 0}
    
    quartiles = stats_quartiles(values)
    iqr = quartiles["q3"] - quartiles["q1"]
    
    lower_bound = quartiles["q1"] - threshold * iqr
    upper_bound = quartiles["q3"] + threshold * iqr
    
    outliers = [x for x in values if x < lower_bound or x > upper_bound]
    
    return {
        "outliers": outliers,
        "lower_bound": lower_bound,
        "upper_bound": upper_bound
    }


def stats_correlation(x_values, y_values):
    """
    Calculate Pearson correlation coefficient.
    
    Args:
        x_values: First list of numbers
        y_values: Second list of numbers
    
    Returns:
        Correlation coefficient (-1 to 1)
    """
    if len(x_values) != len(y_values):
        raise ValueError("Lists must have the same length")
    
    if not x_values:
        return 0
    
    n = len(x_values)
    if n == 1:
        return 0
    
    # Calculate means
    mean_x = stats_mean(x_values)
    mean_y = stats_mean(y_values)
    
    # Calculate covariance and standard deviations
    covariance = sum((x - mean_x) * (y - mean_y) for x, y in zip(x_values, y_values))
    std_x = math.sqrt(sum((x - mean_x) ** 2 for x in x_values))
    std_y = math.sqrt(sum((y - mean_y) ** 2 for y in y_values))
    
    if std_x == 0 or std_y == 0:
        return 0
    
    return covariance / (std_x * std_y)


def stats_covariance(x_values, y_values, sample=True):
    """
    Calculate covariance between two datasets.
    
    Args:
        x_values: First list of numbers
        y_values: Second list of numbers
        sample: If True, use sample covariance (n-1)
    
    Returns:
        Covariance value
    """
    if len(x_values) != len(y_values):
        raise ValueError("Lists must have the same length")
    
    if not x_values:
        return 0
    
    n = len(x_values)
    mean_x = stats_mean(x_values)
    mean_y = stats_mean(y_values)
    
    covariance = sum((x - mean_x) * (y - mean_y) for x, y in zip(x_values, y_values))
    
    if sample and n > 1:
        return covariance / (n - 1)
    else:
        return covariance / n


def stats_linear_regression(x_values, y_values):
    """
    Calculate linear regression (y = mx + b).
    
    Args:
        x_values: Independent variable values
        y_values: Dependent variable values
    
    Returns:
        Dictionary with slope, intercept, r_squared
    """
    if len(x_values) != len(y_values):
        raise ValueError("Lists must have the same length")
    
    if not x_values:
        return {"slope": 0, "intercept": 0, "r_squared": 0}
    
    n = len(x_values)
    mean_x = stats_mean(x_values)
    mean_y = stats_mean(y_values)
    
    # Calculate slope
    numerator = sum((x - mean_x) * (y - mean_y) for x, y in zip(x_values, y_values))
    denominator = sum((x - mean_x) ** 2 for x in x_values)
    
    if denominator == 0:
        return {"slope": 0, "intercept": mean_y, "r_squared": 0}
    
    slope = numerator / denominator
    intercept = mean_y - slope * mean_x
    
    # Calculate R-squared
    y_pred = [slope * x + intercept for x in x_values]
    ss_res = sum((y - y_p) ** 2 for y, y_p in zip(y_values, y_pred))
    ss_tot = sum((y - mean_y) ** 2 for y in y_values)
    
    r_squared = 1 - (ss_res / ss_tot) if ss_tot != 0 else 0
    
    return {
        "slope": slope,
        "intercept": intercept,
        "r_squared": r_squared
    }


def stats_normalize(values, method="zscore"):
    """
    Normalize data.
    
    Args:
        values: List of numbers
        method: "zscore" (standardize) or "minmax" (0-1 scaling)
    
    Returns:
        List of normalized values
    """
    if not values:
        return []
    
    if method == "zscore":
        # Z-score normalization: (x - mean) / stdev
        mean = stats_mean(values)
        stdev = stats_stdev(values)
        
        if stdev == 0:
            return [0] * len(values)
        
        return [(x - mean) / stdev for x in values]
    
    elif method == "minmax":
        # Min-max normalization: (x - min) / (max - min)
        min_val = min(values)
        max_val = max(values)
        
        if min_val == max_val:
            return [0.5] * len(values)
        
        return [(x - min_val) / (max_val - min_val) for x in values]
    
    else:
        raise ValueError(f"Unknown normalization method: {method}")


def stats_histogram(values, bins=10):
    """
    Create histogram bins.
    
    Args:
        values: List of numbers
        bins: Number of bins or list of bin edges
    
    Returns:
        Dictionary with bins, counts, edges
    """
    if not values:
        return {"bins": [], "counts": [], "edges": []}
    
    if isinstance(bins, int):
        # Create equal-width bins
        min_val = min(values)
        max_val = max(values)
        
        if min_val == max_val:
            return {
                "bins": [min_val],
                "counts": [len(values)],
                "edges": [min_val, min_val]
            }
        
        bin_width = (max_val - min_val) / bins
        edges = [min_val + i * bin_width for i in range(bins + 1)]
    else:
        # Use provided bin edges
        edges = sorted(bins)
    
    # Count values in each bin
    counts = [0] * (len(edges) - 1)
    bin_centers = []
    
    for i in range(len(edges) - 1):
        lower = edges[i]
        upper = edges[i + 1]
        bin_centers.append((lower + upper) / 2)
        
        # Count values in this bin (inclusive lower, exclusive upper for all but last bin)
        if i == len(edges) - 2:
            # Last bin - include upper edge
            counts[i] = sum(1 for x in values if lower <= x <= upper)
        else:
            counts[i] = sum(1 for x in values if lower <= x < upper)
    
    return {
        "bins": bin_centers,
        "counts": counts,
        "edges": edges
    }


def stats_summary(values):
    """
    Get comprehensive summary statistics.
    
    Args:
        values: List of numbers
    
    Returns:
        Dictionary with count, mean, median, mode, stdev, min, max, quartiles
    """
    if not values:
        return {
            "count": 0,
            "mean": 0,
            "median": 0,
            "mode": None,
            "stdev": 0,
            "variance": 0,
            "min": 0,
            "max": 0,
            "range": 0,
            "q1": 0,
            "q2": 0,
            "q3": 0,
            "iqr": 0
        }
    
    quartiles = stats_quartiles(values)
    
    return {
        "count": len(values),
        "mean": stats_mean(values),
        "median": stats_median(values),
        "mode": stats_mode(values),
        "stdev": stats_stdev(values),
        "variance": stats_variance(values),
        "min": min(values),
        "max": max(values),
        "range": stats_range(values),
        "q1": quartiles["q1"],
        "q2": quartiles["q2"],
        "q3": quartiles["q3"],
        "iqr": stats_iqr(values)
    }


def register_statistics_functions(runtime: Runtime) -> None:
    """Register statistics functions with the runtime."""
    
    # Central tendency
    runtime.register_function("stats_mean", stats_mean)
    runtime.register_function("stats_median", stats_median)
    runtime.register_function("stats_mode", stats_mode)
    runtime.register_function("stats_modes", stats_modes)
    
    # Dispersion
    runtime.register_function("stats_variance", stats_variance)
    runtime.register_function("stats_stdev", stats_stdev)
    runtime.register_function("stats_range", stats_range)
    runtime.register_function("stats_iqr", stats_iqr)
    
    # Distribution
    runtime.register_function("stats_percentile", stats_percentile)
    runtime.register_function("stats_quartiles", stats_quartiles)
    runtime.register_function("stats_outliers", stats_outliers)
    runtime.register_function("stats_histogram", stats_histogram)
    
    # Correlation and regression
    runtime.register_function("stats_correlation", stats_correlation)
    runtime.register_function("stats_covariance", stats_covariance)
    runtime.register_function("stats_linear_regression", stats_linear_regression)
    
    # Data transformation
    runtime.register_function("stats_normalize", stats_normalize)
    
    # Summary
    runtime.register_function("stats_summary", stats_summary)
