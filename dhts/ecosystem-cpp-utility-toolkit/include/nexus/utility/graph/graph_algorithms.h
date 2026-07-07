#pragma once

#include <vector>
#include <queue>
#include <limits>
#include <functional>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <set>
#include <stack>
#include <stdexcept>

namespace nexus::utility::graph {

template<typename T>
class GraphAlgorithms {
public:
    struct Edge {
        size_t from, to;
        double weight = 1.0;
    };

    struct Graph {
        size_t vertex_count = 0;
        bool directed = true;
        std::vector<std::vector<Edge>> adjacency;
        std::vector<T> vertex_data;
    };

    static Graph create(size_t n, bool directed = true) {
        Graph g;
        g.vertex_count = n;
        g.directed = directed;
        g.adjacency.resize(n);
        g.vertex_data.resize(n);
        return g;
    }

    static void addEdge(Graph& g, size_t from, size_t to, double weight = 1.0) {
        g.adjacency[from].push_back({from, to, weight});
        if (!g.directed) g.adjacency[to].push_back({to, from, weight});
    }

    // BFS traversal
    static std::vector<size_t> bfs(const Graph& g, size_t start) {
        std::vector<bool> visited(g.vertex_count, false);
        std::vector<size_t> order;
        std::queue<size_t> q;
        q.push(start);
        visited[start] = true;
        while (!q.empty()) {
            auto v = q.front(); q.pop();
            order.push_back(v);
            for (auto& e : g.adjacency[v]) {
                if (!visited[e.to]) {
                    visited[e.to] = true;
                    q.push(e.to);
                }
            }
        }
        return order;
    }

    // DFS traversal
    static std::vector<size_t> dfs(const Graph& g, size_t start) {
        std::vector<bool> visited(g.vertex_count, false);
        std::vector<size_t> order;
        std::stack<size_t> s;
        s.push(start);
        while (!s.empty()) {
            auto v = s.top(); s.pop();
            if (visited[v]) continue;
            visited[v] = true;
            order.push_back(v);
            for (auto& e : g.adjacency[v]) {
                if (!visited[e.to]) s.push(e.to);
            }
        }
        return order;
    }

    // Dijkstra shortest path
    static std::vector<double> dijkstra(const Graph& g, size_t start) {
        const double INF = std::numeric_limits<double>::infinity();
        std::vector<double> dist(g.vertex_count, INF);
        using Pair = std::pair<double, size_t>;
        std::priority_queue<Pair, std::vector<Pair>, std::greater<Pair>> pq;

        dist[start] = 0;
        pq.push({0, start});

        while (!pq.empty()) {
            auto [d, v] = pq.top(); pq.pop();
            if (d > dist[v]) continue;
            for (auto& e : g.adjacency[v]) {
                double nd = d + e.weight;
                if (nd < dist[e.to]) {
                    dist[e.to] = nd;
                    pq.push({nd, e.to});
                }
            }
        }
        return dist;
    }

    // Topological sort (Kahn's algorithm)
    static std::vector<size_t> topologicalSort(const Graph& g) {
        if (!g.directed) throw std::runtime_error("Topological sort requires directed graph");
        std::vector<int> indegree(g.vertex_count, 0);
        for (size_t v = 0; v < g.vertex_count; ++v)
            for (auto& e : g.adjacency[v]) indegree[e.to]++;

        std::queue<size_t> q;
        for (size_t v = 0; v < g.vertex_count; ++v)
            if (indegree[v] == 0) q.push(v);

        std::vector<size_t> order;
        while (!q.empty()) {
            auto v = q.front(); q.pop();
            order.push_back(v);
            for (auto& e : g.adjacency[v]) {
                if (--indegree[e.to] == 0) q.push(e.to);
            }
        }

        if (order.size() != g.vertex_count)
            throw std::runtime_error("Graph has a cycle");
        return order;
    }

    // Detect cycles
    static bool hasCycle(const Graph& g) {
        std::vector<int> state(g.vertex_count, 0); // 0=unvisited, 1=visiting, 2=done
        std::function<bool(size_t)> dfs_cycle = [&](size_t v) {
            state[v] = 1;
            for (auto& e : g.adjacency[v]) {
                if (state[e.to] == 1) return true;
                if (state[e.to] == 0 && dfs_cycle(e.to)) return true;
            }
            state[v] = 2;
            return false;
        };
        for (size_t v = 0; v < g.vertex_count; ++v)
            if (state[v] == 0 && dfs_cycle(v)) return true;
        return false;
    }

    // Connected components
    static std::vector<std::vector<size_t>> connectedComponents(const Graph& g) {
        std::vector<bool> visited(g.vertex_count, false);
        std::vector<std::vector<size_t>> components;
        for (size_t v = 0; v < g.vertex_count; ++v) {
            if (!visited[v]) {
                components.push_back(bfs(g, v));
                for (auto u : components.back()) visited[u] = true;
            }
        }
        return components;
    }

    // GraphViz DOT output
    static std::string toDot(const Graph& g, const std::string& name = "G") {
        std::ostringstream oss;
        oss << (g.directed ? "digraph" : "graph") << " " << name << " {\n";
        for (size_t v = 0; v < g.vertex_count; ++v) {
            oss << "  " << v << " [label=\"" << v << "\"];\n";
        }
        std::set<std::pair<size_t, size_t>> seen;
        for (size_t v = 0; v < g.vertex_count; ++v) {
            for (auto& e : g.adjacency[v]) {
                auto key = g.directed ? std::make_pair(e.from, e.to)
                         : std::make_pair(std::min(e.from, e.to), std::max(e.from, e.to));
                if (seen.count(key)) continue;
                seen.insert(key);
                oss << "  " << e.from << (g.directed ? " -> " : " -- ") << e.to;
                if (e.weight != 1.0) oss << " [label=\"" << e.weight << "\"]";
                oss << ";\n";
            }
        }
        oss << "}\n";
        return oss.str();
    }
};

} // namespace nexus::utility::graph
