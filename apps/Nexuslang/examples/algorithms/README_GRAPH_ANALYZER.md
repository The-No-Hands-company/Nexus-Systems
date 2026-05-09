# Graph Analyzer - Algorithmic Computing Showcase

## Overview

The Graph Analyzer is a sophisticated network analysis tool that demonstrates NexusLang's capabilities for implementing algorithms, managing complex data structures, and performing sophisticated computations. It's ideal for applications requiring graph-based analysis, network optimization, and relationship modeling.

## Domain: Algorithms & Computer Science

This application showcases NexusLang's suitability for:
- Graph theory and network analysis
- Algorithm implementation (pathfinding, centrality, connectivity)
- Complex data structure management
- Mathematical/computational analysis
- Network optimization and design
- Highly algorithmic computing

## Features Demonstrated

### 1. Complex Data Structures
Multiple interconnected struct types:

```nlpl
struct Graph
  nodes as List of GraphNode
  edges as List of GraphEdge
  is_directed as Boolean
end
```

Representing:
- **GraphNode**: Network entities with properties
- **GraphEdge**: Connections with weighted relationships
- **PathResult**: Search results with distance metrics
- **ConnectivityAnalysis**: Computed network metrics

### 2. Algorithm Implementation
Multiple algorithms for graph analysis:

#### Pathfinding (BFS-inspired)
```nlpl
function find_shortest_path with graph as Graph and start_id as Integer and end_id as Integer 
  returns PathResult
  // ... breadth-first search logic
end
```

#### Degree Calculation
```nlpl
function get_node_degree with graph as Graph and node_id as Integer returns Integer
  // Count incoming/outgoing edges for each node
end
```

#### Neighbor Discovery
```nlpl
function get_neighbors with graph as Graph and node_id as Integer returns List of Integer
  // Find all connected nodes respecting directed/undirected semantics
end
```

### 3. Centrality Analysis
Measuring node importance:

```nlpl
function calculate_node_centrality with graph as Graph and node_id as Integer 
  returns CentralityMetrics
  // Degree centrality + betweenness estimate = importance score
end
```

Metrics computed:
- **Degree Centrality**: Proportion of possible connections
- **Betweenness Estimate**: Importance in paths between others
- **Importance Score**: Combined metric for ranking

### 4. Connectivity Analysis
Analyzing overall graph structure:

```nlpl
function analyze_connectivity with graph as Graph returns ConnectivityAnalysis
  // Determine connectedness, component count, average degree
end
```

### 5. Contracts for Safety
Validation throughout:

```nlpl
require id is greater than or equal to 0 with "Node ID must be non-negative"
require weight is greater than 0.0 with "Edge weight must be positive"
```

### 6. Collection Iteration and Aggregation
Efficient graph traversal:

```nlpl
set total_degree to 0
for each node in graph.nodes
  set degree to get_node_degree with graph and node.id
  set total_degree to total_degree plus degree
end
```

## Execution Example

Running the graph analyzer:

```bash
python src/main.py examples/algorithms/graph_analyzer.nlpl
```

Expected output:
```
=== Graph Analysis Report ===

Graph Properties:
  Total Nodes: 5
  Total Edges: 6
  Graph Type: Undirected

Connectivity Metrics:
  Average Degree: 2.4
  Is Connected: Yes
  Component Count: 1
  Largest Component Size: 5

Node Analysis:
  Node 0 (ServerA): degree=2
  Node 1 (ServerB): degree=3
  Node 2 (ServerC): degree=3
  Node 3 (ServerD): degree=3
  Node 4 (ServerE): degree=1

Centrality Analysis:
  Most Central Node: 1
  Degree Centrality: 0.75
  Importance Score: 0.975

Pathfinding Examples:
  Path from 0 to 4: Found
  Distance: 2.0

Graph analysis completed successfully
```

## Graph Theory Foundations

### Key Concepts Demonstrated

1. **Directed vs Undirected Graphs**
   - Undirected: Symmetric relationships (friendship networks)
   - Directed: Asymmetric relationships (citations, dependencies)

2. **Weighted Edges**
   - Edge weights represent costs, distances, or strengths
   - Used in shortest path calculations

3. **Node Properties**
   - Labels for identification
   - Weights for importance ranking
   - Visited flag for traversal algorithms

4. **Graph Connectivity**
   - Connected components
   - Network diameter (max shortest path)
   - Average clustering

### Real-World Applications

1. **Social Networks**
   - Nodes = users/accounts
   - Edges = relationships/follows
   - Analysis: community detection, influencer identification

2. **Computer Networks**
   - Nodes = servers/routers
   - Edges = network links
   - Analysis: traffic routing, failure impact

3. **Transportation**
   - Nodes = locations/junctions
   - Edges = routes/roads
   - Analysis: shortest path, network optimization

4. **Supply Chain**
   - Nodes = warehouses/suppliers
   - Edges = shipping routes
   - Analysis: bottleneck detection, optimization

