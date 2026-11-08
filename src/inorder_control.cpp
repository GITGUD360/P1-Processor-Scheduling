#include "decisions.hpp"
namespace p1 {
// G1: The supplied controller has already selected the oldest waiting instruction.
// sources_ready: both inputs are available. fu_free: its required FU is available.
// older_inflight: an older instruction has not yet published its result.
// Replace the return expression. Does in-order START require all older work to
// finish, or can different FUs overlap while starts remain in program order?
bool inorder_can_launch(bool sources_ready, bool fu_free, bool older_inflight) {
    (void)older_inflight;
    // G1: launch order is already in program order because the controller only
    // offers the oldest waiting instruction. Older work that is still executing on
    // a DIFFERENT FU is not a hazard: true RAW dependences are covered by
    // sources_ready (pending producer tags), and the same-FU structural hazard is
    // covered by fu_free. Requiring !older_inflight would serialize the machine.
    return sources_ready && fu_free;
}
}
