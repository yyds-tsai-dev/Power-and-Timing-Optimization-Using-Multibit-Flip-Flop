#include "MILPBanking.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <lemon/matching.h>
#include <lemon/smart_graph.h>

#include "Banking.h"
#include "Cell.h"
#include "Cell_Library.h"
#include "FF.h"
#include "Legalizer.h"
#include "Manager.h"
#include "Util.h"

#ifdef USE_ORTOOLS
#include "ortools/linear_solver/linear_solver.h"
#endif

namespace {
inline double envDouble(const char* key, double fallback) {
    const char* s = std::getenv(key);
    return s ? std::atof(s) : fallback;
}
inline int envInt(const char* key, int fallback) {
    const char* s = std::getenv(key);
    return s ? std::atoi(s) : fallback;
}
inline std::string envStr(const char* key, const char* fallback) {
    const char* s = std::getenv(key);
    return s ? std::string(s) : std::string(fallback);
}
} // namespace

MILPBanking::MILPBanking(Manager& mgr_, Banking& banking_)
    : mgr(mgr_), banking(banking_) {}

double MILPBanking::rowHeight() const {
    auto it = mgr.Bit_FF_Map.find(1);
    if (it == mgr.Bit_FF_Map.end() || it->second.empty()) {
        // Fall back: any cell in library.
        for (const auto& bitLib : mgr.Bit_FF_Map) {
            if (!bitLib.second.empty()) return bitLib.second[0]->getH();
        }
        return 0.0;
    }
    double h = DBL_MAX;
    for (Cell* c : it->second) {
        if (c->getH() < h) h = c->getH();
    }
    return (h == DBL_MAX) ? 0.0 : h;
}

std::vector<Cell*> MILPBanking::cellsForBits(int bits) const {
    auto it = mgr.Bit_FF_Map.find(bits);
    if (it == mgr.Bit_FF_Map.end()) return {};
    return it->second;
}

std::vector<Coor> MILPBanking::coordVariantsForGroup(
    const std::vector<FF*>& ffs, Cell* cell, bool multiCoord) const {
    std::vector<Coor> out;
    if (ffs.empty() || !cell) return out;

    // Primary seed: median of FF.getNewCoor() (L1-optimal target).
    std::vector<double> xs, ys;
    xs.reserve(ffs.size());
    ys.reserve(ffs.size());
    for (FF* f : ffs) {
        xs.push_back(f->getNewCoor().x);
        ys.push_back(f->getNewCoor().y);
    }
    std::sort(xs.begin(), xs.end());
    std::sort(ys.begin(), ys.end());
    Coor seed(xs[xs.size() / 2], ys[ys.size() / 2]);
    out.push_back(seed);

    if (!multiCoord || !mgr.legalizer) return out;

    // Add FindTop2 legal-slot variants near seed. FindTop2 is read-only.
    auto top2 = mgr.legalizer->FindTop2LegalCoors(seed, cell);
    auto quantize = [](const Coor& c) {
        // Coarse grid: 1 site-unit; we just need near-duplicate filtering.
        return std::make_pair(static_cast<long long>(std::llround(c.x)),
                              static_cast<long long>(std::llround(c.y)));
    };
    std::unordered_set<long long> seen;
    auto key = [&](const Coor& c) {
        auto q = quantize(c);
        return (q.first << 32) ^ (q.second & 0xffffffffll);
    };
    seen.insert(key(seed));
    for (const auto& lc : top2) {
        long long k = key(lc.coor);
        if (seen.insert(k).second) out.push_back(lc.coor);
    }
    return out;
}

