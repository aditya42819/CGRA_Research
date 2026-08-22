#include "Graph.hpp"
#include <algorithm>
#include <functional>
#include <queue>
#include <stdexcept>

namespace ufm {

InstrId DFG::addInstruction(OpType op, const std::string& label) {
    Instruction in;
    in.id = static_cast<InstrId>(instrs_.size());
    in.op = op;
    in.label = label;
    instrs_.push_back(std::move(in));
    return in.id;
}

void DFG::addEdge(InstrId src, InstrId dst, int distance) {
    edges_.push_back(Edge{src, dst, distance});
    instrs_.at(dst).sources.push_back(src);
}

std::vector<InstrId> DFG::topologicalSort() const {
    // Recurrence edges (distance > 0) are back-edges and must not
    // constrain the topological order.
    const int n = static_cast<int>(instrs_.size());
    std::vector<int> indeg(n, 0);
    std::vector<std::vector<InstrId>> adj(n);
    
    for (const Edge& e : edges_) {
        if (e.distance == 0) {
            adj[e.src].push_back(e.dst);
            ++indeg[e.dst];
        }
    }
    
    // Deterministic Kahn: smallest free id first.
    std::priority_queue<InstrId, std::vector<InstrId>, std::greater<InstrId>> ready;
    for (int i = 0; i < n; ++i)
        if (indeg[i] == 0) ready.push(i);

    std::vector<InstrId> order;
    order.reserve(n);
    
    while (!ready.empty()) {
        const int u = ready.top();
        ready.pop();
        order.push_back(u);
        
        for (const InstrId v : adj[u]) {
            if (--indeg[v] == 0) {
                ready.push(v);
            }
        }
    }
    
    if (order.size() != static_cast<std::size_t>(n)) {
        throw std::invalid_argument("DFG contains a zero-distance cycle");
    }
    
    return order;
}

int DFG::computeRecMII() const {
    // RecMII = max over recurrence cycles of ceil(cycle latency / cycle distance)
    // with unit operation latency.
    const int n = static_cast<int>(instrs_.size());
    std::vector<std::vector<const Edge*>> adj(n);
    
    for (const Edge& e : edges_) {
        adj[e.src].push_back(&e);
    }

    int recMII = 0;
    std::vector<int> pos(n, -1);   // Position of node in current DFS path.
    std::vector<int> path;         // Current DFS path (nodes).
    std::vector<int> edgeInto;     // edgeInto[i] = distance of edge into path[i].

    std::function<void(int)> dfs = [&](int u) {
        pos[u] = static_cast<int>(path.size());
        path.push_back(u);
        
        for (const Edge* e : adj[u]) {
            const int v = e->dst;
            
            if (pos[v] >= 0) {
                // Back-edge: cycle = path[pos[v] .. back] + (u -> v).
                const int latency = static_cast<int>(path.size()) - pos[v];
                int distance = e->distance;
                
                for (std::size_t i = pos[v] + 1; i < path.size(); ++i) {
                    distance += edgeInto[i];
                }
                
                if (distance > 0) {
                    recMII = std::max(recMII, (latency + distance - 1) / distance);
                }
                // distance == 0 cycle would be a combinational loop (invalid).
            } else {
                edgeInto.push_back(e->distance);
                dfs(v);
                edgeInto.pop_back();
            }
        }
        
        path.pop_back();
        pos[u] = -1;
    };

    for (int r = 0; r < n; ++r) {
        if (pos[r] < 0) {
            dfs(r);
        }
    }
    
    return recMII;
}

} // namespace ufm