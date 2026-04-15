#include "MeanShift.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <unordered_map>

namespace {

typedef std::pair<Point, int> CenterPoint;
typedef bgi::rtree<CenterPoint, bgi::quadratic<P_PER_NODE>> CenterRTree;

inline double manhattan(const Coor &a, const Coor &b){
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

// Clamp movement so the FF does not leave its timing-feasible region.
void writeBackClamped(FF *f, const Coor &target, double maxSqDisp){
    Coor orig = f->getCoor();
    double dx = target.x - orig.x;
    double dy = target.y - orig.y;
    double sq = dx * dx + dy * dy;
    if(sq <= maxSqDisp){
        f->setNewCoor(target);
    } else {
        double scale = std::sqrt(maxSqDisp / sq);
        f->setNewCoor(Coor(orig.x + dx * scale, orig.y + dy * scale));
    }
}

} // anonymous namespace

MeanShift::MeanShift(){}
MeanShift::~MeanShift(){}

// Algorithm 1 (P10): recursive bipartition to seed K cluster centers.
void MeanShift::initCentersBipartition(std::vector<FF*> group, int K, bool splitByX,
                                       std::vector<Coor> &out){
    if(group.empty()) return;
    if(K <= 1 || (int)group.size() <= 1){
        double sx = 0, sy = 0;
        for(FF *f : group){
            sx += f->getCoor().x;
            sy += f->getCoor().y;
        }
        out.push_back(Coor(sx / group.size(), sy / group.size()));
        return;
    }
    std::sort(group.begin(), group.end(), [splitByX](FF *a, FF *b){
        if(splitByX){
            if(a->getCoor().x != b->getCoor().x) return a->getCoor().x < b->getCoor().x;
            return a->getCoor().y < b->getCoor().y;
        } else {
            if(a->getCoor().y != b->getCoor().y) return a->getCoor().y < b->getCoor().y;
            return a->getCoor().x < b->getCoor().x;
        }
    });
    int K1 = K / 2;
    int K2 = K - K1;
    size_t n1 = (size_t)((double)group.size() * K1 / K);
    if(n1 == 0) n1 = 1;
    if(n1 >= group.size()) n1 = group.size() - 1;
    std::vector<FF*> S1(group.begin(), group.begin() + n1);
    std::vector<FF*> S2(group.begin() + n1, group.end());
    initCentersBipartition(std::move(S1), K1, !splitByX, out);
    initCentersBipartition(std::move(S2), K2, !splitByX, out);
}

void MeanShift::runKMeansOnClkGroup(std::vector<FF*> &ffs, int sizeLimit, double maxSqDisp){
    if(ffs.empty()) return;

    // Trivial case: single cluster.
    if((int)ffs.size() <= sizeLimit){
        double sx = 0, sy = 0;
        for(FF *f : ffs){
            sx += f->getCoor().x;
            sy += f->getCoor().y;
        }
        Coor c(sx / ffs.size(), sy / ffs.size());
        for(FF *f : ffs) writeBackClamped(f, c, maxSqDisp);
        return;
    }

    const int K = (int)((ffs.size() + sizeLimit - 1) / sizeLimit);
    std::vector<Coor> centers;
    centers.reserve(K);
    initCentersBipartition(ffs, K, true, centers);
    if(centers.empty()) return;

    const int actualK = (int)centers.size();
    std::vector<int> assignment(ffs.size(), -1);
    std::vector<int> clusterSize(actualK, 0);

    // How many nearest centers to score per FF. 16 balances coverage vs runtime.
    const int NEAR = std::min(16, actualK);
    const int MAX_ITER = 10;

    for(int iter = 0; iter < MAX_ITER; iter++){
        // Build rtree of current centers.
        CenterRTree ctrRtree;
        {
            std::vector<CenterPoint> pts;
            pts.reserve(actualK);
            for(int k = 0; k < actualK; k++){
                pts.push_back(std::make_pair(Point(centers[k].x, centers[k].y), k));
            }
            ctrRtree.insert(pts.begin(), pts.end());
        }

        bool anyChange = false;
        // Reassign each FF. Online weight update: clusterSize changes as we
        // move FFs, so subsequent FFs in this iteration see live sizes.
        for(size_t i = 0; i < ffs.size(); i++){
            FF *f = ffs[i];
            std::vector<CenterPoint> near;
            near.reserve(NEAR);
            ctrRtree.query(bgi::nearest(Point(f->getCoor().x, f->getCoor().y), NEAR),
                           std::back_inserter(near));
            int bestK = -1;
            double bestCost = DBL_MAX;
            for(const auto &cp : near){
                int k = cp.second;
                double md = manhattan(f->getCoor(), centers[k]);
                double w = (iter == 0)
                         ? 1.0
                         : std::max(1.0, (double)clusterSize[k] / sizeLimit);
                double cost = md * w;
                if(cost < bestCost){
                    bestCost = cost;
                    bestK = k;
                }
            }
            if(bestK < 0) continue;
            if(bestK != assignment[i]){
                if(assignment[i] >= 0) clusterSize[assignment[i]]--;
                clusterSize[bestK]++;
                assignment[i] = bestK;
                anyChange = true;
            }
        }

        // Recompute center positions from current assignments.
        std::vector<double> sumX(actualK, 0.0), sumY(actualK, 0.0);
        std::vector<int> count(actualK, 0);
        for(size_t i = 0; i < ffs.size(); i++){
            int k = assignment[i];
            if(k < 0) continue;
            sumX[k] += ffs[i]->getCoor().x;
            sumY[k] += ffs[i]->getCoor().y;
            count[k]++;
        }
        for(int k = 0; k < actualK; k++){
            if(count[k] > 0){
                centers[k] = Coor(sumX[k] / count[k], sumY[k] / count[k]);
            }
        }

        if(!anyChange) break;
    }

    // Write back: each FF moves to its cluster center, clamped by the
    // per-FF displacement limit.
    for(size_t i = 0; i < ffs.size(); i++){
        if(assignment[i] >= 0){
            writeBackClamped(ffs[i], centers[assignment[i]], maxSqDisp);
        }
    }
}

void MeanShift::run(Manager &mgr){
    DEBUG_MS("Running weighted K-means clustering (Phase 3D-C)");

    int sizeLimit = mgr.MaxBit;
    if(sizeLimit < 2) sizeLimit = 2;
    double maxSqDisp = mgr.param.MAX_SQUARE_DISPLACEMENT;

    // Group FFs by clock domain; reset newCoor to original coord so every
    // FF starts from the parser's placement.
    std::unordered_map<int, std::vector<FF*>> byClk;
    for(const auto &p : mgr.FF_Map){
        byClk[p.second->getClkIdx()].push_back(p.second);
        p.second->setNewCoor(p.second->getCoor());
    }

    std::vector<std::vector<FF*>> clkGroups;
    clkGroups.reserve(byClk.size());
    for(auto &kv : byClk){
        clkGroups.emplace_back(std::move(kv.second));
    }

    // Clock groups are independent — parallelize across them.
    #pragma omp parallel for schedule(dynamic) num_threads(MAX_THREADS)
    for(size_t i = 0; i < clkGroups.size(); i++){
        runKMeansOnClkGroup(clkGroups[i], sizeLimit, maxSqDisp);
    }
}