void MILPBanking::enumerate(const std::vector<FF*>& ffs,
                            std::vector<MILPCandidate>& out) {
    if (ffs.empty()) return;

    const double prune_mult = envDouble("MILP_PRUNE_HPWL_MULT", 2.0);
    const double quad_mult  = envDouble("MILP_QUAD_HPWL_MULT", 3.0);
    const int bits_cap = std::max(1, envInt("MILP_BITS_CAP", 2));
    const bool multiCoord = envInt("MILP_MULTICOORD", 1) != 0;

    const double rh = rowHeight();
    const double pair_hpwl_cap = prune_mult * rh;
    const double quad_hpwl_cap = quad_mult * rh;

    // 1-bit singletons: cost = 0 (design-unchanged baseline).
    // Each FF MUST be coverable, so we emit one singleton per FF using its
    // current cell. No gain/loss — these are fallbacks.
    out.reserve(out.size() + ffs.size());
    for (FF* f : ffs) {
        MILPCandidate c;
        c.ffs = {f};
        c.cell = f->getCell();
        c.coord = f->getNewCoor();
        c.cost = 0.0;
        c.variantId = 0;
        out.push_back(std::move(c));
    }
    if (bits_cap < 2) return;

    // 2-bit pruned pairs. O(N^2) is fine for windowless prototype with
    // MILP_PROTOTYPE_N ≈ 100; the production path will partition into
    // row-band windows before enumerating.
    std::vector<Cell*> cells2 = cellsForBits(2);
    if (cells2.empty()) return;

    // Record surviving (i,j) pairs so 4-bit enumeration can reuse them.
    struct PairIdx { size_t i, j; };
    std::vector<PairIdx> goodPairs;

    const size_t N = ffs.size();
    for (size_t i = 0; i < N; ++i) {
        FF* a = ffs[i];
        if (!a || !a->getCell()) continue;
        if (a->getCell()->getBits() != 1) continue;       // 2-bit cell needs 1-bit atoms
        int clkA = a->getClkIdx();
        for (size_t j = i + 1; j < N; ++j) {
            FF* b = ffs[j];
            if (!b || !b->getCell()) continue;
            if (b->getCell()->getBits() != 1) continue;
            if (b->getClkIdx() != clkA) continue;
            if (HPWL(a->getNewCoor(), b->getNewCoor()) > pair_hpwl_cap) continue;

            std::vector<FF*> pair = {a, b};
            for (Cell* cell : cells2) {
                auto variants = coordVariantsForGroup(pair, cell, multiCoord);
                int vid = 0;
                for (const Coor& coord : variants) {
                    double gain = banking.CostCompare(coord, cell, pair);
                    MILPCandidate c;
                    c.ffs = pair;
                    c.cell = cell;
                    c.coord = coord;
                    c.cost = -gain;
                    c.variantId = vid++;
                    out.push_back(std::move(c));
                }
            }
            goodPairs.push_back({i, j});
        }
    }
    if (bits_cap < 4) return;

    // 4-bit pruned quadruples, seeded from 2-bit pair×pair combinations.
    // Two disjoint pairs (i,j) and (k,l) fuse into a 4-tuple if all 4 pairwise
    // HPWLs (a-c, a-d, b-c, b-d) stay <= quad_hpwl_cap. Dedup by sorted-FF-ptr.
    std::vector<Cell*> cells4 = cellsForBits(4);
    if (cells4.empty()) {
        std::cerr << "[MILP_PROTO] no 4-bit cells in library; skipping 4-bit enumeration\n";
        return;
    }

    size_t quad_tried = 0, quad_reject_clk = 0, quad_reject_hpwl = 0,
           quad_reject_dup = 0, quad_accepted = 0;
    std::unordered_set<uint64_t> seenQuad;
    auto quadKey = [](FF* a, FF* b, FF* c, FF* d) -> uint64_t {
        std::array<FF*, 4> t{a, b, c, d};
        std::sort(t.begin(), t.end());
        uint64_t h = 1469598103934665603ull;
        for (FF* p : t) {
            h ^= reinterpret_cast<uintptr_t>(p);
            h *= 1099511628211ull;
        }
        return h;
    };

    for (size_t p1 = 0; p1 < goodPairs.size(); ++p1) {
        const size_t i = goodPairs[p1].i;
        const size_t j = goodPairs[p1].j;
        FF* a = ffs[i];
        FF* b = ffs[j];
        int clkAB = a->getClkIdx();
        for (size_t p2 = p1 + 1; p2 < goodPairs.size(); ++p2) {
            const size_t k = goodPairs[p2].i;
            const size_t l = goodPairs[p2].j;
            if (k == i || k == j || l == i || l == j) continue; // disjoint only
            ++quad_tried;
            FF* c = ffs[k];
            FF* d = ffs[l];
            if (c->getClkIdx() != clkAB || d->getClkIdx() != clkAB) {
                ++quad_reject_clk;
                continue;
            }
            if (HPWL(a->getNewCoor(), c->getNewCoor()) > quad_hpwl_cap ||
                HPWL(a->getNewCoor(), d->getNewCoor()) > quad_hpwl_cap ||
                HPWL(b->getNewCoor(), c->getNewCoor()) > quad_hpwl_cap ||
                HPWL(b->getNewCoor(), d->getNewCoor()) > quad_hpwl_cap) {
                ++quad_reject_hpwl;
                continue;
            }
            if (!seenQuad.insert(quadKey(a, b, c, d)).second) {
                ++quad_reject_dup;
                continue;
            }
            ++quad_accepted;

            std::vector<FF*> quad = {a, b, c, d};
            for (Cell* cell : cells4) {
                auto variants = coordVariantsForGroup(quad, cell, multiCoord);
                int vid = 0;
                for (const Coor& coord : variants) {
                    double gain = banking.CostCompare(coord, cell, quad);
                    MILPCandidate q;
                    q.ffs = quad;
                    q.cell = cell;
                    q.coord = coord;
                    q.cost = -gain;
                    q.variantId = vid++;
                    out.push_back(std::move(q));
                }
            }
        }
    }
    std::cerr << "[MILP_PROTO] quad_enum tried=" << quad_tried
              << " rej_clk=" << quad_reject_clk
              << " rej_hpwl=" << quad_reject_hpwl
              << " rej_dup=" << quad_reject_dup
              << " accepted=" << quad_accepted
              << " pair_count=" << goodPairs.size()
              << " quad_cap=" << quad_hpwl_cap
              << " cells4=" << cells4.size()
              << "\n";

    // Stage 3: direct (2+2)->4 candidates from two 2-bit MBFF atoms.
    // Blossom sees these as 2-node edges (ffs.size()==2) with cell.bits()==4.
    // CostCompare and bankFF_deferred iterate MBFF->getClusterFF() so scoring
    // and commit handle MBFF inputs transparently.
    size_t mbff4_tried = 0, mbff4_reject_hpwl = 0, mbff4_accepted = 0;
    std::vector<size_t> twoBitIdx;
    for (size_t i = 0; i < N; ++i) {
        if (ffs[i] && ffs[i]->getCell() && ffs[i]->getCell()->getBits() == 2)
            twoBitIdx.push_back(i);
    }
    for (size_t p1 = 0; p1 < twoBitIdx.size(); ++p1) {
        FF* a = ffs[twoBitIdx[p1]];
        int clkA = a->getClkIdx();
        for (size_t p2 = p1 + 1; p2 < twoBitIdx.size(); ++p2) {
            FF* b = ffs[twoBitIdx[p2]];
            if (b->getClkIdx() != clkA) continue;
            ++mbff4_tried;
            if (HPWL(a->getNewCoor(), b->getNewCoor()) > quad_hpwl_cap) {
                ++mbff4_reject_hpwl;
                continue;
            }
            ++mbff4_accepted;
            std::vector<FF*> pair = {a, b};
            for (Cell* cell : cells4) {
                auto variants = coordVariantsForGroup(pair, cell, multiCoord);
                int vid = 0;
                for (const Coor& coord : variants) {
                    double gain = banking.CostCompare(coord, cell, pair);
                    MILPCandidate q;
                    q.ffs = pair;
                    q.cell = cell;
                    q.coord = coord;
                    q.cost = -gain;
                    q.variantId = vid++;
                    out.push_back(std::move(q));
                }
            }
        }
    }
    std::cerr << "[MILP_PROTO] mbff4_enum tried=" << mbff4_tried
              << " rej_hpwl=" << mbff4_reject_hpwl
              << " accepted=" << mbff4_accepted
              << " 2bit_atoms=" << twoBitIdx.size()
              << "\n";
}

