#include "Manager.h"
#include <chrono>
#include <iostream>
#include <vector>

#define STAGE(label, expr) do {                                              \
    auto _s = std::chrono::high_resolution_clock::now();                     \
    expr;                                                                    \
    auto _e = std::chrono::high_resolution_clock::now();                     \
    double _ms = std::chrono::duration<double, std::milli>(_e - _s).count(); \
    std::cout << "[STAGE] " << label << " " << _ms << " ms" << std::endl;    \
} while (0)

int main(int argc, char *argv[]){

    std::cout << std::fixed << std::setprecision(2);

    if (argc < 2){
        std::cerr << "Usage: " << argv[0] << " <inputfile> <outputfile>" << std::endl;
        return EXIT_FAILURE;
    }

    // PRODUCTION=1 skips all debug overhead (dumpVisual, mid-stage cost tables,
    // evaluator fork, checker). Use for submission / runtime-factor benchmarking.
    const bool production = std::getenv("PRODUCTION") && std::atoi(std::getenv("PRODUCTION"));
    bool cost_verbose = !production;
    // STAGE_COST=1: print per-stage cost (non-verbose) even in production mode.
    // Used to bisect-locate inter-binary score drift.
    const bool stageCost = std::getenv("STAGE_COST") && std::atoi(std::getenv("STAGE_COST"));
    // ACCURATE_TNS=1: use BFS-based accurate TNS in getOverallCost() for reporting.
    // Does NOT affect optimization stages (getSlack() is unchanged). Read in Manager::getOverallCost().
    auto printStageCost = [&](const char* tag, Manager &m){
        if(!stageCost) return;
        double c = m.getOverallCost(false, 0);
        std::cerr << "[STAGE_COST] " << tag << " cost=" << c << "\n";
    };

    auto _all_s = std::chrono::high_resolution_clock::now();

    Manager mgr;
    // Optional env-driven knobs for experimentation.
    if(const char* e = std::getenv("SLACK_REDIST_MODE")) mgr.param.SLACK_REDIST_MODE = std::atoi(e);
    if(const char* e = std::getenv("SLACK_OVERSHOOT_WEIGHT")) mgr.param.SLACK_OVERSHOOT_WEIGHT = std::atof(e);
    STAGE("parse",             mgr.parse(argv[1]));
    STAGE("libScoring",        mgr.libScoring());
    if(!production) mgr.getOverallCost(cost_verbose, 0);
    STAGE("preprocess",        mgr.preprocess());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("Preprocessor.out"); }

    STAGE("preLegalize",       mgr.preLegalize());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("PreLegalize.out"); }

    STAGE("slackRedist",       mgr.computeSlackRedistribution());
    STAGE("timingPreReloc",    mgr.timingPreRelocation());

    if(std::getenv("ACCURATE_BANKING") && std::atoi(std::getenv("ACCURATE_BANKING")))
        STAGE("refreshArr(pre-bank)", mgr.refreshArrivalCorrections());
    STAGE("banking",           mgr.banking());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("Banking.out"); }
    printStageCost("banking", mgr);

    STAGE("postBankingOpt",    mgr.postBankingOptimize());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("PostCG.out"); }
    printStageCost("postBankingOpt", mgr);

    STAGE("legalize",          mgr.legalize());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); mgr.dumpVisual("Legalize.out"); mgr.checker(); }
    printStageCost("legalize", mgr);

    STAGE("postLGDecluster",   mgr.postLGDecluster());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); }
    printStageCost("postLGDecluster", mgr);

    STAGE("unbankRebank",      mgr.unbankRebank());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); }
    printStageCost("unbankRebank", mgr);

    STAGE("unbankRebankGlobal", mgr.unbankRebankGlobal());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); }
    printStageCost("unbankRebankGlobal", mgr);

    STAGE("postLGResynth",     mgr.postLGResynth());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); }
    printStageCost("postLGResynth", mgr);

    if(!std::getenv("SKIP_DP") || std::string(std::getenv("SKIP_DP")) == "0")
        STAGE("detailplacement",   mgr.detailplacement());
    printStageCost("detailplacement", mgr);
    if(!production){
        mgr.getOverallCost(cost_verbose, 1);
        mgr.dumpVisual("DetailPlacement.out");
        mgr.checker();
    }

    STAGE("dump",              mgr.dump(argv[2]));

    auto _all_e = std::chrono::high_resolution_clock::now();
    double _total_ms = std::chrono::duration<double, std::milli>(_all_e - _all_s).count();
    std::cout << "[STAGE] TOTAL " << _total_ms << " ms" << std::endl;
    return 0;
}
