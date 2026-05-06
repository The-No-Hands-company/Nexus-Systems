"""
Algorithm utilities (C++ STL <algorithm> equivalent).

Provides standard algorithms for searching, sorting, and transforming data.

Features:
- Sorting algorithms (sort, stable_sort, partial_sort)
- Searching algorithms (find, binary_search, lower_bound, upper_bound)
- Mutating algorithms (reverse, rotate, shuffle, unique)
- Aggregation (accumulate, reduce, min/max)
- Transformation (transform, map, filter)

Example usage in NexusLang:
    # Sorting
    set data to [3, 1, 4, 1, 5, 9, 2, 6]
    set sorted to algo_sort with data
    
    # Searching
    set index to algo_find with sorted and 5
    set found to algo_binary_search with sorted and 5
    
    # Transformation
    set doubled to algo_transform with data and double_func
    set evens to algo_filter with data and is_even_func
    
    # Aggregation
    set sum to algo_accumulate with data and 0
    set max_val to algo_max_element with data
"""

from typing import Set
from ...runtime.runtime import Runtime
import random
import bisect


def algo_sort(lst, reverse=False):
    """
    Sort list in ascending order (or descending if reverse=True).
    
    Args:
        lst: List to sort
        reverse: If True, sort in descending order
    
    Returns:
        Sorted list (new list)
    """
    if not isinstance(lst, list):
        return []
    
    return sorted(lst, reverse=bool(reverse))


def algo_stable_sort(lst, reverse=False):
    """
    Stable sort (preserves relative order of equal elements).
    
    Args:
        lst: List to sort
        reverse: If True, sort in descending order
    
    Returns:
        Sorted list (new list)
    """
    # Python's sorted() is stable by default
    return algo_sort(lst, reverse)


def algo_partial_sort(lst, n, reverse=False):
    """
    Partially sort list (only first n elements are sorted).
    
    Args:
        lst: List to sort
        n: Number of elements to sort
        reverse: If True, sort in descending order
    
    Returns:
        Partially sorted list
    """
    if not isinstance(lst, list) or not lst:
        return []
    
    n = min(int(n), len(lst))
    result = lst.copy()
    
    # Sort and take first n elements, keep rest unsorted
    sorted_part = sorted(result[:n], reverse=bool(reverse))
    
    # For partial sort, we want smallest/largest n elements sorted
    # Get all elements, sort, take n, then append unsorted remainder
    all_sorted = sorted(result, reverse=bool(reverse))
    return all_sorted[:n] + result[n:]


def algo_find(lst, value):
    """
    Find first occurrence of value in list.
    
    Args:
        lst: List to search
        value: Value to find
    
    Returns:
        Index of first occurrence, or -1 if not found
    """
    if not isinstance(lst, list):
        return -1
    
    try:
        return lst.index(value)
    except ValueError:
        return -1


def algo_find_if(lst, predicate_func):
    """
    Find first element satisfying predicate.
    
    Args:
        lst: List to search
        predicate_func: Function that returns True for matching element
    
    Returns:
        Index of first matching element, or -1
    """
    if not isinstance(lst, list):
        return -1
    
    for i, item in enumerate(lst):
        try:
            if predicate_func(item):
                return i
        except:
            continue
    
    return -1


def algo_binary_search(lst, value):
    """
    Binary search in sorted list.
    
    Args:
        lst: Sorted list
        value: Value to find
    
    Returns:
        True if found, False otherwise
    """
    if not isinstance(lst, list):
        return False
    
    index = bisect.bisect_left(lst, value)
    return index < len(lst) and lst[index] == value


def algo_lower_bound(lst, value):
    """
    Find first position where value could be inserted to maintain sorted order.
    
    Args:
        lst: Sorted list
        value: Value to search for
    
    Returns:
        Index for insertion
    """
    if not isinstance(lst, list):
        return 0
    
    return bisect.bisect_left(lst, value)


def algo_upper_bound(lst, value):
    """
    Find last position where value could be inserted to maintain sorted order.
    
    Args:
        lst: Sorted list
        value: Value to search for
    
    Returns:
        Index for insertion
    """
    if not isinstance(lst, list):
        return 0
    
    return bisect.bisect_right(lst, value)