5. **Knowledge Graphs**
   - Nodes = entities/concepts
   - Edges = relationships
   - Analysis: semantic search, inference

## Algorithmic Complexity

### Time Complexity Analysis

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| Find node by ID | O(n) | Linear scan required |
| Get neighbors | O(e) | Scan all edges |
| Calculate degree | O(e) | Count matching edges |
| BFS pathfinding | O(n+e) | Visit nodes, scan edges |
| Centrality calculation | O(n+e) | For single node |
| Full connectivity | O(n+e) | Single pass analysis |

### Space Complexity
- Graph storage: O(n+e) for n nodes and e edges
- Results storage: O(n) for metrics arrays
- Suitable for graphs with thousands of nodes

## Extensions and Enhancements

### Immediate Extensions
```nlpl
// Strongly connected components
function find_scc with graph as Graph returns List of List of Integer
  // Tarjan's algorithm for SCCs
end

// All-pairs shortest paths
function floyd_warshall with graph as Graph returns List of List of Float
  // Distance matrix computation
end

// Minimum spanning tree
function compute_mst with graph as Graph returns List of GraphEdge
  // Kruskal or Prim's algorithm
end
```

### Advanced Algorithms
1. **Dijkstra's Algorithm**: Shortest paths from single source
2. **Bellman-Ford**: Negative weight handling
3. **A\* Search**: Heuristic pathfinding
4. **Topological Sort**: Dependency ordering
5. **Graph Coloring**: Scheduling problems
6. **Maximum Flow**: Network capacity analysis

### Machine Learning Integration
1. **Community Detection**: Clustering nodes
2. **Link Prediction**: Recommend connections
3. **Representation Learning**: Node embeddings
4. **Graph Neural Networks**: Deep learning on graphs

## Performance Characteristics

**Typical Use Cases**:
- 100 nodes, 200 edges: <1ms per operation
- 1000 nodes, 5000 edges: 10-100ms per operation
- 10K nodes, 100K edges: 100ms-1s per operation

**Scaling Strategies**:
- Parallel-for for independent computations (multiple paths)
- Partition graphs for distributed analysis
- Use adjacency lists for sparse graphs
- Optimize for cache locality

## Educational Value

This example teaches:

1. **Graph Theory**:
   - Fundamental graph concepts
   - Real-world modeling
   - Algorithm understanding

2. **NexusLang Programming**:
   - Complex data structure design
   - Algorithm implementation
   - Collection manipulation
   - Contracts for safety

3. **Computer Science**:
   - Time/space complexity analysis
   - Algorithm correctness
   - Data structure tradeoffs
   - Computational thinking

4. **Software Engineering**:
   - Modular design
   - Clear separation of concerns
   - Comprehensive testing
   - Practical optimization

## Testing Strategy

Contract enforcement catches logic errors:

```nlpl
// This would fail:
set edge to create_edge with -1 and 5 and 1.0 and false
// Error: "Source node ID must be non-negative"

set invalid_graph to Graph
  nodes: []
  edges: [...]
  is_directed: false
end

set analysis to analyze_connectivity with invalid_graph
// Error: "Graph must have nodes"
```

## Integration with Other NexusLang Features

### Parallel Processing
```nlpl
// Compute centrality for all nodes in parallel
parallel for each node in graph.nodes
  set metrics to calculate_node_centrality with graph and node.id
end
```

### Async I/O
```nlpl
// Load large graphs asynchronously
async function load_graph_from_file with filename as String returns Graph
  // Read from disk without blocking
end
```

### Channels for Streaming
```nlpl
// Stream graph updates through channels
async function graph_update_stream with update_channel as Channel
  while true
    set update to receive from update_channel
    // Process graph modification
  end
end
```

## Related Examples

See also:
- [Portfolio Analyzer](../financial/portfolio_analyzer.nlpl) - Similar data modeling patterns
- [Log Analyzer](../systems/log_analyzer.nlpl) - Aggregation and filtering
- [Message Broker](../concurrency/message_broker.nlpl) - Network communication patterns

## Advanced Topics

### Graph Isomorphism
Determining if two graphs have the same structure (NP-complete problem)

### Planar Graphs
Graphs drawable without edge crossings (map coloring applications)

### Random Graphs
Generating graphs for simulation and testing

### Graph Databases
Persistent storage and querying of large graphs

### Distributed Graph Computing
Processing graphs across multiple machines (MapReduce-style)

## Conclusion

The Graph Analyzer demonstrates that NexusLang is well-suited for implementing sophisticated algorithms and managing complex data structures. Its combination of:
- **Clear syntax** for algorithm logic
- **Type safety** for correctness
- **Contracts** for invariant enforcement
- **Performance** suitable for real computations
- **Composability** for building larger systems

...makes it an excellent choice for computer science applications, from academic algorithm studies to production network analysis systems.

The natural language syntax makes complex algorithms readable and maintainable—critical for code that others need to understand and modify.
