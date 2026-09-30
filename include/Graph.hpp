#pragma once
// ============================================================================
// Graph.hpp - Data Flow Graph container, topological ordering, RecMII.
// ============================================================================
#include "Instruction.hpp"
#include <vector>

namespace ufm {

// A data-dependence edge src -> dst.
// `distance` is the loop-iteration difference (0 = same iteration,
// >= 1 = loop-carried / recurrence edge). [REASONABLE INFERENCE] The paper
// mentions RecurrentEdges() implying recurrence edges exist in the model.
struct Edge {
    InstrId src = -1;
    InstrId dst = -1;
    int distance = 0;
};

class DFG {
public:
    // Appends an instruction; returns its id (== index).
    InstrId addInstruction(OpType op, const std::string& label);

    // Adds dependency src -> dst and records src in dst's source list.
    void addEdge(InstrId src, InstrId dst, int distance = 0);

    const std::vector<Instruction>& instructions() const { return instrs_; }
    const std::vector<Edge>&        edges()        const { return edges_; }
    std::size_t size() const { return instrs_.size(); }

    // Kahn's algorithm. [PAPER-DERIVED] "first order instructions in
    // topological order" (Section III.A). [REASONABLE INFERENCE] recurrence
    // edges (distance > 0) are excluded from the sort (they are back-edges in
    // modulo scheduling); ties broken by smallest id for determinism.
    std::vector<InstrId> topologicalSort() const;

    // [PAPER-DERIVED] Algorithm 1 line 1: RecMII = RecurrentEdges(Edges).
    // [REASONABLE INFERENCE] Implemented with the standard definition
    // (Rau, MICRO'94, ref [12] of the paper): RecMII = max over recurrence
    // cycles of ceil(total latency / total iteration distance), with unit
    // operation latency.
    int computeRecMII() const;
private:
    std::vector<Instruction> instrs_;
    std::vector<Edge>        edges_;
};

} // namespace ufm