std::vector<FF*> MILPBanking::sampleSubset(const std::vector<FF*>& all,
                                           size_t n,
                                           const std::string& mode,
                                           unsigned seed) {
    if (n == 0 || n >= all.size()) return all;
    std::vector<FF*> out;
    out.reserve(n);
    if (mode == "tail") {
        for (size_t i = all.size() - n; i < all.size(); ++i) out.push_back(all[i]);
    } else if (mode == "random") {
        std::vector<size_t> idx(all.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::mt19937 rng(seed);
        std::shuffle(idx.begin(), idx.end(), rng);
        for (size_t i = 0; i < n; ++i) out.push_back(all[idx[i]]);
    } else { // "head" (default)
        for (size_t i = 0; i < n; ++i) out.push_back(all[i]);
    }
    return out;
}

void MILPBanking::prototypeDump() {
    const int n_env = envInt("MILP_PROTOTYPE_N", 100);
    const std::string mode = envStr("MILP_PROTOTYPE_SAMPLE", "head");
    const unsigned seed = static_cast<unsigned>(envInt("MILP_PROTOTYPE_SEED", 0));

    // Flatten FF_Map into a deterministic order (sort by instance name).
    std::vector<FF*> all;
    all.reserve(mgr.FF_Map.size());
    for (const auto& kv : mgr.FF_Map) {
        if (kv.second) all.push_back(kv.second);
    }
    std::sort(all.begin(), all.end(), [](FF* a, FF* b) {
        return a->getInstanceName() < b->getInstanceName();
    });

    std::vector<FF*> subset =
        sampleSubset(all, static_cast<size_t>(std::max(0, n_env)), mode, seed);

    std::vector<MILPCandidate> cands;
    enumerate(subset, cands);

    // Histograms.
    size_t n1 = 0, n2 = 0, n4 = 0;
    size_t n2_neg = 0, n2_zero = 0, n2_pos = 0;
    size_t n4_neg = 0, n4_zero = 0, n4_pos = 0;
    double c2_min = DBL_MAX, c2_max = -DBL_MAX, c2_sum = 0.0;
    double c4_min = DBL_MAX, c4_max = -DBL_MAX, c4_sum = 0.0;
    size_t n2_variants_gt1 = 0;

    // Per-(pair) variant count tally.
    // Key: (min ff ptr, max ff ptr, cell ptr) — we just want "was this group
    // emitted with >1 coord variants?".
    struct GKey {
        FF* a; FF* b; Cell* c;
        bool operator==(const GKey& o) const {
            return a == o.a && b == o.b && c == o.c;
        }
    };
    struct GKeyHash {
        size_t operator()(const GKey& k) const {
            auto h = [](void* p) { return std::hash<void*>()(p); };
            return h(k.a) ^ (h(k.b) << 1) ^ (h(k.c) << 2);
        }
    };
    std::unordered_map<size_t, int> variant_count; // group-hash -> variant count
    auto groupHash = [](FF* a, FF* b, Cell* c) {
        if (a > b) std::swap(a, b);
        size_t h = reinterpret_cast<size_t>(a);
        h ^= reinterpret_cast<size_t>(b) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        h ^= reinterpret_cast<size_t>(c) + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
        return h;
    };

    for (const auto& c : cands) {
        if (c.bits() == 1) {
            ++n1;
        } else if (c.bits() == 2) {
            ++n2;
            if (c.cost < -1e-9) ++n2_neg;
            else if (c.cost > 1e-9) ++n2_pos;
            else ++n2_zero;
            c2_min = std::min(c2_min, c.cost);
            c2_max = std::max(c2_max, c.cost);
            c2_sum += c.cost;
            size_t gh = groupHash(c.ffs[0], c.ffs[1], c.cell);
            ++variant_count[gh];
        } else if (c.bits() == 4) {
            ++n4;
            if (c.cost < -1e-9) ++n4_neg;
            else if (c.cost > 1e-9) ++n4_pos;
            else ++n4_zero;
            c4_min = std::min(c4_min, c.cost);
            c4_max = std::max(c4_max, c.cost);
            c4_sum += c.cost;
        }
    }
    for (const auto& kv : variant_count) {
        if (kv.second > 1) ++n2_variants_gt1;
    }

    const size_t total_groups2 = variant_count.size();

    std::cerr << "[MILP_PROTO] ffs_total=" << mgr.FF_Map.size()
              << " subset=" << subset.size()
              << " mode=" << mode
              << " seed=" << seed
              << " prune_mult=" << envDouble("MILP_PRUNE_HPWL_MULT", 2.0)
              << " rh=" << rowHeight()
              << "\n";
    std::cerr << "[MILP_PROTO] candidates total=" << cands.size()
              << " bits1=" << n1
              << " bits2=" << n2
              << " bits4=" << n4
              << " bits2_groups=" << total_groups2
              << " variants_gt1=" << n2_variants_gt1
              << "\n";
    if (n2 > 0) {
        std::cerr << "[MILP_PROTO] bits2_cost min=" << c2_min
                  << " mean=" << (c2_sum / static_cast<double>(n2))
                  << " max=" << c2_max
                  << " neg=" << n2_neg
                  << " zero=" << n2_zero
                  << " pos=" << n2_pos
                  << "\n";
    } else {
        std::cerr << "[MILP_PROTO] bits2_cost: no pair candidates survived pruning\n";
    }
    if (n4 > 0) {
        std::cerr << "[MILP_PROTO] bits4_cost min=" << c4_min
                  << " mean=" << (c4_sum / static_cast<double>(n4))
                  << " max=" << c4_max
                  << " neg=" << n4_neg
                  << " zero=" << n4_zero
                  << " pos=" << n4_pos
                  << "\n";
    }

    // [RA/3] Optional: solver comparison on the same candidate pool.
    const int do_solve = envInt("MILP_SOLVE", 0);
    if (do_solve != 0 && !cands.empty()) {
        auto res_bl = runBlossom(cands, subset);
        std::cerr << "[MILP_SOLVE] blossom "
                  << "pairs=" << res_bl.n_pairs
                  << " quads=" << res_bl.n_quads
                  << " singletons=" << res_bl.n_singletons
                  << " total_cost=" << res_bl.total_cost
                  << " elapsed_ms=" << res_bl.elapsed_ms
                  << " ok=" << (res_bl.ok ? 1 : 0)
                  << "\n";

        auto res_mi = runMILP(cands, subset);
        std::cerr << "[MILP_SOLVE] " << res_mi.backend
                  << " pairs=" << res_mi.n_pairs
                  << " quads=" << res_mi.n_quads
                  << " singletons=" << res_mi.n_singletons
                  << " total_cost=" << res_mi.total_cost
                  << " elapsed_ms=" << res_mi.elapsed_ms
                  << " ok=" << (res_mi.ok ? 1 : 0)
                  << "\n";

        if (res_bl.ok && res_mi.ok) {
            std::cerr << "[MILP_SOLVE] delta milp-blossom=" << (res_mi.total_cost - res_bl.total_cost)
                      << " gap_from_bound=" << (res_mi.total_cost - res_bl.total_cost)
                      << " (negative = MILP strictly better; zero = Blossom optimal on subset)\n";
        }
    }
}

// ---------------------------------------------------------------------------
// [RA/3] Solvers
// ---------------------------------------------------------------------------

MILPBanking::SolveResult MILPBanking::runBlossom(
    const std::vector<MILPCandidate>& cands,
    const std::vector<FF*>& subset) const {
    SolveResult r;
    r.backend = "blossom";

    const auto t0 = std::chrono::high_resolution_clock::now();

    // Index map: FF* -> local index in `subset`.
    std::unordered_map<FF*, int> ff2idx;
    ff2idx.reserve(subset.size() * 2);
    for (size_t i = 0; i < subset.size(); ++i) ff2idx[subset[i]] = static_cast<int>(i);

    // For each pair group, collect best (= min cost) candidate variant.
    // Key: (min_ptr, max_ptr) hash (cell-agnostic — we pick best overall per
    // FF pair; Blossom has no notion of "which cell" beyond the cost).
    struct BestVariant {
        int candIdx = -1;
        double cost = DBL_MAX;
        int u = -1;
        int v = -1;
    };
    std::unordered_map<uint64_t, BestVariant> best;
    auto pairKey = [](FF* a, FF* b) -> uint64_t {
        if (a > b) std::swap(a, b);
        uint64_t ha = reinterpret_cast<uintptr_t>(a);
        uint64_t hb = reinterpret_cast<uintptr_t>(b);
        return ha * 0x9e3779b97f4a7c15ull ^ (hb + (ha << 7));
    };

    for (size_t i = 0; i < cands.size(); ++i) {
        const auto& c = cands[i];
        if (c.bits() != 2) continue;
        auto itA = ff2idx.find(c.ffs[0]);
        auto itB = ff2idx.find(c.ffs[1]);
        if (itA == ff2idx.end() || itB == ff2idx.end()) continue;
        uint64_t k = pairKey(c.ffs[0], c.ffs[1]);
        BestVariant& bv = best[k];
        if (c.cost < bv.cost) {
            bv.cost = c.cost;
            bv.candIdx = static_cast<int>(i);
            bv.u = itA->second;
            bv.v = itB->second;
        }
    }

    // Build LEMON graph. Edge weight = -cost (so max-weight minimizes cost).
    // Negative-cost edges (weight > 0) are beneficial; positive-cost edges are
    // kept only if they carry through matching — MaxWeightedMatching will skip
    // them because it maximizes and we never force inclusion.
    lemon::SmartGraph g;
    std::vector<lemon::SmartGraph::Node> nodes(subset.size());
    for (size_t i = 0; i < subset.size(); ++i) nodes[i] = g.addNode();

    lemon::SmartGraph::EdgeMap<long long> weight(g);
    // Edge -> candidate index so we can read back selection.
    std::unordered_map<int, int> edgeIdToCand; // edge id -> cand idx
    int n_edges = 0;
    const double WSCALE = 1000.0;
    for (const auto& kv : best) {
        const BestVariant& bv = kv.second;
        // Skip neutral / harmful pairs (Blossom baseline behavior).
        if (bv.cost >= -1e-9) continue;
        auto e = g.addEdge(nodes[bv.u], nodes[bv.v]);
        long long w = static_cast<long long>(-bv.cost * WSCALE);
        weight[e] = w;
        edgeIdToCand[g.id(e)] = bv.candIdx;
        ++n_edges;
    }

    lemon::MaxWeightedMatching<lemon::SmartGraph,
        lemon::SmartGraph::EdgeMap<long long>> mwm(g, weight);
    mwm.run();

    std::vector<bool> matched(subset.size(), false);
    for (size_t i = 0; i < subset.size(); ++i) {
        auto mate = mwm.mate(nodes[i]);
        if (mate == lemon::INVALID) continue;
        int j = g.id(mate);
        if (j < static_cast<int>(i)) continue; // count each pair once
        // Find the edge to extract our cand-idx mapping.
        for (lemon::SmartGraph::IncEdgeIt e(g, nodes[i]); e != lemon::INVALID; ++e) {
            lemon::SmartGraph::Node u = g.u(e), v = g.v(e);
            if ((u == nodes[i] && v == mate) || (v == nodes[i] && u == mate)) {
                auto it = edgeIdToCand.find(g.id(e));
                if (it != edgeIdToCand.end()) {
                    r.selected.push_back(it->second);
                    r.total_cost += cands[it->second].cost;
                    int uu = g.id(u), vv = g.id(v);
                    matched[uu] = matched[vv] = true;
                    ++r.n_pairs;
                }
                break;
            }
        }
    }

    // Singletons for unmatched FFs.
    // Map FF* -> singleton cand idx.
    std::unordered_map<FF*, int> singletonCand;
    for (size_t i = 0; i < cands.size(); ++i) {
        if (cands[i].bits() == 1) singletonCand[cands[i].ffs[0]] = static_cast<int>(i);
    }
    for (size_t i = 0; i < subset.size(); ++i) {
        if (matched[i]) continue;
        auto it = singletonCand.find(subset[i]);
        if (it != singletonCand.end()) {
            r.selected.push_back(it->second);
            r.total_cost += cands[it->second].cost; // 0
            ++r.n_singletons;
        }
    }

    (void)n_edges;
    r.elapsed_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();
    r.ok = true;
    return r;
}

MILPBanking::SolveResult MILPBanking::runMILP(
    const std::vector<MILPCandidate>& cands,
    const std::vector<FF*>& subset,
    const std::vector<int>& warmStart) const {
    SolveResult r;
#ifndef USE_ORTOOLS
    (void)cands; (void)subset; (void)warmStart;
    r.backend = "n/a";
    std::cerr << "[MILP_SOLVE] runMILP: compiled without USE_ORTOOLS; skipping\n";
    return r;
#else
    using operations_research::MPConstraint;
    using operations_research::MPObjective;
    using operations_research::MPSolver;
    using operations_research::MPVariable;

    const std::string backend = envStr("MILP_SOLVER", "CBC");
    r.backend = backend;

    const auto t0 = std::chrono::high_resolution_clock::now();
    std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver(backend));
    if (!solver) {
        std::cerr << "[MILP_SOLVE] runMILP: solver '" << backend
                  << "' unavailable\n";
        return r;
    }

    // Time limit (ms). Default 30 s — prototype only.
    const int time_ms = envInt("MILP_TIME_MS", 30000);
    if (time_ms > 0) solver->set_time_limit(time_ms);

    // Subset index for constraint-building.
    std::unordered_map<FF*, int> ff2idx;
    ff2idx.reserve(subset.size() * 2);
    for (size_t i = 0; i < subset.size(); ++i) ff2idx[subset[i]] = static_cast<int>(i);

    // Binary vars per candidate.
    std::vector<MPVariable*> y(cands.size(), nullptr);
    for (size_t i = 0; i < cands.size(); ++i) {
        y[i] = solver->MakeBoolVar("y_" + std::to_string(i));
    }

    // One equality constraint per FF: Σ_{i: ff ∈ cands[i].ffs} y_i == 1.
    std::vector<MPConstraint*> cover(subset.size(), nullptr);
    for (size_t i = 0; i < subset.size(); ++i) {
        cover[i] = solver->MakeRowConstraint(1.0, 1.0,
                                             "cover_" + std::to_string(i));
    }
    for (size_t i = 0; i < cands.size(); ++i) {
        for (FF* f : cands[i].ffs) {
            auto it = ff2idx.find(f);
            if (it == ff2idx.end()) continue;
            cover[it->second]->SetCoefficient(y[i], 1);
        }
    }

    // Objective: minimize Σ cost_i · y_i.
    MPObjective* obj = solver->MutableObjective();
    for (size_t i = 0; i < cands.size(); ++i) {
        obj->SetCoefficient(y[i], cands[i].cost);
    }
    obj->SetMinimization();

    // Warm-start (MIP hint) — seed CBC's primal heuristic with the Blossom
    // selection so MILP is guaranteed to find an objective <= Blossom's.
    if (!warmStart.empty() && warmStart.size() == cands.size()) {
        std::vector<std::pair<const MPVariable*, double>> hints;
        hints.reserve(cands.size());
        for (size_t i = 0; i < cands.size(); ++i) {
            hints.emplace_back(y[i], static_cast<double>(warmStart[i] ? 1 : 0));
        }
        solver->SetHint(hints);
    }

    const auto status = solver->Solve();
    r.elapsed_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t0).count();

    if (status != MPSolver::OPTIMAL && status != MPSolver::FEASIBLE) {
        std::cerr << "[MILP_SOLVE] runMILP: solve status=" << status << "\n";
        return r;
    }

    r.ok = (status == MPSolver::OPTIMAL);
    r.total_cost = obj->Value();
    for (size_t i = 0; i < cands.size(); ++i) {
        if (y[i]->solution_value() > 0.5) {
            r.selected.push_back(static_cast<int>(i));
            int b = cands[i].bits();
            if (b == 1) ++r.n_singletons;
            else if (b == 2) ++r.n_pairs;
            else if (b == 4) ++r.n_quads;
        }
    }
    return r;
