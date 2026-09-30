#include "Scheduler.hpp"
#include <algorithm>
#include <vector>

namespace ufm {

// -----------------------------------------------------------------------------
// Check the modulo-scheduling timing constraint for a dependency.
//
// For:
//
//     src(i) -> dst(i + distance)
//
// the constraint is:
//
//     Csrc + latency <= Cdst + distance * II
//
// We assume unit operation latency, so:
//
//     Csrc + 1 <= Cdst + distance * II
//
// distance = 0  -> same-iteration dependency
// distance > 0  -> loop-carried dependency
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
    // 1. Get the topological order.
    //
    // topologicalSort() ignores recurrence edges (distance > 0), which is
    // exactly what we want here. Recurrence edges will be handled after all
    // instructions have been placed.
    // -------------------------------------------------------------------------

    const std::vector<InstrId> order = dfg.topologicalSort();

    const int numInstr = static_cast<int>(dfg.size());
    const int numPE    = cgra_.numPEs();

    // Recurrence-constrained II.
    const int recMII = dfg.computeRecMII();

    // Resource-constrained II.
    const int resMII =
        (numInstr + numPE - 1) / numPE;

    const int minII =
        std::max(recMII, resMII);


    // -------------------------------------------------------------------------
    // 2. Build incoming-edge lists.
    //
    // We cannot use Instruction::sources alone because sources[] does not
    // contain the dependency distance.
    //
    // Example:
    //
    //     A -> B, distance = 0
    //     C -> B, distance = 1
    //
    // Both appear in B.sources, but only Edge tells us which one is
    // loop-carried.
    // -------------------------------------------------------------------------

    std::vector<std::vector<const Edge*>> incoming(numInstr);

    for (const Edge& e : dfg.edges()) {
        incoming[e.dst].push_back(&e);
    }


    // -------------------------------------------------------------------------
    // 3. Try every possible II.
    //
    // If placement/routing fails for one II, restart completely with the
    // next II.
    // -------------------------------------------------------------------------

