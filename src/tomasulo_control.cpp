#include "decisions.hpp"
namespace p1 {
// G4: The supplied code finds the first free RS and keeps issue in program order.
// rs_free: a reservation station exists. fu_free: required execution unit is free.
// any_inflight: another instruction remains in flight.
// Admission to a waiting structure and launch onto a FU are separate decisions.
bool tomasulo_can_issue(bool rs_free, bool fu_free, bool any_inflight) {
    (void)fu_free; (void)any_inflight;
    // G4: issue only needs a free reservation station. The RS is the waiting
    // structure, so a busy FU is checked later, at launch. WAW is removed by
    // register renaming (RAT), so other in-flight work never blocks admission.
    return rs_free;
}

// G5: Sources are bound BEFORE the destination is renamed (provided code).
// Return true to wait on the producer tag; false to capture the register value now.
bool tomasulo_wait_for_source(Tag producer_tag) {
    // G5: a nonzero RAT tag means a pending producer will supply a newer value,
    // so the RS records the tag and waits for its broadcast. Tag 0 means the
    // register file already holds the correct value, which is captured now.
    return producer_tag != 0;
}

// G6: Consumers ALWAYS receive the broadcast in the provided code.
// This decision is only about writing the destination register and clearing its RAT.
// current_tag: current RAT entry for the destination. result_tag: arriving producer.
bool tomasulo_write_register(Tag current_tag, Tag result_tag) {
    // G6: only the producer that the RAT still names owns the architectural
    // register. If a younger instruction has since been renamed onto it
    // (current_tag != result_tag), this older result must not overwrite the
    // register or clear the younger mapping (it still reaches waiting consumers).
    return current_tag == result_tag;
}
}
