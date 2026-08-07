#pragma once
// ============================================================================
// ReservationTable.hpp - Modulo reservation tables for PEs and links.
// ============================================================================
#include "CGRA.hpp"
#include <set>
#include <vector>

namespace ufm {

// Modulo Routing Resource Graph state (HyCUBE MRRG notion, ref [8]):
//  - PE occupancy per (PE, cycle%II) slot,
//  - link ownership per (link, cycle%II) slot,
//  - per-source "data reach": the set of PEs a source's data already visits
//    (its PE plus every PE on its reserved multicast trees).
class ReservationTable {
public:
    void reset(const CGRA& cgra, int II);
    int  II() const { return II_; }

    // --- PE slots -----------------------------------------------------------
    bool    peSlotFree(PEId pe, int slot) const;
    void    occupyPE(PEId pe, int slot, InstrId instr);
    InstrId peOccupant(PEId pe, int slot) const;   // kNoOwner if free.

    // --- Links (shared per cycle % II) --------------------------------------
    int  linkOwner(const Link& l, int slot) const; // kNoOwner if free.
    void reserveLink(const Link& l, int slot, InstrId owner);

    // --- Multicast reach ------------------------------------------------------
    const std::set<PEId>& dataReach(InstrId src) const;
    void seedReach(InstrId src, PEId pe);          // Source produces data at its PE.
    void markReach(InstrId src, PEId pe);          // Data now also visits `pe`.

private:
    const CGRA* cgra_ = nullptr;
    int II_ = 0;
    std::vector<std::vector<InstrId>> peOcc_;     // [pe][slot]
    std::vector<std::vector<InstrId>> linkOcc_;   // [linkId][slot]
    std::vector<std::set<PEId>>       reach_;     // [instrId]
};

} // namespace ufm