#include "sim.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace p1;
static void check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){try{
    auto c=load_config("config/baseline.cfg");check(std::abs(c.dt_ns()-0.25)<1e-12,"4 GHz clock conversion");
    check(c.tag_bits()==4,"8 RS + zero require 4-bit tags");c.rs_entries=4;check(c.tag_bits()==3,"4 RS tag width");
    c.rs_entries=16;check(c.tag_bits()==5,"16 RS tag width");
    auto p=parse_program("workloads/iot_serial.asm");check(p.code.size()==96,"frozen instruction count");
    auto r=interpret(p);check(r[1].u==16,"sample count");
    double state=4;for(int j=0;j<16;++j)state=((state+2*(j+1))*0.5+1)/2;
    check(std::abs(r[32].f-state)<1e-12,"independent recurrence check");
    auto par=interpret(parse_program("workloads/iot_four_streams.asm"));
    for(int k=0;k<4;++k){double x=4;for(int j=k;j<16;j+=4)x=((x+2*(j+1))*0.5+1)/2;check(std::abs(par[32+k].f-x)<1e-12,"four-stream functional check");}
    check(interpret(parse_program("tests/cases/unsigned_wrap.asm"))[1].u==0,"unsigned arithmetic");
    EnergyLedger e;e.add("test",4,2.5);check(e.dynamic_pj()==10,"event energy sum");
    Result test;test.mode=Mode::InOrder;test.cfg=Config{};test.cycles=40;test.energy=e;
    check(test.elapsed_ns()==10,"40 cycles -> 10 ns");check(test.background_pj()==200,"20 mW * 10 ns = 200 pJ");
    check(test.task_pj()==210,"background plus dynamic, no extra time multiplication");
    check(std::abs(test.task_average_mw()-21)<1e-12,"energy/time -> power");
    check(std::abs(test.period_pj()-309)<1e-12,"add sleep energy over equal period");
    test.cfg.period_ns=1;check(!test.period_met(),"period overflow");
    bool threw=false;try{(void)test.period_pj();}catch(const std::exception&){threw=true;}check(threw,"never use negative sleep time");
    std::cout<<"Framework tests: PASS (parser, ISA, oracle, clock, units, tags, energy, period).\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
