#include "Scheduler.hpp"
#include <algorithm>
#include <vector>

namespace ufm {

// -----------------------------------------------------------------------------
// Check the modulo-scheduling timing constraint for a dependency.
//
// For: src(i) -> dst(i + distance)
// Constraint: Csrc + latency <= Cdst + distance * II
// We assume unit operation latency: Csrc + 1 <= Cdst + distance * II
// -----------------------------------------------------------------------------
static bool dependenceTimingOK(Cycle srcCycle,
                               Cycle dstCycle,
                               int distance,
                               int II)
{
    return static_cast<long long>(srcCycle) + 1
        <= static_cast<long long>(dstCycle)
         + static_cast<long long>(distance) * II;
}

bool Scheduler::schedule(const DFG& dfg)
{
    // -------------------------------------------------------------------------
    // 1. Get the topological order and compute MinII.
    // -------------------------------------------------------------------------
    const std::vector<InstrId> order = dfg.topologicalSort();
    const int numInstr = static_cast<int>(dfg.size());
    const int numPE    = cgra_.numPEs();
    const int recMII   = dfg.computeRecMII();
    const int resMII   = (numInstr + numPE - 1) / numPE;
    const int minII    = std::max(recMII, resMII);

    // -------------------------------------------------------------------------
    // 2. Build incoming-edge lists (we need distance info).
    // -------------------------------------------------------------------------
    std::vector<std::vector<const Edge*>> incoming(numInstr);
    for (const Edge& e : dfg.edges()) {
        incoming[e.dst].push_back(&e);
    }

    // -------------------------------------------------------------------------
    // 3. Try every possible II.
    // -------------------------------------------------------------------------
    for (int II = std::max(minII, 1); II <= cgra_.maxII(); ++II)
    {
        rt_.reset(cgra_, II);
        placements_.clear();
        routes_.clear();

        std::vector<Cycle> cycleOf(numInstr, -1);
        std::vector<PEId> peOf(numInstr, -1);

        int compCursor = 0;
        int memCursor  = 0;
        bool success = true;

        // =====================================================================
        // PASS 1: Place all instructions and route SAME-ITERATION dependencies.
        // Loop-carried dependencies (distance > 0) are skipped here.
        // =====================================================================
        for (const InstrId id : order)
        {
            const Instruction& in = dfg.instructions()[id];
            const std::vector<PEId> ord = cgra_.placementOrder(in.op);
            int& cursor = isMemoryOp(in.op) ? memCursor : compCursor;
            const int totalSlots = static_cast<int>(ord.size()) * II;
            bool placed = false;

            while (cursor < totalSlots)
            {
                const int s = cursor++;
                const PEId pe = ord[s / II];
                const Cycle cycle = s;
                const int slot = s % II;

                // PE must be free in this modulo slot.
                if (!rt_.peSlotFree(pe, slot))
                    continue;

                // Check ONLY distance == 0 dependencies.
                bool timingOK = true;
                for (const Edge* e : incoming[id])
                {
                    if (e->distance != 0)
                        continue;

                    const Cycle srcCycle = cycleOf[e->src];
                    if (srcCycle < 0) {
                        timingOK = false;
                        break;
                    }
                    if (!dependenceTimingOK(srcCycle, cycle, 0, II)) {
                        timingOK = false;
                        break;
                    }
                }
                if (!timingOK)
                    continue;

                // Build instruction with ONLY same-iteration sources.
                Instruction routeInstr = in;
                routeInstr.sources.clear();
                std::vector<Cycle> srcCycles;

                for (const Edge* e : incoming[id])
                {
                    if (e->distance != 0)
                        continue;
                    routeInstr.sources.push_back(e->src);
                    srcCycles.push_back(cycleOf[e->src]);
                }

                // Route same-iteration dependencies.
                std::vector<Route> pending;
                if (!Router::findRoutes(routeInstr, pe, cgra_, rt_, srcCycles, pending))
                    continue;

                // Commit PE reservation and routes.
                rt_.occupyPE(pe, slot, id);

                for (const Route& r : pending)
                {
                    for (const Link& l : r.newLinks)
                        rt_.reserveLink(l, r.modCycle, r.source);
                    for (const PEId n : r.path)
                        rt_.markReach(r.source, n);
                    routes_.push_back(r);
                }

                rt_.seedReach(id, pe);
                cycleOf[id] = cycle;
                peOf[id] = pe;
                placements_.push_back(Placement{id, pe, cycle, slot});
                placed = true;
                break;
            }

            if (!placed) {
                success = false;
                break;
            }
        }

        if (!success)
            continue;

        // =====================================================================
        // PASS 2: Handle LOOP-CARRIED dependencies (distance > 0).
        // Now that every instruction has a PE and Cycle, we check and route them.
        // =====================================================================
        for (const Edge& e : dfg.edges())
        {
            if (e.distance <= 0)
                continue;

            const Cycle srcCycle = cycleOf[e.src];
            const Cycle dstCycle = cycleOf[e.dst];
            const PEId srcPE = peOf[e.src];
            const PEId dstPE = peOf[e.dst];

            if (srcCycle < 0 || dstCycle < 0 || srcPE < 0 || dstPE < 0) {
                success = false;
                break;
            }

            // Check loop-carried timing constraint.
            if (!dependenceTimingOK(srcCycle, dstCycle, e.distance, II)) {
                success = false;
                break;
            }

            // Create temporary instruction for routing.
            Instruction routeInstr;
            routeInstr.id = e.dst;
            routeInstr.op = dfg.instructions()[e.dst].op;
            routeInstr.sources.push_back(e.src);
            std::vector<Cycle> srcCycles{srcCycle};

            std::vector<Route> pending;
            if (!Router::findRoutes(routeInstr, dstPE, cgra_, rt_, srcCycles, pending)) {
                success = false;
                break;
            }

            // Commit recurrence routes.
            for (const Route& r : pending)
            {
                for (const Link& l : r.newLinks)
                    rt_.reserveLink(l, r.modCycle, r.source);
                for (const PEId n : r.path)
                    rt_.markReach(r.source, n);
                routes_.push_back(r);
            }
        }

        if (success) {
            achievedII_ = II;
            return true;
        }
    }

    return false;
}

} // namespace ufm