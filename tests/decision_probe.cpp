#include "decisions.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace p1;
int main() {
    try {
        std::string id; unsigned a,b,c;
        while(std::cin>>id>>a>>b>>c) {
            bool out=false;
            if(id=="G1") out=inorder_can_launch(a!=0,b!=0,c!=0);
            else if(id=="G2") out=scoreboard_can_issue(a!=0,b,c!=0);
            else if(id=="G3") out=scoreboard_war_blocks(a!=0,b!=0);
            else if(id=="G4") out=tomasulo_can_issue(a!=0,b!=0,c!=0);
            else if(id=="G5") out=tomasulo_wait_for_source(a);
            else if(id=="G6") out=tomasulo_write_register(a,b);
            else throw std::runtime_error("Unknown decision ID");
            std::cout<<(out?1:0)<<'\n';
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