#endif
}

// ---------------------------------------------------------------------------
// [RA/4] Production: row-band windowing + Blossom warm-start + commit
// ---------------------------------------------------------------------------

int MILPBanking::runProduction() {
    const int rowsPerWin = std::max(1, envInt("MILP_ROWS_PER_WINDOW", 8));
    const int time_ms = envInt("MILP_WIN_TIME_MS", 5000);
    const bool warm = envInt("MILP_WARM_START", 1) != 0;

    const double rh = rowHeight();
    if (rh <= 0.0) {
        std::cerr << "[MILP_PROD] runProduction: row height unknown; skipping\n";
        return 0;
    }
    const double winH = rh * static_cast<double>(rowsPerWin);

    // Partition FFs by (clkIdx, y-band). Y-band = floor(y / winH).
    // Key: (clkIdx, band).
    std::unordered_map<uint64_t, std::vector<FF*>> windows;
    auto wkey = [](int clk, int band) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(clk)) << 32) |
               static_cast<uint32_t>(band);
    };
    for (const auto& kv : mgr.FF_Map) {
        FF* f = kv.second;
        if (!f) continue;
        // Only 1-bit FFs enter banking; MBFFs already committed earlier bypass.
        if (f->getCell() && f->getCell()->getBits() != 1) continue;
        if (f->getIsLegalize()) continue;
        int band = static_cast<int>(std::floor(f->getNewCoor().y / winH));
        windows[wkey(f->getClkIdx(), band)].push_back(f);
    }

    // Deterministic window order: sort keys.
    std::vector<uint64_t> wkeys;
    wkeys.reserve(windows.size());
    for (auto& kv : windows) wkeys.push_back(kv.first);
    std::sort(wkeys.begin(), wkeys.end());

    const auto t_total0 = std::chrono::high_resolution_clock::now();
    double t_enum = 0, t_bl = 0, t_mi = 0, t_cmt = 0;

    size_t win_total = 0, win_skipped_empty = 0, win_milp_used = 0, win_bl_used = 0;
    size_t cand_total = 0;
    size_t pairs_sel = 0, quads_sel = 0;
    size_t pairs_cmt = 0, quads_cmt = 0, drop_legal = 0, drop_cost = 0;
    double milp_cost_sum = 0.0, bl_cost_sum = 0.0;

    // Stable cluster id counter — match doClustering semantics.
    int clusterTotalNum = 0;
    for (const auto& kv : mgr.FF_Map) {
        if (kv.second && kv.second->getClusterIdx() >= clusterTotalNum) {
            clusterTotalNum = kv.second->getClusterIdx() + 1;
        }
    }

    for (uint64_t k : wkeys) {
        auto& winFFs = windows[k];
        ++win_total;
        if (winFFs.size() < 2) {
            ++win_skipped_empty;
            continue;
        }

        // Enumerate candidates for this window.
        auto te0 = std::chrono::high_resolution_clock::now();
        std::vector<MILPCandidate> cands;
        enumerate(winFFs, cands);
        t_enum += std::chrono::duration<double, std::milli>(
                      std::chrono::high_resolution_clock::now() - te0).count();
        cand_total += cands.size();

        if (cands.empty()) continue;

        // Blossom baseline (also used as MIP-start).
        auto tb0 = std::chrono::high_resolution_clock::now();
        SolveResult bl = runBlossom(cands, winFFs);
        t_bl += std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - tb0).count();
        bl_cost_sum += bl.total_cost;

        // MILP solve, warm-started.
        SolveResult pick = bl;
        std::string who = "blossom";