def algo_reverse(lst):
    """
    Reverse list order.
    
    Args:
        lst: List to reverse
    
    Returns:
        Reversed list (new list)
    """
    if not isinstance(lst, list):
        return []
    
    return list(reversed(lst))


def algo_rotate(lst, n):
    """
    Rotate list elements.
    
    Args:
        lst: List to rotate
        n: Number of positions to rotate (positive = left, negative = right)
    
    Returns:
        Rotated list (new list)
    """
    if not isinstance(lst, list) or not lst:
        return []
    
    n = int(n) % len(lst)
    return lst[n:] + lst[:n]


def algo_shuffle(lst):
    """
    Randomly shuffle list elements.
    
    Args:
        lst: List to shuffle
    
    Returns:
        Shuffled list (new list)
    """
    if not isinstance(lst, list):
        return []
    
    result = lst.copy()
    random.shuffle(result)
    return result


def algo_unique(lst):
    """
    Remove consecutive duplicate elements.
    
    Args:
        lst: List to process
    
    Returns:
        List with consecutive duplicates removed
    """
    if not isinstance(lst, list) or not lst:
        return []
    
    result = [lst[0]]
    for item in lst[1:]:
        if item != result[-1]:
            result.append(item)
    
    return result


def algo_unique_all(lst):
    """
    Remove all duplicate elements (keeping first occurrence).
    
    Args:
        lst: List to process
    
    Returns:
        List with all duplicates removed
    """
    if not isinstance(lst, list):
        return []
    
    seen = set()
    result = []
    
    for item in lst:
        # Handle unhashable types
        try:
            if item not in seen:
                seen.add(item)
                result.append(item)
        except TypeError:
            # Unhashable type - check manually
            if item not in result:
                result.append(item)
    
    return result


def algo_partition(lst, predicate_func):
    """
    Partition list based on predicate.
    
    Args:
        lst: List to partition
        predicate_func: Function returning True for first partition
    
    Returns:
        Dictionary with 'true' and 'false' partitions
    """
    if not isinstance(lst, list):
        return {"true": [], "false": []}
    
    true_partition = []
    false_partition = []
    
    for item in lst:
        try:
            if predicate_func(item):
                true_partition.append(item)
            else:
                false_partition.append(item)
        except:
            false_partition.append(item)
    
    return {"true": true_partition, "false": false_partition}


def algo_accumulate(lst, init=0, func=None):
    """
    Accumulate values using binary operation.
    
    Args:
        lst: List to accumulate
        init: Initial value
        func: Binary function (default: addition)
    
    Returns:
        Accumulated result
    """
    if not isinstance(lst, list):
        return init
    
    result = init
    
    if func is None:
        # Default: addition
        for item in lst:
            result = result + item
    else:
        # Custom function
        for item in lst:
            try:
                result = func(result, item)
            except:
                continue
    
    return result


def algo_reduce(lst, func, init=None):
    """
    Reduce list to single value using binary function.
    
    Args:
        lst: List to reduce
        func: Binary reduction function
        init: Initial value (if None, use first element)
    
    Returns:
        Reduced value
    """
    if not isinstance(lst, list) or not lst:
        return init
    
    if init is None:
        result = lst[0]
        start = 1
    else:
        result = init
        start = 0
    
    for item in lst[start:]:
        try:
            result = func(result, item)
        except:
            continue
    
    return result


def algo_transform(lst, func):
    """
    Transform each element using function.
    
    Args:
        lst: List to transform
        func: Transformation function
    
    Returns:
        Transformed list
    """
    if not isinstance(lst, list):
        return []
    
    result = []
    for item in lst:
        try:
            result.append(func(item))
        except:
            result.append(item)
    
    return result


def algo_map(lst, func):
    """
    Map function over list (alias for transform).
    
    Args:
        lst: List to map
        func: Mapping function
    
    Returns:
        Mapped list
    """
    return algo_transform(lst, func)


def algo_filter(lst, predicate_func):
    """
    Filter list elements by predicate.
    
    Args:
        lst: List to filter
        predicate_func: Predicate function
    
    Returns:
        Filtered list
    """
    if not isinstance(lst, list):
        return []
    
    result = []
    for item in lst:
        try:
            if predicate_func(item):
                result.append(item)
        except:
            continue
    
    return result


