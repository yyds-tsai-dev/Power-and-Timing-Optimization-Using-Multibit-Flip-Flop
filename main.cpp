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

    bool cost_verbose = true;

    auto _all_s = std::chrono::high_resolution_clock::now();

    Manager mgr;
    STAGE("parse",             mgr.parse(argv[1]));
    STAGE("libScoring",        mgr.libScoring());
    mgr.getOverallCost(cost_verbose, 0);
    STAGE("preprocess",        mgr.preprocess());
    mgr.getOverallCost(cost_verbose, 0);
    mgr.dumpVisual("Preprocessor.out");

    // Phase 1.5: mid-stage external evaluator calls are dropped (runEvaluator=0).
    // The external evaluator forks preliminary-evaluator on every call (~25-45s on big
    // cases) which dominated wall time before. Only the final call after DetailPlacement
    // still runs it so the [EVALUATOR] Score remains visible for submission sanity.
    STAGE("preLegalize",       mgr.preLegalize());
    mgr.getOverallCost(cost_verbose, 0);
    mgr.dumpVisual("PreLegalize.out");

    STAGE("banking",           mgr.banking());
    mgr.getOverallCost(cost_verbose, 0);
    mgr.dumpVisual("Banking.out");

    STAGE("postBankingOpt",    mgr.postBankingOptimize());
    mgr.getOverallCost(cost_verbose, 0);
    mgr.dumpVisual("PostCG.out");

    STAGE("legalize",          mgr.legalize());
    mgr.getOverallCost(cost_verbose, 0);
    mgr.dumpVisual("Legalize.out");
    mgr.checker();

    STAGE("detailplacement",   mgr.detailplacement());
    mgr.getOverallCost(cost_verbose, 1);
    mgr.dumpVisual("DetailPlacement.out");
    mgr.checker();

    STAGE("dump",              mgr.dump(argv[2]));

    auto _all_e = std::chrono::high_resolution_clock::now();
    double _total_ms = std::chrono::duration<double, std::milli>(_all_e - _all_s).count();
    std::cout << "[STAGE] TOTAL " << _total_ms << " ms" << std::endl;
    return 0;
}
