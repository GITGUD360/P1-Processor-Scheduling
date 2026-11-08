#include "sim.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace p1 {
namespace {
void need(bool ok,const std::string& why) {if(!ok)throw std::runtime_error("Simulation contract: "+why);}
bool index_ok(const State& s,int i) {return i>=0 && static_cast<std::size_t>(i)<s.slots.size();}
double probe_cost(const Config& c,Mode m) {
    return c.control_scale*(m==Mode::InOrder?c.inorder_probe_pj:m==Mode::Scoreboard?c.scoreboard_probe_pj:c.tomasulo_probe_pj);
}
double transition_cost(const Config& c,Mode m) {
    return c.control_scale*(m==Mode::InOrder?c.inorder_transition_pj:m==Mode::Scoreboard?c.scoreboard_transition_pj:c.tomasulo_transition_pj);
}
void account_inspection(Result& r,const State& b,const Program& p) {
    std::uint64_t probes=b.next_issue<p.code.size()?1:0;
    for(const auto& x:b.slots)if(x.busy)++probes;
    r.energy.add("scheduler_entry_probes",probes,probe_cost(r.cfg,r.mode));
    // Defined hardware policy: full WAR comparator bank is probed for EVERY
    // completed candidate and every older ready-but-unread source. No early exit.
    if(r.mode==Mode::Scoreboard) {
        std::uint64_t comparisons=0;
        for(const auto& writer:b.slots)if(writer.busy && writer.phase==Phase::Complete)
            for(const auto& reader:b.slots)if(reader.busy && reader.instruction<writer.instruction)
                for(const Operand* o:{&reader.a,&reader.b})if(o->ready_unread())++comparisons;
        r.energy.add("war_register_bit_comparisons",comparisons*6,r.cfg.register_bit_pj*r.cfg.control_scale);
    }
}
void account_publication(Result& r,const State& b,int slot) {
    Tag tag=tag_of(slot);std::uint64_t pending=0,matches=0;
    for(const auto& x:b.slots)if(x.busy)
        for(const Operand* o:{&x.a,&x.b})if(o->waiting) {++pending;if(o->waiting==tag)++matches;}
    if(r.mode==Mode::Tomasulo) {
        std::uint64_t rat=0;for(Tag q:b.producer)if(q)++rat;
        r.energy.add("cdb_tag_bit_comparisons",(pending+rat)*r.cfg.tag_bits(),r.cfg.tag_bit_pj*r.cfg.control_scale);
    } else r.energy.add("producer_tag_bit_comparisons",pending*3,r.cfg.tag_bit_pj*r.cfg.control_scale);
    r.energy.add("operand_deliveries",matches,r.cfg.delivery_pj*r.cfg.control_scale);
    r.energy.add("entry_transitions",matches,transition_cost(r.cfg,r.mode));
    r.energy.add("result_bus_transfers",1,r.cfg.result_bus_pj);
}
void invariant(const State& s,const Program& p) {
    for(int u=0;u<NFU;++u)if(s.fu_owner[u]>=0) {
        need(index_ok(s,s.fu_owner[u]),"FU owner out of range");
        const auto& x=s.slots[s.fu_owner[u]];need(x.busy && x.fu==u,"FU points to inactive/wrong slot");
    }
    for(Tag q:s.producer)if(q)need(index_ok(s,static_cast<int>(q)-1)&&s.slots[q-1].busy,"producer map contains a stale tag");
    std::vector<int> seen(p.code.size(),0);
    for(const auto& x:s.slots)if(x.busy) {
        need(x.instruction<p.code.size() && ++seen[x.instruction]==1,"duplicate/out-of-range instruction");
        for(const Operand* o:{&x.a,&x.b}) {
            need(!(o->captured && o->waiting),"operand is both captured and waiting");
            if(o->waiting)need(index_ok(s,static_cast<int>(o->waiting)-1)&&s.slots[o->waiting-1].busy,"operand contains a stale tag");
            if(x.phase!=Phase::Waiting)need(o->reg<0 || o->captured,"execution without captured input");
        }
        if(x.fu>=0)need(x.fu<NFU && s.fu_owner[x.fu]>=0 &&
            s.slots[s.fu_owner[x.fu]].instruction==x.instruction,"slot FU ownership mismatch");
    }
}
void trace_cycle(std::ostream& out,const State& s,const Program& p,const Plan& plan) {
    // One clock row even on an empty cycle; subsequent rows show end-of-cycle state.
    out<<s.cycle<<",clock,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,"<<plan.issue<<','<<plan.start<<','<<plan.write<<'\n';
    for(std::size_t i=0;i<s.slots.size();++i) {const auto& x=s.slots[i];if(!x.busy)continue;
        const char* ph=x.phase==Phase::Waiting?"waiting":x.phase==Phase::Running?"running":"complete";
        out<<s.cycle<<','<<ph<<','<<i<<','<<x.instruction+1<<','<<x.fu<<','
           <<x.a.reg<<','<<x.a.waiting<<','<<x.a.captured<<','<<x.b.reg<<','<<x.b.waiting<<','<<x.b.captured
           <<','<<x.finishes<<','<<s.producer[p.code[x.instruction].dst]<<",-1,-1,-1\n";
    }
}
std::string csv(const std::string& s) {std::string r="\"";for(char c:s){if(c=='\"')r+='\"';r+=c;}return r+'\"';}
}
Result simulate(const Program& p,const Config& cfg,Policy policy,std::ostream* trace) {
    cfg.validate();State s;s.regs=p.initial;s.written.assign(p.code.size(),false);
    s.slots.resize(policy.mode==Mode::InOrder?NFU+1:policy.mode==Mode::Scoreboard?NFU:cfg.rs_entries);
    Result r;r.mode=policy.mode;r.cfg=cfg;r.instructions=p.code.size();r.timings.resize(p.code.size());
    if(trace)*trace<<"cycle,phase,slot,instruction_1based,fu,a_reg_index,a_tag,a_captured,b_reg_index,b_tag,b_captured,scheduled_finish,destination_current_tag,planned_issue_slot,planned_start_slot,planned_write_slot\n";
    while(s.finished<p.code.size()) {
        if(s.cycle>=cfg.max_cycles)throw std::runtime_error("max_cycles reached: possible deadlock or missing student logic.");
        ++s.cycle;const State before=s;const View view{before,p,cfg};Plan plan=policy.plan(view);
        account_inspection(r,before,p);
        if(before.next_issue<p.code.size() && plan.issue<0)++r.issue_blocked_cycles;
        if(plan.start<0)++r.no_launch_cycles;
        // All three selections use the SAME snapshot. Thus no same-cycle slot/FU
        // reuse, wakeup-to-launch, completion-to-write, or issue-to-launch.
        for(int n:{plan.write,plan.start,plan.issue})need(n==-1 || index_ok(before,n),"invalid planned slot");
        if(plan.write>=0) {
            int k=plan.write;const auto& x=before.slots[k];
            need(x.busy && x.phase==Phase::Complete,"write requires a previously completed instruction");
            const auto& in=p.code[x.instruction];account_publication(r,before,k);
            if(before.producer[in.dst]==tag_of(k))r.energy.add("register_writes",1,cfg.register_write_pj);
            policy.publish(s,before,k,in);
            need(s.fu_owner[x.fu]==k,"written instruction lost its FU");s.fu_owner[x.fu]=-1;
            s.slots[k]=Slot{};need(!s.written[x.instruction],"double publication");
            s.written[x.instruction]=true;++s.finished;r.timings[x.instruction].write=s.cycle;
            r.energy.add("entry_transitions",1,transition_cost(cfg,policy.mode));
        }
        if(plan.start>=0) {
            int k=plan.start;const auto& old=before.slots[k];auto& x=s.slots[k];
            need(old.busy && old.phase==Phase::Waiting && operands_ready(old),"start requires a ready waiting instruction");
            need(old.issued<s.cycle,"cannot issue and start in same cycle");
            const auto& in=p.code[old.instruction];int fu=fu_for(in.op);
            if(policy.mode==Mode::Scoreboard)need(before.fu_owner[fu]==k,"scoreboard must reserve FU at issue");
            else need(before.fu_owner[fu]==-1,"FU unavailable in start snapshot");
            x.fu=fu;s.fu_owner[fu]=k;
            // For in-order/scoreboard, both register values are captured together.
            // Read from the start snapshot, never from a future writeback.
            Operand* next_ops[2]={&x.a,&x.b};
            const Operand* old_ops[2]={&old.a,&old.b};
            for(int operand_index=0;operand_index<2;++operand_index) {
                auto& dst=*next_ops[operand_index];const auto& src=*old_ops[operand_index];
                if(src.reg>=0 && !src.captured) {dst.value=before.regs[src.reg];dst.captured=true;r.energy.add("register_reads",1,cfg.register_read_pj);}
            }
            x.phase=Phase::Running;x.started=s.cycle;
            x.finishes=s.cycle+cfg.latency[static_cast<int>(in.op)]-1;
            r.timings[x.instruction].start=s.cycle;
            r.energy.add(std::string("op_")+op_name(in.op),1,cfg.operation_pj[static_cast<int>(in.op)]);
            r.energy.add("entry_transitions",1,transition_cost(cfg,policy.mode));
        }
        if(plan.issue>=0) {
            int k=plan.issue;need(!before.slots[k].busy,"issue slot must be free in snapshot");
            need(before.next_issue<p.code.size(),"issue past end of program");
            const auto& in=p.code[before.next_issue];int fu=fu_for(in.op);
            if(policy.mode!=Mode::Tomasulo)need(before.producer[in.dst]==0,"non-renaming issue has WAW conflict");
            if(policy.mode==Mode::Scoreboard)need(k==fu && before.fu_owner[fu]==-1,"scoreboard issue requires matching free FU row");
            if(policy.mode==Mode::InOrder)for(std::size_t j=0;j<before.slots.size();++j) {const auto& old=before.slots[j];need(!old.busy||old.phase!=Phase::Waiting||static_cast<int>(j)==plan.start,"in-order input latch cannot accept another instruction unless its current instruction launches");}
            // Important boundary rule: issue reads the post-writeback register/map
            // state. This avoids missing a broadcast on the cycle of issue.
            // The issue decision still cannot reuse a slot or FU freed this cycle.
            State binding_snapshot=s;
            Slot fresh;fresh.busy=true;fresh.instruction=before.next_issue;fresh.issued=s.cycle;
            if(policy.mode==Mode::Scoreboard){fresh.fu=fu;s.fu_owner[fu]=k;}
            s.slots[k]=fresh;policy.bind(s,binding_snapshot,k,in);
            const auto& x=s.slots[k];
            for(const Operand* o:{&x.a,&x.b})if(o->reg>=0 && o->captured)r.energy.add("register_reads",1,cfg.register_read_pj);
            need(s.producer[in.dst]==tag_of(k),"issue must install destination producer after binding sources");
            ++s.next_issue;r.timings[before.next_issue].issue=s.cycle;
            r.energy.add("instructions_admitted",1,cfg.instruction_pj);
            r.energy.add("entry_transitions",1,transition_cost(cfg,policy.mode));
        }
        // A latency-L operation launched at c occupies c..c+L-1. Its result
        // becomes Complete at the end of c+L-1, and is writable no earlier than c+L.
        for(auto& x:s.slots)if(x.busy && x.phase==Phase::Running && x.finishes==s.cycle) {
            const auto& in=p.code[x.instruction];x.result=evaluate(in,x.a.value,x.b.value,p);
            x.phase=Phase::Complete;r.timings[x.instruction].complete=s.cycle;
            r.energy.add("entry_transitions",1,transition_cost(cfg,policy.mode));
        }
        invariant(s,p);if(trace)trace_cycle(*trace,s,p,plan);
    }
    r.cycles=s.cycle;r.registers=s.regs;
    const auto expected=interpret(p);
    for(int reg=0;reg<NREG;++reg)if(!same_value(reg,r.registers[reg],expected[reg]))
        { r.functional_ok=false; r.mismatched_registers.push_back(reg); }
    // A completed teaching run is printable even with wrong values. This is NOT
    // acceptance: CLI output is unverified, and tools/workflow.py compares timing,
    // event counts, numbers, source integrity and decision tests with the baseline.
    return r;
}
void print_summary(std::ostream& o,const Result& r) {
    o<<std::fixed<<std::setprecision(6)<<"Model: "<<mode_name(r.mode)<<"\nInstructions: "<<r.instructions
     <<"\nCycles: "<<r.cycles<<"\nSimulated time (ns): "<<r.elapsed_ns()<<"\nCPI: "<<double(r.cycles)/r.instructions
     <<"\nDynamic energy (nJ): "<<r.energy.dynamic_pj()/1000.0<<"\nBackground energy (nJ): "<<r.background_pj()/1000.0
     <<"\nTask energy (nJ): "<<r.task_pj()/1000.0<<"\nTask-average power (mW): "<<r.task_average_mw()
     <<"\nDeadline met: "<<(r.deadline_met()?"yes":"NO")<<"\nPeriod met: "<<(r.period_met()?"yes":"NO");
    if(r.period_met())o<<"\nPeriod energy (nJ): "<<r.period_pj()/1000.0<<"\nPeriod-average power (mW): "<<r.period_pj()/r.cfg.period_ns;
    o<<"\nFunctional oracle: "<<(r.functional_ok?"PASS":"FAIL")<<'\n';
    if(!r.functional_ok) {o<<"Mismatched registers:";for(int reg:r.mismatched_registers)o<<' '<<reg_name(reg);o<<'\n';}
    o<<"Target-model validation: UNVERIFIED (standalone run).\n"
       <<"Use make check and make evaluate. A completed run is not proof of correct timing/energy.\n";
}
void write_outputs(const std::string& prefix,const Program& p,const Result& r) {
    auto parent=std::filesystem::path(prefix).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
    auto open=[&](const std::string& suffix){std::ofstream f(prefix+suffix);if(!f)throw std::runtime_error("Cannot write "+prefix+suffix);f<<std::setprecision(17);return f;};
    auto sum=open("_summary.csv");
    sum<<"architecture,rs_entries,control_scale,frequency_ghz,instructions,cycles,time_ns,cpi,background_mw,dynamic_nj,background_nj,task_nj,task_avg_mw,deadline_ns,deadline_met,period_ns,sleep_mw,period_met,period_nj,period_avg_mw,issue_blocked_cycles,no_launch_cycles,functional_ok\n";
    sum<<mode_name(r.mode)<<','<<r.cfg.rs_entries<<','<<r.cfg.control_scale<<','<<r.cfg.frequency_ghz<<','<<r.instructions<<','<<r.cycles<<','<<r.elapsed_ns()<<','<<double(r.cycles)/r.instructions<<','<<r.cfg.background_mw(r.mode)<<','<<r.energy.dynamic_pj()/1000<<','<<r.background_pj()/1000<<','<<r.task_pj()/1000<<','<<r.task_average_mw()<<','<<r.cfg.deadline_ns<<','<<r.deadline_met()<<','<<r.cfg.period_ns<<','<<r.cfg.sleep_mw<<','<<r.period_met()<<',';
    if(r.period_met())sum<<r.period_pj()/1000<<','<<r.period_pj()/r.cfg.period_ns;else sum<<"NA,NA";
    sum<<','<<r.issue_blocked_cycles<<','<<r.no_launch_cycles<<','<<r.functional_ok<<'\n';
    auto events=open("_energy.csv");events<<"event,count,energy_per_event_pj,total_pj\n";
    for(const auto& kv:r.energy.events)events<<kv.first<<','<<kv.second.count<<','<<kv.second.pj_each<<','<<kv.second.count*kv.second.pj_each<<'\n';
    auto timing=open("_timing.csv");timing<<"instruction_1based,assembly,issue,start,complete,write\n";
    for(std::size_t i=0;i<p.code.size();++i){const auto& t=r.timings[i];timing<<i+1<<','<<csv(p.code[i].text)<<','<<t.issue<<','<<t.start<<','<<t.complete<<','<<t.write<<'\n';}
    auto regs=open("_registers.csv");regs<<"register,value\n";
    for(int i=0;i<NREG;++i){regs<<reg_name(i)<<',';if(i<32)regs<<r.registers[i].u;else regs<<r.registers[i].f;regs<<'\n';}
}
} // namespace p1
