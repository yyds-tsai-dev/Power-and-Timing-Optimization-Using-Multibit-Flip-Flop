// Toy set-partitioning MILP smoke test for Route A build chain.
// Scenario: 5 FFs must each be covered by exactly one selected group.
// Candidates:  five 1-bit singletons + three pruned 2-bit pairs.
//   c0: {0}       cost = 10
//   c1: {1}       cost = 10
//   c2: {2}       cost = 10
//   c3: {3}       cost = 10
//   c4: {4}       cost = 10
//   c5: {0,1}     cost = 14   (cheaper than 2x singleton)
//   c6: {2,3}     cost = 15   (cheaper than 2x singleton)
//   c7: {3,4}     cost = 16   (not cheaper than c6 + c4-singleton path)
// Optimal: c5 + c6 + c4 = 14 + 15 + 10 = 39.
// Verifies: MPSolver construction, binary vars, set-partitioning constraints,
// objective min, solve, extract.

#include <iostream>
#include <memory>
#include <vector>

#include "ortools/linear_solver/linear_solver.h"

using operations_research::MPConstraint;
using operations_research::MPObjective;
using operations_research::MPSolver;
using operations_research::MPVariable;

int main() {
    const int N_FFS = 5;
    struct Cand {
        std::vector<int> ffs;
        double cost;
    };
    std::vector<Cand> cands = {
        {{0},    10},
        {{1},    10},
        {{2},    10},
        {{3},    10},
        {{4},    10},
        {{0, 1}, 14},
        {{2, 3}, 15},
        {{3, 4}, 16},
    };

    std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("CBC"));
    if (!solver) {
        std::cerr << "CBC solver unavailable." << std::endl;
        return 1;
    }

    std::vector<MPVariable*> y(cands.size(), nullptr);
    for (size_t i = 0; i < cands.size(); i++) {
        y[i] = solver->MakeBoolVar("y_" + std::to_string(i));
    }

    // Set-partition: each FF covered exactly once.
    for (int ff = 0; ff < N_FFS; ff++) {
        MPConstraint* c = solver->MakeRowConstraint(1.0, 1.0, "cover_" + std::to_string(ff));
        for (size_t i = 0; i < cands.size(); i++) {
            for (int f : cands[i].ffs) if (f == ff) c->SetCoefficient(y[i], 1);
        }
    }

    MPObjective* obj = solver->MutableObjective();
    for (size_t i = 0; i < cands.size(); i++) obj->SetCoefficient(y[i], cands[i].cost);
    obj->SetMinimization();

    const auto status = solver->Solve();
    if (status != MPSolver::OPTIMAL) {
        std::cerr << "Solve failed, status=" << status << std::endl;
        return 2;
    }

    std::cout << "objective=" << obj->Value() << "\n";
    std::cout << "selected:";
    for (size_t i = 0; i < cands.size(); i++) {
        if (y[i]->solution_value() > 0.5) {
            std::cout << " c" << i << "{";
            for (size_t j = 0; j < cands[i].ffs.size(); j++) {
                if (j) std::cout << ",";
                std::cout << cands[i].ffs[j];
            }
            std::cout << "}";
        }
    }
    std::cout << "\n";

    const double expected = 39.0;
    if (std::abs(obj->Value() - expected) > 1e-6) {
        std::cerr << "FAIL: expected " << expected << " got " << obj->Value() << std::endl;
        return 3;
    }
    std::cout << "PASS" << std::endl;
    return 0;
}