#ifdef USE_ORTOOLS
        std::vector<int> hint(cands.size(), 0);
        if (warm) {
            for (int idx : bl.selected) hint[idx] = 1;
        }
        // Forward per-window time budget via env; runMILP reads MILP_TIME_MS.
        std::string prev_tm;
        if (const char* e = std::getenv("MILP_TIME_MS")) prev_tm = e;
        setenv("MILP_TIME_MS", std::to_string(time_ms).c_str(), 1);

        auto tm0 = std::chrono::high_resolution_clock::now();
        SolveResult mi = runMILP(cands, winFFs, warm ? hint : std::vector<int>{});
        t_mi += std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - tm0).count();

        if (!prev_tm.empty()) setenv("MILP_TIME_MS", prev_tm.c_str(), 1);
        else unsetenv("MILP_TIME_MS");

        milp_cost_sum += mi.total_cost;
        // Pick the best of (blossom, milp) — MILP with warm-start should match
        // or beat Blossom, but if it times out without a feasible solution we
        // fall back cleanly.
        if (mi.ok || (mi.backend != "n/a" && mi.total_cost < bl.total_cost - 1e-9)) {
            pick = mi;
            who = mi.backend;
            ++win_milp_used;
        } else {
            ++win_bl_used;
        }
#else
        ++win_bl_used;
        (void)warm;
        milp_cost_sum += bl.total_cost; // no-op bookkeeping
