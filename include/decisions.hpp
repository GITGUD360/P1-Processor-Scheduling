#pragma once
#include "sim.hpp"
// Six pure decisions. No clock, energy, memory, table updates or loops are required.
// Positive tags name producers; tag 0 means there is no pending producer.
namespace p1 {
bool inorder_can_launch(bool sources_ready, bool fu_free, bool older_inflight); // G1
bool scoreboard_can_issue(bool row_free, Tag destination_tag, bool any_inflight); // G2
bool scoreboard_war_blocks(bool ready_unread, bool same_register); // G3
bool tomasulo_can_issue(bool rs_free, bool fu_free, bool any_inflight); // G4
bool tomasulo_wait_for_source(Tag producer_tag); // G5
bool tomasulo_write_register(Tag current_tag, Tag result_tag); // G6
}
