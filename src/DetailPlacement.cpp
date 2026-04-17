#include "DetailPlacement.h"

DetailPlacement::DetailPlacement(Manager &mgr) : mgr(mgr){
    this->legalizer = mgr.legalizer;
    for(const auto &ff : legalizer->ffs){
        cellSet.insert(ff->getCell());
    }
    CheckSwapSanity();
}

DetailPlacement::~DetailPlacement(){

}

void DetailPlacement::run(){
    DEBUG_DP("Running detail placement!");
    BuildGlobalRtreeMaps();

    const int MAX_ITER = 10;
    for(int iter = 0; iter < MAX_ITER; iter++){
        size_t swaps = GlobalSwap();
        size_t changes = ChangeCell();
        DEBUG_DP("Iter " << iter << ": swaps=" << swaps << " cellChanges=" << changes);
        if(swaps == 0 && changes == 0) break;
        if(changes > 0) BuildGlobalRtreeMaps();
    }
}

void DetailPlacement::BuildGlobalRtreeMaps(){
    DEBUG_DP("Build Global Rtree");
    // init/reset rtree
    for(const auto &cell : cellSet){
        RtreeMaps[cell] = RTree();
    }

    // sort the ff by TNS
    std::sort(legalizer->ffs.begin(), legalizer->ffs.end(), [](const Node *a, const Node *b){
        return a->getTNS() > b->getTNS();
    });

    // insert ff into rtree
    for(size_t i = 0; i < legalizer->ffs.size(); i++){
        Node *ff = legalizer->ffs[i];
        PointWithID pointwithid;
        pointwithid = std::make_pair(Point(ff->getLGCoor().x, ff->getLGCoor().y), i);
        RtreeMaps[ff->getCell()].insert(pointwithid);
    }
}

void DetailPlacement::CheckSwapSanity(){
    DEBUG_DP("Swap Sanity Checker");
    for(const auto &ff : legalizer->ffs){
        assert(ff->getLGCoor().y == legalizer->rows[ff->getPlaceRowIdx()]->getStartCoor().y);
        assert(cellSet.find(ff->getCell()) != cellSet.end());
    }
}

size_t DetailPlacement::GlobalSwap(){
    DEBUG_DP("Global Swap");
    int GS_K = 5;
    if(const char* e = std::getenv("GS_K")) GS_K = std::atoi(e);
    size_t swapCount = 0;
    for(size_t id = 0; id < legalizer->ffs.size(); id++){
        Node *ff = legalizer->ffs[id];
        Node *ff_current = ff;
        Coor origCoorA = ff_current->getLGCoor();
        double costA = ff_current->getFFPtr()->getCost();

        // Query K nearest same-cell-type FFs near GP coordinate
        Point queryPoint(ff->getGPCoor().x, ff->getGPCoor().y);
        std::vector<PointWithID> nearestResults;
        RtreeMaps[ff->getCell()].query(bgi::nearest(queryPoint, GS_K), std::back_inserter(nearestResults));

        // Find best swap partner among K candidates
        double bestImprovement = 0;
        int bestIdx = -1;
        PointWithID bestPoint;
        for(auto& candidate : nearestResults){
            if(candidate.second == (int)id) continue;

            Node *ff_target = legalizer->ffs[candidate.second];
            Coor origCoorB = ff_target->getLGCoor();

            double costBefore = costA + ff_target->getFFPtr()->getCost();

            // Tentatively swap
            ff_current->getFFPtr()->setNewCoor(origCoorB);
            ff_target->getFFPtr()->setNewCoor(origCoorA);

            double costAfter = ff_current->getFFPtr()->getCost()
                             + ff_target->getFFPtr()->getCost();

            // Undo
            ff_current->getFFPtr()->setNewCoor(origCoorA);
            ff_target->getFFPtr()->setNewCoor(origCoorB);

            double improvement = costBefore - costAfter;
            if(improvement > bestImprovement){
                bestImprovement = improvement;
                bestIdx = candidate.second;
                bestPoint = candidate;
            }
        }

        if(bestIdx < 0) continue; // no improving swap found
        swapCount++;

        // Commit the best swap
        Node *ff_choose_to_swap = legalizer->ffs[bestIdx];
        Coor origCoorB = ff_choose_to_swap->getLGCoor();

        ff_current->getFFPtr()->setNewCoor(origCoorB);
        ff_choose_to_swap->getFFPtr()->setNewCoor(origCoorA);
        ff_current->setLGCoor(ff_current->getFFPtr()->getNewCoor());
        ff_choose_to_swap->setLGCoor(ff_choose_to_swap->getFFPtr()->getNewCoor());

        size_t ff_current_placeIdx = ff->getPlaceRowIdx();
        size_t ff_choose_to_swap_placeIdx = ff_choose_to_swap->getPlaceRowIdx();
        ff->setPlaceRowIdx(ff_choose_to_swap_placeIdx);
        ff_choose_to_swap->setPlaceRowIdx(ff_current_placeIdx);

        // Maintain rtree: remove old, insert new for both FFs
        RtreeMaps[ff->getCell()].remove(bestPoint);
        PointWithID oldEntry = std::make_pair(Point(origCoorA.x, origCoorA.y), id);
        RtreeMaps[ff->getCell()].remove(oldEntry);

        PointWithID newEntryA = std::make_pair(Point(origCoorB.x, origCoorB.y), id);
        PointWithID newEntryB = std::make_pair(Point(origCoorA.x, origCoorA.y), bestIdx);
        RtreeMaps[ff->getCell()].insert(newEntryA);
        RtreeMaps[ff->getCell()].insert(newEntryB);
    }
    CheckSwapSanity();
    return swapCount;
}

