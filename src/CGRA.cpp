#include "CGRA.hpp"

namespace ufm {

CGRA::CGRA(int rows, int cols, bool memoryColumn, int maxII)
    : rows_(rows), cols_(cols), maxII_(maxII), memoryColumn_(memoryColumn) {
    pes_.reserve(rows * cols);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            PE p;
            p.id = r * cols + c;
            p.row = r;
            p.col = c;
            // [PAPER-DERIVED] "4 LSUs at the first column" (Section IV).
            p.canLoad = memoryColumn_ && (c == 0);
            pes_.push_back(p);
        }
    }
    // [PAPER-DERIVED] 2D mesh; directed links in both directions per edge.
    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};
    for (const PE& p : pes_) {
        for (int d = 0; d < 4; ++d) {
            const int nr = p.row + dr[d];
            const int nc = p.col + dc[d];
            if (nr < 0 || nr >= rows_ || nc < 0 || nc >= cols_) continue;
            const PEId nid = peAt(nr, nc);
            pes_[p.id].addNeighbor(nid);
            Link l{p.id, nid};
            linkId_[l] = static_cast<int>(links_.size());
            links_.push_back(l);
        }
    }
}

bool CGRA::hasLink(PEId from, PEId to) const {
    return linkId_.count(Link{from, to}) > 0;
}

int CGRA::linkId(const Link& l) const {
    auto it = linkId_.find(l);
    return it == linkId_.end() ? -1 : it->second;
}

std::vector<PEId> CGRA::placementOrder(OpType op) const {
    std::vector<PEId> order;
    if (memoryColumn_ && isMemoryOp(op)) {
        // [PAPER-DERIVED] Fig. 4 order 0'->1'->2'->3' (left column, top-down).
        for (int r = 0; r < rows_; ++r) order.push_back(peAt(r, 0));
        return order;
    }
    // [PAPER-DERIVED] Fig. 4 serpentine order over compute PEs.
    for (int r = 0; r < rows_; ++r) {
        if (r % 2 == 0) {
            for (int c = 0; c < cols_; ++c) {
                const PEId id = peAt(r, c);
                if (memoryColumn_ && pes_[id].canLoad) continue;
                order.push_back(id);
            }
        } else {
            for (int c = cols_ - 1; c >= 0; --c) {
                const PEId id = peAt(r, c);
                if (memoryColumn_ && pes_[id].canLoad) continue;
                order.push_back(id);
            }
        }
    }
    return order;
}

} // namespace ufm