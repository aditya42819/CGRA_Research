#include "Scheduler.hpp"
#include <algorithm>

namespace ufm {

bool Scheduler::schedule(const DFG& dfg) {
    // [PAPER-DERIVED] Algorithm 1 lines 1-3.
    const std::vector<InstrId> order = dfg.topologicalSort();
    const int numInstr = static_cast<int>(dfg.size());
    const int numPE    = cgra_.numPEs();
    const int recMII   = dfg.computeRecMII();
    const int resMII   = (numInstr + numPE - 1) / numPE;  // ceil division.
    const int minII    = std::max(recMII, resMII);

    for (int II = std::max(minII, 1); II <= cgra_.maxII(); ++II) {   // line 4
        rt_.reset(cgra_, II);
        placements_.clear();
        routes_.clear();

        std::vector<Cycle> cycleOf(numInstr, -1);
        int compCursor = 0;   // Next free diagonal slot for compute PEs.
        int memCursor  = 0;   // Next free diagonal slot for memory PEs.
        bool success = true;

        for (const InstrId id : order) {                             // lines 6-7
            const Instruction& in = dfg.instructions()[id];
            const std::vector<PEId> ord = cgra_.placementOrder(in.op);
            int& cursor = isMemoryOp(in.op) ? memCursor : compCursor;
            const int totalSlots = static_cast<int>(ord.size()) * II;
            bool placed = false;

            while (cursor < totalSlots) {                            // line 8 (getPE)
                const int s  = cursor++;
                const PEId pe    = ord[s / II];   // "When a PE is full, the next
                const Cycle cycle = s;            //  PE is returned" (diagonal).
                const int slot   = s % II;

                // [REASONABLE INFERENCE] Timing guard: a child must execute
                // strictly after its sources in absolute cycle so that
                // zero-distance dependencies never read a previous iteration.
                // (The paper relies on topological + diagonal ordering for
                // this; with separate mem/compute slot spaces we enforce it.)
                bool timingOk = true;
                for (const InstrId p : in.sources)
                    if (cycleOf[p] >= cycle) { timingOk = false; break; }
                if (!timingOk) continue;

                std::vector<Cycle> srcCycles;
                srcCycles.reserve(in.sources.size());
                for (const InstrId p : in.sources) srcCycles.push_back(cycleOf[p]);

                std::vector<Route> pending;                          // line 13
                if (!Router::findRoutes(in, pe, cgra_, rt_, srcCycles, pending))
                    continue;                                        // lines 18-19

                // Commit: setPE / setRoutes (lines 15-16).
                rt_.occupyPE(pe, slot, id);
                for (const Route& r : pending) {
                    for (const Link& l : r.newLinks)
                        rt_.reserveLink(l, r.modCycle, r.source);
                    for (const PEId n : r.path)
                        rt_.markReach(r.source, n);  // "PEs on the paths are
                    routes_.push_back(r);            //  marked as visited"
                }
                rt_.seedReach(id, pe);
                cycleOf[id] = cycle;
                placements_.push_back(Placement{id, pe, cycle, slot});
                placed = true;
                break;                                               // line 17 pop()
            }

            if (!placed) { success = false; break; }                 // lines 9-11
        }

        if (success) { achievedII_ = II; return true; }              // lines 23-25
    }
    return false;
}

} // namespace ufm