#endif
        (void)who;

        // Commit pass: re-FindPlace each selected multi-bit candidate; drop
        // if the slot is no longer legal (earlier commits may have consumed
        // it) or CostCompare turned negative at the materialized coord.
        auto tc0 = std::chrono::high_resolution_clock::now();
        for (int idx : pick.selected) {
            const auto& c = cands[idx];
            if (c.bits() < 2) continue; // singletons = no-op
            Coor placeCoor = mgr.legalizer->FindPlace(c.coord, c.cell);
            if (placeCoor.x == DBL_MAX) {
                ++drop_legal;
                continue;
            }
            double gain = banking.CostCompare(placeCoor, c.cell, c.ffs);
            if (gain < 0.0) {
                ++drop_cost;
                continue;
            }
            FF* newFF = mgr.bankFF(placeCoor, c.cell, c.ffs);
            mgr.legalizer->UpdateRows(newFF);
            newFF->setIsLegalize(true);
            for (FF* f : c.ffs) {
                f->setClusterIdx(clusterTotalNum);
                f->setNewCoor(placeCoor);
            }
            ++clusterTotalNum;
            if (c.bits() == 2) { ++pairs_sel; ++pairs_cmt; }
            else if (c.bits() == 4) { ++quads_sel; ++quads_cmt; }
        }
        t_cmt += std::chrono::duration<double, std::milli>(
                     std::chrono::high_resolution_clock::now() - tc0).count();
    }

    const double t_total =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t_total0).count();

    std::cerr << "[MILP_PROD] windows=" << win_total
              << " skipped=" << win_skipped_empty
              << " used_milp=" << win_milp_used
              << " used_blossom=" << win_bl_used
              << "\n"
              << "[MILP_PROD] cands_total=" << cand_total
              << " pairs_committed=" << pairs_cmt
              << " quads_committed=" << quads_cmt
              << " drop_legal=" << drop_legal
              << " drop_cost=" << drop_cost
              << "\n"
              << "[MILP_PROD] obj_sum blossom=" << bl_cost_sum
              << " milp=" << milp_cost_sum
              << " delta=" << (milp_cost_sum - bl_cost_sum)
              << "\n"
              << "[MILP_PROD] time_ms total=" << t_total
              << " enum=" << t_enum
              << " blossom=" << t_bl
              << " milp=" << t_mi
              << " commit=" << t_cmt
              << "\n";

    // Mirror the legacy higher-bit phase teardown so Manager::legalize() can
    // safely re-Tetris. Two things needed:
    //   1) Rebuild legalizer — our UpdateRows added MBFF Nodes on top of the
    //      preLegalize FF Nodes (now stale: their FF* was erased by bankFF),
    //      and LegalizeWriteBack would dereference FF_Map[erased_name] → null.
    //   2) Reset isLegalize=false for every surviving FF so LoadFF picks up
    //      the committed MBFFs (and the unbanked 1-bits) and Tetris gives
    //      them non-overlapping positions. Legacy path does exactly this at
    //      Banking.cpp L2474 inside the 4-bit greedy loop.
    for (auto& kv : mgr.FF_Map) kv.second->setIsLegalize(false);
    delete mgr.legalizer;
    mgr.legalizer = new Legalizer(mgr);
    mgr.legalizer->initial();

    return static_cast<int>(pairs_cmt + quads_cmt);
}

// ---------------------------------------------------------------------------
// Hybrid Route A (Stage 1): post-matching Layer 1 improver with rollback.
// ---------------------------------------------------------------------------
//
// Stage 1 goal: prove that checkpoint/commit/rollback is bit-exact.
// With HYBRID_ROLLBACK_ALWAYS=1, every window's commit is immediately rolled
// back; final dump must byte-match the Layer 0 baseline on all 9 cases.
//
// Stage 2 will add a safety gate (local_cost_with_boundary) to accept or
// reject per window. Stage 3 extends enumerate() to direct k=4 (the novelty).
// Stage 1 only wires the machinery.