    for (int II = std::max(minII, 1);
         II <= cgra_.maxII();
         ++II)
    {
        rt_.reset(cgra_, II);

        placements_.clear();
        routes_.clear();


        // Absolute cycle of each instruction in the first iteration.
        std::vector<Cycle> cycleOf(numInstr, -1);

        // PE on which each instruction is placed.
        std::vector<PEId> peOf(numInstr, -1);


        // Separate placement cursors for compute and memory operations.
        int compCursor = 0;
        int memCursor  = 0;

        bool success = true;


        // =====================================================================
        // PASS 1
        //
        // Place all instructions and route all SAME-ITERATION dependencies.
        //
        // Loop-carried dependencies are deliberately skipped here because
        // their source may appear later in the topological order.
        // =====================================================================

        for (const InstrId id : order)
        {
            const Instruction& in =
                dfg.instructions()[id];

            const std::vector<PEId> ord =
                cgra_.placementOrder(in.op);

            int& cursor =
                isMemoryOp(in.op)
                    ? memCursor
                    : compCursor;

            const int totalSlots =
                static_cast<int>(ord.size()) * II;

            bool placed = false;


            // -----------------------------------------------------------------
            // Try candidate PE/cycle positions in the predetermined diagonal
            // placement order.
            // -----------------------------------------------------------------

            while (cursor < totalSlots)
            {
                const int s = cursor++;

                const PEId pe =
                    ord[s / II];

                const Cycle cycle =
                    s;

                const int slot =
                    s % II;


                // -------------------------------------------------------------
                // PE must be free in this modulo slot.
                // -------------------------------------------------------------

                if (!rt_.peSlotFree(pe, slot))
                    continue;


                // -------------------------------------------------------------
                // Check dependencies.
                //
                // ONLY distance == 0 is checked here.
                //
                // For:
                //
                //     A(i) -> B(i)
                //
                // A must already have been scheduled and must occur before B.
                // -------------------------------------------------------------

                bool timingOK = true;

                for (const Edge* e : incoming[id])
                {
                    if (e->distance != 0)
                        continue;


                    const Cycle srcCycle =
                        cycleOf[e->src];


                    // A zero-distance source should already have been placed
                    // because the DFG was topologically ordered.
                    if (srcCycle < 0)
                    {
                        timingOK = false;
                        break;
                    }


                    if (!dependenceTimingOK(
                            srcCycle,
                            cycle,
                            0,
                            II))
                    {
                        timingOK = false;
                        break;
                    }
                }


                if (!timingOK)
                    continue;


                // -------------------------------------------------------------
                // Build an Instruction containing ONLY same-iteration sources.
                //
                // We intentionally do NOT give recurrence sources to the
                // router yet.
                // -------------------------------------------------------------

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


                // -------------------------------------------------------------
                // Route the same-iteration dependencies through the actual
                // CGRA graph.
                //
                // Router uses:
                //
                //     CGRA neighbors
                //     BFS
                //     ReservationTable
                //     multicast dataReach
                // -------------------------------------------------------------

                std::vector<Route> pending;

                if (!Router::findRoutes(
                        routeInstr,
                        pe,
                        cgra_,
                        rt_,
                        srcCycles,
                        pending))
                {
                    // No physical route for this candidate.
                    // Try the next diagonal position.
                    continue;
                }


                // -------------------------------------------------------------
                // Candidate is valid.
                //
                // Commit PE reservation.
                // -------------------------------------------------------------

                rt_.occupyPE(
                    pe,
                    slot,
                    id
                );


                // -------------------------------------------------------------
                // Commit all newly discovered routes.
                // -------------------------------------------------------------

                for (const Route& r : pending)
                {
                    for (const Link& l : r.newLinks)
                    {
                        rt_.reserveLink(
                            l,
                            r.modCycle,
                            r.source
                        );
                    }


                    // Every PE on the path becomes part of this source's
                    // multicast reach set.
                    for (const PEId n : r.path)
                    {
                        rt_.markReach(
                            r.source,
                            n
                        );
                    }


                    routes_.push_back(r);
                }


                // The instruction produces its result at its own PE.
                rt_.seedReach(id, pe);


                // Save placement.
                cycleOf[id] = cycle;
                peOf[id]    = pe;


                placements_.push_back(
                    Placement{
                        id,
                        pe,
                        cycle,
                        slot
                    }
                );


                placed = true;
                break;
            }


            // Could not place this instruction for this II.
            if (!placed)
            {
                success = false;
                break;
            }
        }


        // If normal placement failed, try the next II.
        if (!success)
            continue;


        // =====================================================================
        // PASS 2
        //
        // HANDLE LOOP-CARRIED DEPENDENCIES.
        //
        // At this point EVERY instruction has:
        //
        //     PE
        //     cycle
        //
        // Therefore recurrence edges can now be checked and routed.
        // =====================================================================

        for (const Edge& e : dfg.edges())
        {
            // Ignore normal same-iteration dependencies.
            if (e.distance <= 0)
                continue;


            const Cycle srcCycle =
                cycleOf[e.src];

            const Cycle dstCycle =
                cycleOf[e.dst];

            const PEId srcPE =
                peOf[e.src];

            const PEId dstPE =
                peOf[e.dst];


            // -------------------------------------------------------------
            // Make sure both instructions were successfully placed.
            // -------------------------------------------------------------

            if (srcCycle < 0 ||
                dstCycle < 0 ||
                srcPE < 0 ||
                dstPE < 0)
            {
                success = false;
                break;
            }


            // -------------------------------------------------------------
            // Apply the loop-carried dependency constraint:
            //
            //     Csrc + 1 <= Cdst + distance * II
            //
            // Example:
            //
            //     A(i) -> B(i+1)
            //
            // distance = 1
            //
            //     C_A + 1 <= C_B + II
            //
            // -------------------------------------------------------------

            if (!dependenceTimingOK(
                    srcCycle,
                    dstCycle,
                    e.distance,
                    II))
            {
                success = false;
                break;
            }


            // -------------------------------------------------------------
            // Create a temporary instruction representing this one
            // recurrence dependency.
            //
            // We only need:
            //
            //     destination
            //     source
            //
            // for the router.
            // -------------------------------------------------------------

            Instruction routeInstr;

            routeInstr.id =
                e.dst;

            routeInstr.op =
                dfg.instructions()[e.dst].op;

            routeInstr.sources.push_back(
                e.src
            );


            // -------------------------------------------------------------
            // The source's data travels on the source execution cycle
            // modulo II.
            //
            // Router already implements this through:
            //
            //     sourceCycles[i] % II
            //
            // -------------------------------------------------------------

            std::vector<Cycle> srcCycles{
                srcCycle
            };


            std::vector<Route> pending;


            // -------------------------------------------------------------
            // Route source PE -> destination PE using the ACTUAL CGRA
            // topology and existing BFS router.
            // -------------------------------------------------------------

            if (!Router::findRoutes(
                    routeInstr,
                    dstPE,
                    cgra_,
                    rt_,
                    srcCycles,
                    pending))
            {
                success = false;
                break;
            }


            // -------------------------------------------------------------
            // Commit recurrence routes to the reservation table.
            // -------------------------------------------------------------

            for (const Route& r : pending)
            {
                for (const Link& l : r.newLinks)
                {
                    rt_.reserveLink(
                        l,
                        r.modCycle,
                        r.source
                    );
                }


                for (const PEId n : r.path)
                {
                    rt_.markReach(
                        r.source,
                        n
                    );
                }


                routes_.push_back(r);
            }
        }


        // ---------------------------------------------------------------------
        // If both:
        //
        //     normal dependencies
        //     recurrence dependencies
        //
        // are valid, this II is accepted.
        // ---------------------------------------------------------------------

        if (success)
        {
            achievedII_ = II;
            return true;
        }
    }


    // No II produced a valid schedule.
    return false;
}

} // namespace ufm