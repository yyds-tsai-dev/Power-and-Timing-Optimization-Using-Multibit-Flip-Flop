#include "Banking.h"
#include "ConflictPartition.h"
#include <omp.h>
#include <chrono>
#include <unordered_set>
#include <lemon/smart_graph.h>
#include <lemon/matching.h>

Banking::Banking(Manager& mgr) : mgr(mgr){
    for(const auto &bitLib : mgr.Bit_FF_Map){
        clusterNum[bitLib.first] = 0;
    }
}

Banking::~Banking(){}

void Banking::run(){
    DEBUG_BAN("Running cluster...");
    bitOrdering();
    Timer t = Timer();
    t.start();
    if(bitOrder[0] != 1){
        const char* mode = std::getenv("BANKING_MODE");
        if(mode && std::string(mode) == "matching"){
            doMatchingClustering();
        } else {
            doClustering();
        }
    }
    t.stop();
    restoreUnclusterFFCoor();
    ClusterResult();
}

void Banking::bitOrdering(){
    std::vector<std::pair<double, int>> bitScoreVector;
    for(auto &pair: mgr.Bit_FF_Map){
        std::vector<Cell *> &cell_vector = pair.second;
        bitScoreVector.push_back({cell_vector[0]->getScore()/pair.first, pair.first});
    }
    
    std::sort(bitScoreVector.begin(), bitScoreVector.end());
    DEBUG_BAN("[LIB MBFF SCORE]");

    for(auto const& bit_pair: bitScoreVector){
        bitOrder.push_back(bit_pair.second);
        // DEBUG
        DEBUG_BAN("\t\t" + mgr.Bit_FF_Map[bit_pair.second][0]->getCellName() + "(" + std::to_string(bit_pair.second)  +  "): " + std::to_string(bit_pair.first));
    }
}

bool Banking::chooseCandidateFF(FF* nowFF, const std::vector<FF*> &localFFs, std::vector<PointWithID>& resultFFs, std::vector<PointWithID>& toRemoveFFs, std::vector<FF*> &FFToBank, const int &targetBit){
    std::vector<std::pair<int, double>> nearFFs;
    PointWithID curFF;
    for(int i = 0; i < (int)resultFFs.size(); i++){
        FF* ff = localFFs[resultFFs[i].second];
        double dis = HPWL(nowFF->getNewCoor(), ff->getNewCoor());
        if(dis == 0){
            curFF = resultFFs[i];
        }
        else{
            nearFFs.push_back ({i, dis});
        }
    }

    if(nearFFs.size() > 0){
        sortFFs(nearFFs);
        toRemoveFFs.push_back(curFF);
        FFToBank.push_back(localFFs[curFF.second]);
        int bitSum = localFFs[curFF.second]->getCell()->getBits();
        int i = 0;
        while(bitSum < targetBit && i < (int)nearFFs.size()){
            PointWithID ffPoint = resultFFs[nearFFs[i].first];
            FF* ff = localFFs[ffPoint.second];
            if(bitSum + ff->getCell()->getBits() <= targetBit){
                toRemoveFFs.push_back(ffPoint);
                FFToBank.push_back(ff);
                bitSum += ff->getCell()->getBits();
            }
            i++;
        }
        if(bitSum == targetBit)
            return true;
        else
            return false;
    }
    return false;
}

// // Can construct the LUT first...
// Cell* Banking::chooseCellLib(int bitNum){
//     int order = 0;
//     int targetBit = bitOrder[order];
//     while(bitNum != targetBit && order < (int)bitOrder.size()){
//         if(targetBit > bitNum){
//             order++;
//             targetBit = bitOrder[order];
//             continue;
//         }
//         bitNum--;
//     }
//     assert(bitNum > 0);
//     return mgr.Bit_FF_Map[bitNum][0];
// }

Coor Banking::getMedian(const std::vector<FF*> &localFFs, std::vector<PointWithID>& toRemoveFFs){
    std::vector<double> median_x, median_y;
    for(size_t i = 0; i < toRemoveFFs.size(); i++){
        median_x.push_back(localFFs[toRemoveFFs[i].second]->getNewCoor().x);
        median_y.push_back(localFFs[toRemoveFFs[i].second]->getNewCoor().y);
    }
    std::sort(median_x.begin(), median_x.end());
    std::sort(median_y.begin(), median_y.end());
    double x = median_x[(int) toRemoveFFs.size() / 2];
    double y = median_y[(int) toRemoveFFs.size() / 2];
    return Coor(x,y);
}


void Banking::sortFFs(std::vector<std::pair<int, double>> &nearFFs){
    auto FFcmp = [](const std::pair<int, double> &neighbor1, const std::pair<int, double> &neighbor2){
        return neighbor1.second < neighbor2.second;
    };
    std::sort(nearFFs.begin(), nearFFs.end(), FFcmp);
}

void Banking::computePinTNS(const std::vector<FF*>& FFToBank, Cell* targetCell,
                            const Coor& placeCoor, double& oldTNS, double& newTNS){
    // Per-pin TNS calculation: compute actual per-constituent-FF slack change
    // using driver/load positions and the max(0, -slack) TNS filter.
    oldTNS = 0;
    newTNS = 0;
    size_t flatIdx = 0; // slot index into target cell's pin layout

    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        double oldCellQDelay = ff->getCell()->getQpinDelay();
        double newCellQDelay = targetCell->getQpinDelay();

        for(auto& cf : ff->getClusterFF()){
            // Target cell pin names: "D0","D1",... for multi-bit, "D" for 1-bit
            std::string slotStr = (targetCell->getBits() == 1)
                ? "" : std::to_string(flatIdx);

            // ---- D-pin slack of this constituent FF ----
            double curSlackD = cf->getSlack();

            // Predict D-pin slack at new position using actual driver position
            Coor curDpin = ff->getNewCoor() + ff->getPinCoor(
                "D" + cf->getPhysicalPinName());
            Coor newDpin = placeCoor + targetCell->getPinCoor("D" + slotStr);

            double deltaHpwlD = 0;
            PrevInstance prev = cf->getPrevInstance();
            if(prev.instance){
                Coor driverCoor;
                if(prev.cellType == CellType::IO){
                    driverCoor = prev.instance->getCoor();
                } else if(prev.cellType == CellType::GATE){
                    driverCoor = prev.instance->getCoor()
                               + prev.instance->getPinCoor(prev.pinName);
                } else {
                    FF* inputFF = dynamic_cast<FF*>(prev.instance);
                    driverCoor = inputFF->getPhysicalFF()->getNewCoor()
                               + inputFF->getPhysicalFF()->getPinCoor(
                                   "Q" + inputFF->getPhysicalPinName());
                }
                // positive = new position is closer to driver (timing improves)
                deltaHpwlD = HPWL(driverCoor, curDpin)
                           - HPWL(driverCoor, newDpin);
            }

            double predSlackD = curSlackD + mgr.DisplacementDelay * deltaHpwlD;
            oldTNS += std::max(0.0, -curSlackD);
            newTNS += std::max(0.0, -predSlackD);

            // ---- Q-pin: downstream FFs' D-pin slack ----
            for(auto& next : cf->getNextStage()){
                double nextCurSlack = next.ff->getSlack();

                // Q-pin delay change: positive if new cell is faster
                double qDelayBenefit = oldCellQDelay - newCellQDelay;

                // Q-pin displacement: compute using actual load position
                Coor curQpin = ff->getNewCoor() + ff->getPinCoor(
                    "Q" + cf->getPhysicalPinName());
                Coor newQpin = placeCoor + targetCell->getPinCoor("Q" + slotStr);

                Coor loadCoor;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                }
                // positive = Q-pin moved closer to load (timing improves)
                double deltaHpwlQ = HPWL(loadCoor, curQpin)
                                  - HPWL(loadCoor, newQpin);

                double predNextSlack = nextCurSlack + qDelayBenefit
                                     + mgr.DisplacementDelay * deltaHpwlQ;
                oldTNS += std::max(0.0, -nextCurSlack);
                newTNS += std::max(0.0, -predNextSlack);
            }

            flatIdx++;
        }
    }
}

double Banking::CostCompare(const Coor clusterCoor, Cell* chooseCell, std::vector<FF*> FFToBank){
    // --- Power + Area savings (exact) ---
    double costOptimize = 0;
    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        costOptimize += mgr.beta * (ff->getCell()->getGatePower());
        costOptimize += mgr.gamma * (ff->getCell()->getArea());
    }
    costOptimize -= mgr.beta * (chooseCell->getGatePower()) + mgr.gamma * (chooseCell->getArea());

    // --- Per-pin TNS ---
    double oldTNS = 0, newTNS = 0;
    computePinTNS(FFToBank, chooseCell, clusterCoor, oldTNS, newTNS);
    double deltaTNS = newTNS - oldTNS; // positive = TNS worsened
    costOptimize -= mgr.alpha * deltaTNS;
    return costOptimize;
}

double Banking::weightedMedian(std::vector<std::pair<double,double>>& cw){
    // cw = {(coordinate, weight)}. Returns weighted median.
    std::sort(cw.begin(), cw.end());
    double totalW = 0;
    for(auto& p : cw) totalW += p.second;
    double cumW = 0;
    for(auto& p : cw){
        cumW += p.second;
        if(cumW >= totalW * 0.5) return p.first;
    }
    return cw.back().first;
}

