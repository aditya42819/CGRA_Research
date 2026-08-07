#include "Routing.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace ufm {

bool Router::findRoutes(const Instruction& instr,
                        PEId target,
                        const CGRA& cgra,
                        const ReservationTable& rt,
                        const std::vector<Cycle>& sourceCycles,
                        std::vector<Route>& out) {
    out.clear();
    for (std::size_t i = 0; i < instr.sources.size(); ++i) {
        const InstrId src = instr.sources[i];
        // [PAPER-DERIVED] "Links are shared for each cycle%II": a source's
        // data travels on its own execution cycle modulo II.
        const int c = sourceCycles[i] % rt.II();
        const std::set<PEId>& start = rt.dataReach(src);

        Route route;
        route.source   = src;
        route.modCycle = c;

        // [PAPER-DERIVED] "it can find that the output of Instruction0 has
        // already visited PE1 and it does not have to find the route."
        if (start.count(target)) { out.push_back(route); continue; }

        // Multi-source BFS from every PE the source's data already reaches.
        std::set<PEId> visited(start);
        std::map<PEId, std::pair<PEId, Link>> parent;
        std::vector<PEId> queue(visited.begin(), visited.end());
        bool found = false;

        for (std::size_t qi = 0; qi < queue.size() && !found; ++qi) {
            const PEId u = queue[qi];
            for (const PEId v : cgra.pe(u).neighbors()) {
                if (visited.count(v)) continue;
                const Link l{u, v};
                const int owner = rt.linkOwner(l, c);
                // [PAPER-DERIVED] A link is usable if free, or if it already
                // carries this same source's data (multicast, HyCUBE Fig. 2:
                // "S_pq ∩ S_pr need not be empty").
                if (owner != kNoOwner && owner != src) continue;
                visited.insert(v);
                parent[v] = {u, l};
                queue.push_back(v);
                if (v == target) { found = true; break; }
            }
        }
        if (!found) return false;   // [PAPER-DERIVED] Routing_success = false.

        // Reconstruct path back to a node already in the multicast tree.
        std::vector<PEId> path;
        std::vector<Link> newLinks;
        PEId cur = target;
        path.push_back(cur);
        while (!start.count(cur)) {
            const auto& [pu, pl] = parent.at(cur);
            if (rt.linkOwner(pl, c) == kNoOwner) newLinks.push_back(pl);
            cur = pu;
            path.push_back(cur);
        }
        std::reverse(path.begin(), path.end());
        std::reverse(newLinks.begin(), newLinks.end());
        route.path     = std::move(path);
        route.newLinks = std::move(newLinks);
        out.push_back(std::move(route));
    }
    return true;
}

} // namespace ufm