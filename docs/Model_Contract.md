# Frozen hardware model

This document is normative. The supplied tests calibrate selected boundaries;
textbook schedules with different boundary rules are not reference answers here.
All numeric hardware/energy parameters below are pedagogical assumptions, not
measurements or specifications of an actual 4 GHz processor.

## 1\. Program and arithmetic semantics

The teaching ISA contains IADD, FADD, FSUB, FMUL, FDIV and LD. It is not a complete
MIPS, RISC-V or x86 ISA. Instructions have destination-first syntax.

* `IADD Rd, Ra, Rb`: unsigned 64-bit addition modulo 2^64.
* `FADD`, `FSUB`, `FMUL`, `FDIV`: destination and two floating sources, using C++ `double`.
* `LD Fd, \[a]`: load a floating value from immutable memory word index a.
An address is NOT a byte address. There is no base-register address computation.
* `.reg register value` initializes architectural state before simulated time zero.
* `.mem word\_index value` initializes read-only data before simulated time zero.
* Directives precede instructions. Other registers initialize to zero. R0 is an
ordinary writable register. R0..R31 and F0..F31 are separate architectural banks.
* No stores, branches, loops, dynamic instruction generation or program rewriting
occurs during simulation. The frozen assembly is explicitly unrolled.
* Inputs must not produce division by zero or non-finite floating results. Those
are rejected as out of scope, not modeled as precise hardware exceptions.
* Final integer results match exactly. Floating comparison uses the supplied
absolute/relative tolerance (1e-10). Every architectural register is checked,
including temporaries; a matching F0 alone is insufficient.

A sequential functional interpreter supplies final values only. Its host runtime
is not a processor timing estimate and it does not produce energy measurements.

## 2\. Frozen workload and scenario

`workloads/iot\_serial.asm` contains 96 instructions: 16 samples times 6 instructions.
The samples are x\[k] = 2(k+1), k=0..15. Starting with state=4.0, each sample computes:

&#x20;   state\_next = ((state + sample) \* 0.5 + 1.0) / 2.0


State is F0; scratch registers are F8..F11; F24=0.5, F25=1.0 and F26=2.0.
R1 is a final logical sample counter; R2=1. In a running OoO simulation, R1 is NOT a completion barrier for the floating updates. The final result has R1=16
and F0=11.111111111938953. The full functional oracle also checks scratch registers.
The recurrence is a serial RAW chain. Reusing the same scratch names additionally
creates name dependencies; do not rename or reorder the baseline assembly.

Scenario: a compute-only microkernel inside a hypothetical industrial IoT data
processing module. A new 16-sample batch is available every 1,000 ns. The computation
must finish within 140 ns. Initialization, sensing, communication, OS activity and
external memory transfers are outside the measured interval. These boundaries are
not a claim about whole-device battery life.

Choose the configuration with the lowest PERIOD energy among correct candidates
that meet the deadline. Report task energy and execution-average power separately.
At 4 GHz the deadline is 560 cycles, and a cycle is 0.25 ns. The task starts at
cycle 1, with an empty scheduling system, and ends after its last result publication.
A subsequent batch begins from the supplied initialization and an empty system;
there is no overlap between batches in this assignment.

`iot\_four\_streams.asm` has the same 96-instruction operation mix, but samples are
assigned round-robin to four independent state variables F0..F3. This changes the
application, NOT just its schedule. It is not a valid replacement for the serial
computation. Compare architectures only within a given workload.

For this changed four-stream scenario, also evaluate a 70 ns deadline (280 cycles)
using the already measured times. The baseline CSV retains the original 140 ns flag;
you must calculate and label the separate 70 ns feasibility decision yourself.

## 3\. Common hardware inventory and operation latency

All designs share one INT unit, one FP ADD/SUB unit, one FP MUL unit, one FP DIV
unit and one MEMORY unit. There is one issue lane, one launch/operand-capture lane,
and one result-publication lane. These lanes can work on different instructions
in the same cycle. Different FUs execute concurrently, but each FU is non-pipelined.

|Operation|FU|Execution latency (cycles)|Operation energy (pJ, once per launch)|
|-|-|-:|-:|
|IADD|INT|1|2|
|FADD, FSUB|FP ADD/SUB|3|8|
|FMUL|FP MUL|5|20|
|FDIV|FP DIV|12|60|
|LD|MEMORY|8|40|

The LD models an idealized, fixed-latency on-module scratchpad access, NOT external
DRAM and NOT a cache miss. Functional-unit latency is not its complete utilization
interval: a unit keeps its result until publication. It cannot accept a new operation
until the following cycle after its previous result was written. Scoreboard FUs are
reserved even earlier, at issue, so they may also be occupied while waiting for data.
There is no separate pipelined initiation interval and no unbounded execution queue.

## 4\. Exact cycle boundary

