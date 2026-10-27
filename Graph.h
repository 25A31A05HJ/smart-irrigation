#ifndef GRAPH_H
#define GRAPH_H

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "MinHeap.h"

// Farm water pipe network: nodes = tank / junctions / zone outlets,
// edges = pipes weighted by length in metres (undirected).
class Graph {
public:
    static constexpr double INF = std::numeric_limits<double>::infinity();

    struct ShortestPaths {
        std::vector<double> dist;  // distance from source to every node
        std::vector<int> parent;   // previous node on the best path (-1 = none)
    };

    struct Pipe {
        int u;
        int v;
        double length;
    };

private:
    std::vector<std::string> names;
    std::vector<int> xs, ys;  // drawing position used by the frontend
    std::vector<std::vector<std::pair<int, double>>> adj;
    std::vector<Pipe> pipes;

public:
    int addNode(const std::string& name, int x = 0, int y = 0) {
        names.push_back(name);
        xs.push_back(x);
        ys.push_back(y);
        adj.emplace_back();
        return static_cast<int>(names.size()) - 1;
    }

    int size() const { return static_cast<int>(names.size()); }
    bool validNode(int id) const { return id >= 0 && id < size(); }
    const std::string& nodeName(int id) const { return names[id]; }
    int nodeX(int id) const { return xs[id]; }
    int nodeY(int id) const { return ys[id]; }
    const std::vector<Pipe>& allPipes() const { return pipes; }

    bool addPipe(int u, int v, double lengthM) {
        if (!validNode(u) || !validNode(v) || u == v || lengthM <= 0) return false;
        adj[u].push_back({v, lengthM});
        adj[v].push_back({u, lengthM});
        pipes.push_back({u, v, lengthM});
        return true;
    }

    // Dijkstra's algorithm using our own MinHeap.
    ShortestPaths dijkstra(int source) const {
        ShortestPaths sp;
        sp.dist.assign(size(), INF);
        sp.parent.assign(size(), -1);
        if (!validNode(source)) return sp;

        MinHeap<std::pair<double, int>> pq;  // (distance, node), smallest first
        sp.dist[source] = 0.0;
        pq.push({0.0, source});

        while (!pq.empty()) {
            std::pair<double, int> cur = pq.pop();
            double d = cur.first;
            int u = cur.second;
            if (d > sp.dist[u]) continue;  // stale entry
            for (const auto& edge : adj[u]) {
                int v = edge.first;
                double nd = d + edge.second;
                if (nd < sp.dist[v]) {
                    sp.dist[v] = nd;
                    sp.parent[v] = u;
                    pq.push({nd, v});
                }
            }
        }
        return sp;
    }

    // Rebuilds the node list source -> target. Empty if unreachable.
    std::vector<int> buildPath(const ShortestPaths& sp, int target) const {
        std::vector<int> path;
        if (!validNode(target) || sp.dist[target] == INF) return path;
        for (int v = target; v != -1; v = sp.parent[v]) path.push_back(v);
        return std::vector<int>(path.rbegin(), path.rend());
    }
};

#endif
