#pragma once

#include <vector>
#include <queue>
#include <limits>
#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <cstdint>

namespace nexus::utility::graph {

/// Minimum spanning tree algorithms (Prim, Kruskal)
class MinimumSpanningTree {
public:
    struct Edge {
        size_t from, to;
        double weight;
        bool operator<(const Edge& o) const { return weight < o.weight; }
    };

    /// Prim's algorithm - O(E log V)
    static std::vector<Edge> prim(const std::vector<std::vector<Edge>>& adj) {
        size_t n = adj.size();
        std::vector<bool> in_tree(n, false);
        std::vector<Edge> result;
        using P = std::pair<double, size_t>;
        std::priority_queue<std::pair<double, std::pair<size_t, size_t>>,
                           std::vector<std::pair<double, std::pair<size_t, size_t>>>,
                           std::greater<>> pq;

        in_tree[0] = true;
        for (auto& e : adj[0]) pq.push({e.weight, {e.from, e.to}});

        while (!pq.empty() && result.size() < n - 1) {
            auto [w, endpoints] = pq.top(); pq.pop();
            auto [from, to] = endpoints;
            if (in_tree[to]) continue;
            in_tree[to] = true;
            result.push_back({from, to, w});
            for (auto& e : adj[to]) {
                if (!in_tree[e.to]) pq.push({e.weight, {e.from, e.to}});
            }
        }
        return result;
    }

    /// Kruskal's algorithm using Union-Find
    static std::vector<Edge> kruskal(size_t n, std::vector<Edge> edges) {
        std::sort(edges.begin(), edges.end());
        std::vector<size_t> parent(n);
        std::iota(parent.begin(), parent.end(), 0);

        std::function<size_t(size_t)> find = [&](size_t x) -> size_t {
            if (parent[x] != x) parent[x] = find(parent[x]);
            return parent[x];
        };
        auto unite = [&](size_t a, size_t b) {
            a = find(a); b = find(b);
            if (a != b) parent[b] = a;
        };

        std::vector<Edge> result;
        for (auto& e : edges) {
            if (find(e.from) != find(e.to)) {
                unite(e.from, e.to);
                result.push_back(e);
                if (result.size() == n - 1) break;
            }
        }
        return result;
    }

    /// Compute total weight
    static double totalWeight(const std::vector<Edge>& mst) {
        double total = 0;
        for (auto& e : mst) total += e.weight;
        return total;
    }
};

/// Max flow algorithms (Ford-Fulkerson, Edmonds-Karp)
class FlowNetwork {
public:
    struct Edge {
        size_t to;
        int capacity;
        int flow = 0;
        size_t reverse_index;
    };

    static std::vector<std::vector<Edge>> create(size_t n) {
        return std::vector<std::vector<Edge>>(n);
    }

    static void addEdge(std::vector<std::vector<Edge>>& graph, size_t from, size_t to, int cap) {
        graph[from].push_back({to, cap, 0, graph[to].size()});
        graph[to].push_back({from, 0, 0, graph[from].size() - 1});
    }

    /// Edmonds-Karp: O(V * E^2)
    static int maxFlow(std::vector<std::vector<Edge>>& graph, size_t source, size_t sink) {
        int flow = 0;
        size_t n = graph.size();
        while (true) {
            std::vector<size_t> parent(n, SIZE_MAX);
            std::vector<size_t> parent_edge(n, 0);
            std::queue<size_t> q;
            q.push(source);

            while (!q.empty() && parent[sink] == SIZE_MAX) {
                auto v = q.front(); q.pop();
                for (size_t i = 0; i < graph[v].size(); ++i) {
                    auto& e = graph[v][i];
                    if (parent[e.to] == SIZE_MAX && e.capacity > e.flow) {
                        parent[e.to] = v;
                        parent_edge[e.to] = i;
                        q.push(e.to);
                    }
                }
            }

            if (parent[sink] == SIZE_MAX) break;

            int push = std::numeric_limits<int>::max();
            for (size_t v = sink; v != source; v = parent[v]) {
                auto& e = graph[parent[v]][parent_edge[v]];
                push = std::min(push, e.capacity - e.flow);
            }

            for (size_t v = sink; v != source; v = parent[v]) {
                auto& e = graph[parent[v]][parent_edge[v]];
                e.flow += push;
                graph[v][e.reverse_index].flow -= push;
            }
            flow += push;
        }
        return flow;
    }
};

} // namespace nexus::utility::graph