The engine owns the global simulated clock. It increments it ONCE per cycle.
It does not sum operation latencies, and does not use sleep(), chrono or host timers.
The engine takes an immutable start-of-cycle snapshot for all action selections.

1. Select at most one previously Complete instruction to publish.
2. Select at most one Waiting instruction to launch, using readiness and FU
availability from that same snapshot.
3. Select whether the next program instruction may issue, again using the snapshot.
4. Apply publication, launch and issue. Issue operand binding uses the register/map
state AFTER this cycle's publication but BEFORE its own destination is renamed.
5. At the end of the cycle, mark all operations whose execution interval ends now
as Complete. Such newly completed results cannot publish until the next cycle.

An operation of latency L launched at cycle c executes in c..c+L-1, becomes Complete
at the end of c+L-1, and can publish no earlier than c+L. A just-issued instruction
cannot launch in the same cycle. A previously waiting consumer awakened by a
publication cannot launch until the next cycle. A freed FU/slot cannot be reused
in the same cycle. No early EX-result forwarding is modeled in any design.

One explicitly pipelined latch rule: the in-order input latch may accept the next
instruction when its current instruction launches in the same cycle, provided a
DIFFERENT slot is free in the snapshot. This is not same-cycle FU or slot reuse.

One explicit binding rule prevents a lost wakeup: an instruction admitted on the
same cycle as its producer publishes sees the post-publication value/map when its
operands are bound. It therefore must not wait forever for a broadcast that already
occurred. It still cannot launch before the next cycle. `binding` in each bind
callback is this post-publication, pre-renaming snapshot.

All eligibility and arbitration choices below are deterministic. Use the oldest
program instruction among eligible candidates, and the lowest free slot index for
allocation. A stalled eligible stage does not freeze independent FUs.

## 5\. In-order pipeline model

This is an execution-side multicycle in-order pipeline, not a literal five-stage
IF-ID-EX-MEM-WB implementation. Frontend fetch/decode time is abstracted equally
for all three designs. Operand capture and execution launch use one boundary.

Storage has one waiting input latch plus one record/result latch per FU (represented
by 6 software slots). These slots are NOT a reorder buffer or six reservation stations.
At most one not-yet-launched instruction may wait. Already launched instructions may
execute concurrently on different FUs. Launch and architectural writeback are in order.

* Issue: input latch empty OR its current instruction launches this cycle; a free
slot exists; destination has no pending writer (WAW interlock).
* Bind: save the two source register names and current pending-producer tags, but
do not capture values yet. Install the destination producer only after sources
have been bound. A missing source is ready/captured with register=-1 and tag=0.
* Launch: the waiting instruction has both tags zero and its FU is free. Both
actual register values are captured together at launch, from the cycle snapshot.
* Write: the next instruction in program order must be Complete. A younger complete
instruction holds its FU/result until it can write. There is no separate retirement
mechanism and no precise-exception claim.
* Publication writes the register, clears its pending producer and clears matching
waiting source tags. Younger writes cannot overtake older reads because launch
and writeback order are constrained.

## 6\. Scoreboard model

There are five fixed FU rows; row index equals FU index. The row and FU are reserved
from issue through publication. In-order issue can be blocked by WAW or structural
hazards even if a later instruction could otherwise make progress.

* Issue: the matching row and FU are free and no pending writer names the destination.
* Bind: record source names and pending tags from the binding snapshot. Source values
are not captured yet. Set the destination producer after recording both sources.
* Launch/read: oldest waiting row with both source tags zero. The row already owns
the required FU. Capture BOTH operands together; that is the modeled Read Operands
boundary. At most one row launches per cycle in this assignment.
* Write: among Complete rows, choose the oldest row with no older instruction whose
ready-but-unread source names this destination. Check both source positions (WAR).
An older blocked writer does not prevent another eligible writer from publishing.
* Publication writes the register and clears the producer. Matching dependent tags
become zero but do NOT latch the value. Their actual values are captured at launch.

Mapping to lecture notation: destination is Fi; source register fields are Fj/Fk;
waiting tags are Qj/Qk. Rj/Rk are `ready\_unread()`, not simply `waiting==0`:
R=true means a real source is available AND has not yet been captured. After capture,
R is false. Empty/missing operands do not create WAR hazards.

## 7\. Tomasulo-style model

There is a shared pool of 8 RS entries in the baseline, with 4 and 16 entries used
only in the specified capacity experiment. FUs are not reserved by waiting RS entries.
The shared pool is an educational simplification; it is not the original machine's
exact distribution of load buffers or operation-specific reservation stations.

