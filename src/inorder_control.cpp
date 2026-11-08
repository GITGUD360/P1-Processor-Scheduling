#include "decisions.hpp"
namespace p1 {
// G1: The supplied controller has already selected the oldest waiting instruction.
// sources_ready: both inputs are available. fu_free: its required FU is available.
// older_inflight: an older instruction has not yet published its result.
// Replace the return expression. Does in-order START require all older work to
// finish, or can different FUs overlap while starts remain in program order?
bool inorder_can_launch(bool sources_ready, bool fu_free, bool older_inflight) {
    (void)older_inflight; 
    // TODO G1; you will need to fix this decision
    return sources_ready && fu_free && !older_inflight;
}
}