void DetailPlacement::DetailAssignmentMBFF(){
    DEBUG_DP("DetailAssignmentMBFF");
    srand(2001);
    size_t max_clk_idx = 0;
    for(const auto &pair : mgr.FF_Map){
        max_clk_idx = std::max((int)max_clk_idx, pair.second->getClkIdx());
    }

    for(size_t clkIDX = 0; clkIDX <= max_clk_idx; clkIDX++){
        size_t FFcount = 0;
        std::vector<FF*> MBFFs;
        std::vector<FF*> sameCLKFFs;
        std::vector<std::pair<int, int>> slotMap;// i -> (j, k) --> i's waitSlot map to j's MBFF k's slot
        for(const auto& pair : mgr.FF_Map){
            if((size_t)pair.second->getClkIdx() == clkIDX){
                MBFFs.push_back(pair.second);
                size_t curSlot = 0;
                for(auto& ff : pair.second->getClusterFF()){
                    sameCLKFFs.push_back(ff);
                    slotMap.push_back({FFcount, curSlot});
                    curSlot++;
                }
                FFcount++;
            }
        }
        
        size_t querySize = 100;
        if(sameCLKFFs.size() < querySize){
            querySize = sameCLKFFs.size();
        }
        if(querySize < 2) continue;

        vector<FF*> FFs(querySize);
        RTree rtree = RTree();
        // Using rtree to do Hungarain for nearest FF
        for(size_t i = 0; i <sameCLKFFs.size(); i++){
            FF *ff = sameCLKFFs[i];
            PointWithID pointwithid;
            pointwithid = std::make_pair(Point(ff->getPhysicalFF()->getNewCoor().x, ff->getPhysicalFF()->getNewCoor().y), i);
            rtree.insert(pointwithid);
        }

        for(size_t repeatTime = 0; repeatTime < sameCLKFFs.size()/querySize * 2; repeatTime++){
            FF* centorFF = sameCLKFFs[rand() % sameCLKFFs.size()];
            Point queryPoint(centorFF->getPhysicalFF()->getNewCoor().x, centorFF->getPhysicalFF()->getNewCoor().y);
            std::vector<PointWithID> nearestResults;
            rtree.query(bgi::nearest(queryPoint, querySize), std::back_inserter(nearestResults));

            std::vector<size_t> FFsMap(querySize);
            size_t count = 0;
            for(auto& nP : nearestResults){
                FFs[count] = sameCLKFFs[nP.second];
                FFsMap[count] = nP.second;
                count++;
            }

            // get the cost of FF to all the slot
            std::vector<std::vector<double>> cost(FFs.size(), std::vector<double>(FFs.size(), 0)); // ith FF cost for putting it in j slot
            #pragma omp parallel for num_threads(MAX_THREADS)
            for(size_t idx=0;idx<FFs.size();idx++){
                FF* curFF = FFs[idx];
                for(size_t j=0;j<FFs.size();j++){
                    PrevInstance prevInstance = curFF->getPrevInstance();
                    FF* newFF = MBFFs[slotMap[FFsMap[j]].first];
                    string pinName = newFF->getCell()->getBits() == 1 ? "" : std::to_string(slotMap[FFsMap[j]].second);
                    Coor newCoorD = newFF->getNewCoor() + newFF->getPinCoor("D" + pinName);
                    double delta_hpwl = 0;
                    Coor inputCoor;
                    // D pin cost
                    if(prevInstance.instance){
                        if(prevInstance.cellType == CellType::IO){
                            inputCoor = prevInstance.instance->getCoor();
                            double old_hpwl = HPWL(inputCoor, curFF->getOriginalD());
                            double new_hpwl = HPWL(inputCoor, newCoorD);
                            delta_hpwl += old_hpwl - new_hpwl;
                        }
                        else if(prevInstance.cellType == CellType::GATE){
                            inputCoor = prevInstance.instance->getCoor() + prevInstance.instance->getPinCoor(prevInstance.pinName);
                            double old_hpwl = HPWL(inputCoor, curFF->getOriginalD());
                            double new_hpwl = HPWL(inputCoor, newCoorD);
                            delta_hpwl += old_hpwl - new_hpwl;
                        }
                        else{
                            FF* inputFF = dynamic_cast<FF*>(prevInstance.instance);
                            inputCoor = inputFF->getOriginalQ();
                            Coor newCoorQ = inputFF->getPhysicalFF()->getNewCoor() + inputFF->getPhysicalFF()->getPinCoor("Q" + inputFF->getPhysicalPinName());
                            double old_hpwl = HPWL(inputCoor, curFF->getOriginalD());
                            double new_hpwl = HPWL(newCoorQ, newCoorD);
                            delta_hpwl += old_hpwl - new_hpwl;
                        }
                    }
                    double newSlack = curFF->getTimingSlack("D") + mgr.DisplacementDelay * delta_hpwl;
                    cost[idx][j] = newSlack < 0 ? -newSlack : 0;

                    // Q pin cost
                    for(auto& nextFF : curFF->getNextStage()){
                        Coor originalInput = curFF->getOriginalQ();
                        Coor newInput = newFF->getNewCoor() + newFF->getPinCoor("Q" + pinName);
                        if(nextFF.outputGate){
                            inputCoor = nextFF.outputGate->getCoor() + nextFF.outputGate->getPinCoor(nextFF.pinName);
                            double old_hpwl = HPWL(inputCoor, originalInput);
                            double new_hpwl = HPWL(inputCoor, newInput);
                            delta_hpwl = old_hpwl - new_hpwl;
                        }
                        else{
                            newCoorD = nextFF.ff->getPhysicalFF()->getNewCoor() + nextFF.ff->getPhysicalFF()->getPinCoor("D" + nextFF.ff->getPhysicalPinName());
                            double old_hpwl = HPWL(nextFF.ff->getOriginalD(), originalInput);
                            double new_hpwl = HPWL(newCoorD, newInput);
                            delta_hpwl = old_hpwl - new_hpwl;
                        }
                        newSlack = (curFF->getOriginalQpinDelay() - newFF->getCell()->getQpinDelay()) + nextFF.ff->getTimingSlack("D") + mgr.DisplacementDelay * delta_hpwl;
                        cost[idx][j] += newSlack < 0 ? -newSlack : 0;
                    }
                }
            }
            
            HungarianAlgorithm HungAlgo;
            std::vector<int> assignment;
            HungAlgo.Solve(cost, assignment);
            
            std::vector<std::pair<size_t, size_t>> newSlotMap(querySize);
            for(size_t i=0;i<FFs.size();i++){ // write back assignment result
                FF* newFF = MBFFs[slotMap[FFsMap[assignment[i]]].first];
                size_t curSlot = slotMap[FFsMap[assignment[i]]].second;
                FF* curFF = FFs[i];
                Coor oldCoor = curFF->getPhysicalFF()->getNewCoor();
                curFF->setPhysicalFF(newFF, curSlot);
                newFF->addClusterFF(curFF, curSlot);

                // maintain slotMap
                newSlotMap[i] = slotMap[FFsMap[assignment[i]]];

                // maintain rtree
                PointWithID pointwithid;
                pointwithid = std::make_pair(Point(curFF->getPhysicalFF()->getNewCoor().x, curFF->getPhysicalFF()->getNewCoor().y), FFsMap[i]);
                rtree.insert(pointwithid);
                pointwithid = std::make_pair(Point(oldCoor.x, oldCoor.y), FFsMap[i]);
                rtree.remove(pointwithid);
            }
            
            // update slotMap
            for(size_t slotI=0;slotI<querySize;slotI++)
                slotMap[FFsMap[slotI]] = newSlotMap[slotI];
        }
    }

}

