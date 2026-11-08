#include "sim.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <set>

namespace p1 {
unsigned Config::tag_bits() const {
    unsigned bits=0,n=static_cast<unsigned>(rs_entries);
    do { ++bits; n>>=1; } while(n);
    return bits; // ceil(log2(rs_entries+1)); zero is reserved
}
double Config::background_mw(Mode m) const {
    double control=m==Mode::InOrder?inorder_control_mw:
        m==Mode::Scoreboard?scoreboard_control_mw:
        tomasulo_fixed_mw+tomasulo_per_rs_mw*rs_entries;
    return common_background_mw+control_scale*control;
}
void Config::validate() const {
    if (!(frequency_ghz>0) || !std::isfinite(frequency_ghz) ||
        !(period_ns>0) || !(deadline_ns>0) || deadline_ns>period_ns ||
        !std::isfinite(period_ns) || !std::isfinite(deadline_ns) ||
        !(control_scale>0) || !std::isfinite(control_scale) ||
        rs_entries<1 || rs_entries>64 || max_cycles<1)
        throw std::runtime_error("Invalid clock, deadline/period, scale, RS size or max_cycles.");
    for(auto l:latency) if(l<1 || l>100000) throw std::runtime_error("Invalid latency.");
    std::vector<double> costs={sleep_mw,common_background_mw,inorder_control_mw,
        scoreboard_control_mw,tomasulo_fixed_mw,tomasulo_per_rs_mw,instruction_pj,
        register_read_pj,register_write_pj,result_bus_pj,delivery_pj,inorder_probe_pj,
        scoreboard_probe_pj,tomasulo_probe_pj,inorder_transition_pj,scoreboard_transition_pj,
        tomasulo_transition_pj,tag_bit_pj,register_bit_pj};
    for(double d:operation_pj) costs.push_back(d);
    for(double d:costs) if(d<0 || !std::isfinite(d)) throw std::runtime_error("Invalid energy input.");
}
int fu_for(Op o) {
    switch(o) {case Op::IADD:return 0; case Op::FADD:case Op::FSUB:return 1;
        case Op::FMUL:return 2;case Op::FDIV:return 3;case Op::LD:return 4;}
    throw std::logic_error("Bad opcode.");
}
const char* op_name(Op o) {
    const char* n[]={"IADD","FADD","FSUB","FMUL","FDIV","LD"};
    return n[static_cast<int>(o)];
}
const char* mode_name(Mode m) {return m==Mode::InOrder?"inorder":m==Mode::Scoreboard?"scoreboard":"tomasulo";}
Mode parse_mode(const std::string& s) {
    if(s=="inorder")return Mode::InOrder;
    if(s=="scoreboard")return Mode::Scoreboard;
    if(s=="tomasulo")return Mode::Tomasulo;
    throw std::runtime_error("Mode must be inorder, scoreboard or tomasulo.");
}
std::string reg_name(int r) {return std::string(r<32?"R":"F")+std::to_string(r%32);}
Tag tag_of(int slot) {return static_cast<Tag>(slot+1);}
bool operands_ready(const Slot& x) {return x.a.waiting==0 && x.b.waiting==0;}
int first_free(const State& s) {
    for(std::size_t i=0;i<s.slots.size();++i)if(!s.slots[i].busy)return static_cast<int>(i);
    return -1;
}
int oldest_slot(const State& s,Phase phase) {
    int best=-1;
    for(std::size_t i=0;i<s.slots.size();++i) {
        const auto& x=s.slots[i];
        if(x.busy && x.phase==phase && (best<0 || x.instruction<s.slots[best].instruction))best=static_cast<int>(i);
    }
    return best;
}
Value evaluate(const Instruction& in,Value a,Value b,const Program& p) {
    Value v;
    switch(in.op) {
        case Op::IADD:v.u=a.u+b.u;break; // unsigned addition: modulo 2^64
        case Op::FADD:v.f=a.f+b.f;break;
        case Op::FSUB:v.f=a.f-b.f;break;
        case Op::FMUL:v.f=a.f*b.f;break;
        case Op::FDIV:if(b.f==0.0)throw std::runtime_error("Division by zero is outside this model.");v.f=a.f/b.f;break;
        case Op::LD:{auto it=p.memory.find(in.address);if(it==p.memory.end())throw std::runtime_error("Uninitialized LD address.");v.f=it->second;break;}
    }
    if(in.dst>=32 && !std::isfinite(v.f))throw std::runtime_error("Non-finite floating result outside this model.");
    return v;
}
bool same_value(int r,Value a,Value b) {
    if(r<32)return a.u==b.u;
    return std::isfinite(a.f) && std::isfinite(b.f) &&
        std::abs(a.f-b.f)<=1e-10*std::max({1.0,std::abs(a.f),std::abs(b.f)});
}
static std::string strip(std::string s) {auto p=s.find('#');if(p!=std::string::npos)s.resize(p);return s;}
static std::vector<std::string> tokens(std::string s) {
    for(char& c:s)if(c==','||c=='['||c==']'||c=='=')c=' ';
    std::istringstream is(s);std::vector<std::string> v;std::string w;while(is>>w)v.push_back(w);return v;
}
static std::uint64_t natural(const std::string& s) {
    if(s.empty()||s[0]=='-'||s[0]=='+')throw std::runtime_error("Expected unsigned decimal number.");
    std::size_t n=0;auto v=std::stoull(s,&n,10);if(n!=s.size())throw std::runtime_error("Bad unsigned number.");return v;
}
static double real(const std::string& s) {std::size_t n=0;double v=std::stod(s,&n);if(n!=s.size()||!std::isfinite(v))throw std::runtime_error("Bad finite number.");return v;}
static int reg(const std::string& s) {
    if(s.size()<2 || (s[0]!='R'&&s[0]!='F'))throw std::runtime_error("Expected R0..R31 or F0..F31.");
    auto n=natural(s.substr(1));if(n>31)throw std::runtime_error("Register index out of range.");return static_cast<int>(n)+(s[0]=='F'?32:0);
}
Program parse_program(const std::string& path) {
    std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open assembly: "+path);
    Program p;std::string line;int lineno=0;std::set<std::string> directives;
    while(std::getline(f,line)) {
        ++lineno;auto t=tokens(strip(line));if(t.empty())continue;
        try {
            if(t[0]==".reg"||t[0]==".mem") {
                if(!p.code.empty())throw std::runtime_error("Directives must precede instructions.");
                if(t.size()!=3)throw std::runtime_error("Directive expects two arguments.");
                if(!directives.insert(t[0]+":"+t[1]).second)throw std::runtime_error("Duplicate initialization.");
                if(t[0]==".reg") {int r=reg(t[1]);if(r<32)p.initial[r].u=natural(t[2]);else p.initial[r].f=real(t[2]);}
                else {auto a=natural(t[1]);if(a>65535)throw std::runtime_error("Word address too large.");p.memory[static_cast<unsigned>(a)]=real(t[2]);}
                continue;
            }
            Instruction in;in.text=strip(line);
            if(t[0]=="IADD")in.op=Op::IADD;else if(t[0]=="FADD")in.op=Op::FADD;
            else if(t[0]=="FSUB")in.op=Op::FSUB;else if(t[0]=="FMUL")in.op=Op::FMUL;
            else if(t[0]=="FDIV")in.op=Op::FDIV;else if(t[0]=="LD")in.op=Op::LD;
            else throw std::runtime_error("Unsupported instruction: "+t[0]);
            if(t.size()!=(in.op==Op::LD?3U:4U))throw std::runtime_error("Wrong operand count.");
            in.dst=reg(t[1]);
            if(in.op==Op::LD) {auto a=natural(t[2]);if(a>65535)throw std::runtime_error("Word address too large.");in.address=static_cast<unsigned>(a);if(in.dst<32)throw std::runtime_error("LD loads only F registers.");if(!p.memory.count(in.address))throw std::runtime_error("LD address must be initialized.");}
            else {in.a=reg(t[2]);in.b=reg(t[3]);bool integer=in.op==Op::IADD;
                for(int r:{in.dst,in.a,in.b})if((r<32)!=integer)throw std::runtime_error("Operand register type mismatch.");}
            p.code.push_back(in);
        } catch(const std::exception& e) {throw std::runtime_error(path+":"+std::to_string(lineno)+": "+e.what());}
    }
    if(p.code.empty())throw std::runtime_error("Empty programs are not valid benchmark inputs.");
    return p;
}
Config load_config(const std::string& path) {
    Config c;std::ifstream f(path);if(!f)throw std::runtime_error("Cannot open config: "+path);
    std::map<std::string,double*> fields={
        {"frequency_ghz",&c.frequency_ghz},{"deadline_ns",&c.deadline_ns},{"period_ns",&c.period_ns},
        {"sleep_mw",&c.sleep_mw},{"control_scale",&c.control_scale},
        {"common_background_mw",&c.common_background_mw},{"inorder_control_mw",&c.inorder_control_mw},
        {"scoreboard_control_mw",&c.scoreboard_control_mw},{"tomasulo_fixed_mw",&c.tomasulo_fixed_mw},
        {"tomasulo_per_rs_mw",&c.tomasulo_per_rs_mw},{"instruction_pj",&c.instruction_pj},
        {"register_read_pj",&c.register_read_pj},{"register_write_pj",&c.register_write_pj},
        {"result_bus_pj",&c.result_bus_pj},{"delivery_pj",&c.delivery_pj},
        {"inorder_probe_pj",&c.inorder_probe_pj},{"scoreboard_probe_pj",&c.scoreboard_probe_pj},
        {"tomasulo_probe_pj",&c.tomasulo_probe_pj},{"inorder_transition_pj",&c.inorder_transition_pj},
        {"scoreboard_transition_pj",&c.scoreboard_transition_pj},{"tomasulo_transition_pj",&c.tomasulo_transition_pj},
        {"tag_bit_pj",&c.tag_bit_pj},{"register_bit_pj",&c.register_bit_pj}};
    std::string line;int n=0;std::set<std::string> keys;
    while(std::getline(f,line)) {++n;auto t=tokens(strip(line));if(t.empty())continue;
        if(t.size()!=2)throw std::runtime_error(path+":"+std::to_string(n)+": expected key = value.");
        if(!keys.insert(t[0]).second)throw std::runtime_error("Duplicate config key: "+t[0]);
        if(auto it=fields.find(t[0]);it!=fields.end())*it->second=real(t[1]);
        else if(t[0]=="rs_entries") {auto x=natural(t[1]);if(x>64)throw std::runtime_error("RS size out of range.");c.rs_entries=static_cast<int>(x);}
        else if(t[0]=="max_cycles")c.max_cycles=natural(t[1]);
        else {bool found=false;for(int i=0;i<6;++i) {
            auto op=static_cast<Op>(i);
            if(t[0]==std::string("latency_")+op_name(op)) {auto x=natural(t[1]);if(x>100000)throw std::runtime_error("Latency too large.");c.latency[i]=static_cast<unsigned>(x);found=true;}
            if(t[0]==std::string("energy_")+op_name(op)+"_pj") {c.operation_pj[i]=real(t[1]);found=true;}
        } if(!found)throw std::runtime_error("Unknown config key: "+t[0]);}
    }
    c.validate();return c;
}
std::array<Value,NREG> interpret(const Program& p) {
    auto r=p.initial;
    for(const auto& in:p.code)r[in.dst]=evaluate(in,in.a<0?Value{}:r[in.a],in.b<0?Value{}:r[in.b],p);
    return r;
}
Policy policy_for(Mode m) {
    if(m==Mode::InOrder)return {m,plan_inorder,bind_inorder,publish_inorder};
    if(m==Mode::Scoreboard)return {m,plan_scoreboard,bind_scoreboard,publish_scoreboard};
    return {m,plan_tomasulo,bind_tomasulo,publish_tomasulo};
}
void EnergyLedger::add(const std::string& key,std::uint64_t n,double pj) {
    if(n==0)return;
    auto& e=events[key];if(e.count!=0 && std::abs(e.pj_each-pj)>1e-12)throw std::logic_error("Energy unit changed mid-run.");
    e.pj_each=pj;e.count+=n;
}
double EnergyLedger::dynamic_pj() const {double d=0;for(const auto& kv:events)d+=kv.second.count*kv.second.pj_each;return d;}
double Result::period_pj() const {if(!period_met())throw std::runtime_error("Task exceeds period; no valid sleep interval.");return task_pj()+cfg.sleep_mw*(cfg.period_ns-elapsed_ns());}
} // namespace p1
