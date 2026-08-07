#pragma once
// ============================================================================
// Instruction.hpp - A node of the data flow graph (DFG).
// ============================================================================
#include "Types.hpp"
#include <string>
#include <vector>

namespace ufm {

// [PAPER-DERIVED] "instructions" of the DFG (Section III). Each instruction
// is placed on exactly one PE at one cycle; its data dependencies are its
// "source" instructions (parents in the DFG).
struct Instruction {
    InstrId id = -1;
    OpType  op = OpType::COMPUTE;
    std::string label;                 // Human-readable name for printing.
    std::vector<InstrId> sources;      // Parent instruction ids (DFG edges into this node).
};

} // namespace ufm