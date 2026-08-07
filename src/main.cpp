// ============================================================================
// main.cpp - Toy demonstration: map the paper's Fig. 3a DFG (with a LOAD
// root) onto a 4x4 HyCUBE-like CGRA and print the resulting schedule.
// ============================================================================
#include "CGRA.hpp"
#include "Graph.hpp"
#include "Scheduler.hpp"
#include <iomanip>
#include <iostream>
#include <map>

using namespace ufm;

int main() {
    // [PAPER-DERIVED] 4x4 CGRA, 4 LSUs in the first column, II <= 32 (HyCUBE).
    CGRA cgra(4, 4, /*memoryColumn=*/true, /*maxII=*/32);

    // Toy DFG: Fig. 3a of the paper, node 0 turned into a LOAD.
    DFG dfg;
    const InstrId n0 = dfg.addInstruction(OpType::LOAD,    "ld0");
    const InstrId n1 = dfg.addInstruction(OpType::COMPUTE, "i1");
    const InstrId n2 = dfg.addInstruction(OpType::COMPUTE, "i2");
    const InstrId n3 = dfg.addInstruction(OpType::COMPUTE, "i3");
    const InstrId n4 = dfg.addInstruction(OpType::COMPUTE, "i4");
    const InstrId n5 = dfg.addInstruction(OpType::COMPUTE, "i5");
    const InstrId n6 = dfg.addInstruction(OpType::COMPUTE, "i6");
    dfg.addEdge(n0, n1);
    dfg.addEdge(n0, n2);
    dfg.addEdge(n0, n3);
    dfg.addEdge(n1, n4);
    dfg.addEdge(n2, n5);
    dfg.addEdge(n3, n5);
    dfg.addEdge(n4, n6);
    dfg.addEdge(n5, n6);

    Scheduler scheduler(cgra);
    if (!scheduler.schedule(dfg)) {
        std::cout << "Scheduling failed for all II <= MaxII.\n";
        return 1;
    }

    std::cout << "Scheduled successfully with II = " << scheduler.achievedII()
              << "\n\nFirst-iteration schedule (rows = cycle, cols = PE):\n";

    // Build (cycle, pe) -> label grid.
    Cycle maxCycle = 0;
    std::map<std::pair<Cycle, PEId>, std::string> grid;
    for (const Placement& p : scheduler.placements()) {
        grid[{p.cycle, p.pe}] = dfg.instructions()[p.instr].label;
        maxCycle = std::max(maxCycle, p.cycle);
    }

    std::cout << "cycle |";
    for (int pe = 0; pe < cgra.numPEs(); ++pe)
        std::cout << std::setw(5) << ("PE" + std::to_string(pe));
    std::cout << "\n------+";
    for (int pe = 0; pe < cgra.numPEs(); ++pe) std::cout << "-----";
    std::cout << "\n";
    for (Cycle c = 0; c <= maxCycle; ++c) {
        std::cout << std::setw(5) << c << " |";
        for (int pe = 0; pe < cgra.numPEs(); ++pe) {
            auto it = grid.find({c, pe});
            std::cout << std::setw(5) << (it == grid.end() ? "." : it->second);
        }
        std::cout << "\n";
    }

    std::cout << "\nRoutes reserved (source -> target path, mod cycle):\n";
    for (const Route& r : scheduler.routes()) {
        std::cout << "  src " << r.source << " @c%" << scheduler.achievedII()
                  << "=" << r.modCycle << " :";
        for (const PEId n : r.path) std::cout << " PE" << n;
        std::cout << "  (new links: " << r.newLinks.size() << ")\n";
    }
    return 0;
}