def algo_count(lst, value):
    """
    Count occurrences of value.
    
    Args:
        lst: List to search
        value: Value to count
    
    Returns:
        Number of occurrences
    """
    if not isinstance(lst, list):
        return 0
    
    return lst.count(value)


def algo_count_if(lst, predicate_func):
    """
    Count elements satisfying predicate.
    
    Args:
        lst: List to search
        predicate_func: Predicate function
    
    Returns:
        Count of matching elements
    """
    if not isinstance(lst, list):
        return 0
    
    count = 0
    for item in lst:
        try:
            if predicate_func(item):
                count += 1
        except:
            continue
    
    return count


def algo_min_element(lst):
    """
    Find minimum element.
    
    Args:
        lst: List to search
    
    Returns:
        Minimum value, or None if empty
    """
    if not isinstance(lst, list) or not lst:
        return None
    
    return min(lst)


def algo_max_element(lst):
    """
    Find maximum element.
    
    Args:
        lst: List to search
    
    Returns:
        Maximum value, or None if empty
    """
    if not isinstance(lst, list) or not lst:
        return None
    
    return max(lst)


def algo_minmax_element(lst):
    """
    Find both minimum and maximum elements.
    
    Args:
        lst: List to search
    
    Returns:
        Dictionary with 'min' and 'max' keys
    """
    if not isinstance(lst, list) or not lst:
        return {"min": None, "max": None}
    
    return {"min": min(lst), "max": max(lst)}


def algo_all_of(lst, predicate_func):
    """
    Check if all elements satisfy predicate.
    
    Args:
        lst: List to check
        predicate_func: Predicate function
    
    Returns:
        True if all satisfy predicate
    """
    if not isinstance(lst, list):
        return True
    
    for item in lst:
        try:
            if not predicate_func(item):
                return False
        except:
            return False
    
    return True


def algo_any_of(lst, predicate_func):
    """
    Check if any element satisfies predicate.
    
    Args:
        lst: List to check
        predicate_func: Predicate function
    
    Returns:
        True if any satisfies predicate
    """
    if not isinstance(lst, list):
        return False
    
    for item in lst:
        try:
            if predicate_func(item):
                return True
        except:
            continue
    
    return False


def algo_none_of(lst, predicate_func):
    """
    Check if no elements satisfy predicate.
    
    Args:
        lst: List to check
        predicate_func: Predicate function
    
    Returns:
        True if none satisfy predicate
    """
    return not algo_any_of(lst, predicate_func)


def register_algorithms_functions(runtime: Runtime) -> None:
    """Register algorithm functions with the runtime."""
    
    # Sorting
    runtime.register_function("algo_sort", algo_sort)
    runtime.register_function("algo_stable_sort", algo_stable_sort)
    runtime.register_function("algo_partial_sort", algo_partial_sort)
    
    # Searching
    runtime.register_function("algo_find", algo_find)
    runtime.register_function("algo_find_if", algo_find_if)
    runtime.register_function("algo_binary_search", algo_binary_search)
    runtime.register_function("algo_lower_bound", algo_lower_bound)
    runtime.register_function("algo_upper_bound", algo_upper_bound)
    
    # Mutating
    runtime.register_function("algo_reverse", algo_reverse)
    runtime.register_function("algo_rotate", algo_rotate)
    runtime.register_function("algo_shuffle", algo_shuffle)
    runtime.register_function("algo_unique", algo_unique)
    runtime.register_function("algo_unique_all", algo_unique_all)
    runtime.register_function("algo_partition", algo_partition)
    
    # Aggregation
    runtime.register_function("algo_accumulate", algo_accumulate)
    runtime.register_function("algo_reduce", algo_reduce)
    runtime.register_function("algo_min_element", algo_min_element)
    runtime.register_function("algo_max_element", algo_max_element)
    runtime.register_function("algo_minmax_element", algo_minmax_element)
    
    # Transformation
    runtime.register_function("algo_transform", algo_transform)
    runtime.register_function("algo_map", algo_map)
    runtime.register_function("algo_filter", algo_filter)
    
    # Counting
    runtime.register_function("algo_count", algo_count)
    runtime.register_function("algo_count_if", algo_count_if)
    
    # Predicates
    runtime.register_function("algo_all_of", algo_all_of)
    runtime.register_function("algo_any_of", algo_any_of)
    runtime.register_function("algo_none_of", algo_none_of)

    # Graph
    runtime.register_function("algo_dfs", algo_dfs)
    runtime.register_function("algo_bfs", algo_bfs)
    runtime.register_function("algo_dijkstra", algo_dijkstra)
    runtime.register_function("algo_topological_sort", algo_topological_sort)
    runtime.register_function("algo_astar", algo_astar)

    # String search
    runtime.register_function("algo_kmp_search", algo_kmp_search)
    runtime.register_function("algo_rabin_karp", algo_rabin_karp)


