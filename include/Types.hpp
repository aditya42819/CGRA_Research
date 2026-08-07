#pragma once
// ============================================================================
// Types.hpp - Shared primitive types for the Ultra-Fast CGRA mapper.
// ============================================================================
#include <string>
#include <vector>

namespace ufm {

using InstrId = int;   // Index of an instruction in the DFG.
using PEId    = int;   // Index of a processing element in the CGRA.
using Cycle   = int;   // Absolute cycle within the first (unrolled) iteration.

// [PAPER-DERIVED] The paper distinguishes memory-capable PEs (left column,
// Fig. 4) from compute PEs; LOAD/STORE must map to the memory column.
enum class OpType { COMPUTE, LOAD, STORE };

inline bool isMemoryOp(OpType t) { return t == OpType::LOAD || t == OpType::STORE; }

// A directed interconnect link between two adjacent PEs.
// [PAPER-DERIVED] HyCUBE-style 2D mesh; links are the routable resources
// tracked by the modulo reservation table ("links are shared for each
// cycle % II", Algorithm 1 line 13 comment).
struct Link {
    PEId from = -1;
    PEId to   = -1;
    bool operator<(const Link& o) const {
        if (from != o.from) return from < o.from;
        return to < o.to;
    }
    bool operator==(const Link& o) const { return from == o.from && to == o.to; }
};

// Sentinel meaning "no instruction owns this resource".
constexpr int kNoOwner = -1;

} // namespace ufm