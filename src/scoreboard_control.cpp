#include "decisions.hpp"
namespace p1 {
// G2: row_free already includes the FU and its scoreboard row being available.
// destination_tag is the pending writer of this instruction's destination (0=none).
// any_inflight says whether ANY other instruction has not yet published.
// Replace only the decision, not the supplied allocation or table-maintenance code.
bool scoreboard_can_issue(bool row_free, Tag destination_tag, bool any_inflight) {
    (void)any_inflight;
    // TODO G2
    return row_free && destination_tag == 0 && !any_inflight;
}

// G3: The caller is already looking at a REAL source of an OLDER instruction.
// ready_unread: its old value is available but has not yet been captured.
// same_register: that source register is this completed writer's destination.
// Return true exactly when this source blocks the writer because of WAR.
bool scoreboard_war_blocks(bool ready_unread, bool same_register) {
    (void)ready_unread;
    // TODO G3
    return same_register;
}
}