# ---------------------------------------------------------------------------
# Graph algorithms
# ---------------------------------------------------------------------------
#
# Graphs are represented as dicts of adjacency lists:
#   {"A": ["B", "C"], "B": ["D"], ...}
# Weighted graphs use dicts of {neighbour: weight}:
#   {"A": {"B": 1, "C": 4}, "B": {"D": 2}, ...}


def algo_dfs(graph: dict, start, visited=None) -> list:
    """
    Depth-first search traversal.

    Args:
        graph: Adjacency dict  {node: [neighbour, ...]}
        start: Starting node
        visited: Set of already-visited nodes (optional, used for recursion)

    Returns:
        List of nodes in DFS visit order.
    """
    if visited is None:
        visited = set()
    result = []
    stack = [start]
    while stack:
        node = stack.pop()
        if node in visited:
            continue
        visited.add(node)
        result.append(node)
        neighbours = graph.get(node, [])
        # reverse so left-most neighbour is visited first
        if isinstance(neighbours, dict):
            neighbours = list(neighbours.keys())
        for nb in reversed(list(neighbours)):
            if nb not in visited:
                stack.append(nb)
    return result


def algo_bfs(graph: dict, start) -> list:
    """
    Breadth-first search traversal.

    Args:
        graph: Adjacency dict  {node: [neighbour, ...]}
        start: Starting node

    Returns:
        List of nodes in BFS visit order.
    """
    from collections import deque
    visited = set()
    result = []
    queue = deque([start])
    visited.add(start)
    while queue:
        node = queue.popleft()
        result.append(node)
        neighbours = graph.get(node, [])
        if isinstance(neighbours, dict):
            neighbours = list(neighbours.keys())
        for nb in neighbours:
            if nb not in visited:
                visited.add(nb)
                queue.append(nb)
    return result


def algo_dijkstra(graph: dict, start) -> dict:
    """
    Dijkstra shortest-path algorithm.

    Args:
        graph: Weighted adjacency dict  {node: {neighbour: weight, ...}}
        start: Source node

    Returns:
        Dict mapping each reachable node to its shortest distance from start.
        Unreachable nodes are absent from the result.
    """
    import heapq
    dist: dict = {start: 0}
    heap = [(0, start)]
    while heap:
        d, u = heapq.heappop(heap)
        if d > dist.get(u, float("inf")):
            continue
        neighbours = graph.get(u, {})
        if isinstance(neighbours, list):
            # Unweighted: treat each edge as weight 1
            neighbours = {v: 1 for v in neighbours}
        for v, w in neighbours.items():
            nd = d + w
            if nd < dist.get(v, float("inf")):
                dist[v] = nd
                heapq.heappush(heap, (nd, v))
    return dist


def algo_topological_sort(graph: dict) -> list:
    """
    Topological sort of a directed acyclic graph (Kahn's algorithm).

    Args:
        graph: Adjacency dict  {node: [neighbour, ...]}

    Returns:
        Topologically sorted list of nodes, or raises ValueError on cycle.
    """
    from collections import deque
    in_degree: dict = {}
    for node in graph:
        if node not in in_degree:
            in_degree[node] = 0
        neighbours = graph[node]
        if isinstance(neighbours, dict):
            neighbours = list(neighbours.keys())
        for nb in neighbours:
            in_degree[nb] = in_degree.get(nb, 0) + 1

    queue = deque(n for n, d in in_degree.items() if d == 0)
    result = []
    while queue:
        node = queue.popleft()
        result.append(node)
        neighbours = graph.get(node, [])
        if isinstance(neighbours, dict):
            neighbours = list(neighbours.keys())
        for nb in neighbours:
            in_degree[nb] -= 1
            if in_degree[nb] == 0:
                queue.append(nb)

    if len(result) != len(in_degree):
        raise ValueError("Graph contains a cycle — topological sort not possible.")
    return result


