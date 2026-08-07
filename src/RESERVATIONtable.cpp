#include "ReservationTable.hpp"

namespace ufm {

void ReservationTable::reset(const CGRA& cgra, int II) {
    cgra_ = &cgra;
    II_   = II;
    peOcc_.assign(cgra.numPEs(), std::vector<InstrId>(II, kNoOwner));
    linkOcc_.assign(cgra.links().size(), std::vector<InstrId>(II, kNoOwner));
    reach_.clear();
}

bool ReservationTable::peSlotFree(PEId pe, int slot) const {
    return peOcc_[pe][slot] == kNoOwner;
}

void ReservationTable::occupyPE(PEId pe, int slot, InstrId instr) {
    peOcc_[pe][slot] = instr;
}

InstrId ReservationTable::peOccupant(PEId pe, int slot) const {
    return peOcc_[pe][slot];
}

int ReservationTable::linkOwner(const Link& l, int slot) const {
    const int id = cgra_->linkId(l);
    if (id < 0) return kNoOwner;
    return linkOcc_[id][slot];
}

void ReservationTable::reserveLink(const Link& l, int slot, InstrId owner) {
    const int id = cgra_->linkId(l);
    if (id >= 0) linkOcc_[id][slot] = owner;
}

const std::set<PEId>& ReservationTable::dataReach(InstrId src) const {
    static const std::set<PEId> kEmpty;
    if (src < 0 || static_cast<std::size_t>(src) >= reach_.size()) return kEmpty;
    return reach_[src];
}

void ReservationTable::seedReach(InstrId src, PEId pe) {
    if (reach_.size() <= static_cast<std::size_t>(src)) reach_.resize(src + 1);
    reach_[src].insert(pe);
}

void ReservationTable::markReach(InstrId src, PEId pe) {
    if (reach_.size() <= static_cast<std::size_t>(src)) reach_.resize(src + 1);
    reach_[src].insert(pe);
}

} // namespace ufm