size_t DetailPlacement::ChangeCell(){
    DEBUG_DP("Change Cell");
    size_t changeCount = 0;
    vector<FF*> FFs;
    FFs.reserve(mgr.FF_Map.size());
    for(auto& ff_m : mgr.FF_Map){
        FFs.push_back(ff_m.second);
    }
    // Single-threaded: setCell + getCost reads neighbor state through
    // nextStage connections, OMP causes data races when probing cells.
    for(size_t i=0;i<FFs.size();i++){
        FF* curFF = FFs[i];
        double bestCost = curFF->getCost();
        Cell* bestCell = curFF->getCell();
        Cell* originalCell = curFF->getCell();
        size_t bit = curFF->getCell()->getBits();
        size_t bitMapSize = mgr.Bit_FF_Map[bit].size();
        for(size_t j=0;j<bitMapSize;j++){
            Cell* targetCell = mgr.Bit_FF_Map[bit][j];
            if(targetCell->getW() <= originalCell->getW() && targetCell->getH() <= originalCell->getH()){
                curFF->setCell(targetCell);
                double totalCost = curFF->getCost();
                curFF->setCell(originalCell);
                if(totalCost < bestCost){
                    bestCost = totalCost;
                    bestCell = targetCell;
                }
            }
        }
        if(bestCell != originalCell) changeCount++;
        curFF->setCell(bestCell);
    }
    return changeCount;
}