* Issue: a free RS exists; WAW is NOT an issue stall. Use its lowest free index.
* Bind each source: if its current producer is zero, capture the current register
value; otherwise save that tag and leave the value uncaptured. After BOTH sources
are bound, map the destination to this RS tag. Handle self-source instructions.
* Launch: choose the oldest RS with both operands available and the relevant FU free.
A ready RS is not an FU. At most one operation launches in the entire machine/cycle.
* Write: choose the oldest Complete instruction; one CDB transfer per cycle.
* Broadcast its (tag,value) to every source waiting for that tag. Such sources capture
the value and clear their tag. Update a register and clear its RAT entry ONLY when
the current entry still matches this tag. An older late result must not overwrite
a younger mapping/result. Results must still be broadcast even if RAT no longer
points to their producer, because older consumers can still need that version.

Tags are positive integer slot IDs plus one; zero means no producer. A tag stays
allocated through publication and is not recycled in that cycle. Tag width is
ceil(log2(RS\_entries+1)): 3, 4 and 5 bits for 4, 8 and 16 entries respectively.
Strings such as "Mult1" are display labels only; their character lengths are irrelevant.

## 8\. Energy accounting: dimensions and physical-event policy

The immutable ledger is in `engine.cpp` and `support.cpp`. Students implement control
logic; they do not independently insert arbitrary timing or energy increments.
The observer counts events in the modeled machine, so two equivalent C++ algorithms
produce the same accounting even if one uses a hash table and the other a loop.

&#x20;   time\_ns       = cycles / frequency\_GHz
    E\_dynamic\_pJ  = sum(event\_count\[k] \* pJ\_per\_event\[k])
    E\_background\_pJ = P\_background\_mW \* time\_ns
    E\_task\_pJ     = E\_dynamic\_pJ + E\_background\_pJ
    P\_task\_avg\_mW = E\_task\_pJ / time\_ns
    E\_period\_pJ   = E\_task\_pJ + P\_sleep\_mW \* (period\_ns - time\_ns)


Because 1 mW \* 1 ns = 1 pJ, these equations are dimensionally consistent. Convert
pJ to nJ by dividing by 1,000. Do NOT multiply an already accumulated energy sum by
time again. If a task exceeds the period, period energy is undefined (NA), not a
negative sleep interval. Deadline feasibility and period feasibility are separate.

Background power covers the assumed common always-on/clock costs and controller
costs. It is charged during EVERY simulated cycle, including stalls and draining:

&#x20;   in-order:   18 + control\_scale \* 2          mW
    scoreboard: 18 + control\_scale \* 6          mW
    Tomasulo:   18 + control\_scale \* (6 + 0.75 \* RS\_entries) mW


These are allocated model costs, NOT transistor-based power estimates. Idle allocated
RS capacity still has background cost. Functional-unit operation energy is charged
ONCE at launch for the entire operation, never once per latency cycle. Sleep power
is 0.10 mW for all configurations; entry/exit delay and energy are zero in this model.

|Event|Count rule|Baseline pJ per counted event|
|-|-|-:|
|instructions\_admitted|once per issued instruction|2.0|
|register\_reads|each actual operand value read from RF|1.0|
|register\_writes|each result whose current destination mapping is still valid|1.2|
|result\_bus\_transfers|one per publication, including an obsolete RAT destination|2.0|
|op\_OPCODE|once at launch; costs from Section 3|operation-specific|
|scheduler\_entry\_probes|one per busy slot per cycle, plus one while any unissued instruction remains|0.12 / 0.25 / 0.30 (in-order/SB/T)|
|entry\_transitions|issue, launch, completion, publication; plus each matching operand wakeup|0.25 / 0.50 / 0.75|
|operand\_deliveries|each matching pending source on publication; both positions count separately|0.50|
|producer\_tag\_bit\_comparisons|each nonzero pending source tag on a non-T publication, times 3 bits|0.06 per bit|
|cdb\_tag\_bit\_comparisons|every nonzero pending source tag AND every nonzero RAT entry, times current T tag width|0.06 per bit|
|war\_register\_bit\_comparisons|every Complete scoreboard candidate times every older ready-unread source, times 6 bits|0.03 per bit|

`control\_scale` multiplies controller background terms, scheduler probes, entry
transitions, operand deliveries and tag/register comparison energy. It does NOT
multiply common background, arithmetic, memory, instruction admission, RF access,
result-bus energy or sleep power. Timing does not change when only this scale changes.

Comparator charging is a full-width, non-short-circuit rule. For a CDB broadcast at
8 RS entries, 7 pending operands plus 4 active RAT entries means 11 whole-tag
comparisons = 44 bit comparisons, independent of matches and printed label lengths.
At baseline comparison cost that is 2.64 pJ for that comparison component alone.
Do not charge one event per C++ string character or stop charging after a first mismatch.
The observer charges the full defined comparator bank even if student code searches
more efficiently. Unmodeled controller details are folded into the aggregate costs;
this is not a gate-level switching-activity simulator.

`issue\_blocked\_cycles` and `no\_launch\_cycles` can overlap; do not add them as a
partition of all cycles. They are diagnostics, not separate time/energy contributions.
No joules are assigned to parsing, logging, printing, C++ instructions or Python runs.



