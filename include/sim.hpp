#pragma once
// All time and energy refer to the SIMULATED hardware, not to host C++ execution.
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace p1 {
constexpr int NREG = 64;     // R0..R31 followed by F0..F31. R0 is NOT hardwired.
constexpr int NFU = 5;       // INT, FP_ADD, FP_MUL, FP_DIV, MEMORY
using Cycle = std::uint64_t;
using Tag = unsigned;       // 0 = no pending producer; slot index + 1 otherwise
struct Value { std::uint64_t u = 0; double f = 0; };
enum class Op { IADD, FADD, FSUB, FMUL, FDIV, LD };
enum class Mode { InOrder, Scoreboard, Tomasulo };
enum class Phase { Waiting, Running, Complete };
struct Instruction {
    Op op{};
    int dst=-1, a=-1, b=-1;
    unsigned address=0;      // word index, NOT byte address; used only by LD
    std::string text;
};
struct Program {
    std::vector<Instruction> code;
    std::array<Value,NREG> initial{};
    std::map<unsigned,double> memory; // read-only, preloaded, no cache/store model
};
struct Config {
    double frequency_ghz=4.0;
    double deadline_ns=140.0;
    double period_ns=1000.0;
    double sleep_mw=0.10;    // equal for all designs; zero transition costs assumed
    int rs_entries=8;
    double control_scale=1.0;
    Cycle max_cycles=200000;
    std::array<unsigned,6> latency{{1,3,3,5,12,8}};
    // Energy inputs are deliberately hypothetical teaching parameters.
    // Units: mW for background; pJ/event for dynamic events.
    double common_background_mw=18.0;
    double inorder_control_mw=2.0, scoreboard_control_mw=6.0;
    double tomasulo_fixed_mw=6.0, tomasulo_per_rs_mw=0.75;
    double instruction_pj=2.0, register_read_pj=1.0, register_write_pj=1.2;
    double result_bus_pj=2.0, delivery_pj=0.5;
    double inorder_probe_pj=0.12, scoreboard_probe_pj=0.25, tomasulo_probe_pj=0.30;
    double inorder_transition_pj=0.25, scoreboard_transition_pj=0.50, tomasulo_transition_pj=0.75;
    double tag_bit_pj=0.06, register_bit_pj=0.03;
    std::array<double,6> operation_pj{{2.0,8.0,8.0,20.0,60.0,40.0}};
    double dt_ns() const { return 1.0/frequency_ghz; }
    unsigned tag_bits() const;
    double background_mw(Mode mode) const;
    void validate() const;
};
struct Operand {
    int reg=-1;             // -1 = no source (e.g., direct-address LD)
    Tag waiting=0;          // producer tag Q; nonzero means unavailable
    bool captured=false;    // actual value safely latched in this instruction
    Value value{};
    bool ready_unread() const { return reg>=0 && waiting==0 && !captured; }
};
struct Slot {
    bool busy=false;
    std::size_t instruction=0;
    int fu=-1;
    Phase phase=Phase::Waiting;
    Operand a,b;
    Value result{};
    Cycle issued=0, started=0, finishes=0;
};
struct Timing { Cycle issue=0,start=0,complete=0,write=0; };
struct State {
    Cycle cycle=0;
    std::size_t next_issue=0, finished=0;
    std::array<Value,NREG> regs{};
    std::array<Tag,NREG> producer{};
    std::array<int,NFU> fu_owner{{-1,-1,-1,-1,-1}};
    std::vector<Slot> slots;
    std::vector<bool> written;
};
struct View {
    const State& s;         // immutable start-of-cycle snapshot
    const Program& p;
    const Config& cfg;
};
struct Plan {
    int write=-1;           // slot to publish; at most one result per cycle
    int start=-1;           // slot to launch; at most one launch per cycle
    int issue=-1;           // free slot for p.code[s.next_issue], or -1
};
struct Policy {
    Mode mode;
    Plan (*plan)(const View&);
    void (*bind)(State&,const State&,int,const Instruction&);
    void (*publish)(State&,const State&,int,const Instruction&);
};
// Student implementation contracts: decisions use only View.s. bind and publish
// may change operand/producer/register state, but not clock, FU timing or energy.
Plan plan_inorder(const View&);
void bind_inorder(State&,const State&,int,const Instruction&);
void publish_inorder(State&,const State&,int,const Instruction&);
Plan plan_scoreboard(const View&);
void bind_scoreboard(State&,const State&,int,const Instruction&);
void publish_scoreboard(State&,const State&,int,const Instruction&);
Plan plan_tomasulo(const View&);
void bind_tomasulo(State&,const State&,int,const Instruction&);
void publish_tomasulo(State&,const State&,int,const Instruction&);
Policy policy_for(Mode);

// Supplied read-only helpers. A tag identifies an in-flight slot, never a letter.
int fu_for(Op);
const char* op_name(Op);
const char* mode_name(Mode);
Mode parse_mode(const std::string&);
std::string reg_name(int);
Tag tag_of(int slot);
bool operands_ready(const Slot&);
int first_free(const State&);
int oldest_slot(const State&,Phase);
Value evaluate(const Instruction&,Value,Value,const Program&);
Program parse_program(const std::string&);
Config load_config(const std::string&);
std::array<Value,NREG> interpret(const Program&); // functional oracle; NOT timing
bool same_value(int reg,Value,Value);

struct Event { std::uint64_t count=0; double pj_each=0; };
class EnergyLedger {
public:
    std::map<std::string,Event> events;
    void add(const std::string&,std::uint64_t,double);
    double dynamic_pj() const;
};
struct Result {
    Mode mode{};
    Config cfg;
    Cycle cycles=0;
    bool functional_ok=true;
    std::vector<int> mismatched_registers;
    std::size_t instructions=0;
    std::array<Value,NREG> registers{};
    std::vector<Timing> timings;
    EnergyLedger energy;
    std::uint64_t issue_blocked_cycles=0, no_launch_cycles=0;
    double elapsed_ns() const { return cycles*cfg.dt_ns(); }
    double background_pj() const { return cfg.background_mw(mode)*elapsed_ns(); }
    double task_pj() const { return background_pj()+energy.dynamic_pj(); }
    double task_average_mw() const { return cycles ? task_pj()/elapsed_ns():0.0; }
    bool deadline_met() const { return elapsed_ns()<=cfg.deadline_ns+1e-9; }
    bool period_met() const { return elapsed_ns()<=cfg.period_ns+1e-9; }
    double period_pj() const; // throws when task does not fit period
};
Result simulate(const Program&,const Config&,Policy,std::ostream* trace=nullptr);
void write_outputs(const std::string& prefix,const Program&,const Result&);
void print_summary(std::ostream&,const Result&);
} // namespace p1
