#ifndef _MILP_BANKING_H_
#define _MILP_BANKING_H_

// Route A: Window-local ILP MBFF Banking.
//
// [RA/2] — Candidate enumerator (windowless prototype).
// This header exposes a standalone enumerator that, given a subset of FFs,
// builds the set-partition candidate pool for the MILP:
//
//   - 1-bit singleton candidates (baseline "no banking" fallback, cost = 0)
//   - pruned 2-bit pair candidates (pairwise HPWL <= MULT * row_height)
//   - multi-coord variants per (ffs, cell) group (median + FindTop2 legal coords)
//
// Costs are derived from the existing Banking::CostCompare, negated so the
// MILP is a plain min-cost set-partition:
//
//   min Σ cost_i * y_i   s.t.   Σ_{i: ff ∈ group_i} y_i = 1  ∀ ff in window
//                                y_i ∈ {0, 1}
//
// This file does NOT pull in OR-tools; MPSolver wiring is added in [RA/3].
//
// No USE_ORTOOLS gate is required on this header — it is pure enumeration.

#include "Coor.h"
#include <vector>
#include <string>

class FF;
class Cell;
class Manager;
class Banking;

struct MILPCandidate {
    std::vector<FF*> ffs;       // covered FFs (size 1 = singleton, 2 = pair)
    Cell*            cell;      // target library cell (must match ffs.size() bits)
    Coor             coord;     // proposed placement (lower-left)
    double           cost;      // -CostCompare gain; singleton = 0
    int              variantId; // index within (ffs, cell) group (0-based)

    MILPCandidate() : cell(nullptr), coord(0.0, 0.0), cost(0.0), variantId(0) {}

    int bits() const { return static_cast<int>(ffs.size()); }
};

class MILPBanking {
public:
    MILPBanking(Manager& mgr, Banking& banking);
    ~MILPBanking() = default;

    // Builds the candidate list for the given FF subset. Appends into `out`.
    // Reads env knobs (once per call):
    //   MILP_PRUNE_HPWL_MULT  default 2.0  (2-bit pair HPWL <= MULT * rowHeight)
    //   MILP_QUAD_HPWL_MULT   default 3.0  (4-bit max pairwise HPWL <= MULT * rh)
    //   MILP_BITS_CAP         default 2    (max bit size; 2 or 4 in v1)
    //   MILP_MULTICOORD       default 1    (0 = median-only; 1 = +FindTop2 variants)
    void enumerate(const std::vector<FF*>& ffs,
                   std::vector<MILPCandidate>& out);

    // Diagnostic dump, env-gated by caller (MILP_PROTOTYPE=1 in Banking.cpp).
    // Reads:
    //   MILP_PROTOTYPE_N       default 100   (subset FF count; 0 = all)
    //   MILP_PROTOTYPE_SAMPLE  default "head" (one of: head | tail | random)
    //   MILP_PROTOTYPE_SEED    default 0     (for random sampling)
    //   MILP_SOLVE             default 0     (1 = also run Blossom + MILP compare)
    //
    // Writes a multi-line summary to std::cerr with the [MILP_PROTO] prefix.
    // Does NOT mutate Manager / legalizer state.
    void prototypeDump();

    // Solver comparison result.
    struct SolveResult {
        std::vector<int> selected;   // indices into cands
        double total_cost;            // Σ cands[selected].cost
        double elapsed_ms;
        int n_pairs;                  // bits == 2
        int n_quads;                  // bits == 4
        int n_singletons;             // bits == 1
        bool ok;                      // solve succeeded (OPTIMAL for MILP)
        std::string backend;          // "blossom" | "cbc" | "scip" | "n/a"

        SolveResult()
            : total_cost(0.0), elapsed_ms(0.0), n_pairs(0), n_quads(0),
              n_singletons(0), ok(false), backend("n/a") {}
    };

    // Run max-weight matching on the pair candidates (picks best coord variant
    // per group, weight = -cost, negative-cost edges dropped). For each FF not
    // matched, falls back to the singleton. Always available (LEMON-only).
    SolveResult runBlossom(const std::vector<MILPCandidate>& cands,
                           const std::vector<FF*>& subset) const;

    // Run the set-partition MILP via OR-tools (CBC by default). When
    // USE_ORTOOLS is not defined at compile time, returns a failed result
    // with backend = "n/a". If `warmStart` is non-empty, its entries (bool
    // per candidate) are passed to MPSolver::SetHint for primal-heuristic
    // seeding — typically from a prior runBlossom() selection.
    SolveResult runMILP(const std::vector<MILPCandidate>& cands,
                        const std::vector<FF*>& subset,
                        const std::vector<int>& warmStart = {}) const;

    // Route A production path: partition all FFs by (clkIdx, y-band),
    // solve window-local set-partition MILP (warm-started by Blossom),
    // and materialize selected candidates via mgr.bankFF / UpdateRows.
    //
    // Env knobs:
    //   MILP_ROWS_PER_WINDOW   default 8
    //   MILP_WIN_TIME_MS       default 5000   per-window MILP time cap (ms)
    //   MILP_WARM_START        default 1      0 = no MIP-start
    //   MILP_BITS_CAP          default 2      max group size to enumerate
    //   (+ all other MILP_* env knobs from enumerate())
    //
    // Returns the number of MBFFs materialized (pairs + quads).
    int runProduction();

    // Hybrid Route A (Stage 1+) — per-window MILP improver that runs AFTER
    // Layer 0 matching completes. For each window: snapshot → enumerate →
    // blossom/MILP → commit via bankFF_deferred → (Stage 2 safety-gate) →
    // commit-finalize OR rollback. Layer 0 remains untouched if every window
    // ends in rollback. Stage 1 has no safety gate yet: it is gated on
    //   HYBRID_ROLLBACK_ALWAYS=1  → force rollback of every window, so the
    //                               final score must be Layer 0 bit-exact.
    // Env knobs (Stage 1):
    //   HYBRID_ROWS_PER_WINDOW   default 8
    //   HYBRID_WIN_MS            default 300    per-window MILP time cap
    //   HYBRID_TOTAL_MS          default 120000 full Layer 1 wall cap
    //   HYBRID_PRUNE_HPWL_MULT   default 2.0
    //   HYBRID_QUAD_HPWL_MULT    default 2.5
    //   HYBRID_BITS_CAP          default 2      (2 = pair-only in Stage 1)
    //   HYBRID_ROLLBACK_ALWAYS   default 0      1 = force rollback (smoke)
    int runHybrid();

private:
    Manager& mgr;
    Banking& banking;

    double rowHeight() const;
    std::vector<Cell*> cellsForBits(int bits) const;
    std::vector<Coor> coordVariantsForGroup(const std::vector<FF*>& ffs,
                                            Cell* cell,
                                            bool multiCoord) const;

    // Helpers for the dump.
    static std::vector<FF*> sampleSubset(const std::vector<FF*>& all,
                                         size_t n,
                                         const std::string& mode,
                                         unsigned seed);
};

#endif