int MILPBanking::runHybrid() {
    const int rowsPerWin = std::max(1, envInt("HYBRID_ROWS_PER_WINDOW", 8));
    const int win_ms = envInt("HYBRID_WIN_MS", 300);
    const int total_ms = envInt("HYBRID_TOTAL_MS", 120000);
    const bool rollbackAlways = envInt("HYBRID_ROLLBACK_ALWAYS", 0) != 0;
    // Stage 2: per-window safety gate. Rollback the window unless the sum of
    // committed realGain exceeds MIN_GAIN. Default 0 (require strict > 0, but
    // per-op filter already enforces that so the gate becomes a no-op when the
    // commit set is non-empty). Set HYBRID_SAFETY_GATE=0 to disable entirely.
    const bool gateOn = envInt("HYBRID_SAFETY_GATE", 1) != 0;
    const double gateMinGain = envDouble("HYBRID_MIN_GAIN", 0.0);

    const double rh = rowHeight();
    if (rh <= 0.0) {
        std::cerr << "[HYBRID] runHybrid: row height unknown; skipping\n";
        return 0;
    }
    const double winH = rh * static_cast<double>(rowsPerWin);

    // Partition FF atoms by (clkIdx, y-band). Stage 3: include 2-bit MBFFs as
    // atoms so enumerate() can produce (2+2)->4 rebanking candidates; pair of
    // 2-bit atoms targets a 4-bit cell and is scored/committed via the same
    // bankFF_deferred path (bankFF iterates MBFF->getClusterFF()).
    //   HYBRID_MAX_ATOM_BITS default 2  (1 = Stage 1/2 behavior; 2 = Stage 3)
    const int maxAtomBits = std::max(1, envInt("HYBRID_MAX_ATOM_BITS", 2));
    std::unordered_map<uint64_t, std::vector<FF*>> windows;
    auto wkey = [](int clk, int band) {
        return (static_cast<uint64_t>(static_cast<uint32_t>(clk)) << 32) |
               static_cast<uint32_t>(band);
    };
    for (const auto& kv : mgr.FF_Map) {
        FF* f = kv.second;
        if (!f || !f->getCell()) continue;
        int bits = f->getCell()->getBits();
        if (bits < 1 || bits > maxAtomBits) continue;
        int band = static_cast<int>(std::floor(f->getNewCoor().y / winH));
        windows[wkey(f->getClkIdx(), band)].push_back(f);
    }

    std::vector<uint64_t> wkeys;
    wkeys.reserve(windows.size());
    for (auto& kv : windows) wkeys.push_back(kv.first);
    std::sort(wkeys.begin(), wkeys.end());

    // Stable cluster id counter — mirror doMatchingClustering semantics.
    int clusterTotalNum = 0;
    for (const auto& kv : mgr.FF_Map) {
        if (kv.second && kv.second->getClusterIdx() >= clusterTotalNum) {
            clusterTotalNum = kv.second->getClusterIdx() + 1;
        }
    }

    // Forward MILP_PRUNE_HPWL_MULT / MILP_QUAD_HPWL_MULT / MILP_BITS_CAP
    // through HYBRID_* aliases so enumerate() reads the Hybrid defaults
    // without changing its knob set. Restore original env on exit.
    struct EnvGuard {
        std::string key;
        bool had = false;
        std::string prev;
    };
    auto envOverride = [](const char* src, const char* dst, EnvGuard& g) {
        g.key = dst;
        const char* prev = std::getenv(dst);
        if (prev) { g.had = true; g.prev = prev; }
        const char* v = std::getenv(src);
        if (v) setenv(dst, v, 1);
    };
    auto envRestore = [](EnvGuard& g) {
        if (g.had) setenv(g.key.c_str(), g.prev.c_str(), 1);
        else unsetenv(g.key.c_str());
    };
    EnvGuard g_prune, g_quad, g_cap;
    envOverride("HYBRID_PRUNE_HPWL_MULT", "MILP_PRUNE_HPWL_MULT", g_prune);
    envOverride("HYBRID_QUAD_HPWL_MULT", "MILP_QUAD_HPWL_MULT", g_quad);
    envOverride("HYBRID_BITS_CAP", "MILP_BITS_CAP", g_cap);

    const auto t_total0 = std::chrono::high_resolution_clock::now();
    double t_enum = 0, t_bl = 0, t_mi = 0, t_cmt = 0, t_rb = 0;

    size_t win_total = 0, win_skipped = 0, win_committed = 0,
           win_reverted = 0, win_skipped_time = 0, win_gate_reverted = 0;
    size_t cand_total = 0, pairs_cmt = 0, quads_cmt = 0;
    size_t drop_legal = 0, drop_cost = 0;
    double gain_total_committed = 0.0;

    for (uint64_t k : wkeys) {
        auto& winFFs = windows[k];
        ++win_total;

        // Respect run-level time budget.
        double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t_total0).count();
        if (elapsed_ms > static_cast<double>(total_ms)) {
            ++win_skipped_time;
            continue;
        }

        if (winFFs.size() < 2) { ++win_skipped; continue; }

        auto te0 = std::chrono::high_resolution_clock::now();
        std::vector<MILPCandidate> cands;
        enumerate(winFFs, cands);
        t_enum += std::chrono::duration<double, std::milli>(
                      std::chrono::high_resolution_clock::now() - te0).count();
        cand_total += cands.size();
        if (cands.empty()) { ++win_skipped; continue; }

        // Blossom baseline (and MIP-start).
        auto tb0 = std::chrono::high_resolution_clock::now();
        SolveResult bl = runBlossom(cands, winFFs);
        t_bl += std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - tb0).count();

        // MILP solve (Stage 1 keeps parity with runProduction's choose-best).
        SolveResult pick = bl;
#ifdef USE_ORTOOLS
        std::vector<int> hint(cands.size(), 0);
        for (int idx : bl.selected) hint[idx] = 1;
        std::string prev_tm;
        if (const char* e = std::getenv("MILP_TIME_MS")) prev_tm = e;
        setenv("MILP_TIME_MS", std::to_string(win_ms).c_str(), 1);

        auto tm0 = std::chrono::high_resolution_clock::now();
        SolveResult mi = runMILP(cands, winFFs, hint);
        t_mi += std::chrono::duration<double, std::milli>(
                    std::chrono::high_resolution_clock::now() - tm0).count();

        if (!prev_tm.empty()) setenv("MILP_TIME_MS", prev_tm.c_str(), 1);
        else unsetenv("MILP_TIME_MS");
        if (mi.ok || (mi.backend != "n/a" && mi.total_cost < bl.total_cost - 1e-9)) {
            pick = mi;
        }
