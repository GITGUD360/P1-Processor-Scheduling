# Six guided decisions (G1-G6)

You replace existing return expressions; you do not add a simulator or a counter.
Read `include/decisions.hpp` for interfaces and `src/controllers.cpp` for the provided
calls. Equivalent correct C++ is accepted. Additional input flags can be distractions:
decide which conditions belong to this stage rather than requiring every flag.

| ID | Function | Input meaning | Question |
|---|---|---|---|
| G1 | inorder_can_launch | ready sources; available FU; older work still in flight | Must older work finish before the oldest waiting instruction can launch? |
| G2 | scoreboard_can_issue | matching row/FU free; destination producer tag; any work in flight | Which resource and name hazards block admission? |
| G3 | scoreboard_war_blocks | older real source ready but unread; same register as writer | When is this reader still protecting an old value? |
| G4 | tomasulo_can_issue | free RS; free required FU; any work in flight | Is waiting-space admission the same as execution? |
| G5 | tomasulo_wait_for_source | current producer tag of a source | Capture a value now, or bind to a future producer? |
| G6 | tomasulo_write_register | destination's current tag; broadcasting producer tag | Does this result still own the architectural mapping? |

Tag 0 means no pending producer. Source maps are read before destination renaming.
Both source positions are handled; same-register source/destination cases are not
special work for you. The caller has already filtered older real readers for G3.
A G6 false result does not stop consumers from receiving the broadcast.

## What is already done

The driver advances one global cycle and takes one snapshot. Complete results are
chosen, ready instructions are selected, and the next program instruction is considered
for admission. `src/controllers.cpp` fixes deterministic order and updates all states.
`src/engine.cpp` fixes FU timing, resource ownership, immutable-cycle boundaries and
hardware-event counts. Do not rewrite these to compensate for an incorrect decision.

An issued instruction cannot launch immediately. A running operation lasts its fixed
latency; publication is a separate step. A dependency awakened by a publication starts
no earlier than the next cycle. Issue binding sees a result published in its own issue
cycle before renaming its own destination. You explain these boundaries but do not
implement their plumbing.

## Repair order and diagnostics

Start with G1, then G2 and G3 together. For Tomasulo repair G5 and G6 before relaxing
G4. The initial G4 admits one operation at a time; this masks some value/tag problems
in whole programs, but the G5/G6 local tests still fail. Arbitrary partly repaired
combinations can give wrong register values or explicit invariant errors.

`make decisions-test` checks 40 small input cases and reports G# and first principles.
`make check` additionally checks 13 short programs for all three models (39 runs),
comparing every issue/start/complete/write, event count, final register and energy.
Use `make check MODEL=...` to focus. A model passing local tests still needs trace tests.

## Reading the output

- `Functional oracle: PASS`: only the final ISA values match.
- `CHECK: PASS`: protected files, framework, decision cases and short traces passed.
- `VERIFIED_FOR_THIS_RUN`: these checks plus the particular frozen workload/configuration
  matched the reference trace/event/numeric signatures. Finite checks are not a proof.
- `UNVERIFIED`: permitted debug output; not valid final baseline evidence.
- `RUN_ERROR`: an invariant, arithmetic, timeout or other error prevented a complete run.

Reports and logs retain source/executable hashes. Run `make evaluate` again after code
changes; a previous screenshot does not verify your current code. `make demo` always
labels its runs UNVERIFIED because it deliberately skips full acceptance.
