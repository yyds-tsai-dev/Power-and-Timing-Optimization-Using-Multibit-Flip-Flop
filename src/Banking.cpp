#include "Banking.h"
#include <omp.h>
#include <chrono>

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
        doClustering();
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

double Banking::CostCompare(const Coor clusterCoor, Cell* chooseCell, std::vector<FF*> FFToBank){
    double costOptimize = 0;
    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        //costOptimize += mgr.alpha * (ff->getCell()->getQpinDelay());
        costOptimize += mgr.beta * (ff->getCell()->getGatePower());
        costOptimize += mgr.gamma * (ff->getCell()->getArea());
    }
    costOptimize -= mgr.beta * (chooseCell->getGatePower()) + mgr.gamma * (chooseCell->getArea());
    double increaseTNS = 0;
    double slackOvershoot = 0;
    const double slackW = mgr.param.SLACK_OVERSHOOT_WEIGHT;
    for(size_t i = 0; i < FFToBank.size(); i++){
        FF* ff = FFToBank[i];
        int affectNum = 1;
        for(const auto & clusterFF : ff->getClusterFF()){
            affectNum += clusterFF->getNextStage().size();
            costOptimize += (ff->getCell()->getQpinDelay() - chooseCell->getQpinDelay()) * clusterFF->getNextStage().size();
        }
        double predictedDelay = mgr.DisplacementDelay * HPWL(ff->getNewCoor(), clusterCoor);
        increaseTNS += predictedDelay * affectNum;

        // Phase 3C (a): slack-aware soft penalty. Uses the D-pin slack of the
        // to-be-banked FFs; banks whose displacement eats into negative slack
        // are extra-penalized, but nothing is hard-rejected.
        // Method D Stage A — Step 2: when SLACK_REDIST_MODE > 0, read the
        // path-aware redistributed budget instead of the raw D-pin slack. For
        // 1-bit FFs (banking input) this is always populated by
        // Manager::computeSlackRedistribution(). Multi-bit inputs fall back to
        // the raw min-pin slack.
        if(slackW > 0){
            double slackD;
            const bool useRedist = (mgr.param.SLACK_REDIST_MODE > 0)
                                   && (ff->getClusterFF().size() <= 1);
            if(useRedist){
                slackD = ff->getRedistributedSlackD();
            } else if(ff->getClusterFF().size() <= 1){
                slackD = ff->getTimingSlack("D");
            } else {
                slackD = DBL_MAX;
                for(size_t s = 0; s < ff->getClusterFF().size(); s++){
                    double sd = ff->getTimingSlack("D" + std::to_string(s));
                    if(sd < slackD) slackD = sd;
                }
            }
            double overshoot = predictedDelay - slackD;
            if(overshoot > 0) slackOvershoot += overshoot * affectNum;
        }
    }
    costOptimize -= mgr.alpha * increaseTNS;
    if(slackW > 0) costOptimize -= mgr.alpha * slackW * slackOvershoot;
    // if(costOptimize > -100 && costOptimize < 0) std::cout << costOptimize << std::endl;
    return costOptimize;
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