#endif

        // Commit pass — deferred bank + UpdateRows, one BankOp per selected
        // multi-bit candidate. Snap owns the undo data for the window.
        struct BankOp {
            Manager::BankUndo undo;
            FF* newFF = nullptr;
            std::vector<RowSubrowSnap> rowSnap;
            double realGain = 0.0;   // Stage 2: CostCompare at placeCoor (>0 = improvement)
            // Per-constituent state we overwrote in the commit loop so rollback
            // can restore it. Rollback-the-bank alone only touches FF_Map and
            // physicalFF; clusterIdx/newCoor live on the constituent wrappers.
            struct CoorUndo {
                FF* ff;
                Coor oldNewCoor;
                int oldClusterIdx;
            };
            std::vector<CoorUndo> coorUndo;
        };
        std::vector<BankOp> ops;
        ops.reserve(pick.selected.size());

        auto tc0 = std::chrono::high_resolution_clock::now();
        bool commit_ok = true;
        for (int idx : pick.selected) {
            const auto& c = cands[idx];
            if (c.bits() < 2) continue;
            Coor placeCoor = mgr.legalizer->FindPlace(c.coord, c.cell);
            if (placeCoor.x == DBL_MAX) { ++drop_legal; continue; }
            double gain = banking.CostCompare(placeCoor, c.cell, c.ffs);
            if (gain < 0.0) { ++drop_cost; continue; }

            BankOp op;
            op.realGain = gain;
            // Snapshot rows BEFORE UpdateRows mutates subrows.
            op.rowSnap = mgr.legalizer->SnapshotRowsForRect(placeCoor,
                                                           c.cell->getH());
            op.newFF = mgr.bankFF_deferred(placeCoor, c.cell, c.ffs, op.undo);
            mgr.legalizer->UpdateRows(op.newFF);
            op.newFF->setIsLegalize(true);
            // Capture constituent state before overwrite, then overwrite.
            op.coorUndo.reserve(c.ffs.size());
            for (FF* f : c.ffs) {
                BankOp::CoorUndo cu;
                cu.ff = f;
                cu.oldNewCoor = f->getNewCoor();
                cu.oldClusterIdx = f->getClusterIdx();
                op.coorUndo.push_back(cu);
                f->setClusterIdx(clusterTotalNum);
                f->setNewCoor(placeCoor);
            }
            ++clusterTotalNum;
            // Count by TARGET cell bits (c.bits()==ffs.size() is 2 for both
            // 1+1->2 and 2+2->4, so use cell bits to distinguish).
            int cellBits = c.cell ? c.cell->getBits() : 0;
            if (cellBits == 2) ++pairs_cmt;
            else if (cellBits == 4) ++quads_cmt;
            ops.push_back(std::move(op));
        }
        t_cmt += std::chrono::duration<double, std::milli>(
                     std::chrono::high_resolution_clock::now() - tc0).count();

        // Stage 2: per-window safety gate. Sum committed realGain; rollback if
        // the window did not net a positive improvement. The per-op filter
        // already requires realGain > 0, so the sum-gate only triggers when
        // ops is empty (degenerate) — but the infrastructure generalizes to
        // Stage 3 where cross-op cascade may shrink the window's net gain.
        double window_gain = 0.0;
        for (const auto& op : ops) window_gain += op.realGain;
        const bool gate_fail = gateOn && !ops.empty() && window_gain <= gateMinGain;
        const bool do_rollback = rollbackAlways || !commit_ok || gate_fail;

        if (do_rollback) {
            auto tr0 = std::chrono::high_resolution_clock::now();
            // Unwind in reverse commit order.
            for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
                // 1. Restore constituents' overwritten newCoor / clusterIdx.
                for (auto& cu : it->coorUndo) {
                    cu.ff->setNewCoor(cu.oldNewCoor);
                    cu.ff->setClusterIdx(cu.oldClusterIdx);
                }
                // 2. Drop the appended Node from legalizer->ffs.
                mgr.legalizer->RemoveNodeByFFPtr(it->newFF);
                // 3. Restore subrow state.
                mgr.legalizer->RestoreRowSubrowsFromSnap(it->rowSnap);
                // 4. Restore FF_Map / physicalFF and recycle the new MBFF.
                mgr.rollbackBank(it->undo);
                // Undo local cluster counter bump so a subsequent committed
                // window keeps the ids monotonic.
                --clusterTotalNum;
            }
            ++win_reverted;
            if (gate_fail && !rollbackAlways) ++win_gate_reverted;
            t_rb += std::chrono::duration<double, std::milli>(
                        std::chrono::high_resolution_clock::now() - tr0).count();
        } else {
            // Commit-finalize: fire deferred deletes, drop row snaps.
            for (auto& op : ops) {
                mgr.legalizer->DiscardRowSnap(op.rowSnap);
                mgr.commitFinalizeBank(op.undo);
            }
            ++win_committed;
            gain_total_committed += window_gain;
        }
    }

    envRestore(g_prune);
    envRestore(g_quad);
    envRestore(g_cap);

    const double t_total =
        std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t_total0).count();

    std::cerr << "[HYBRID] windows=" << win_total
              << " skipped=" << win_skipped
              << " committed=" << win_committed
              << " reverted=" << win_reverted
              << " gate_reverted=" << win_gate_reverted
              << " skipped_time=" << win_skipped_time
              << "\n"
              << "[HYBRID] cands_total=" << cand_total
              << " pairs_committed=" << pairs_cmt
              << " quads_committed=" << quads_cmt
              << " drop_legal=" << drop_legal
              << " drop_cost=" << drop_cost
              << " gain_total=" << gain_total_committed
              << "\n"
              << "[HYBRID] time_ms total=" << t_total
              << " enum=" << t_enum
              << " blossom=" << t_bl
              << " milp=" << t_mi
              << " commit=" << t_cmt
              << " rollback=" << t_rb
              << (rollbackAlways ? " [ROLLBACK_ALWAYS=1]" : "")
              << (gateOn ? "" : " [GATE_OFF]")
              << "\n";

    return static_cast<int>(pairs_cmt + quads_cmt);
}
