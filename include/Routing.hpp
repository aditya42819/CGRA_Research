#pragma once
// ============================================================================
// Routing.hpp - BFS route finding (findRoutes of Algorithm 1, line 13).
// ============================================================================
#include "CGRA.hpp"
#include "Instruction.hpp"
#include "ReservationTable.hpp"
#include <vector>

namespace ufm {

// One source->target route discovered by BFS.
struct Route {
    InstrId source = -1;
    int     modCycle = 0;              // cycle(source) % II; links reserved here.
    std::vector<PEId> path;            // PEs from an already-reached node to target.
    std::vector<Link> newLinks;        // Previously-free links to reserve now.
};

class Router {
public:
    // [PAPER-DERIVED] "We use Breadth First Search (BFS) at the selected PE
    // until it reaches all PEs that source instructions are placed at or
    // visited by them in a path to another instruction" (Section III.B).
    // Returns true iff every source of `instr` can reach `target`.
    static bool findRoutes(const Instruction& instr,
                           PEId target,
                           const CGRA& cgra,
                           const ReservationTable& rt,
                           const std::vector<Cycle>& sourceCycles,
                           std::vector<Route>& out);
};

} // namespace ufm2