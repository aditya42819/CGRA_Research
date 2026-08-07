#pragma once
// ============================================================================
// CGRA.hpp - Configurable HyCUBE-like CGRA topology.
// ============================================================================
#include "Types.hpp"
#include <map>
#include <vector>

namespace ufm {

// [PAPER-DERIVED] "Each tile comprises an ALU, a configuration memory, and a
// crossbar switch" (HyCUBE, ref [8]); a PE knows its id, coordinates,
// neighbors and occupancy-relevant state (occupancy lives in the
// ReservationTable; the PE here is the static topology node).
class PE {
public:
    PEId id  = -1;
    int  row = 0;
    int  col = 0;
    bool canLoad = false;   // True for the leftmost (LSU) column.

    const std::vector<PEId>& neighbors() const { return neighbors_; }
    void addNeighbor(PEId p) { neighbors_.push_back(p); }

private:
    std::vector<PEId> neighbors_;
};

class CGRA {
public:
    // [PAPER-DERIVED] 4x4 CGRA with 4 LSUs at the first column; MaxII is the
    // number of instructions a PE configuration memory can hold (HyCUBE:
    // 256B per tile supports II <= 32).
    explicit CGRA(int rows, int cols, bool memoryColumn = true, int maxII = 32);

    int rows()   const { return rows_; }
    int cols()   const { return cols_; }
    int maxII()  const { return maxII_; }
    int numPEs() const { return static_cast<int>(pes_.size()); }

    const PE& pe(PEId id) const { return pes_.at(id); }
    PEId peAt(int row, int col) const { return row * cols_ + col; }

    bool hasLink(PEId from, PEId to) const;
    int  linkId(const Link& l) const;          // Index into links(), -1 if absent.
    const std::vector<Link>& links() const { return links_; }

    // [PAPER-DERIVED] Fig. 4: memory ops map to the left column top-to-bottom
    // (0'->1'->2'->3'); compute ops follow a serpentine order over the
    // remaining PEs (order 0..11 in Fig. 4). This fixed, pre-determined order
    // is what makes placement O(1) in the paper.
    std::vector<PEId> placementOrder(OpType op) const;

private:
    int  rows_, cols_, maxII_;
    bool memoryColumn_;
    std::vector<PE>   pes_;
    std::vector<Link> links_;
    std::map<Link, int> linkId_;
};

} // namespace ufm