// graph.cpp
#include "graph.h"

Graph::Graph() {
    nodeCap = 8; numNodes = 0;
    nodeNames = new std::string[nodeCap];
    edgeCap = 16; numEdges = 0;
    edges = new Edge[edgeCap];
}
Graph::~Graph() {
    delete[] nodeNames;
    delete[] edges;
}

int Graph::findNode(const std::string& name) const {
    for (int i = 0; i < numNodes; i++) if (nodeNames[i] == name) return i;
    return -1;
}

int Graph::addNode(const std::string& name) {
    int idx = findNode(name);
    if (idx >= 0) return idx;
    if (numNodes == nodeCap) {
        int nc = nodeCap * 2;
        std::string* nn = new std::string[nc];
        for (int i = 0; i < numNodes; i++) nn[i] = nodeNames[i];
        delete[] nodeNames; nodeNames = nn; nodeCap = nc;
    }
    nodeNames[numNodes] = name;
    return numNodes++;
}

void Graph::addEdge(const std::string& a, const std::string& b, double weight) {
    int u = addNode(a), v = addNode(b);
    if (numEdges == edgeCap) {
        int nc = edgeCap * 2;
        Edge* ne = new Edge[nc];
        for (int i = 0; i < numEdges; i++) ne[i] = edges[i];
        delete[] edges; edges = ne; edgeCap = nc;
    }
    edges[numEdges++] = Edge(u, v, weight);
}

// simple insertion sort - N is tiny (# join edges in a query)
void Graph::edgeSort(Edge* a, int n) {
    for (int i = 1; i < n; i++) {
        Edge key = a[i];
        int j = i - 1;
        while (j >= 0 && a[j].weight > key.weight) {
            a[j+1] = a[j];
            j--;
        }
        a[j+1] = key;
    }
}

// union-find with path compression and union by rank
struct DSU {
    int* parent;
    int* rank_;
    int n;
    DSU(int n_) : n(n_) {
        parent = new int[n]; rank_ = new int[n];
        for (int i = 0; i < n; i++) { parent[i] = i; rank_[i] = 0; }
    }
    ~DSU() { delete[] parent; delete[] rank_; }
    int find(int x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];    // path compression
            x = parent[x];
        }
        return x;
    }
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;
        if (rank_[ra] < rank_[rb]) { int t = ra; ra = rb; rb = t; }
        parent[rb] = ra;
        if (rank_[ra] == rank_[rb]) rank_[ra]++;
        return true;
    }
};

int Graph::mstKruskal(Edge*& outEdges) const {
    // copy edges so we don't mutate our own
    Edge* sorted = new Edge[numEdges];
    for (int i = 0; i < numEdges; i++) sorted[i] = edges[i];
    edgeSort(sorted, numEdges);

    outEdges = new Edge[numNodes > 0 ? numNodes - 1 : 1];
    int picked = 0;
    DSU dsu(numNodes);
    for (int i = 0; i < numEdges && picked < numNodes - 1; i++) {
        if (dsu.unite(sorted[i].u, sorted[i].v)) {
            outEdges[picked++] = sorted[i];
        }
    }
    delete[] sorted;
    return picked;
}