Coor Banking::findWindowOptimal(const std::vector<FF*>& FFToBank, Cell* targetCell,
                                double xlo, double xhi, double ylo, double yhi){
    // Find the TNS-optimal point within feasible window [xlo,xhi] x [ylo,yhi].
    // Collect driver/load positions as breakpoints; weighted median per axis,
    // clamped to window bounds.
    std::vector<std::pair<double,double>> bpX, bpY;
    size_t flatIdx = 0;

    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        for(auto& cf : ff->getClusterFF()){
            std::string slotStr = (targetCell->getBits() == 1)
                ? "" : std::to_string(flatIdx);
            Coor pinOffD = targetCell->getPinCoor("D" + slotStr);
            Coor pinOffQ = targetCell->getPinCoor("Q" + slotStr);

            // D-pin: driver position → anchor = driverCoor - pinOffset
            PrevInstance prev = cf->getPrevInstance();
            if(prev.instance){
                Coor driverCoor;
                if(prev.cellType == CellType::IO){
                    driverCoor = prev.instance->getCoor();
                } else if(prev.cellType == CellType::GATE){
                    driverCoor = prev.instance->getCoor()
                               + prev.instance->getPinCoor(prev.pinName);
                } else {
                    FF* inputFF = dynamic_cast<FF*>(prev.instance);
                    driverCoor = inputFF->getPhysicalFF()->getNewCoor()
                               + inputFF->getPhysicalFF()->getPinCoor(
                                   "Q" + inputFF->getPhysicalPinName());
                }
                double w = mgr.DisplacementDelay;
                bpX.push_back({driverCoor.x - pinOffD.x, w});
                bpY.push_back({driverCoor.y - pinOffD.y, w});
            }

            // Q-pin: each downstream load → anchor = loadCoor - pinOffset
            for(auto& next : cf->getNextStage()){
                Coor loadCoor;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                }
                double w = mgr.DisplacementDelay;
                bpX.push_back({loadCoor.x - pinOffQ.x, w});
                bpY.push_back({loadCoor.y - pinOffQ.y, w});
            }

            flatIdx++;
        }
    }

    // Fallback: if no timing connections, use window center
    if(bpX.empty()){
        return Coor((xlo + xhi) / 2.0, (ylo + yhi) / 2.0);
    }

    double optX = weightedMedian(bpX);
    optX = std::max(xlo, std::min(xhi, optX));
    double optY = weightedMedian(bpY);
    optY = std::max(ylo, std::min(yhi, optY));

    return Coor(optX, optY);
}

Coor Banking::ComputeOptimalPosition(Cell* chooseCell, const std::vector<FF*>& FFToBank){
    // Collect weighted anchor points from ALL drivers/loads.
    // All pins contribute; critical pins (slack < 0) get higher weight.
    // Anchor = driverCoor - pinOffset, so that position + pinOffset ≈ driverCoor.
    std::vector<std::pair<double,double>> anchorsX, anchorsY;
    size_t flatIdx = 0;

    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        for(auto& cf : ff->getClusterFF()){
            std::string slotStr = (chooseCell->getBits() == 1)
                ? "" : std::to_string(flatIdx);
            Coor pinOffD = chooseCell->getPinCoor("D" + slotStr);
            Coor pinOffQ = chooseCell->getPinCoor("Q" + slotStr);

            double slackD = cf->getSlack();
            PrevInstance prev = cf->getPrevInstance();
            if(prev.instance){
                Coor driverCoor;
                if(prev.cellType == CellType::IO){
                    driverCoor = prev.instance->getCoor();
                } else if(prev.cellType == CellType::GATE){
                    driverCoor = prev.instance->getCoor()
                               + prev.instance->getPinCoor(prev.pinName);
                } else {
                    FF* inputFF = dynamic_cast<FF*>(prev.instance);
                    driverCoor = inputFF->getPhysicalFF()->getNewCoor()
                               + inputFF->getPhysicalFF()->getPinCoor(
                                   "Q" + inputFF->getPhysicalPinName());
                }
                // Critical pins: high weight. Non-critical: base weight.
                double w = (slackD < 0)
                    ? mgr.DisplacementDelay * mgr.alpha
                    : mgr.DisplacementDelay * mgr.alpha * 0.1;
                anchorsX.push_back({driverCoor.x - pinOffD.x, w});
                anchorsY.push_back({driverCoor.y - pinOffD.y, w});
            }

            // Q-pin loads
            for(auto& next : cf->getNextStage()){
                double slackQ = next.ff->getSlack();
                Coor loadCoor;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                }
                double w = (slackQ < 0)
                    ? mgr.DisplacementDelay * mgr.alpha
                    : mgr.DisplacementDelay * mgr.alpha * 0.1;
                anchorsX.push_back({loadCoor.x - pinOffQ.x, w});
                anchorsY.push_back({loadCoor.y - pinOffQ.y, w});
            }

            flatIdx++;
        }
    }

    // Fallback to geometric median if no anchors at all (no timing connections)
    if(anchorsX.empty()){
        double sx = 0, sy = 0;
        for(auto* ff : FFToBank){
            sx += ff->getNewCoor().x;
            sy += ff->getNewCoor().y;
        }
        return Coor(sx / FFToBank.size(), sy / FFToBank.size());
    }

    return Coor(weightedMedian(anchorsX), weightedMedian(anchorsY));
}

void Banking::doClustering(){
    int clusterTotalNum = 0;
    size_t max_clk_idx = 0;
    for(const auto &pair : mgr.FF_Map){
        max_clk_idx = std::max((int)max_clk_idx, pair.second->getClkIdx());
    }
    std::map<int, std::vector<Cell *>> orderBitMap(mgr.Bit_FF_Map.begin(), mgr.Bit_FF_Map.end());

    struct PendingBank {
        Coor median;
        Coor threadCoor;
        std::vector<FF*> ffs;
        int tid;
        size_t stubIdx;
    };

    auto tic = [](){ return std::chrono::high_resolution_clock::now(); };
    auto ms = [](std::chrono::high_resolution_clock::time_point a,
                 std::chrono::high_resolution_clock::time_point b){
        return std::chrono::duration<double, std::milli>(b - a).count();
    };
    double t_init = 0, t_par = 0, t_canonical = 0, t_merge = 0;
    int n_pending = 0, n_fastPath = 0, n_fallbackOK = 0, n_dropped = 0;

    for(const auto &bitLib : orderBitMap){
        Cell* chooseCell = bitLib.second[0];
        int targetBit = chooseCell->getBits();
        if(targetBit == 1) continue;
        DEBUG_BAN("Cluster " + std::to_string(targetBit) + " Bit MBFF");

        // Phase 3A: parallelize over clk domains. Threads carry their own
        // Legalizer (populated independently from mgr read-only state) and
        // accumulate PendingBank candidates; the canonical Legalizer commits
        // them serially after the parallel phase so mgr.FF_Map mutations stay
        // single-threaded.
        size_t clkCount = max_clk_idx + 1;
        int nthreads = std::min<int>(MAX_THREADS, (int)std::max<size_t>(1, clkCount));

        auto t0 = tic();
        std::vector<std::unique_ptr<Legalizer>> tlegalizers(nthreads);
        #pragma omp parallel num_threads(nthreads)
        {
            int tid = omp_get_thread_num();
            tlegalizers[tid].reset(new Legalizer(mgr));
            tlegalizers[tid]->initial();
        }
        t_init += ms(t0, tic());

        std::vector<std::vector<PendingBank>> perThreadPending(nthreads);

        auto t1 = tic();
        #pragma omp parallel for schedule(dynamic) num_threads(nthreads)
        for(size_t clkIDX = 0; clkIDX < clkCount; clkIDX++){
            int tid = omp_get_thread_num();
            Legalizer *lgz = tlegalizers[tid].get();

            std::vector<FF*> localFFs;
            for(const auto &pair : mgr.FF_Map){
                if((size_t)pair.second->getClkIdx() == clkIDX){
                    pair.second->setIsLegalize(false);
                    localFFs.push_back(pair.second);
                }
            }
            if(localFFs.empty()) continue;

            std::vector<PointWithID> points;
            points.reserve(localFFs.size());
            for(size_t i = 0; i < localFFs.size(); i++){
                FF *ff = localFFs[i];
                points.push_back(std::make_pair(Point(ff->getNewCoor().x, ff->getNewCoor().y), (int)i));
            }
            bgi::rtree<PointWithID, bgi::quadratic<P_PER_NODE>> rtree;
            rtree.insert(points.begin(), points.end());
            std::vector<bool> isClustered(localFFs.size(), false);

            std::vector<PendingBank> &pending = perThreadPending[tid];

            for(size_t index = 0; index < localFFs.size(); index++){
                FF* nowFF = localFFs[index];
                if(isClustered[index]) continue;
                std::vector<PointWithID> resultFFs, toRemoveFFs;
                resultFFs.reserve(mgr.MaxBit);
                rtree.query(bgi::nearest(Point(nowFF->getNewCoor().x, nowFF->getNewCoor().y), mgr.MaxBit), std::back_inserter(resultFFs));
                std::vector<FF*> FFToBank;
                bool isChoose = chooseCandidateFF(nowFF, localFFs, resultFFs, toRemoveFFs, FFToBank, targetBit);

                if(isChoose){
                    Coor medianCoor = getMedian(localFFs, toRemoveFFs);
                    Coor clusterCoor = lgz->FindPlace(medianCoor, chooseCell);
                    if(clusterCoor.x == DBL_MAX && clusterCoor.y == DBL_MAX)
                        continue;

                    if(CostCompare(clusterCoor, chooseCell, FFToBank) < 0)
                        continue;

                    // simulate the bank on the thread-local legalizer so that
                    // subsequent FindPlace calls in this thread avoid the same site
                    size_t stubIdx = lgz->UpdateRowsFootprint(clusterCoor, chooseCell);

                    for(size_t j = 0; j < toRemoveFFs.size(); j++){
                        isClustered[toRemoveFFs[j].second] = true;
                    }
                    rtree.remove(toRemoveFFs.begin(), toRemoveFFs.end());

                    pending.push_back({medianCoor, clusterCoor, FFToBank, tid, stubIdx});
                }
            }
        }

        t_par += ms(t1, tic());

        // Adopt thread-0's legalizer as canonical: its rows already carry the
        // footprints for every bank thread 0 committed, so we skip a full
        // re-initialization. Stubs are upgraded in place during the merge.
        auto t2 = tic();
        mgr.legalizer = tlegalizers[0].release();
        for(int tid = 1; tid < nthreads; tid++){
            tlegalizers[tid].reset();
        }
        tlegalizers.clear();
        t_canonical += ms(t2, tic());

        auto t3 = tic();

        // Thread-0 pending: footprints already on canonical rows, promote the
        // stubs in place (no canPlace check, no re-slicing).
        for(const PendingBank &pb : perThreadPending[0]){
            n_pending++;
            n_fastPath++;
            Coor clusterCoor = pb.threadCoor;
            FF* newFF = mgr.bankFF(clusterCoor, chooseCell, pb.ffs);
            mgr.legalizer->PromoteFootprintToFF(pb.stubIdx, newFF);
            newFF->setIsLegalize(true);
            for(FF* oldFF : pb.ffs){
                oldFF->setClusterIdx(clusterTotalNum);
                oldFF->setNewCoor(clusterCoor);
            }
            clusterTotalNum++;
        }

        // Other threads' pending: may conflict with thread-0 footprints on the
        // canonical rows — check fast-path, fall back to FindPlace on conflict.
        for(int tid = 1; tid < nthreads; tid++){
            for(const PendingBank &pb : perThreadPending[tid]){
                n_pending++;
                Coor clusterCoor = pb.threadCoor;
                if(mgr.legalizer->canPlaceFootprint(clusterCoor, chooseCell)){
                    n_fastPath++;
                } else {
                    clusterCoor = mgr.legalizer->FindPlace(pb.median, chooseCell);
                    if(clusterCoor.x == DBL_MAX && clusterCoor.y == DBL_MAX){
                        n_dropped++;
                        continue;
                    }
                    if(CostCompare(clusterCoor, chooseCell, pb.ffs) < 0){
                        n_dropped++;
                        continue;
                    }
                    n_fallbackOK++;
                }

                FF* newFF = mgr.bankFF(clusterCoor, chooseCell, pb.ffs);
                mgr.legalizer->UpdateRows(newFF);
                newFF->setIsLegalize(true);
                for(FF* oldFF : pb.ffs){
                    oldFF->setClusterIdx(clusterTotalNum);
                    oldFF->setNewCoor(clusterCoor);
                }
                clusterTotalNum++;
            }
        }
        t_merge += ms(t3, tic());
    }

    std::cout << "[BAN_PROFILE] init=" << t_init << "ms par=" << t_par
              << "ms canonical=" << t_canonical << "ms merge=" << t_merge
              << "ms pending=" << n_pending << " fast=" << n_fastPath
              << " fallbackOK=" << n_fallbackOK << " dropped=" << n_dropped
              << std::endl;
}

