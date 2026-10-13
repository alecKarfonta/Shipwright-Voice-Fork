#include "QuestCatalog.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
int main(int argc,char** argv) {
    using namespace ARPG::Quests;
    auto catalog=nlohmann::json::array();
    for(const auto& d:Catalog) {
        auto objectives=nlohmann::json::array();
        for(const auto& o:d.objectives) objectives.push_back({{"kind",int(o.kind)},
            {"count",o.count},{"scene",o.scene},{"ordered",Ordered(o)},
            {"nodes",Physical(o)||o.kind==Kind::Pack}});
        catalog.push_back({{"id",d.id},{"title",d.title},{"chapter",Chapters[d.chapter]},{"objectives",objectives}});
    }
    if(argc>1) {std::ifstream file(argv[1]);nlohmann::json shipped;file>>shipped;return shipped==catalog?0:1;}
    std::cout<<catalog.dump(2)<<"\n";
}