def algo_astar(graph: dict, start, goal, heuristic=None) -> list:
    """
    A* shortest-path search.

    Args:
        graph    : Weighted adjacency dict  {node: {neighbour: weight, ...}}
        start    : Start node
        goal     : Goal node
        heuristic: Function (node, goal) -> float (default: always 0, same as Dijkstra)

    Returns:
        List of nodes forming the shortest path from start to goal,
        or an empty list if no path exists.
    """
    import heapq

    if heuristic is None:
        def heuristic(n, g):
            return 0

    g_score: dict = {start: 0}
    came_from: dict = {}
    open_set = [(heuristic(start, goal), 0, start)]

    while open_set:
        _, g, current = heapq.heappop(open_set)
        if current == goal:
            path = []
            while current in came_from:
                path.append(current)
                current = came_from[current]
            path.append(start)
            path.reverse()
            return path
        if g > g_score.get(current, float("inf")):
            continue
        neighbours = graph.get(current, {})
        if isinstance(neighbours, list):
            neighbours = {nb: 1 for nb in neighbours}
        for nb, w in neighbours.items():
            tentative = g + w
            if tentative < g_score.get(nb, float("inf")):
                g_score[nb] = tentative
                came_from[nb] = current
                f = tentative + heuristic(nb, goal)
                heapq.heappush(open_set, (f, tentative, nb))

    return []  # No path found


# ---------------------------------------------------------------------------
# String search algorithms
# ---------------------------------------------------------------------------


def algo_kmp_search(text: str, pattern: str) -> list:
    """
    Knuth-Morris-Pratt substring search.

    Args:
        text    : The string to search in.
        pattern : The pattern to search for.

    Returns:
        List of starting indices where pattern occurs in text.
    """
    if not pattern:
        return list(range(len(text) + 1))
    if not text:
        return []

    # Build failure function
    m = len(pattern)
    fail = [0] * m
    j = 0
    for i in range(1, m):
        while j > 0 and pattern[i] != pattern[j]:
            j = fail[j - 1]
        if pattern[i] == pattern[j]:
            j += 1
        fail[i] = j

    # Search
    j = 0
    results = []
    for i, c in enumerate(text):
        while j > 0 and c != pattern[j]:
            j = fail[j - 1]
        if c == pattern[j]:
            j += 1
        if j == m:
            results.append(i - m + 1)
            j = fail[j - 1]
    return results


def algo_rabin_karp(text: str, pattern: str, base: int = 31, mod: int = 10**9 + 7) -> list:
    """
    Rabin-Karp substring search using polynomial rolling hash.

    Args:
        text   : The string to search in.
        pattern: The pattern to search for.
        base   : Hash base (default: 31).
        mod    : Hash modulus (default: 10^9+7).

    Returns:
        List of starting indices where pattern occurs in text.
    """
    n, m = len(text), len(pattern)
    if m == 0:
        return list(range(n + 1))
    if m > n:
        return []

    # Precompute base^(m-1) mod P
    bm = pow(base, m - 1, mod)

    def _h(s: str) -> int:
        h = 0
        for ch in s:
            h = (h * base + ord(ch)) % mod
        return h

    pat_hash = _h(pattern)
    win_hash = _h(text[:m])
    results = []

    if win_hash == pat_hash and text[:m] == pattern:
        results.append(0)

    for i in range(1, n - m + 1):
        win_hash = (win_hash - ord(text[i - 1]) * bm) % mod
        win_hash = (win_hash * base + ord(text[i + m - 1])) % mod
        if win_hash == pat_hash and text[i:i + m] == pattern:
            results.append(i)

    return results