void Banking::doMatchingClustering(){
    // Stage B v1: Max-weight matching for 2-bit banking, then greedy for
    // higher-bit targets (4-bit, 8-bit, ...) on remaining FFs.

    auto tic = [](){ return std::chrono::high_resolution_clock::now(); };
    auto ms_fn = [](std::chrono::high_resolution_clock::time_point a,
                    std::chrono::high_resolution_clock::time_point b){
        return std::chrono::duration<double, std::milli>(b - a).count();
    };

    int clusterTotalNum = 0;
    size_t max_clk_idx = 0;
    for(const auto &pair : mgr.FF_Map){
        max_clk_idx = std::max((int)max_clk_idx, pair.second->getClkIdx());
    }
    size_t clkCount = max_clk_idx + 1;

    // Find the 2-bit cell
    Cell* cell2bit = nullptr;
    for(const auto &bitLib : mgr.Bit_FF_Map){
        if(bitLib.first == 2){
            cell2bit = bitLib.second[0];
            break;
        }
    }
    if(!cell2bit){
        std::cout << "[MATCHING] No 2-bit cell in library, falling back to greedy" << std::endl;
        doClustering();
        return;
    }

    // Tunable matching parameters via env vars
    int K_NEIGHBORS = 15;
    double WEIGHT_SCALE = 1000.0;
    double EDGE_MIN_GAIN = 0.0;   // minimum CostCompare gain to create an edge
    double DIST_BONUS = 0.1;      // proximity bonus coefficient
    const char* envK = std::getenv("MATCH_K");
    if(envK) K_NEIGHBORS = std::atoi(envK);
    const char* envMinGain = std::getenv("MATCH_MIN_GAIN");
    if(envMinGain) EDGE_MIN_GAIN = std::atof(envMinGain);
    const char* envDistBonus = std::getenv("MATCH_DIST_BONUS");
    if(envDistBonus) DIST_BONUS = std::atof(envDistBonus);

    // v2.1 Window-Optimal Decoupled Banking: use L1 feasibility + window-optimal
    // TNS in edge weight instead of CostCompare(median). Default OFF — per-edge
    // adaptive DIST_BONUS on legacy path outperforms v2.1 across all cases.
    bool useV21 = false;
    {
        const char* envV21 = std::getenv("DECOUPLED_V21");
        if(envV21 && std::string(envV21) == "1") useV21 = true;
    }
    if(useV21) std::cout << "[MATCHING] v2.1 window-optimal edge weight enabled" << std::endl;

    // Per-edge adaptive DIST_BONUS: zero DIST_BONUS for timing-critical pairs.
    // Default mode (RISK_ADAPTIVE=0): slack < 0 = critical. Simple, robust.
    // Risk mode  (RISK_ADAPTIVE=1): slack < D_delay*dist/scale = critical.
    //   Physically motivated but non-monotonic across cases; t2_0812 regresses.
    bool adaptiveDistPerEdge = true;
    bool riskAdaptive = false;
    double riskScale = 4.0;
    {
        const char* envAdapt = std::getenv("ADAPTIVE_DIST");
        if(envAdapt && std::string(envAdapt) == "0") adaptiveDistPerEdge = false;
        const char* envRisk = std::getenv("RISK_ADAPTIVE");
        if(envRisk && std::string(envRisk) == "1") riskAdaptive = true;
        const char* envRS = std::getenv("RISK_SCALE");
        if(envRS) riskScale = std::atof(envRS);
    }
    if(adaptiveDistPerEdge)
        std::cout << "[MATCHING] per-edge adaptive DIST_BONUS enabled"
                  << (riskAdaptive ? " (risk mode, scale=" + std::to_string(riskScale) + ")" : " (slack<0 mode)")
                  << std::endl;

    // Phase 4: Space-aware matching for higher-bit. Only affects 4-bit+ graph build.
    bool spaceAware = false;
    double spaceRadiusMult = 3.0;
    {
        const char* envSA = std::getenv("SPACE_AWARE");
        if(envSA && std::string(envSA) != "0") spaceAware = true;
        const char* envSRM = std::getenv("SPACE_RADIUS_MULT");
        if(envSRM) spaceRadiusMult = std::atof(envSRM);
    }
    if(spaceAware)
        std::cout << "[MATCHING] space-aware 4-bit+ enabled (radius_mult=" << spaceRadiusMult << ")" << std::endl;

    // Phase 4 Step 2: slack-budget sigmoid on edge weight. Attacks cascade by
    // down-weighting merges on timing-critical paths before matching picks them.
    // adjGain = gain * sigmoid(min_slack / scale). When min_slack >>0: multiplier→1
    // (no change). When min_slack <<0: multiplier→0 (edge effectively dropped).
    bool slackSigmoid = false;
    double slackSigmoidScale = 1.0;
    {
        const char* envSG = std::getenv("SLACK_SIGMOID");
        if(envSG && std::string(envSG) != "0") slackSigmoid = true;
        const char* envSGS = std::getenv("SLACK_SIGMOID_SCALE");
        if(envSGS) slackSigmoidScale = std::atof(envSGS);
    }
    if(slackSigmoid)
        std::cout << "[MATCHING] slack-budget sigmoid enabled (scale=" << slackSigmoidScale << ")" << std::endl;

    // Phase 3Z Step 5: inter-batch slack release. After each banked commit,
    // credit upstream/downstream FFs with DisplacementDelay*(oldHPWL-newHPWL)
    // when the MBFF landed closer to them than the original FF was. Credit
    // lands on FF::bankingReleasedSlackD and is read by getEffectiveSlack()
    // only under the same gate, so SLACK_RELEASE=0 is byte-exact baseline.
    bool slackRelease = false;
    double slackReleaseCap = 2.0;   // cap = slackReleaseCap * |snapshot_raw_slack|; 0 disables cap
    {
        const char* envSR = std::getenv("SLACK_RELEASE");
        if(envSR && std::string(envSR) != "0") slackRelease = true;
        const char* envSRC = std::getenv("SLACK_RELEASE_CAP");
        if(envSRC) slackReleaseCap = std::atof(envSRC);
    }
    if(slackRelease)
        std::cout << "[MATCHING] slack release enabled (cap=" << slackReleaseCap << ")" << std::endl;

    // Per-FF raw-slack snapshot used as cap reference. Captured lazily on first
    // credit. Local to this invocation of doMatchingClustering.
    std::unordered_map<FF*, double> slackReleaseSnapshot;

    // Counters (reset per phase by caller; see 2-bit and higher-bit phase blocks).
    int sr_released_edges = 0;
    int sr_skipped_non_ff = 0;
    int sr_n_capped = 0;
    int sr_n_neg_skipped = 0;
    double sr_sum_released = 0.0;
    double sr_max_per_ff = 0.0;

    // Return minimum dynamic D-pin slack across all constituent FFs of a
    // cluster FF. Using getSlack() on constituents picks up position-dependent
    // updates as earlier commits move the driver/load pins. Under SLACK_RELEASE
    // we add bankingReleasedSlackD via getEffectiveSlack so credits from prior
    // batches influence edge weights.
    auto minDynSlack = [&](FF* a) -> double {
        auto& cfs = a->getClusterFF();
        if(cfs.empty()){
            double base = a->getTimingSlack("D");
            return slackRelease ? base + a->getBankingReleasedSlackD() : base;
        }
        double m = DBL_MAX;
        for(FF* o : cfs){
            if(!o) continue;
            double s = slackRelease ? o->getEffectiveSlack() : o->getSlack();
            if(s < m) m = s;
        }
        return m;
    };
    auto slackMul = [&](FF* a, FF* b) -> double {
        if(!slackSigmoid) return 1.0;
        double minSlack = std::min(minDynSlack(a), minDynSlack(b));
        return 1.0 / (1.0 + std::exp(-minSlack / slackSigmoidScale));
    };

    // releaseSlackAfterCommit: D-side credit only. For each 1-bit constituent f
    // of the just-banked MBFF, credit the upstream FF whose Q-pin→f-arc just
    // got shorter. Driver position resolved by: (1) prevStage if it has a
    // critical-path gate (outputGate stable, use gate output pin), else
    // (2) prevInstance with cellType==FF (direct FF→FF, use upstream physical
    // Q pin). Non-FF endpoints (IO drivers, gate-only predecessors with no
    // carrier FF) are skipped with a counter.
    //
    // Q-side credit was removed: moving f shortens arc f.Q→ns.D, but
    // ns->getSlack() already tracks that via its own D-side HPWL delta
    // (see FF.cpp:366-395). Crediting ns on top would double-count.
    //
    // Credit is capped at slackReleaseCap * |snapshot_raw_slack| per FF
    // (first-credit snapshot).
    auto creditOne = [&](FF* target, double delta) {
        if(!target || delta <= 0) return;
        auto it = slackReleaseSnapshot.find(target);
        double snap;
        if(it == slackReleaseSnapshot.end()){
            snap = target->getSlack();
            slackReleaseSnapshot.emplace(target, snap);
        } else {
            snap = it->second;
        }
        double before = target->getBankingReleasedSlackD();
        double after = before + delta;
        if(slackReleaseCap > 0.0){
            double cap = slackReleaseCap * std::abs(snap);
            if(after > cap){
                after = cap;
                if(after > before) sr_n_capped++;
                else { sr_n_capped++; return; } // already at cap, skip
            }
        }
        double applied = after - before;
        if(applied <= 0) return;
        target->addBankingReleasedSlackD(applied);
        sr_released_edges++;
        sr_sum_released += applied;
        double cur = target->getBankingReleasedSlackD();
        if(cur > sr_max_per_ff) sr_max_per_ff = cur;
    };
    auto releaseSlackAfterCommit = [&](FF* newFF,
                                       const std::vector<FF*>& constituents) {
        if(!slackRelease) return;
        if(!newFF) return;
        double dd = FF::DisplacementDelay;
        for(FF* f : constituents){
            if(!f) continue;

            // New D-pin position inside the banked MBFF. getPhysicalPinName()
            // returns "" for 1-bit physical, "<slot>" for multi-bit.
            Coor newDpin = newFF->getNewCoor()
                           + newFF->getPinCoor("D" + f->getPhysicalPinName());

            // Resolve upstream driver position + credit target.
            //   Case 1: prevStage populated (critical path through a gate) —
            //           driver = gate output pin (stable).
            //   Case 2: prevInstance.cellType == FF (direct FF→FF) —
            //           driver = upstream physical Q pin (current MBFF pos).
            //   Case 3: IO / gate-only predecessor — no FF carrier, skip.
            FF*  creditTarget = nullptr;
            Coor driverOld, driverNew;
            bool haveArc = false;

            PrevStage prevS = f->getPrevStage();
            if(prevS.ff && prevS.outputGate){
                Coor gatePin = prevS.outputGate->getCoor()
                               + prevS.outputGate->getPinCoor(prevS.pinName);
                driverOld = gatePin;
                driverNew = gatePin;
                creditTarget = prevS.ff;
                haveArc = true;
            } else {
                PrevInstance prevI = f->getPrevInstance();
                if(prevI.instance && prevI.cellType == CellType::FF){
                    FF* upFF = dynamic_cast<FF*>(prevI.instance);
                    if(upFF && upFF->getPhysicalFF()){
                        driverOld = upFF->getOriginalQ();
                        FF* upPhys = upFF->getPhysicalFF();
                        driverNew = upPhys->getNewCoor()
                                    + upPhys->getPinCoor(
                                          "Q" + upFF->getPhysicalPinName());
                        creditTarget = upFF;
                        haveArc = true;
                    }
                }
            }

            if(!haveArc || !creditTarget){
                sr_skipped_non_ff++;
                continue;
            }

            double oldHPWL = HPWL(driverOld, f->getOriginalD());
            double newHPWL = HPWL(driverNew, newDpin);
            double released = dd * (oldHPWL - newHPWL);
            if(released > 0) creditOne(creditTarget, released);
            else             sr_n_neg_skipped++;
        }
    };
    // Reset phase counters (caller prints + resets before next phase).
    auto resetSRCounters = [&]() {
        sr_released_edges = 0;
        sr_skipped_non_ff = 0;
        sr_n_capped = 0;
        sr_n_neg_skipped = 0;
        sr_sum_released = 0.0;
        sr_max_per_ff = 0.0;
    };
    auto printSRCounters = [&](const std::string& phase) {
        if(!slackRelease) return;
        size_t n_boosted = 0;
        for(const auto& kv : slackReleaseSnapshot){
            if(kv.first->getBankingReleasedSlackD() > 0) n_boosted++;
        }
        std::cout << "[SLACK_RELEASE] phase=" << phase
                  << " released_edges=" << sr_released_edges
                  << " skipped_non_ff=" << sr_skipped_non_ff
                  << " sum_released_ns=" << sr_sum_released
                  << " max_per_ff_boost=" << sr_max_per_ff
                  << " n_ff_boosted=" << n_boosted
                  << " n_capped=" << sr_n_capped
                  << " n_neg_skipped=" << sr_n_neg_skipped
                  << std::endl;
    };

    // Phase 3Z Step 1: Library-aware higher-bit gate.
    // Skip higher-bit matching for a target cell whose best-case area+power
    // saving over 2x the cheapest lower-bit alternative is already <= LIB_GATE_MIN.
    // saving = 2*(beta*minSrcPower + gamma*minSrcArea) - (beta*tgtPower + gamma*tgtArea)
    bool libGate = false;
    double libGateMin = 0.0;
    {
        const char* envLG = std::getenv("LIB_GATE");
        if(envLG && std::string(envLG) != "0") libGate = true;
        const char* envLGM = std::getenv("LIB_GATE_MIN");
        if(envLGM) libGateMin = std::atof(envLGM);
    }
    if(libGate)
        std::cout << "[MATCHING] library-aware higher-bit gate enabled (min="
                  << libGateMin << ")" << std::endl;
    auto libBenefitCeiling = [&](Cell* tgt, int srcBit) -> double {
        auto it = mgr.Bit_FF_Map.find(srcBit);
        if(it == mgr.Bit_FF_Map.end() || it->second.empty()) return 0.0;
        double bestSrc = DBL_MAX;
        for(Cell* c : it->second){
            double cc = mgr.beta * c->getGatePower() + mgr.gamma * c->getArea();
            if(cc < bestSrc) bestSrc = cc;
        }
        double tgtCost = mgr.beta * tgt->getGatePower() + mgr.gamma * tgt->getArea();
        return 2.0 * bestSrc - tgtCost;
    };
    int libGate_skipped = 0;

    // Phase 3Z Step 2: Safety-margin tightening on commit-time realGain.
    // Original check: if(realGain < 0) drop. With margin, check becomes
    // if(realGain < safetyMargin) drop — catches "pair looks good in matching
    // math but ends up bad at actual legal coord".
    // Both default 0.0 → bit-exact with pre-3Z behavior when unset.
    double safetyMargin = 0.0;      // higher-bit
    double safetyMargin2B = 0.0;    // 2-bit
    {
        const char* envSM = std::getenv("SAFETY_MARGIN");
        if(envSM) safetyMargin = std::atof(envSM);
        const char* envSM2 = std::getenv("SAFETY_MARGIN_2B");
        if(envSM2) safetyMargin2B = std::atof(envSM2);
    }
    if(safetyMargin != 0.0 || safetyMargin2B != 0.0)
        std::cout << "[MATCHING] safety margins: 2b=" << safetyMargin2B
                  << " hb=" << safetyMargin << std::endl;
    int hb_dropped_by_margin = 0;
    int n_dropped_by_margin = 0;

    // Phase 3Z Step 4: batched matching with conflict-graph partition.
    // Split each clkIDX's FF set into conflict-disjoint batches via DSatur.
    // Matching runs on each batch in sequence; later batches see the updated
    // placement state (legalizer rows) left by earlier batches.
    // Defaults: off (single batch = full clk domain ⇒ bit-exact baseline).
    // BATCH_MATCHING toggles both 2-bit and higher-bit together; per-path
    // overrides (BATCH_MATCHING_2B / BATCH_MATCHING_HB) allow selective use
    // — higher-bit benefits from cascade mitigation, 2-bit usually doesn't.
    bool batchMatching = false;
    bool batchMatching2B = false;
    bool batchMatchingHB = false;
    int batchHops = 2;
    double batchSlackThresh = 0.0;
    int batchMaxK = 8;
    int batchMinSize = 20;
    {
        const char* envBM = std::getenv("BATCH_MATCHING");
        if(envBM && std::string(envBM) != "0") batchMatching = true;
        const char* envBM2 = std::getenv("BATCH_MATCHING_2B");
        if(envBM2) batchMatching2B = (std::string(envBM2) != "0");
        else       batchMatching2B = batchMatching;
        const char* envBMH = std::getenv("BATCH_MATCHING_HB");
        if(envBMH) batchMatchingHB = (std::string(envBMH) != "0");
        else       batchMatchingHB = batchMatching;
        const char* envBH = std::getenv("BATCH_CONFLICT_HOPS");
        if(envBH) batchHops = std::atoi(envBH);
        const char* envBT = std::getenv("BATCH_SLACK_THRESH");
        if(envBT) batchSlackThresh = std::atof(envBT);
        const char* envBK = std::getenv("BATCH_MAX_K");
        if(envBK) batchMaxK = std::atoi(envBK);
        const char* envBMS = std::getenv("BATCH_MIN_SIZE");
        if(envBMS) batchMinSize = std::atoi(envBMS);
    }
    if(batchMatching2B || batchMatchingHB)
        std::cout << "[MATCHING] batched matching enabled (2B=" << batchMatching2B
                  << " HB=" << batchMatchingHB
                  << " hops=" << batchHops
                  << " slackThresh=" << batchSlackThresh
                  << " maxK=" << batchMaxK
                  << " minSize=" << batchMinSize << ")" << std::endl;
    int batch_total_count_2b = 0, batch_total_count_hb = 0;

    // Normalization: distScale = 1 / avg_nn_dist (computed per clk domain below)
    // so dist * distScale ≈ 1.0 for a typical neighbor distance

    auto t_total = tic();
    double t_graph = 0, t_match = 0, t_commit = 0;
    int total_nodes = 0, total_edges = 0, total_matched = 0;
    int n_committed = 0, n_dropped_place = 0, n_dropped_cost = 0;
    int nEdgesChecked = 0, nEdgesZeroed = 0;  // debug: risk-adaptive stats

    // Phase 4 optional: debank all existing MBFFs to 1-bit before re-matching
    {
        const char* envDA = std::getenv("DEBANK_ALL");
        if(envDA && std::string(envDA) == "1")
            mgr.debankAll();
    }

    // ================================================================
    // Phase 1: Max-weight matching for 2-bit on all 1-bit FFs
    // ================================================================
    mgr.legalizer = new Legalizer(mgr);
    mgr.legalizer->initial();

    for(size_t clkIDX = 0; clkIDX < clkCount; clkIDX++){
        std::vector<FF*> allLocalFFs;
        for(const auto &pair : mgr.FF_Map){
            if((size_t)pair.second->getClkIdx() == clkIDX
               && pair.second->getCell()->getBits() == 1){
                allLocalFFs.push_back(pair.second);
            }
        }
        if(allLocalFFs.size() < 2) continue;

        std::vector<std::vector<FF*>> batches;
        if(batchMatching2B && batchMaxK > 1){
            batches = ConflictPartition::partitionByConflict(
                allLocalFFs, batchHops, batchSlackThresh, batchMaxK, batchMinSize);
            std::cout << "[MATCHING] 2bit clk=" << clkIDX
                      << " n=" << allLocalFFs.size()
                      << " batches=" << batches.size() << " sizes:";
            for(auto& b : batches) std::cout << " " << b.size();
            std::cout << std::endl;
            batch_total_count_2b += (int)batches.size();
        } else {
            batches.push_back(std::move(allLocalFFs));
        }

        for(auto& localFFs : batches){
        if(localFFs.size() < 2) continue;

        auto tg0 = tic();

        // Build rtree
        std::vector<PointWithID> points;
        points.reserve(localFFs.size());
        for(size_t i = 0; i < localFFs.size(); i++){
            FF *ff = localFFs[i];
            points.push_back(std::make_pair(
                Point(ff->getNewCoor().x, ff->getNewCoor().y), (int)i));
        }
        bgi::rtree<PointWithID, bgi::quadratic<P_PER_NODE>> rtree;
        rtree.insert(points.begin(), points.end());

        // Compute avg nearest-neighbor distance for distScale normalization
        double distScale = 1.0;
        {
            double sumNN = 0;
            int nSampled = std::min((int)localFFs.size(), 200);
            int step = std::max(1, (int)localFFs.size() / nSampled);
            int cnt = 0;
            for(int si = 0; si < (int)localFFs.size() && cnt < nSampled; si += step, cnt++){
                std::vector<PointWithID> nn2;
                nn2.reserve(2);
                rtree.query(bgi::nearest(Point(localFFs[si]->getNewCoor().x,
                    localFFs[si]->getNewCoor().y), 2), std::back_inserter(nn2));
                if(nn2.size() == 2){
                    int oi = (nn2[0].second == si) ? 1 : 0;
                    sumNN += HPWL(localFFs[si]->getNewCoor(), localFFs[nn2[oi].second]->getNewCoor());
                }
            }
            double avgNN = (cnt > 0) ? sumNN / cnt : 1.0;
            distScale = (avgNN > 1e-9) ? 1.0 / avgNN : 1.0;
        }

        // Build LEMON graph
        lemon::SmartGraph g;
        std::vector<lemon::SmartGraph::Node> gnodes(localFFs.size());
        for(size_t i = 0; i < localFFs.size(); i++){
            gnodes[i] = g.addNode();
        }

        lemon::SmartGraph::EdgeMap<long long> weight(g);
        int edgeCount = 0;

        // Store optimal positions per node-pair for v2.1 commit phase
        // Key: min(i,j) * localFFs.size() + max(i,j)
        std::unordered_map<size_t, Coor> optPMap;
        auto pairKey = [&](size_t a, size_t b) -> size_t {
            return std::min(a,b) * localFFs.size() + std::max(a,b);
        };

        for(size_t i = 0; i < localFFs.size(); i++){
            FF* ffA = localFFs[i];
            Coor coorA = ffA->getNewCoor();

            std::vector<PointWithID> neighbors;
            neighbors.reserve(K_NEIGHBORS + 1);
            rtree.query(
                bgi::nearest(Point(coorA.x, coorA.y), K_NEIGHBORS + 1),
                std::back_inserter(neighbors));

            for(const auto &nb : neighbors){
                int j = nb.second;
                if(j <= (int)i) continue;

                FF* ffB = localFFs[j];
                Coor coorB = ffB->getNewCoor();
                std::vector<FF*> pair_ffs = {ffA, ffB};

                if(useV21){
                    // v2.1: window-optimal TNS when L1 feasible, median fallback.
                    // No DIST_BONUS — window size reflects proximity naturally.
                    double ra = ffA->getRedistributedSlackD() / mgr.DisplacementDelay;
                    double rb = ffB->getRedistributedSlackD() / mgr.DisplacementDelay;
                    double xlo = std::max(coorA.x - ra, coorB.x - rb);
                    double xhi = std::min(coorA.x + ra, coorB.x + rb);
                    double ylo = std::max(coorA.y - ra, coorB.y - rb);
                    double yhi = std::min(coorA.y + ra, coorB.y + rb);

                    Coor evalP;
                    if(xlo <= xhi && ylo <= yhi){
                        evalP = findWindowOptimal(pair_ffs, cell2bit, xlo, xhi, ylo, yhi);
                    } else {
                        evalP = Coor((coorA.x + coorB.x) / 2.0,
                                     (coorA.y + coorB.y) / 2.0);
                    }

                    // Combined weight: power+area savings - α·ΔTNS (no DIST_BONUS)
                    double oldTNS = 0, newTNS = 0;
                    computePinTNS(pair_ffs, cell2bit, evalP, oldTNS, newTNS);
                    double savings = mgr.beta * (ffA->getCell()->getGatePower()
                                               + ffB->getCell()->getGatePower()
                                               - cell2bit->getGatePower())
                                   + mgr.gamma * (ffA->getCell()->getArea()
                                                + ffB->getCell()->getArea()
                                                - cell2bit->getArea());
                    double gain = savings - mgr.alpha * (newTNS - oldTNS);

                    if(gain > 0){
                        double adjGain = gain * slackMul(ffA, ffB);
                        auto e = g.addEdge(gnodes[i], gnodes[j]);
                        weight[e] = (long long)(adjGain * WEIGHT_SCALE);
                        edgeCount++;
                        optPMap[pairKey(i, j)] = evalP;
                    }
                } else {
                    // Legacy: CostCompare(median) + per-edge adaptive DIST_BONUS
                    Coor median((coorA.x + coorB.x) / 2.0,
                                (coorA.y + coorB.y) / 2.0);
                    double gain = CostCompare(median, cell2bit, pair_ffs);

                    if(gain > EDGE_MIN_GAIN){
                        double adjGain = gain;
                        if(DIST_BONUS > 0){
                            double edgeBonus = DIST_BONUS;
                            double dist = HPWL(coorA, coorB);
                            if(adaptiveDistPerEdge){
                                double slA = ffA->getTimingSlack("D");
                                double slB = ffB->getTimingSlack("D");
                                nEdgesChecked++;
                                bool critical;
                                if(riskAdaptive){
                                    double risk = mgr.DisplacementDelay * dist / riskScale;
                                    critical = (slA < risk || slB < risk);
                                } else {
                                    critical = (slA < 0 || slB < 0);
                                }
                                if(critical){
                                    edgeBonus = 0;
                                    nEdgesZeroed++;
                                }
                            }
                            if(edgeBonus > 0){
                                adjGain += gain * edgeBonus / (1.0 + dist * distScale);
                            }
                        }
                        adjGain *= slackMul(ffA, ffB);
                        auto e = g.addEdge(gnodes[i], gnodes[j]);
                        weight[e] = (long long)(adjGain * WEIGHT_SCALE);
                        edgeCount++;
                    }
                }
            }
        }

        t_graph += ms_fn(tg0, tic());
        total_nodes += (int)localFFs.size();
        total_edges += edgeCount;

        if(edgeCount == 0) continue;

        // Run max-weight matching
        auto tm0 = tic();
        lemon::MaxWeightedMatching<lemon::SmartGraph,
            lemon::SmartGraph::EdgeMap<long long>> mwm(g, weight);
        mwm.run();
        t_match += ms_fn(tm0, tic());

        // Commit matched pairs
        auto tc0 = tic();
        lemon::SmartGraph::NodeMap<int> nodeIdx(g, -1);
        for(size_t i = 0; i < localFFs.size(); i++){
            nodeIdx[gnodes[i]] = (int)i;
        }
        std::vector<bool> committed(localFFs.size(), false);

        for(size_t i = 0; i < localFFs.size(); i++){
            if(committed[i]) continue;
            lemon::SmartGraph::Node mate = mwm.mate(gnodes[i]);
            if(mate == lemon::INVALID) continue;

            int j = nodeIdx[mate];
            if(j < 0 || committed[j]) continue;
            total_matched++;

            FF* ffA = localFFs[i];
            FF* ffB = localFFs[j];

            std::vector<FF*> pair_ffs = {ffA, ffB};

            // FindPlace target: use storedOptP (v2.1) or median (legacy)
            Coor fpTarget;
            if(useV21){
                auto it = optPMap.find(pairKey(i, j));
                if(it != optPMap.end()){
                    fpTarget = it->second;
                } else {
                    fpTarget = Coor((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                                    (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
                }
            } else {
                fpTarget = Coor((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                                (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
            }
            Coor placeCoor = mgr.legalizer->FindPlace(fpTarget, cell2bit);
            if(placeCoor.x == DBL_MAX && placeCoor.y == DBL_MAX){
                n_dropped_place++;
                continue;
            }

            double realGain = CostCompare(placeCoor, cell2bit, pair_ffs);
            if(realGain < 0){
                n_dropped_cost++;
                continue;
            }
            if(safetyMargin2B > 0.0 && realGain < safetyMargin2B){
                n_dropped_by_margin++;
                continue;
            }

            // Step 5: capture logical 1-bit constituents BEFORE bankFF runs;
            // bankFF calls deleteFF(ffA)/deleteFF(ffB) which clears their
            // clusterFF. The logical 1-bit children survive (live in newFF's
            // clusterFF after bankFF) but we need pointers to them now.
            std::vector<FF*> constituents_2b;
            if(slackRelease){
                for(FF* pf : {ffA, ffB}){
                    auto& cfs = pf->getClusterFF();
                    if(cfs.empty()) constituents_2b.push_back(pf);
                    else for(FF* cf : cfs) if(cf) constituents_2b.push_back(cf);
                }
            }

            FF* newFF = mgr.bankFF(placeCoor, cell2bit, pair_ffs);
            mgr.legalizer->UpdateRows(newFF);
            newFF->setIsLegalize(true);

            ffA->setClusterIdx(clusterTotalNum);
            ffA->setNewCoor(placeCoor);
            ffB->setClusterIdx(clusterTotalNum);
            ffB->setNewCoor(placeCoor);

            // Step 5: credit upstream FFs whose Q→f-arc got shorter by this
            // commit (no-op when SLACK_RELEASE=0).
            releaseSlackAfterCommit(newFF, constituents_2b);

            committed[i] = true;
            committed[j] = true;
            clusterTotalNum++;
            n_committed++;
        }
        t_commit += ms_fn(tc0, tic());
        } // end for(auto& localFFs : batches)
    }
    printSRCounters("2bit");
    resetSRCounters();

    std::cout << "[MATCHING] params: K=" << K_NEIGHBORS
              << " minGain=" << EDGE_MIN_GAIN
              << " distBonus=" << DIST_BONUS << std::endl;
    std::cout << "[MATCHING] 2bit: graph=" << t_graph << "ms match=" << t_match
              << "ms commit=" << t_commit << "ms"
              << " nodes=" << total_nodes << " edges=" << total_edges
              << " matched=" << total_matched << " committed=" << n_committed
              << " dropped_place=" << n_dropped_place
              << " dropped_cost=" << n_dropped_cost
              << " dropped_margin=" << n_dropped_by_margin << std::endl;
    if(nEdgesChecked > 0)
        std::cout << "[MATCHING] risk-adaptive: " << nEdgesZeroed << "/" << nEdgesChecked
                  << " edges zeroed (" << (100.0*nEdgesZeroed/nEdgesChecked) << "%)" << std::endl;

    // ================================================================
    // Phase 2+3: For each higher-bit target (4, 8, ...):
    //   (a) Max-weight matching on sourceBit pairs (if applicable)
    //   (b) Greedy fallback for remaining FFs
    // Both share the same legalizer so placements are consistent.
    // ================================================================
    std::map<int, std::vector<Cell *>> orderBitMap(mgr.Bit_FF_Map.begin(), mgr.Bit_FF_Map.end());
    for(const auto &bitLib : orderBitMap){
        Cell* chooseCell = bitLib.second[0];
        int targetBit = chooseCell->getBits();
        if(targetBit <= 2) continue;
        int sourceBit = targetBit / 2;
        bool canMatch = (mgr.Bit_FF_Map.find(sourceBit) != mgr.Bit_FF_Map.end());

        // Fresh legalizer for this bit level
        delete mgr.legalizer;
        mgr.legalizer = new Legalizer(mgr);
        mgr.legalizer->initial();

        // --- (a) Matching phase: pair sourceBit MBFFs ---
        // Higher-bit matching: default OFF for both legacy and v2.1.
        // 4-bit merges still cause excessive displacement; CostCompare re-verify
        // at commit rejects ~45% but remaining merges still hurt TNS.
        bool doHigherMatch = canMatch;
        if(doHigherMatch){
            const char* envHB = std::getenv("MATCH_HIGHER_BIT");
            doHigherMatch = (envHB && std::string(envHB) != "0");
        }
        if(doHigherMatch && libGate){
            double ceiling = libBenefitCeiling(chooseCell, sourceBit);
            if(ceiling <= libGateMin){
                std::cout << "[MATCHING] " << targetBit << "bit: LIB_GATE skip"
                          << " (ceiling=" << ceiling
                          << " <= " << libGateMin
                          << ", tgt=" << chooseCell->getCellName() << ")" << std::endl;
                doHigherMatch = false;
                libGate_skipped++;
            }
        }
        if(doHigherMatch){
            std::cout << "[MATCHING] " << targetBit << "bit: pairing "
                      << sourceBit << "+" << sourceBit << " MBFFs" << std::endl;

            double t_graph_hb = 0, t_match_hb = 0, t_commit_hb = 0;
            int hb_nodes = 0, hb_edges = 0, hb_matched = 0;
            int hb_committed = 0, hb_dropped_place = 0, hb_dropped_cost = 0;
            int hb_space_found = 0, hb_space_missed = 0;
            double hb_sumDisp = 0, hb_maxDisp = 0;
            int hb_nCFs = 0;
            double hb_sumPowerSav = 0, hb_sumAreaSav = 0, hb_sumTNSCost = 0;

            for(size_t clkIDX = 0; clkIDX < clkCount; clkIDX++){
                std::vector<FF*> allLocalFFs;
                for(const auto &pair : mgr.FF_Map){
                    if((size_t)pair.second->getClkIdx() == clkIDX
                       && pair.second->getCell()->getBits() == sourceBit){
                        allLocalFFs.push_back(pair.second);
                    }
                }
                if(allLocalFFs.size() < 2) continue;

                std::vector<std::vector<FF*>> batches_hb;
                if(batchMatchingHB){
                    batches_hb = ConflictPartition::partitionByConflict(
                        allLocalFFs, batchHops, batchSlackThresh, batchMaxK, batchMinSize);
                    std::cout << "[MATCHING] " << targetBit << "bit clk=" << clkIDX
                              << " n=" << allLocalFFs.size()
                              << " batches=" << batches_hb.size() << " sizes:";
                    for(auto& b : batches_hb) std::cout << " " << b.size();
                    std::cout << std::endl;
                    batch_total_count_hb += (int)batches_hb.size();
                } else {
                    batches_hb.push_back(std::move(allLocalFFs));
                }

                for(auto& localFFs : batches_hb){
                if(localFFs.size() < 2) continue;

                auto tg0 = tic();

                std::vector<PointWithID> points;
                points.reserve(localFFs.size());
                for(size_t i = 0; i < localFFs.size(); i++){
                    FF *ff = localFFs[i];
                    points.push_back(std::make_pair(
                        Point(ff->getNewCoor().x, ff->getNewCoor().y), (int)i));
                }
                bgi::rtree<PointWithID, bgi::quadratic<P_PER_NODE>> rtree_hb;
                rtree_hb.insert(points.begin(), points.end());

                double distScale_hb = 1.0;
                double avgNN_hb = 1.0;
                {
                    double sumNN = 0;
                    int nSampled = std::min((int)localFFs.size(), 200);
                    int step = std::max(1, (int)localFFs.size() / nSampled);
                    int cnt = 0;
                    for(int si = 0; si < (int)localFFs.size() && cnt < nSampled; si += step, cnt++){
                        std::vector<PointWithID> nn2;
                        nn2.reserve(2);
                        rtree_hb.query(bgi::nearest(Point(localFFs[si]->getNewCoor().x,
                            localFFs[si]->getNewCoor().y), 2), std::back_inserter(nn2));
                        if(nn2.size() == 2){
                            int oi = (nn2[0].second == si) ? 1 : 0;
                            sumNN += HPWL(localFFs[si]->getNewCoor(), localFFs[nn2[oi].second]->getNewCoor());
                        }
                    }
                    avgNN_hb = (cnt > 0) ? sumNN / cnt : 1.0;
                    distScale_hb = (avgNN_hb > 1e-9) ? 1.0 / avgNN_hb : 1.0;
                }

                lemon::SmartGraph g_hb;
                std::vector<lemon::SmartGraph::Node> gnodes_hb(localFFs.size());
                for(size_t i = 0; i < localFFs.size(); i++){
                    gnodes_hb[i] = g_hb.addNode();
                }

                lemon::SmartGraph::EdgeMap<long long> weight_hb(g_hb);
                int edgeCount_hb = 0;

                // Store optimal positions for v2.1 commit
                std::unordered_map<size_t, Coor> optPMap_hb;
                // Phase 4: store pre-computed legal space positions for commit
                std::unordered_map<size_t, Coor> spaceMap_hb;
                auto pairKey_hb = [&](size_t a, size_t b) -> size_t {
                    return std::min(a,b) * localFFs.size() + std::max(a,b);
                };
                double maxSpaceDist = avgNN_hb * spaceRadiusMult;

                for(size_t i = 0; i < localFFs.size(); i++){
                    FF* ffA = localFFs[i];
                    Coor coorA = ffA->getNewCoor();

                    std::vector<PointWithID> neighbors;
                    neighbors.reserve(K_NEIGHBORS + 1);
                    rtree_hb.query(
                        bgi::nearest(Point(coorA.x, coorA.y), K_NEIGHBORS + 1),
                        std::back_inserter(neighbors));

                    for(const auto &nb : neighbors){
                        int j = nb.second;
                        if(j <= (int)i) continue;

                        FF* ffB = localFFs[j];
                        Coor coorB = ffB->getNewCoor();
                        std::vector<FF*> pair_ffs = {ffA, ffB};

                        // Compute candidate position (used by both v2.1 and legacy)
                        Coor evalP;
                        if(useV21){
                            std::vector<FF*> allCFs;
                            for(FF* mbff : pair_ffs)
                                for(auto& cf : mbff->getClusterFF())
                                    allCFs.push_back(cf);
                            double xlo_w = -DBL_MAX, xhi_w = DBL_MAX;
                            double ylo_w = -DBL_MAX, yhi_w = DBL_MAX;
                            for(FF* cf : allCFs){
                                double r = cf->getRedistributedSlackD() / mgr.DisplacementDelay;
                                FF* phys = cf->getPhysicalFF();
                                Coor cfc = phys ? phys->getNewCoor() : cf->getNewCoor();
                                xlo_w = std::max(xlo_w, cfc.x - r);
                                xhi_w = std::min(xhi_w, cfc.x + r);
                                ylo_w = std::max(ylo_w, cfc.y - r);
                                yhi_w = std::min(yhi_w, cfc.y + r);
                            }
                            if(xlo_w <= xhi_w && ylo_w <= yhi_w)
                                evalP = findWindowOptimal(pair_ffs, chooseCell,
                                    xlo_w, xhi_w, ylo_w, yhi_w);
                            else
                                evalP = Coor((coorA.x + coorB.x) / 2.0,
                                             (coorA.y + coorB.y) / 2.0);
                        } else {
                            evalP = Coor((coorA.x + coorB.x) / 2.0,
                                         (coorA.y + coorB.y) / 2.0);
                        }

                        // Phase 4: space feasibility check
                        Coor costEvalPos = evalP;
                        if(spaceAware){
                            Coor spaceCoor = mgr.legalizer->FindNearestLegalSpace(
                                evalP, chooseCell, maxSpaceDist);
                            if(spaceCoor.x == DBL_MAX){
                                hb_space_missed++;
                                continue;
                            }
                            hb_space_found++;
                            costEvalPos = spaceCoor;
                            spaceMap_hb[pairKey_hb(i, j)] = spaceCoor;
                        }

                        // Compute gain
                        double gain;
                        if(useV21){
                            double oldTNS_h = 0, newTNS_h = 0;
                            computePinTNS(pair_ffs, chooseCell, costEvalPos, oldTNS_h, newTNS_h);
                            double savings_h = 0;
                            for(FF* ff : pair_ffs){
                                savings_h += mgr.beta * ff->getCell()->getGatePower()
                                           + mgr.gamma * ff->getCell()->getArea();
                            }
                            savings_h -= mgr.beta * chooseCell->getGatePower()
                                       + mgr.gamma * chooseCell->getArea();
                            gain = savings_h - mgr.alpha * (newTNS_h - oldTNS_h);
                        } else {
                            gain = CostCompare(costEvalPos, chooseCell, pair_ffs);
                        }

                        if(gain <= EDGE_MIN_GAIN) continue;

                        double adjGain = gain;
                        if(!useV21 && DIST_BONUS > 0){
                            double dist = HPWL(coorA, coorB);
                            adjGain += gain * DIST_BONUS / (1.0 + dist * distScale_hb);
                        }
                        adjGain *= slackMul(ffA, ffB);

                        auto e = g_hb.addEdge(gnodes_hb[i], gnodes_hb[j]);
                        weight_hb[e] = (long long)(adjGain * WEIGHT_SCALE);
                        edgeCount_hb++;
                        if(useV21) optPMap_hb[pairKey_hb(i, j)] = costEvalPos;
                    }
                }

                t_graph_hb += ms_fn(tg0, tic());
                hb_nodes += (int)localFFs.size();
                hb_edges += edgeCount_hb;

                if(edgeCount_hb == 0) continue;

                auto tm0 = tic();
                lemon::MaxWeightedMatching<lemon::SmartGraph,
                    lemon::SmartGraph::EdgeMap<long long>> mwm_hb(g_hb, weight_hb);
                mwm_hb.run();
                t_match_hb += ms_fn(tm0, tic());

                // --- Commit matched pairs ---
                auto tc0 = tic();
                lemon::SmartGraph::NodeMap<int> nodeIdx_hb(g_hb, -1);
                for(size_t i = 0; i < localFFs.size(); i++){
                    nodeIdx_hb[gnodes_hb[i]] = (int)i;
                }
                std::vector<bool> committed_hb(localFFs.size(), false);
                hb_matched = 0;

                for(size_t i = 0; i < localFFs.size(); i++){
                    if(committed_hb[i]) continue;
                    lemon::SmartGraph::Node mate = mwm_hb.mate(gnodes_hb[i]);
                    if(mate == lemon::INVALID) continue;
                    int j = nodeIdx_hb[mate];
                    if(j < 0 || committed_hb[j]) continue;
                    hb_matched++;

                    FF* ffA = localFFs[i];
                    FF* ffB = localFFs[j];
                    std::vector<FF*> pair_ffs = {ffA, ffB};

                    Coor fpTarget;
                    if(spaceAware){
                        auto it = spaceMap_hb.find(pairKey_hb(i, j));
                        if(it != spaceMap_hb.end())
                            fpTarget = it->second;
                        else
                            fpTarget = Coor((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                                            (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
                    } else if(useV21){
                        auto it = optPMap_hb.find(pairKey_hb(i, j));
                        if(it != optPMap_hb.end())
                            fpTarget = it->second;
                        else
                            fpTarget = Coor((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                                            (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
                    } else {
                        fpTarget = Coor((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                                        (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
                    }
                    // Re-validate: space may have been consumed by earlier commits
                    Coor placeCoor = mgr.legalizer->FindNearestLegalSpace(
                        fpTarget, chooseCell, avgNN_hb);
                    if(placeCoor.x == DBL_MAX)
                        placeCoor = mgr.legalizer->FindPlace(fpTarget, chooseCell);
                    if(placeCoor.x == DBL_MAX && placeCoor.y == DBL_MAX){
                        hb_dropped_place++;
                        continue;
                    }

                    double realGain = CostCompare(placeCoor, chooseCell, pair_ffs);
                    if(realGain < 0){
                        hb_dropped_cost++;
                        continue;
                    }
                    if(safetyMargin > 0.0 && realGain < safetyMargin){
                        hb_dropped_by_margin++;
                        continue;
                    }

                    // Diagnostic: compute per-constituent displacement
                    double maxCFDisp = 0;
                    double sumCFDisp = 0;
                    int nCFs = 0;
                    for(FF* mbff : pair_ffs){
                        for(auto& cf : mbff->getClusterFF()){
                            FF* phys = cf->getPhysicalFF();
                            Coor cfPos = phys ? phys->getNewCoor() : cf->getNewCoor();
                            double d = HPWL(cfPos, placeCoor);
                            sumCFDisp += d;
                            maxCFDisp = std::max(maxCFDisp, d);
                            nCFs++;
                        }
                    }
                    hb_sumDisp += sumCFDisp;
                    hb_maxDisp = std::max(hb_maxDisp, maxCFDisp);
                    hb_nCFs += nCFs;

                    // Diagnostic: cost component breakdown
                    double powerSav = 0, areaSav = 0;
                    for(FF* ff : pair_ffs){
                        powerSav += mgr.beta * ff->getCell()->getGatePower();
                        areaSav += mgr.gamma * ff->getCell()->getArea();
                    }
                    powerSav -= mgr.beta * chooseCell->getGatePower();
                    areaSav -= mgr.gamma * chooseCell->getArea();
                    double oldTNS_d = 0, newTNS_d = 0;
                    computePinTNS(pair_ffs, chooseCell, placeCoor, oldTNS_d, newTNS_d);
                    double tnsCost = mgr.alpha * (newTNS_d - oldTNS_d);
                    hb_sumPowerSav += powerSav;
                    hb_sumAreaSav += areaSav;
                    hb_sumTNSCost += tnsCost;

                    // Step 5: capture logical 1-bit constituents BEFORE bankFF
                    // clears pair_ffs wrappers. Flatten each wrapper's
                    // clusterFF (may be empty for a fresh 1-bit).
                    std::vector<FF*> hb_constituents;
                    if(slackRelease){
                        for(FF* pf : pair_ffs){
                            auto& cfs = pf->getClusterFF();
                            if(cfs.empty()) hb_constituents.push_back(pf);
                            else for(FF* cf : cfs) if(cf) hb_constituents.push_back(cf);
                        }
                    }

                    FF* newFF = mgr.bankFF(placeCoor, chooseCell, pair_ffs);
                    mgr.legalizer->UpdateRows(newFF);
                    newFF->setIsLegalize(true);

                    if(slackRelease)
                        releaseSlackAfterCommit(newFF, hb_constituents);

                    committed_hb[i] = true;
                    committed_hb[j] = true;
                    clusterTotalNum++;
                    hb_committed++;
                }
                t_commit_hb += ms_fn(tc0, tic());
                } // end for(auto& localFFs : batches_hb)
            }
            printSRCounters(std::to_string(targetBit) + "bit");
            resetSRCounters();

            std::cout << "[MATCHING] " << targetBit << "bit: graph=" << t_graph_hb
                      << "ms match=" << t_match_hb << "ms commit=" << t_commit_hb << "ms"
                      << " nodes=" << hb_nodes << " edges=" << hb_edges
                      << " matched=" << hb_matched << " committed=" << hb_committed
                      << " dropped_place=" << hb_dropped_place
                      << " dropped_cost=" << hb_dropped_cost
                      << " dropped_margin=" << hb_dropped_by_margin;
            if(spaceAware)
                std::cout << " space_found=" << hb_space_found
                          << " space_missed=" << hb_space_missed;
            std::cout << std::endl;
            if(hb_committed > 0){
                std::cout << "[MATCHING] " << targetBit << "bit diag:"
                          << " avgCFDisp=" << hb_sumDisp / hb_nCFs
                          << " maxCFDisp=" << hb_maxDisp
                          << " nCFs=" << hb_nCFs
                          << " powerSav=" << hb_sumPowerSav
                          << " areaSav=" << hb_sumAreaSav
                          << " tnsCost=" << hb_sumTNSCost
                          << " netGain=" << (hb_sumPowerSav + hb_sumAreaSav - hb_sumTNSCost)
                          << std::endl;
            }

            // Rebuild legalizer to see committed matching results before greedy
            delete mgr.legalizer;
            mgr.legalizer = new Legalizer(mgr);
            mgr.legalizer->initial();
        }

        // --- (b) Greedy fallback: cluster remaining FFs into targetBit ---
        DEBUG_BAN("Cluster " + std::to_string(targetBit) + " Bit MBFF (greedy fallback)");

        for(size_t clkIDX = 0; clkIDX < clkCount; clkIDX++){
            std::vector<FF*> localFFs;
            for(const auto &pair : mgr.FF_Map){
                if((size_t)pair.second->getClkIdx() == clkIDX){
                    pair.second->setIsLegalize(false);
                    localFFs.push_back(pair.second);
                }
            }
            if(localFFs.empty()) continue;

            std::vector<PointWithID> points;
            points.reserve(localFFs.size());
            for(size_t i = 0; i < localFFs.size(); i++){
                FF *ff = localFFs[i];
                points.push_back(std::make_pair(
                    Point(ff->getNewCoor().x, ff->getNewCoor().y), (int)i));
            }
            bgi::rtree<PointWithID, bgi::quadratic<P_PER_NODE>> rtree;
            rtree.insert(points.begin(), points.end());
            std::vector<bool> isClustered(localFFs.size(), false);

            for(size_t index = 0; index < localFFs.size(); index++){
                FF* nowFF = localFFs[index];
                if(isClustered[index]) continue;
                std::vector<PointWithID> resultFFs, toRemoveFFs;
                resultFFs.reserve(mgr.MaxBit);
                rtree.query(bgi::nearest(Point(nowFF->getNewCoor().x,
                    nowFF->getNewCoor().y), mgr.MaxBit),
                    std::back_inserter(resultFFs));
                std::vector<FF*> FFToBank;
                bool isChoose = chooseCandidateFF(nowFF, localFFs, resultFFs,
                    toRemoveFFs, FFToBank, targetBit);

                if(isChoose){
                    Coor medianCoor = getMedian(localFFs, toRemoveFFs);
                    Coor clusterCoor = mgr.legalizer->FindPlace(medianCoor, chooseCell);
                    if(clusterCoor.x == DBL_MAX && clusterCoor.y == DBL_MAX)
                        continue;
                    if(CostCompare(clusterCoor, chooseCell, FFToBank) < 0)
                        continue;

                    FF* newFF = mgr.bankFF(clusterCoor, chooseCell, FFToBank);
                    mgr.legalizer->UpdateRows(newFF);
                    newFF->setIsLegalize(true);

                    for(size_t j = 0; j < toRemoveFFs.size(); j++){
                        isClustered[toRemoveFFs[j].second] = true;
                    }
                    rtree.remove(toRemoveFFs.begin(), toRemoveFFs.end());

                    for(FF* oldFF : FFToBank){
                        oldFF->setClusterIdx(clusterTotalNum);
                        oldFF->setNewCoor(clusterCoor);
                    }
                    clusterTotalNum++;
                }
            }
        }
    }

    double t_total_ms = ms_fn(t_total, tic());
    std::cout << "[MATCHING] total=" << t_total_ms << "ms" << std::endl;
}

void Banking::restoreUnclusterFFCoor(){
    for(const auto &MBFF : mgr.FF_Map){
        std::vector<FF*> clusterFFs = MBFF.second->getClusterFF();
        if(clusterFFs.size() == 1){
            Coor originalCoor = MBFF.second->getCoor();
            MBFF.second->setNewCoor(originalCoor);
        }
    }
}

void Banking::ClusterResult(){
    for(const auto &MBFF : mgr.FF_Map){
        std::vector<FF*> clusterFFs = MBFF.second->getClusterFF();
        clusterNum[clusterFFs.size()]++;
    }
    std::map<int, int> clusterMap(clusterNum.begin(), clusterNum.end()); 
    DEBUG_BAN("[CLUSTER RESULT]");
    for(const auto &cluster : clusterMap){
        DEBUG_BAN("\t\tFF" +  std::to_string(cluster.first) + " : " + std::to_string(cluster.second));
    }
}