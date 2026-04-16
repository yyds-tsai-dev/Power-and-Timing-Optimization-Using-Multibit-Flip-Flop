#include "Banking.h"
#include <omp.h>
#include <chrono>
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
    // Normalization: distScale = 1 / avg_nn_dist (computed per clk domain below)
    // so dist * distScale ≈ 1.0 for a typical neighbor distance

    auto t_total = tic();
    double t_graph = 0, t_match = 0, t_commit = 0;
    int total_nodes = 0, total_edges = 0, total_matched = 0;
    int n_committed = 0, n_dropped_place = 0, n_dropped_cost = 0;

    // ================================================================
    // Phase 1: Max-weight matching for 2-bit on all 1-bit FFs
    // ================================================================
    mgr.legalizer = new Legalizer(mgr);
    mgr.legalizer->initial();

    for(size_t clkIDX = 0; clkIDX < clkCount; clkIDX++){
        std::vector<FF*> localFFs;
        for(const auto &pair : mgr.FF_Map){
            if((size_t)pair.second->getClkIdx() == clkIDX
               && pair.second->getCell()->getBits() == 1){
                localFFs.push_back(pair.second);
            }
        }
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
                Coor median((coorA.x + coorB.x) / 2.0,
                            (coorA.y + coorB.y) / 2.0);
                std::vector<FF*> pair_ffs = {ffA, ffB};
                double gain = CostCompare(median, cell2bit, pair_ffs);

                // Distance-aware edge weight: proximity bonus for closer pairs.
                // gain * DIST_BONUS / (1 + normDist) rewards pairs that are
                // near each other, since FindPlace is more likely to succeed
                // close to the median for close pairs.
                if(gain > EDGE_MIN_GAIN){
                    double adjGain = gain;
                    if(DIST_BONUS > 0){
                        double dist = HPWL(coorA, coorB);
                        adjGain += gain * DIST_BONUS / (1.0 + dist * distScale);
                    }
                    auto e = g.addEdge(gnodes[i], gnodes[j]);
                    weight[e] = (long long)(adjGain * WEIGHT_SCALE);
                    edgeCount++;
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

            Coor median((ffA->getNewCoor().x + ffB->getNewCoor().x) / 2.0,
                        (ffA->getNewCoor().y + ffB->getNewCoor().y) / 2.0);
            Coor placeCoor = mgr.legalizer->FindPlace(median, cell2bit);
            if(placeCoor.x == DBL_MAX && placeCoor.y == DBL_MAX){
                n_dropped_place++;
                continue;
            }

            double realGain = CostCompare(placeCoor, cell2bit, pair_ffs);
            if(realGain < 0){
                n_dropped_cost++;
                continue;
            }

            FF* newFF = mgr.bankFF(placeCoor, cell2bit, pair_ffs);
            mgr.legalizer->UpdateRows(newFF);
            newFF->setIsLegalize(true);

            ffA->setClusterIdx(clusterTotalNum);
            ffA->setNewCoor(placeCoor);
            ffB->setClusterIdx(clusterTotalNum);
            ffB->setNewCoor(placeCoor);

            committed[i] = true;
            committed[j] = true;
            clusterTotalNum++;
            n_committed++;
        }
        t_commit += ms_fn(tc0, tic());
    }

    std::cout << "[MATCHING] params: K=" << K_NEIGHBORS
              << " minGain=" << EDGE_MIN_GAIN
              << " distBonus=" << DIST_BONUS << std::endl;
    std::cout << "[MATCHING] 2bit: graph=" << t_graph << "ms match=" << t_match
              << "ms commit=" << t_commit << "ms"
              << " nodes=" << total_nodes << " edges=" << total_edges
              << " matched=" << total_matched << " committed=" << n_committed
              << " dropped_place=" << n_dropped_place
              << " dropped_cost=" << n_dropped_cost << std::endl;

    // ================================================================
    // Phase 2: Greedy for higher-bit targets (4, 8, ...)
    // ================================================================
    std::map<int, std::vector<Cell *>> orderBitMap(mgr.Bit_FF_Map.begin(), mgr.Bit_FF_Map.end());
    for(const auto &bitLib : orderBitMap){
        Cell* chooseCell = bitLib.second[0];
        int targetBit = chooseCell->getBits();
        if(targetBit <= 2) continue;
        DEBUG_BAN("Cluster " + std::to_string(targetBit) + " Bit MBFF (greedy fallback)");

        delete mgr.legalizer;
        mgr.legalizer = new Legalizer(mgr);
        mgr.legalizer->initial();

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