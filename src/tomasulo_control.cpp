#include "decisions.hpp"
namespace p1 {
// G4: The supplied code finds the first free RS and keeps issue in program order.
// rs_free: a reservation station exists. fu_free: required execution unit is free.
// any_inflight: another instruction remains in flight.
// Admission to a waiting structure and launch onto a FU are separate decisions.
bool tomasulo_can_issue(bool rs_free, bool fu_free, bool any_inflight) {
    (void)fu_free; (void)any_inflight;
    // TODO G4
    return rs_free && fu_free && !any_inflight;
}

// G5: Sources are bound BEFORE the destination is renamed (provided code).
// Return true to wait on the producer tag; false to capture the register value now.
bool tomasulo_wait_for_source(Tag producer_tag) {
    (void)producer_tag;
    // TODO G5
    return false;
}

// G6: Consumers ALWAYS receive the broadcast in the provided code.
// This decision is only about writing the destination register and clearing its RAT.
// current_tag: current RAT entry for the destination. result_tag: arriving producer.
bool tomasulo_write_register(Tag current_tag, Tag result_tag) {
    (void)current_tag; (void)result_tag;
    // TODO G6
    return true;
}
}
