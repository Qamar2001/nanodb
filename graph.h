// graph.h - weighted undirected graph for multi-table join optimization.
// Nodes = tables (by name). Edges = join cost (e.g., estimated cardinality).
// MST via Kruskal with custom union-find.
#ifndef GRAPH_H
#define GRAPH_H

#include <string>

struct Edge {
    int u, v;
    double weight;
    Edge() : u(0), v(0), weight(0) {}
    Edge(int a, int b, double w) : u(a), v(b), weight(w) {}
};

class Graph {
    std::string* nodeNames;
    int numNodes;
    int nodeCap;

    Edge* edges;
    int numEdges;
    int edgeCap;

    // for deterministic sort output
    static void edgeSort(Edge* a, int n);   // simple insertion-sort (N^2) - fine for small N

public:
    Graph();
    ~Graph();
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    // returns node index (creates if new)
    int addNode(const std::string& name);
    int findNode(const std::string& name) const;
    const std::string& nodeName(int idx) const { return nodeNames[idx]; }
    int nodeCount() const { return numNodes; }

    void addEdge(const std::string& a, const std::string& b, double weight);

    // runs Kruskal's MST. writes chosen edges into outEdges (newly allocated).
    // returns number of edges (numNodes - 1 if connected).
    // caller must delete[] outEdges.
    int mstKruskal(Edge*& outEdges) const;
};

#endif
