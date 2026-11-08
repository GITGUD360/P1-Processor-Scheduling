#include "sim.hpp"
#include "decisions.hpp"

namespace p1 {
namespace {
bool any_busy(const State& s) {
    for(const auto& x:s.slots) if(x.busy) return true;
    return false;
}
void bind_register_sources(State& s,const State& before,int k,const Instruction& in) {
    auto make=[&](int r) {
        Operand o; o.reg=r;
        if(r<0) o.captured=true;
        else o.waiting=before.producer[r];
        return o;
    };
    s.slots[k].a=make(in.a); s.slots[k].b=make(in.b);
    s.producer[in.dst]=tag_of(k); // Always AFTER reading old source mappings.
}
void publish_register_result(State& s,const State& before,int k,const Instruction& in) {
    Tag tag=tag_of(k); s.regs[in.dst]=before.slots[k].result; s.producer[in.dst]=0;
    for(auto& x:s.slots) if(x.busy)
        for(Operand* o:{&x.a,&x.b}) if(o->waiting==tag) o->waiting=0;
}
}
Plan plan_inorder(const View& v) {
    Plan p; const State& s=v.s;
    for(std::size_t k=0;k<s.slots.size();++k) {
        const auto& x=s.slots[k];
        if(x.busy && x.phase==Phase::Complete && x.instruction==s.finished)
            p.write=static_cast<int>(k);
    }
    int head=oldest_slot(s,Phase::Waiting);
    if(head>=0) {
        bool older=false;
        for(const auto& x:s.slots)
            if(x.busy && x.instruction<s.slots[head].instruction) older=true;
        bool free=s.fu_owner[fu_for(v.p.code[s.slots[head].instruction].op)]==-1;
        if(inorder_can_launch(operands_ready(s.slots[head]),free,older)) // G1
            p.start=head;
    }
    if((head<0 || p.start==head) && s.next_issue<v.p.code.size()
       && s.producer[v.p.code[s.next_issue].dst]==0)
        p.issue=first_free(s);
    return p;
}
void bind_inorder(State& s,const State& b,int k,const Instruction& in) {
    bind_register_sources(s,b,k,in);
}
void publish_inorder(State& s,const State& b,int k,const Instruction& in) {
    publish_register_result(s,b,k,in);
}
Plan plan_scoreboard(const View& v) {
    Plan p; const auto& s=v.s;
    for(std::size_t k=0;k<s.slots.size();++k) {
        const auto& x=s.slots[k]; if(!x.busy) continue;
        if(x.phase==Phase::Waiting && operands_ready(x)
           && (p.start<0 || x.instruction<s.slots[p.start].instruction))
            p.start=static_cast<int>(k);
        if(x.phase!=Phase::Complete) continue;
        int dst=v.p.code[x.instruction].dst; bool war=false;
        for(const auto& reader:s.slots) if(reader.busy && reader.instruction<x.instruction)
            for(const Operand* o:{&reader.a,&reader.b}) if(o->reg>=0)
                if(scoreboard_war_blocks(o->ready_unread(),o->reg==dst)) war=true; // G3
        if(!war && (p.write<0 || x.instruction<s.slots[p.write].instruction))
            p.write=static_cast<int>(k);
    }
    if(s.next_issue<v.p.code.size()) {
        const auto& in=v.p.code[s.next_issue]; int u=fu_for(in.op);
        bool free=s.fu_owner[u]==-1 && !s.slots[u].busy;
        if(scoreboard_can_issue(free,s.producer[in.dst],any_busy(s))) p.issue=u; // G2
    }
    return p;
}
void bind_scoreboard(State& s,const State& b,int k,const Instruction& in) {
    bind_register_sources(s,b,k,in);
}
void publish_scoreboard(State& s,const State& b,int k,const Instruction& in) {
    publish_register_result(s,b,k,in);
}
Plan plan_tomasulo(const View& v) {
    Plan p; const auto& s=v.s;
    for(std::size_t k=0;k<s.slots.size();++k) {
        const auto& x=s.slots[k]; if(!x.busy) continue;
        if(x.phase==Phase::Complete && (p.write<0 || x.instruction<s.slots[p.write].instruction))
            p.write=static_cast<int>(k);
        if(x.phase==Phase::Waiting && operands_ready(x)
           && s.fu_owner[fu_for(v.p.code[x.instruction].op)]==-1
           && (p.start<0 || x.instruction<s.slots[p.start].instruction))
            p.start=static_cast<int>(k);
    }
    if(s.next_issue<v.p.code.size()) {
        int k=first_free(s); int u=fu_for(v.p.code[s.next_issue].op);
        if(tomasulo_can_issue(k>=0,s.fu_owner[u]==-1,any_busy(s))) p.issue=k; // G4
    }
    return p;
}
void bind_tomasulo(State& s,const State& before,int k,const Instruction& in) {
    auto make=[&](int r) {
        Operand o; o.reg=r;
        if(r<0) o.captured=true;
        else if(tomasulo_wait_for_source(before.producer[r])) { // G5
            o.waiting=before.producer[r];
        } else {
            o.value=before.regs[r]; o.captured=true;
        }
        return o;
    };
    s.slots[k].a=make(in.a); s.slots[k].b=make(in.b);
    s.producer[in.dst]=tag_of(k); // Sources FIRST, destination SECOND.
}
void publish_tomasulo(State& s,const State& before,int k,const Instruction& in) {
    Tag tag=tag_of(k); Value value=before.slots[k].result;
    // A broadcast must reach waiting consumers even if the destination was renamed.
    for(auto& x:s.slots) if(x.busy)
        for(Operand* o:{&x.a,&x.b}) if(o->waiting==tag) {
            o->waiting=0; o->value=value; o->captured=true;
        }
    if(tomasulo_write_register(before.producer[in.dst],tag)) { // G6
        s.regs[in.dst]=value; s.producer[in.dst]=0;
    }
}
}
