#include "CGRA.hpp"
#include "Graph.hpp"
#include "Scheduler.hpp"
#include <iostream>
#include <string>

using namespace ufm;

// Fig. 3a DFG of the paper.
// rootLoad = true  -> node 0 is a LOAD   (your current T1 run)
// rootLoad = false -> node 0 is COMPUTE  (the paper's exact Fig. 3)
static DFG makeFig3(bool rootLoad) {
    DFG dfg;
    const InstrId n0 = dfg.addInstruction(rootLoad ? OpType::LOAD : OpType::COMPUTE,
                                          rootLoad ? "ld0" : "i0");
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
    return dfg;
}

// A -> B -> C -> A with a distance-1 back-edge. RecMII = ceil(3/1) = 3.
static DFG makeRecurrence() {
    DFG dfg;
    const InstrId a = dfg.addInstruction(OpType::COMPUTE, "A");
    const InstrId b = dfg.addInstruction(OpType::COMPUTE, "B");
    const InstrId c = dfg.addInstruction(OpType::COMPUTE, "C");
    dfg.addEdge(a, b);
    dfg.addEdge(b, c);
    dfg.addEdge(c, a, 1);   // loop-carried: result needed next iteration
    return dfg;
}

static void run(const std::string& name, const CGRA& cgra, const DFG& dfg, int expectII) {
    std::cout << "\n==== " << name << "  (expect II = " << expectII << ") ====\n";
    Scheduler scheduler(cgra);
    if (!scheduler.schedule(dfg)) {
        
        std::cout << "Scheduling failed for all II <= MaxII.\n";
        return;
    }

    const int gotII = scheduler.achievedII();
    std::cout << "Achieved II = " << gotII
              << (gotII == expectII ? "  [PASS]" : "  [FAIL: expected " + std::to_string(expectII) + "]") << "\n";

    std::cout << "Placements:\n";
    for (const auto& p : scheduler.placements())
        std::cout << "  id " << p.instr << " -> PE" << p.pe
                  << "  cycle " << p.cycle << "  slot " << p.slot << "\n";

    std::cout << "Routes:\n";
    for (const auto& r : scheduler.routes()) {
        std::cout << "  src " << r.source << " @c%" << gotII << "=" << r.modCycle << " :";
        for (const PEId n : r.path) std::cout << " PE" << n;
        std::cout << "  (new links: " << r.newLinks.size() << ")\n";
    }
    // >>> PASTE your existing printing block here (the grid + routes code
    // >>> from your current main.cpp, unchanged). Then eyeball the "@c%II"
    // >>> tag: it must print the expected II.
}

int main() {
    // T1 regression: your current run. Expect II = 1.
    { CGRA cgra(4, 4, true, 32);  run("T1 4x4 memcol regression", cgra, makeFig3(true), 1); }

    // T2 paper replication: 2x2, no memory column, root is COMPUTE. Expect II = 2.
    { CGRA cgra(2, 2, false, 32); run("T2 2x2 paper Fig.3",       cgra, makeFig3(false), 2); }

    // T3 recurrence: RecMII dominates. Expect II = 3.
    { CGRA cgra(4, 4, true, 32);  run("T3 recurrence RecMII=3",   cgra, makeRecurrence(), 3); }

    return 0;
}