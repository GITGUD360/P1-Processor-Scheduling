#include "sim.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace p1;
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="--help") {
            std::cout<<"Usage: sim MODE program.asm config.cfg output_prefix [--trace]\n"
                <<"MODE: inorder | scoreboard | tomasulo | functional\n"
                <<"Guided v2.0: all models run immediately, but starter results are deliberately unverified.\n";return 0;
        }
        if(argc<5 || argc>6 || (argc==6 && std::string(argv[5])!="--trace"))
            throw std::runtime_error("Usage: sim MODE program.asm config.cfg output_prefix [--trace]");
        auto program=parse_program(argv[2]);auto cfg=load_config(argv[3]);std::string prefix=argv[4];
        if(std::string(argv[1])=="functional") {
            auto regs=interpret(program);auto parent=std::filesystem::path(prefix).parent_path();
            if(!parent.empty())std::filesystem::create_directories(parent);
            std::ofstream out(prefix+"_registers.csv");if(!out)throw std::runtime_error("Cannot write functional output.");
            out<<std::setprecision(17)<<"register,value\n";
            for(int i=0;i<NREG;++i){out<<reg_name(i)<<',';if(i<32)out<<regs[i].u;else out<<regs[i].f;out<<'\n';}
            std::cout<<"Functional interpretation complete: "<<program.code.size()<<" instructions.\n"
                <<"No simulated timing or energy is attributed to this oracle.\n";return 0;
        }
        std::cout<<"Project 1 | Guided release v2.0 | target model v1.0\n";
        Mode mode=parse_mode(argv[1]);std::ofstream trace;
        if(argc==6) {
            auto parent=std::filesystem::path(prefix).parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
            trace.open(prefix+"_trace.csv");if(!trace)throw std::runtime_error("Cannot open trace file.");
        }
        Result result=simulate(program,cfg,policy_for(mode),argc==6?&trace:nullptr);
        write_outputs(prefix,program,result);print_summary(std::cout,result);return 0;
    } catch(const std::exception& e) {std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
