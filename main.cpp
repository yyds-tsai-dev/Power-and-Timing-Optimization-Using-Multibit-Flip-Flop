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
    const bool ntuFlow = std::getenv("NTU_FLOW") && std::atoi(std::getenv("NTU_FLOW"));
    if(ntuFlow){
        // NTU_FLOW auto-enables TIMING_PRELOC unless explicitly set to 0
        if(!std::getenv("TIMING_PRELOC")) setenv("TIMING_PRELOC", "1", 0);
    }
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

    if(!std::getenv("SKIP_CG") || std::string(std::getenv("SKIP_CG")) == "0")
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

    if(std::getenv("IB_BFS") && std::atoi(std::getenv("IB_BFS")))
        STAGE("refreshArr(pre-IB)", mgr.refreshArrivalCorrections());
    STAGE("iterBanking",       mgr.iterativeBankingLoop());
    if(!production){ mgr.getOverallCost(cost_verbose, 0); }
    printStageCost("iterBanking", mgr);

    if(std::getenv("BFS_PRE_DP") && std::atoi(std::getenv("BFS_PRE_DP")))
        STAGE("refreshArr(pre-DP)", mgr.refreshArrivalCorrections());
    if(!std::getenv("SKIP_DP") || std::string(std::getenv("SKIP_DP")) == "0")
        STAGE("detailplacement",   mgr.detailplacement());
    printStageCost("detailplacement", mgr);

    // DP_ROUNDS: run additional DP passes with BFS-refreshed timing between each.
    {
        int dpRounds = 0;
        if(const char* e = std::getenv("DP_ROUNDS")) dpRounds = std::atoi(e);
        for(int r = 0; r < dpRounds; r++){
            STAGE("refreshArr(dp-round)", mgr.refreshArrivalCorrections());
            STAGE("dp-round",             mgr.detailplacement());
        }
    }
    // Alternate {RELOC; CRIT_SWAP; BIT_REPAIR} to a joint fixpoint. ALT_ROUNDS=1 (default)
    // reproduces the single-pass behavior byte-exactly. With the build-once incremental
    // engine, the running incrTNS_ is shared across passes (each operator's moves enable
    // the others'); all accepts are faithful-TNS-monotone at fixed power/area => no regress.
    {
        int altRounds = 1;
        if(const char* e = std::getenv("ALT_ROUNDS")) altRounds = std::atoi(e);
        const char* ckptPfx = std::getenv("EVAL_CHECKPOINT");   // prefix path; dumps solution per stage
        auto ckpt = [&](const char* name, int alt){
            if(!ckptPfx) return;
            auto now = std::chrono::high_resolution_clock::now();
            double ms = std::chrono::duration<double, std::milli>(now - _all_s).count();
            std::string f = std::string(ckptPfx) + "_a" + std::to_string(alt) + "_" + name + "_" + std::to_string((long)ms) + ".out";
            mgr.dump(f);
            std::cerr << "[CKPT] " << name << " alt=" << alt << " t_ms=" << (long)ms
                      << " oracleCost=" << std::fixed << mgr.oracleCostSnapshot() << " file=" << f << "\n";
        };
        ckpt("start", -1);
        for(int alt = 0; alt < altRounds; alt++){
            if(std::getenv("RELOC") && std::atoi(std::getenv("RELOC"))){
                STAGE("refreshArr(pre-reloc)", mgr.refreshArrivalCorrections());
                STAGE("timingReloc",           mgr.timingDrivenRelocation()); ckpt("timingReloc", alt);
            }
            if(std::getenv("CRIT_SWAP") && std::atoi(std::getenv("CRIT_SWAP"))){
                STAGE("critSwapRefine", mgr.criticalPathSwapRefine()); ckpt("critSwap", alt);
            }
            if(std::getenv("BIT_REPAIR") && std::atoi(std::getenv("BIT_REPAIR"))){
                STAGE("bitRepair", mgr.bitRepairRefine()); ckpt("bitRepair", alt);
            }
            if(std::getenv("ORACLE_EJECT") && std::atoi(std::getenv("ORACLE_EJECT"))){
                STAGE("oracleEject", mgr.oracleEjectRefine()); ckpt("eject", alt);
            }
            if(std::getenv("ORACLE_REBANK") && std::atoi(std::getenv("ORACLE_REBANK"))){
                STAGE("oracleRebank", mgr.oracleRebankRefine()); ckpt("rebank", alt);
            }
        }
    }
    if(std::getenv("DENSITY_REPAIR") && std::atoi(std::getenv("DENSITY_REPAIR")))
        STAGE("densityRepair", mgr.densityRepairRefine());
    if(std::getenv("EVAL_CHECKPOINT")){
        auto now = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(now - _all_s).count();
        std::string f = std::string(std::getenv("EVAL_CHECKPOINT")) + "_final_" + std::to_string((long)ms) + ".out";
        mgr.dump(f);
        std::cerr << "[CKPT] final t_ms=" << (long)ms << " oracleCost=" << std::fixed << mgr.oracleCostSnapshot() << " file=" << f << "\n";
    }
    if(std::getenv("EGR") && std::atoi(std::getenv("EGR")))
        STAGE("evalRefinement", mgr.evaluatorRefinement(argv[1]));
    if(!production){
        mgr.getOverallCost(cost_verbose, 1);
        mgr.dumpVisual("DetailPlacement.out");
        mgr.checker();
    }

    if(std::getenv("TNS_ORACLE_VALIDATE"))
        STAGE("oracleValidate", mgr.validateTNSOracle(true));

    STAGE("dump",              mgr.dump(argv[2]));

    auto _all_e = std::chrono::high_resolution_clock::now();
    double _total_ms = std::chrono::duration<double, std::milli>(_all_e - _all_s).count();
    std::cout << "[STAGE] TOTAL " << _total_ms << " ms" << std::endl;
    return 0;
}
