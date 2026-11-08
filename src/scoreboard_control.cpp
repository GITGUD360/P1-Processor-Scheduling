#include "decisions.hpp"
namespace p1 {
// G2: row_free already includes the FU and its scoreboard row being available.
// destination_tag is the pending writer of this instruction's destination (0=none).
// any_inflight says whether ANY other instruction has not yet published.
// Replace only the decision, not the supplied allocation or table-maintenance code.
bool scoreboard_can_issue(bool row_free, Tag destination_tag, bool any_inflight) {
    (void)any_inflight;
    // G2: classic scoreboard issue stalls only on a structural hazard (the
    // matching FU/row is busy) or a WAW hazard (another instruction is already
    // the pending writer of this destination). Other in-flight work is fine.
    return row_free && destination_tag == 0;
}

// G3: The caller is already looking at a REAL source of an OLDER instruction.
// ready_unread: its old value is available but has not yet been captured.
// same_register: that source register is this completed writer's destination.
// Return true exactly when this source blocks the writer because of WAR.
bool scoreboard_war_blocks(bool ready_unread, bool same_register) {
    // G3: a WAR hazard exists only while the older reader still needs the OLD
    // value of this register: it names the same register AND that value is ready
    // but not yet read (Rj/Rk = yes). A reader still waiting on its own producer
    // (not ready) will receive a newer value, and one that already captured its
    // operand no longer needs the register, so neither blocks the write.
    return ready_unread && same_register;
}
}
