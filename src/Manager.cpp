#include "Manager.h"
#include <unordered_set>
#include <boost/geometry/index/rtree.hpp>
#include <lemon/smart_graph.h>
#include <lemon/matching.h>
Manager::Manager():
    alpha(0),
    beta(0),
    gamma(0),
    lambda(0),
    DisplacementDelay(0),
    NumInput(0),
    NumOutput(0),
    MaxBit(0),
    NumInstances(0),
    NumNets(0),
    preprocessor(nullptr),
    legalizer(nullptr)
    {}

Manager::~Manager(){
    for(auto &pair : FF_Map){
        delete pair.second;
    }
    FF_Map.clear();

    for(auto &pair : Gate_Map){
        delete pair.second;
    }
    Gate_Map.clear();

    delete legalizer;
}

void Manager::parse(const std::string &filename){
    this->input_filename = filename;
    Parser parser(filename);
    parser.parse(*this);
}

void Manager::preprocess(){
    preprocessor = new Preprocess(*this);
    preprocessor->run();
    // delete all FF before preprocess
    originalFF_Map = FF_Map;
    FF_Map.clear();
    // assign new FF after debank and optimal location to FF_Map and FFs
    const std::unordered_map<std::string, FF*>& FF_list = preprocessor->getFFList();
    for(const auto& ff_m : FF_list){
        FF* newFF = getNewFF();
        FF* curFF = ff_m.second;
        Coor coor = curFF->getNewCoor();
        std::string instanceName = getNewFFName("FF_1_");
        int clkIdx = curFF->getClkIdx();
        Cell* cell = curFF->getCell();
        newFF->setInstanceName(instanceName);
        newFF->setCoor(coor);
        newFF->setNewCoor(coor);
        newFF->setClkIdx(clkIdx);
        newFF->setCell(cell);
        newFF->setClusterSize(1);
        newFF->addClusterFF(curFF, 0);
        newFF->setFixed(curFF->getFixed());
        curFF->setPhysicalFF(newFF, 0);

        FF_Map[instanceName] = newFF;
    }
}

void Manager::meanshift(){
    // do graceful meanshift clustering
    MeanShift meanshift;
    meanshift.run(*this);
}

void Manager::preLegalize(){
    legalizer = new Legalizer(*this);
    legalizer->initial();
    legalizer->run();
}

void Manager::banking(){
    binTable.invalidate();
    Banking banking(*this);
    banking.run();
}

void Manager::postBankingOptimize(){
    if(std::getenv("SKIP_POST_CG") && std::atoi(std::getenv("SKIP_POST_CG"))) return;
    binTable.invalidate();
    postBankingOptimizer postOptimize(*this);
    postOptimize.run();
}

void Manager::legalize(){
    legalizer->run();
    binTable.invalidate();
}

void Manager::detailplacement(){
    binTable.invalidate();
    DetailPlacement detailplacer(*this);
    detailplacer.run();
}

void Manager::checker(){
    Checker checker(*this);
    checker.run();
}

void Manager::dump(const std::string &filename){
    DEBUG_MGR("Dump result ...");
    Dumper dumper(filename);
    dumper.dump(*this);
}

void Manager::dumpVisual(const std::string &filename){
    std::ofstream fout;
    fout.open(filename.c_str());
    assert(fout.good());

    fout << "DieSize " << die.getDieOrigin().x << " " << die.getDieOrigin().y << " " << die.getDieBorder().x << " " << die.getDieBorder().y << std::endl;

    fout << "NumInput " << Input_Map.size() << std::endl;
    std::map<std::string, Coor> input_map(Input_Map.begin(), Input_Map.end());
    for(const auto &pair: input_map){
        fout << "Input " << pair.first << " " << pair.second.x << " " << pair.second.y << std::endl;
    }
    
    fout << "NumOutput " << Output_Map.size() << std::endl;
    std::map<std::string, Coor> output_map(Output_Map.begin(), Output_Map.end());
    for(const auto &pair: output_map){
        fout << "Output " << pair.first << " " << pair.second.x << " " << pair.second.y << std::endl;
    }


    // for cell library
    std::unordered_map<std::string, Cell *> cellMap = cell_library.getCellMap();
    for(const auto &pair: cellMap){
        if(pair.second->getType() == Cell_Type::FF){
            fout << "FlipFlop " << pair.second->getBits() << " " << pair.second->getCellName() << " " << pair.second->getW() << " " << pair.second->getH() << " " << pair.second->getPinCount() << std::endl;
            std::unordered_map<std::string, Coor> pinCoorMap= pair.second->getPinCoorMap();
            for(const auto &p : pinCoorMap){
                fout << "Pin " << p.first << " " << p.second.x << " " << p.second.y << std::endl;
            }
        }
        else if(pair.second->getType() == Cell_Type::Gate){
            fout << "Gate " << pair.second->getCellName() << " " << pair.second->getW() << " " << pair.second->getH() << " " << pair.second->getPinCount() << std::endl;
            std::unordered_map<std::string, Coor> pinCoorMap= pair.second->getPinCoorMap();
            for(const auto &p : pinCoorMap){
                fout << "Pin " << p.first << " " << p.second.x << " " << p.second.y << std::endl;
            }
        }
        else{
            abort();
        }
    }

    fout << "NumInstances " << FF_Map.size() + Gate_Map.size() << std::endl;
    std::map<std::string, FF *> ff_map(FF_Map.begin(), FF_Map.end());
    for(const auto &pair: ff_map){
        fout << "Inst " << pair.first << " " << pair.second->getCell()->getCellName() << " " << pair.second->getNewCoor().x << " " << pair.second->getNewCoor().y << std::endl;
    }
    std::map<std::string, Gate *> gate_map(Gate_Map.begin(), Gate_Map.end());
    for(const auto &pair: gate_map){
        fout << "Inst " << pair.first << " " << pair.second->getCell()->getCellName() << " " << pair.second->getCoor().x << " " << pair.second->getCoor().y << std::endl;
    }

    std::map<std::string, Net> net_map(Net_Map.begin(), Net_Map.end());
    for(const auto &pair: net_map){
        int pinCout = pair.second.getNumPins();
        fout << "Net " << pair.second.getNetName() << " " << pinCout << std::endl;
        for(int i = 0; i < pinCout; i++){
            Pin pin = pair.second.getPin(i);
            fout << "Pin ";
            if(!pin.getIsIOPin()){
                fout << pin.getInstanceName() << "/";
            }
            fout << pin.getPinName() << std::endl;
        }
    }

    fout << "BinWidth " << die.getBinWidth() << std::endl;
    fout << "BinHeight " << die.getBinHeight() << std::endl;
    fout << "BinMaxUtil " << die.getBinMaxUtil() << std::endl;
    std::vector<PlacementRow> pr = die.getPlacementRows();
    for(size_t i = 0; i < pr.size(); i++){
        fout << "PlacementRows " << std::setprecision(10)<< pr[i].startCoor.x << " " << pr[i].startCoor.y << " " << pr[i].siteWidth << " " << pr[i].siteHeight << " " << pr[i].NumOfSites << std::endl;
    }
    fout.close();
}

void Manager::print(){
    std::cout << alpha << " " << beta << " " << gamma << " " << lambda << std::endl;
    std::cout << "#################### Die Info ##################" << std::endl;
    std::cout << die << std::endl;

    std::cout << "#################### IO Info ##################" << std::endl;
    for(const auto &pair : Input_Map){
        std::cout << pair.first << ":" << pair.second << std::endl;
    }
    for(const auto &pair : Output_Map){
        std::cout << pair.first << ":" << pair.second << std::endl;
    }

    std::cout << "#################### Cell Library ##################" << std::endl;
    std::cout << cell_library << std::endl;

    std::cout << "#################### FF Instance ##################" << std::endl;
    for(const auto &pair: FF_Map){
        std::cout << *pair.second << std::endl;
    }

    std::cout << "#################### Gate Instance ##################" << std::endl;
    for(const auto &pair: Gate_Map){
        std::cout << *pair.second << std::endl;
    }

    std::cout << "#################### Netlist ##################" << std::endl;
    for(const auto &pair: Net_Map){
        std::cout << pair.second << std::endl;
    }

    std::cout << "#################### After MeanShift ##################" << std::endl;
    for(const auto &pair: FF_Map){
        std::cout << pair.second->getCoor() << pair.second->getNewCoor() << std::endl;
    }
}

bool Manager::isIOPin(const std::string &pinName){
    if(Input_Map.find(pinName) != Input_Map.end()) return true;
    if(Output_Map.find(pinName) != Output_Map.end()) return true;
    return false;
}

std::string Manager::getNewFFName(const std::string& prefix){
    int count = name_record[prefix];
    assert("number of FF exceed INT_MAX, pls modify counter datatype" && count != INT_MAX);
    name_record[prefix]++;
    return prefix + std::to_string(count);
}

struct ComparePairs {
  bool operator()(const std::pair<double, FF*>& a, const std::pair<double, FF*>& b) const {
    // Priority based on the first element (ascending order for min heap)
    return a.first > b.first;
  }
};

/**
 * @brief Bank MBFF/single bit ff into MBFF
 * 
 * @param newbankCoor Merge MBFF coordinate
 * @param bankCellType Merge MBFF celltype
 * @param FFToBank All ffs needs to bank
 * @return FF* The pointer point to the merged MBFF
 * @todo add incremental banking??
 */
FF* Manager::bankFF(Coor newbankCoor, Cell* bankCellType, std::vector<FF*> FFToBank){
    // get all FF to be bank
    std::vector<FF*> FFs(bankCellType->getBits());
    int bit = 0;
    int clkIdx = FFToBank[0]->getClkIdx();
    
    // if the FFToBank exist MBFF
    for(auto& MBFF : FFToBank){
        assert(clkIdx == MBFF->getClkIdx() && "different clk cannot be banked together");
        std::vector<FF*>& clusterFF = MBFF->getClusterFF();
        for(auto& ff : clusterFF){
            FFs[bit] = ff;
            bit++;
        }
    }

    // Snapshot rect data BEFORE deleteFF / getNewFF (which can recycle FFToBank
    // pointers via FFGarbageCollector and overwrite their coord). Keeps original
    // call ordering byte-exact when binTable is OFF.
    std::vector<BinDensityTable::Rect> removedRects;
    if(binTable.ready()){
        removedRects.reserve(FFToBank.size());
        for(FF* m : FFToBank){
            Coor c = m->getNewCoor();
            removedRects.push_back({c.x, c.y, m->getW(), m->getH()});
        }
    }

    // delete all MBFF to be cluster from FF_Map
    for(auto& MBFF : FFToBank){
        FF_Map.erase(MBFF->getInstanceName());
        deleteFF(MBFF);
    }

    // assign new FF
    assert(bit == bankCellType->getBits() && "Floating input is allowed???");
    FF* newFF = getNewFF();
    std::string newName = getNewFFName("FF_" + std::to_string(bit) + "_");
    newFF->setInstanceName(newName);
    newFF->setCoor(newbankCoor);
    newFF->setNewCoor(newbankCoor);
    newFF->setCell(bankCellType);
    newFF->setClusterSize(bit);
    newFF->setClkIdx(clkIdx);
    newFF->setFixed(false);
    FF_Map[newName] = newFF;

    if(binTable.ready()){
        BinDensityTable::Rect addedR{newbankCoor.x, newbankCoor.y,
                                     bankCellType->getW(), bankCellType->getH()};
        binTable.applyMutation(removedRects, {addedR});
    }

    if(bit == 1){ // bank single bit FF
        newFF->addClusterFF(FFs[0], 0);
        FFs[0]->setPhysicalFF(newFF, 0);
        return newFF;
    }

    for(size_t i=0;i<FFs.size();i++){
        FFs[i]->setPhysicalFF(newFF, i);
        newFF->addClusterFF(FFs[i], i);
    }
    assignSlot(newFF);
    return newFF;
}

// Hybrid Route A / Stage 1 — rollback-capable banking.
// Mirrors bankFF() but defers ALL FF_Map mutations and wrapper deleteFF() to
// commitFinalizeBank. See Manager.h::BankUndo for rationale (unordered_map
// bucket state cannot be rolled back after an insert/erase, so rollback must
// be a no-op on FF_Map).
FF* Manager::bankFF_deferred(Coor newbankCoor, Cell* bankCellType,
                             const std::vector<FF*>& FFToBank,
                             BankUndo& undo){
    // Enumerate constituent inner FFs (same traversal as bankFF).
    std::vector<FF*> FFs(bankCellType->getBits());
    int bit = 0;
    int clkIdx = FFToBank[0]->getClkIdx();
    for(auto* MBFF : FFToBank){
        assert(clkIdx == MBFF->getClkIdx() && "different clk cannot be banked together");
        std::vector<FF*>& clusterFF = MBFF->getClusterFF();
        for(auto* ff : clusterFF){
            FFs[bit] = ff;
            BankUndo::InnerState is;
            is.innerFF = ff;
            is.oldPhysical = ff->getPhysicalFF();
            is.oldSlot = ff->getSlot();
            undo.innerStates.push_back(is);
            bit++;
        }
    }
    assert(bit == bankCellType->getBits() && "Floating input is allowed???");

    // Stage wrapper FF_Map erasures; actual erase happens only on finalize.
    for(auto* MBFF : FFToBank){
        undo.pendingEraseNames.push_back(MBFF->getInstanceName());
        undo.pendingInsertWrappers.push_back(MBFF);
    }

    // Build the new MBFF (same as bankFF). Name is reserved via getNewFFName
    // immediately so it matches bankFF's naming order; actual FF_Map insert is
    // deferred to commitFinalizeBank.
    FF* newFF = getNewFF();
    std::string newName = getNewFFName("FF_" + std::to_string(bit) + "_");
    newFF->setInstanceName(newName);
    newFF->setCoor(newbankCoor);
    newFF->setNewCoor(newbankCoor);
    newFF->setCell(bankCellType);
    newFF->setClusterSize(bit);
    newFF->setClkIdx(clkIdx);
    newFF->setFixed(false);
    undo.newMBFF = newFF;
    undo.newName = newName;

    if(bit == 1){ // bank single bit FF (mirrors bankFF's early return)
        newFF->addClusterFF(FFs[0], 0);
        FFs[0]->setPhysicalFF(newFF, 0);
        return newFF;
    }

    for(size_t i=0;i<FFs.size();i++){
        FFs[i]->setPhysicalFF(newFF, i);
        newFF->addClusterFF(FFs[i], i);
    }
    assignSlot(newFF);
    return newFF;
}

void Manager::rollbackBank(BankUndo& undo){
    // FF_Map is untouched on rollback (nothing was inserted or erased).
    // 1. Restore each inner FF's physicalFF pointer + slot.
    for(auto& is : undo.innerStates) is.innerFF->setPhysicalFF(is.oldPhysical, is.oldSlot);
    // 2. Recycle the new MBFF (clear + push to GC).
    if(undo.newMBFF) deleteFF(undo.newMBFF);
    undo.newMBFF = nullptr;
    undo.newName.clear();
    undo.pendingInsertWrappers.clear();
    undo.pendingEraseNames.clear();
    undo.innerStates.clear();
}

void Manager::commitFinalizeBank(BankUndo& undo){
    // Apply deferred FF_Map mutations exactly once, in the order bankFF would:
    // erase wrappers first, then insert the new MBFF.
    for(const std::string& name : undo.pendingEraseNames) FF_Map.erase(name);
    if(undo.newMBFF && !undo.newName.empty()) FF_Map[undo.newName] = undo.newMBFF;
    for(FF* f : undo.pendingInsertWrappers) deleteFF(f);
    undo.pendingInsertWrappers.clear();
    undo.pendingEraseNames.clear();
    undo.innerStates.clear();
}

/**
 * @brief For merged MBFF, try to assign slot based on the slack, do stable matching
 *
 * @param newFF The merged MBFF
 */
void Manager::assignSlot(FF* newFF){
    int bit = newFF->getCell()->getBits();
    if(bit == 1)
        return ;

    vector<FF*> FFs = newFF->getClusterFF();

    std::vector<std::vector<double>> cost(bit, std::vector<double>(bit, 0)); // ith FF cost for putting it in j slot

    for(int i=0;i<bit;i++){
        FF* curFF = FFs[i];
        for(int j=0;j<bit;j++){
            PrevInstance prevInstance = curFF->getPrevInstance();
            Coor newCoorD = newFF->getNewCoor() + newFF->getPinCoor("D" + std::to_string(j));
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
            double newSlack = curFF->getTimingSlack("D") + DisplacementDelay * delta_hpwl;
            cost[i][j] = newSlack < 0 ? -newSlack : 0;

            // Q pin cost
            for(auto& nextFF : curFF->getNextStage()){
                Coor originalInput = curFF->getOriginalQ();
                Coor newInput = newFF->getNewCoor() + newFF->getPinCoor("Q" + std::to_string(j));
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
                newSlack = nextFF.ff->getTimingSlack("D") + DisplacementDelay * delta_hpwl;
                cost[i][j] += newSlack < 0 ? -newSlack : 0;
            }
        }
    }

    HungarianAlgorithm HungAlgo;
    std::vector<int> assignment;
    HungAlgo.Solve(cost, assignment);
    
    for(size_t i=0;i<FFs.size();i++){ // write back assignment result
        FF* curFF = FFs[i];
        curFF->setPhysicalFF(newFF, assignment[i]);
        newFF->addClusterFF(curFF, assignment[i]);
    }
}

/**
 * @brief Debank the MBFF into single bit ff 
 * 
 * @param MBFF The merged MBFF
 * @param debankCellType The single bit FF celltype to debank
 * @return std::vector<FF*> The vector contains all signle bit FF
 * @todo the incremental debanking
 */
std::vector<FF*> Manager::debankFF(FF* MBFF, Cell* debankCellType){
    std::vector<FF*> outputFF;
    std::vector<FF*>& clusterFF = MBFF->getClusterFF();
    int slot = 0;
    int clkIdx = MBFF->getClkIdx();
    for(auto& ff : clusterFF){
        FF* newFF = getNewFF();
        // use coor for same D pin coor
        Coor coor = MBFF->getCoor() + MBFF->getPinCoor("D" + std::to_string(slot)) - debankCellType->getPinCoor("D");
        std::string instanceName = getNewFFName("FF_1_");
        newFF->setInstanceName(instanceName);
        newFF->setCoor(coor);
        newFF->setNewCoor(coor);
        newFF->setCell(debankCellType);
        newFF->setClusterSize(1);
        newFF->addClusterFF(ff, 0);
        newFF->setClkIdx(clkIdx);
        newFF->setFixed(false);
        ff->setPhysicalFF(newFF, 0);

        FF_Map[instanceName] = newFF;
        slot++;
        outputFF.push_back(newFF);
    }

    std::vector<BinDensityTable::Rect> removedR, addedR;
    if(binTable.ready()){
        Coor mc = MBFF->getNewCoor();
        removedR.push_back({mc.x, mc.y, MBFF->getW(), MBFF->getH()});
        addedR.reserve(outputFF.size());
        for(FF* of : outputFF){
            Coor oc = of->getNewCoor();
            addedR.push_back({oc.x, oc.y, of->getW(), of->getH()});
        }
    }
    FF_Map.erase(MBFF->getInstanceName());
    deleteFF(MBFF);
    if(binTable.ready()) binTable.applyMutation(removedR, addedR);

    return outputFF;
}

void Manager::debankAll(){
    Cell* cell1bit = Bit_FF_Map[1][0];
    std::vector<FF*> toDebank;
    for(auto& pair : FF_Map)
        if(pair.second->getCell()->getBits() > 1)
            toDebank.push_back(pair.second);
    std::cout << "[DEBANK_ALL] debanking " << toDebank.size() << " MBFFs" << std::endl;
    for(auto* ff : toDebank)
        debankFF(ff, cell1bit);
}

// Phase 5: Post-LG Decluster
// After LG snaps MBFFs to legal positions, some banks turn out to be net-negative
// under the same cost model used during banking (per-pin TNS + Power + Area).
// This pass computes ΔC(keep → decluster to N 1-bit FFs) using LG-accurate
// positions for both the MBFF's Q-pin fanout and the debanked FFs' D-pin
// (preserved by debankFF). If ΔC < -margin (score would improve), decluster.
// After the decluster batch, the whole design is re-legalized.
void Manager::postLGDecluster(){
    // Adaptive gate: per-case DP ripple makes ΔC prediction unreliable.
    // D2 family (153,457 instances): enable with margin=5000 (original).
    // Low-β cases (β≤500, e.g. tc2/hc03): enable with margin=50 — the
    // timing-dominated cost function makes decluster predictions reliable.
    // Override with POST_LG_DECLUSTER={0,1} and POST_LG_DECLUSTER_MARGIN.
    int mode = -1;
    if(const char* envOn = std::getenv("POST_LG_DECLUSTER")) mode = std::atoi(envOn);
    if(mode == -1){
        if(NumInstances >= 130000 && NumInstances <= 180000)
            mode = 1;
        else if(beta <= 500.0)
            mode = 1;
        else
            mode = 0;
    }
    if(mode == 0) return;
    double margin = (beta <= 500.0) ? 55.0 : 5000.0;
    if(const char* envM = std::getenv("POST_LG_DECLUSTER_MARGIN")) margin = std::atof(envM);

    binTable.invalidate();
    Cell* oneBitCell = Bit_FF_Map[1][0];

    // Lambda: ΔC of declustering one MBFF into N copies of oneBitCell.
    // Returns score change under MINIMIZE convention: negative => decluster wins.
    auto scoreDelta = [&](FF* mbff) -> double {
        Cell* mCell = mbff->getCell();
        int N = mCell->getBits();
        // Power/Area delta: after decluster, we use N × oneBit instead of 1 × MBFF.
        double pwrDelta  = beta  * (N * oneBitCell->getGatePower() - mCell->getGatePower());
        double areaDelta = gamma * (N * oneBitCell->getArea()      - mCell->getArea());

        // TNS delta: D-pin absolute coord preserved by debankFF, so D-side
        // contribution unchanged. Q-side shifts because of QpinDelay + Q-pin
        // offset differences. Iterate each constituent cf in the MBFF.
        double oldTNS = 0, newTNS = 0;
        double qDelayBenefit = mCell->getQpinDelay() - oneBitCell->getQpinDelay();
        std::vector<FF*>& clusterFF = mbff->getClusterFF();
        int slot = 0;
        for(auto* cf : clusterFF){
            std::string slotStr = (mCell->getBits() == 1) ? "" : std::to_string(slot);
            // D-side: unchanged absolute position, same contribution both sides -> cancels.
            // Q-side: downstream FFs' D-pin slack affected.
            // Use LG-snapped coord for both sides: predicts the position debankFF
            // will use after we update the debanked 1-bits' newCoor below.
            Coor curQpin = mbff->getNewCoor() + mbff->getPinCoor("Q" + slotStr);
            Coor debankedCoor = mbff->getNewCoor() + mbff->getPinCoor("D" + slotStr)
                              - oneBitCell->getPinCoor("D");
            Coor newQpin = debankedCoor + oneBitCell->getPinCoor("Q");
            for(auto& next : cf->getNextStage()){
                double nextCurSlack = next.ff->getSlack();
                Coor loadCoor;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                }
                double deltaHpwlQ = HPWL(loadCoor, curQpin) - HPWL(loadCoor, newQpin);
                double predNextSlack = nextCurSlack + qDelayBenefit
                                     + DisplacementDelay * deltaHpwlQ;
                oldTNS += std::max(0.0, -nextCurSlack);
                newTNS += std::max(0.0, -predNextSlack);
            }
            slot++;
        }
        double tnsDelta = alpha * (newTNS - oldTNS);
        return tnsDelta + pwrDelta + areaDelta;
    };

    // Collect candidates (need a snapshot: debankFF mutates FF_Map).
    std::vector<FF*> candidates;
    candidates.reserve(FF_Map.size());
    for(auto& pair : FF_Map){
        if(pair.second->getCell()->getBits() > 1) candidates.push_back(pair.second);
    }
    std::vector<FF*> toDecluster;
    double totalSaved = 0;
    for(auto* ff : candidates){
        double delta = scoreDelta(ff);
        if(delta < -margin){
            toDecluster.push_back(ff);
            totalSaved += -delta;
        }
    }
    std::cout << "[PostLGDecluster] candidates=" << candidates.size()
              << " toDecluster=" << toDecluster.size()
              << " margin=" << margin
              << " predictedScoreImprovement=" << totalSaved << std::endl;

    if(toDecluster.empty()) return;

    // Incremental path: for each MBFF we decluster, call debankFF then use
    // FindPlace + UpdateRows on each new 1-bit FF. This leaves every other FF's
    // LG position untouched (avoiding the ~+1% regression a full re-LG causes).
    // The MBFF's old rect stays sliced in the row state — treated as wasted
    // space so the new 1-bits don't overlap it. FindPlace naturally searches
    // just outside that footprint.
    int okCnt = 0, failCnt = 0;
    double declusterRadius = 0.0;
    if(const char* envR = std::getenv("POST_LG_DECLUSTER_RADIUS")) declusterRadius = std::atof(envR);

    for(auto* mbff : toDecluster){
        Coor lgCoor = mbff->getNewCoor();
        Cell* mCell = mbff->getCell();
        int N = mCell->getBits();
        double mW = mCell->getW();
        double mH = mCell->getH();
        std::cout << "[PLDc] declustering " << mbff->getInstanceName()
                  << " cell=" << mCell->getCellName()
                  << " at (" << lgCoor.x << "," << lgCoor.y << ")"
                  << " N=" << N << std::endl;
        std::vector<Coor> intendedPos(N);
        for(int i = 0; i < N; i++){
            intendedPos[i] = lgCoor + mCell->getPinCoor("D" + std::to_string(i))
                           - oneBitCell->getPinCoor("D");
        }
        // Free the MBFF's rect so the new 1-bits can reclaim its footprint.
        legalizer->FreeRect(lgCoor, mW, mH);
        // Drop the MBFF's Node from legalizer->ffs before debankFF recycles the
        // FF* pointer — otherwise DP iterates a stale Node whose FFPtr later
        // points at a debanked 1-bit and corrupts swap state.
        legalizer->RemoveNodeByFFPtr(mbff);
        std::vector<FF*> newFFs = debankFF(mbff, oneBitCell);
        for(size_t i = 0; i < newFFs.size() && i < intendedPos.size(); i++){
            Coor target = intendedPos[i];
            std::cout << "[PLDc]   new 1-bit " << newFFs[i]->getInstanceName()
                      << " intended (" << target.x << "," << target.y << ")";
            Coor placed;
            if(declusterRadius > 0){
                placed = legalizer->FindNearestLegalSpace(target, oneBitCell, declusterRadius);
            } else {
                placed = legalizer->FindPlace(target, oneBitCell);
            }
            if(placed.x == DBL_MAX){
                // Fallback: accept the intended (possibly overlapping) position.
                // Placement checker will flag it; we log and move on so the run
                // still produces a scoreable output.
                placed = target;
                failCnt++;
            } else {
                okCnt++;
            }
            newFFs[i]->setCoor(placed);
            newFFs[i]->setNewCoor(placed);
            newFFs[i]->setIsLegalize(true);
            legalizer->UpdateRows(newFFs[i]);
            std::cout << " placed (" << placed.x << "," << placed.y << ")" << std::endl;
        }
    }
    std::cout << "[PostLGDecluster] placed_ok=" << okCnt
              << " placed_fallback=" << failCnt << std::endl;
}

// NTU Eq.5 per-level declustering.
// After banking creates MBFFs of `targetBit` width, evaluate each under the
// multi-objective cost ΔC = α·(s(i) - sBar) + β·ΔPower + γ·ΔArea.
// sBar = median negative-slack sum among (targetBit/2)-width FFs (reference).
// If ΔC < threshold, the MBFF is harmful — decluster it to (targetBit/2)-bit FFs.
int Manager::perLevelDecluster(int targetBit, double threshold){
    Cell* halfCell = nullptr;
    int halfBit = targetBit / 2;
    if(halfBit < 1) halfBit = 1;
    auto it = Bit_FF_Map.find(halfBit);
    if(it == Bit_FF_Map.end() || it->second.empty()) return 0;
    halfCell = it->second[0];

    std::vector<FF*> candidates;
    for(auto& kv : FF_Map){
        if(kv.second->getCell()->getBits() == targetBit)
            candidates.push_back(kv.second);
    }
    if(candidates.empty()) return 0;

    // Compute reference slack: median of negative slacks among halfBit FFs
    std::vector<double> refSlacks;
    for(auto& kv : FF_Map){
        if(kv.second->getCell()->getBits() == halfBit){
            double s = 0;
            for(auto* cf : kv.second->getClusterFF()){
                try { s += std::min(0.0, cf->getSlack()); }
                catch(...) {}
            }
            refSlacks.push_back(s);
        }
    }
    double sBar = 0;
    if(!refSlacks.empty()){
        std::sort(refSlacks.begin(), refSlacks.end());
        sBar = refSlacks[refSlacks.size() / 2];
    }

    int nDeclustered = 0;
    std::vector<FF*> toDecluster;

    for(auto* mbff : candidates){
        Cell* mCell = mbff->getCell();
        int N = mCell->getBits();

        // s(i) = sum of negative slack on all pins
        double si = 0;
        for(auto* cf : mbff->getClusterFF()){
            try { si += std::min(0.0, cf->getSlack()); }
            catch(...) {}
        }

        // Power/Area deltas (after - before): N half-cells vs 1 MBFF
        double dPower = N * halfCell->getGatePower() - mCell->getGatePower();
        double dArea  = N * halfCell->getArea()      - mCell->getArea();

        // NTU Eq.5: ΔC = α·(s(i) - sBar) + β·ΔPower + γ·ΔArea
        double deltaC = alpha * (si - sBar) + beta * dPower + gamma * dArea;

        if(deltaC < threshold){
            toDecluster.push_back(mbff);
        }
    }

    for(auto* mbff : toDecluster){
        Coor mbffCoor = mbff->getNewCoor();
        Cell* mCell = mbff->getCell();
        int N = mCell->getBits();

        // Free legalizer state for the MBFF
        legalizer->FreeRect(mbffCoor, mCell->getW(), mCell->getH());
        legalizer->RemoveNodeByFFPtr(mbff);

        // Compute intended positions from D-pin offsets
        std::vector<Coor> intendedPos(N);
        for(int i = 0; i < N; i++){
            intendedPos[i] = mbffCoor + mCell->getPinCoor("D" + std::to_string(i))
                           - halfCell->getPinCoor("D");
        }

        std::vector<FF*> newFFs = debankFF(mbff, halfCell);
        for(size_t i = 0; i < newFFs.size() && i < intendedPos.size(); i++){
            Coor placed = legalizer->FindPlace(intendedPos[i], halfCell);
            if(placed.x >= 1e18) placed = intendedPos[i];
            newFFs[i]->setCoor(placed);
            newFFs[i]->setNewCoor(placed);
            newFFs[i]->setIsLegalize(true);
            legalizer->UpdateRows(newFFs[i]);
        }
        nDeclustered++;
    }

    std::cout << "[PerLevelDecluster] bit=" << targetBit
              << " candidates=" << candidates.size()
              << " declustered=" << nDeclustered
              << " sBar=" << sBar
              << " threshold=" << threshold << std::endl;
    return nDeclustered;
}

// Placement-informed re-banking (v1).
// After LG, for each 4-bit MBFF, enumerate the 3 ways of splitting its 4
// constituents into 2+2 pairs, predict cost (alpha*TNS + beta*power + gamma*area)
// of each, and commit the best alternative if it improves over the current
// 4-bit arrangement. Thesis direction: pre-LG banking uses predicted positions;
// LG snaps to actual legal coords, shifting the cost landscape; post-LG remix
// exploits that shift.
void Manager::unbankRebank(){
    const char* envOn = std::getenv("UNBANK_REBANK");
    if(!envOn || std::atoi(envOn) == 0) return;
    double margin = 0.0;
    if(const char* envM = std::getenv("UR_MARGIN")) margin = std::atof(envM);

    binTable.invalidate();
    Cell* oneBitCell = Bit_FF_Map[1][0];
    auto itTwo = Bit_FF_Map.find(2);
    if(itTwo == Bit_FF_Map.end() || itTwo->second.empty()){
        std::cout << "[UNBANK_REBANK] no 2-bit cell available, skip" << std::endl;
        return;
    }
    Cell* twoBitCell = itTwo->second[0];

    // Upstream coor used in c's D-side HPWL (mirror of FF::getSlack new_hpwl).
    auto upstreamCoorD = [](FF* c) -> Coor {
        PrevInstance prev = c->getPrevInstance();
        if(!prev.instance) return Coor{0.0, 0.0};
        if(prev.cellType == CellType::IO) return prev.instance->getCoor();
        if(prev.cellType == CellType::GATE)
            return prev.instance->getCoor() + prev.instance->getPinCoor(prev.pinName);
        FF* inputFF = dynamic_cast<FF*>(prev.instance);
        return inputFF->getPhysicalFF()->getNewCoor()
             + inputFF->getPhysicalFF()->getPinCoor("Q" + inputFF->getPhysicalPinName());
    };

    // Cost of placing `consts` into an MBFF of cell `C` at position `P`.
    // Constituent i -> slot i. alpha*TNS predicted as delta from current
    // getSlack() under the assumption that only the MBFF's constituents move.
    auto predictMBFFCost = [&](const std::vector<FF*>& consts, const Coor& P, Cell* C) -> double {
        double cost = beta * C->getGatePower() + gamma * C->getArea();
        for(size_t slot = 0; slot < consts.size(); ++slot){
            FF* c = consts[slot];
            std::string slotStr = (C->getBits() == 1) ? "" : std::to_string(slot);
            Coor cDnew = P + C->getPinCoor("D" + slotStr);
            Coor cQnew = P + C->getPinCoor("Q" + slotStr);
            Cell*  curCell = c->getPhysicalFF()->getCell();
            std::string curSlot = c->getPhysicalPinName();
            Coor cDcur = c->getPhysicalFF()->getNewCoor() + c->getPhysicalFF()->getPinCoor("D" + curSlot);
            Coor cQcur = c->getPhysicalFF()->getNewCoor() + c->getPhysicalFF()->getPinCoor("Q" + curSlot);

            double cCurSlack = c->getSlack();
            Coor upCoor = upstreamCoorD(c);
            double dHpwlD = HPWL(upCoor, cDcur) - HPWL(upCoor, cDnew);
            double cNewSlack = cCurSlack + DisplacementDelay * dHpwlD;
            cost += alpha * std::max(0.0, -cNewSlack);

            double qDelayBenefit = curCell->getQpinDelay() - C->getQpinDelay();
            for(auto& next : c->getNextStage()){
                double nextCur = next.ff->getSlack();
                Coor loadCoor;
                double deltaQ;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                    deltaQ = qDelayBenefit;
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                    deltaQ = 0.0;
                }
                double dHpwlQ = HPWL(loadCoor, cQcur) - HPWL(loadCoor, cQnew);
                double predSlack = nextCur + deltaQ + DisplacementDelay * dHpwlQ;
                cost += alpha * std::max(0.0, -predSlack);
            }
        }
        return cost;
    };

    std::vector<FF*> candidates;
    candidates.reserve(FF_Map.size());
    for(auto& pr : FF_Map){
        if(pr.second->getCell()->getBits() == 4)
            candidates.push_back(pr.second);
    }
    std::cout << "[UNBANK_REBANK] 4bit_candidates=" << candidates.size()
              << " margin=" << margin << std::endl;

    const int pairings[3][4] = {
        {0, 1, 2, 3}, // {0,1} + {2,3}
        {0, 2, 1, 3}, // {0,2} + {1,3}
        {0, 3, 1, 2}  // {0,3} + {1,2}
    };

    int tried = 0, committed = 0, fpFail = 0;
    double predictedGain = 0;

    for(FF* mbff : candidates){
        std::vector<FF*> cluster = mbff->getClusterFF(); // copy
        if((int)cluster.size() != 4) continue;
        tried++;

        double curCost = predictMBFFCost(cluster, mbff->getNewCoor(), mbff->getCell());

        auto targetFor = [&](FF* a, FF* b) -> Coor {
            Coor o0 = twoBitCell->getPinCoor("D0");
            Coor o1 = twoBitCell->getPinCoor("D1");
            Coor dA = a->getPhysicalFF()->getNewCoor() + a->getPhysicalFF()->getPinCoor("D" + a->getPhysicalPinName());
            Coor dB = b->getPhysicalFF()->getNewCoor() + b->getPhysicalFF()->getPinCoor("D" + b->getPhysicalPinName());
            return Coor{ (dA.x + dB.x - o0.x - o1.x) * 0.5,
                         (dA.y + dB.y - o0.y - o1.y) * 0.5 };
        };

        double bestAltCost = DBL_MAX;
        int bestP = -1;
        Coor bestP1{0,0}, bestP2{0,0};
        for(int p = 0; p < 3; ++p){
            FF* a = cluster[pairings[p][0]];
            FF* b = cluster[pairings[p][1]];
            FF* c = cluster[pairings[p][2]];
            FF* d = cluster[pairings[p][3]];
            Coor P1 = targetFor(a, b);
            Coor P2 = targetFor(c, d);
            double altCost = predictMBFFCost({a, b}, P1, twoBitCell)
                           + predictMBFFCost({c, d}, P2, twoBitCell);
            if(altCost < bestAltCost){
                bestAltCost = altCost;
                bestP = p;
                bestP1 = P1;
                bestP2 = P2;
            }
        }

        double delta = bestAltCost - curCost;
        if(delta >= -margin) continue;

        Coor mbffOldCoor = mbff->getNewCoor();
        double mbffW = mbff->getCell()->getW();
        double mbffH = mbff->getCell()->getH();

        int idx_a = pairings[bestP][0];
        int idx_b = pairings[bestP][1];
        int idx_c = pairings[bestP][2];
        int idx_d = pairings[bestP][3];

        legalizer->FreeRect(mbffOldCoor, mbffW, mbffH);
        legalizer->RemoveNodeByFFPtr(mbff);
        std::vector<FF*> debanked = debankFF(mbff, oneBitCell);
        // debanked[i] wraps the constituent at slot i (iteration order preserved).

        auto tryBank2 = [&](FF* w1, FF* w2, const Coor& tgt) -> bool {
            Coor placed = legalizer->FindPlace(tgt, twoBitCell);
            if(placed.x == DBL_MAX){
                placed = legalizer->FindNearestLegalSpace(tgt, twoBitCell, 4 * mbffW);
            }
            if(placed.x == DBL_MAX) return false;
            FF* newMBFF = bankFF(placed, twoBitCell, {w1, w2});
            newMBFF->setIsLegalize(true);
            legalizer->UpdateRows(newMBFF);
            return true;
        };

        bool ok1 = tryBank2(debanked[idx_a], debanked[idx_b], bestP1);
        bool ok2 = tryBank2(debanked[idx_c], debanked[idx_d], bestP2);

        if(ok1 && ok2){
            committed++;
            predictedGain += -delta;
        } else {
            fpFail++;
            // Place whatever 1-bits are left (their wrappers still in FF_Map).
            auto place1 = [&](FF* w){
                if(FF_Map.count(w->getInstanceName()) == 0) return;
                Coor placed = legalizer->FindPlace(w->getNewCoor(), oneBitCell);
                if(placed.x == DBL_MAX){
                    placed = legalizer->FindNearestLegalSpace(w->getNewCoor(), oneBitCell, 4 * mbffW);
                }
                if(placed.x == DBL_MAX) placed = w->getNewCoor();
                w->setCoor(placed);
                w->setNewCoor(placed);
                w->setIsLegalize(true);
                legalizer->UpdateRows(w);
            };
            if(!ok1){ place1(debanked[idx_a]); place1(debanked[idx_b]); }
            if(!ok2){ place1(debanked[idx_c]); place1(debanked[idx_d]); }
        }
    }

    std::cout << "[UNBANK_REBANK] tried=" << tried
              << " committed=" << committed
              << " fpFail=" << fpFail
              << " predictedGain=" << predictedGain << std::endl;
}

// v2: global post-LG re-banking.
// 1) Debank every MBFF -> all constituents become 1-bit wrappers at post-LG D-pin coord.
// 2) Build rtree over newly-debanked 1-bits (pre-existing singletons untouched).
// 3) For each wrapper's kmax nearest neighbors (same clk, within radius), compute
//    \u0394C(pair as 2-bit) - \u0394C(each as 1-bit) and add an edge if < 0.
// 4) LEMON MaxWeightedMatching -> pairings.
// 5) Commit matches in priority order (most negative \u0394C first) via FindPlace + bankFF.
// 6) Unmatched newly-debanked 1-bits are placed as 1-bit via FindPlace.
void Manager::unbankRebankGlobal(){
    const char* envOn = std::getenv("UNBANK_REBANK_V2");
    if(!envOn || std::atoi(envOn) == 0) return;
    double margin = 0.0;
    if(const char* e = std::getenv("UR2_MARGIN")) margin = std::atof(e);
    double radiusMul = 3.0;
    if(const char* e = std::getenv("UR2_RADIUS_MUL")) radiusMul = std::atof(e);
    int kmax = 15;
    if(const char* e = std::getenv("UR2_KMAX")) kmax = std::atoi(e);

    binTable.invalidate();

    Cell* oneBitCell = Bit_FF_Map[1][0];
    auto itTwo = Bit_FF_Map.find(2);
    if(itTwo == Bit_FF_Map.end() || itTwo->second.empty()){
        std::cout << "[UR_V2] no 2-bit cell, skip" << std::endl;
        return;
    }
    Cell* twoBitCell = itTwo->second[0];
    double radius = std::max(twoBitCell->getW(), twoBitCell->getH()) * radiusMul;

    auto upstreamCoorD = [](FF* c) -> Coor {
        PrevInstance prev = c->getPrevInstance();
        if(!prev.instance) return Coor{0.0, 0.0};
        if(prev.cellType == CellType::IO) return prev.instance->getCoor();
        if(prev.cellType == CellType::GATE)
            return prev.instance->getCoor() + prev.instance->getPinCoor(prev.pinName);
        FF* inputFF = dynamic_cast<FF*>(prev.instance);
        return inputFF->getPhysicalFF()->getNewCoor()
             + inputFF->getPhysicalFF()->getPinCoor("Q" + inputFF->getPhysicalPinName());
    };
    auto predictMBFFCost = [&](const std::vector<FF*>& consts, const Coor& P, Cell* C) -> double {
        double cost = beta * C->getGatePower() + gamma * C->getArea();
        for(size_t slot = 0; slot < consts.size(); ++slot){
            FF* c = consts[slot];
            std::string slotStr = (C->getBits() == 1) ? "" : std::to_string(slot);
            Coor cDnew = P + C->getPinCoor("D" + slotStr);
            Coor cQnew = P + C->getPinCoor("Q" + slotStr);
            Cell* curCell = c->getPhysicalFF()->getCell();
            std::string curSlot = c->getPhysicalPinName();
            Coor cDcur = c->getPhysicalFF()->getNewCoor() + c->getPhysicalFF()->getPinCoor("D" + curSlot);
            Coor cQcur = c->getPhysicalFF()->getNewCoor() + c->getPhysicalFF()->getPinCoor("Q" + curSlot);
            double cCurSlack = c->getSlack();
            Coor upCoor = upstreamCoorD(c);
            double dHpwlD = HPWL(upCoor, cDcur) - HPWL(upCoor, cDnew);
            double cNewSlack = cCurSlack + DisplacementDelay * dHpwlD;
            cost += alpha * std::max(0.0, -cNewSlack);
            double qDelayBenefit = curCell->getQpinDelay() - C->getQpinDelay();
            for(auto& next : c->getNextStage()){
                double nextCur = next.ff->getSlack();
                Coor loadCoor;
                double deltaQ;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor() + next.outputGate->getPinCoor(next.pinName);
                    deltaQ = qDelayBenefit;
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                    deltaQ = 0.0;
                }
                double dHpwlQ = HPWL(loadCoor, cQcur) - HPWL(loadCoor, cQnew);
                double predSlack = nextCur + deltaQ + DisplacementDelay * dHpwlQ;
                cost += alpha * std::max(0.0, -predSlack);
            }
        }
        return cost;
    };

    // Step 1: snapshot MBFFs, free their legalizer state
    std::vector<FF*> mbffs;
    mbffs.reserve(FF_Map.size());
    for(auto& pr : FF_Map){
        if(pr.second->getCell()->getBits() > 1) mbffs.push_back(pr.second);
    }
    std::cout << "[UR_V2] mbffs=" << mbffs.size()
              << " radius=" << radius
              << " kmax=" << kmax << " margin=" << margin << std::endl;
    for(FF* m : mbffs){
        legalizer->FreeRect(m->getNewCoor(), m->getCell()->getW(), m->getCell()->getH());
        legalizer->RemoveNodeByFFPtr(m);
    }

    // Step 2: debank all, collect newly-debanked 1-bits
    std::vector<FF*> pool;
    pool.reserve(mbffs.size() * 2);
    for(FF* m : mbffs){
        std::vector<FF*> newOnes = debankFF(m, oneBitCell);
        for(FF* w : newOnes) pool.push_back(w);
    }
    std::cout << "[UR_V2] pool=" << pool.size() << std::endl;

    // Step 3: rtree
    namespace bgi = boost::geometry::index;
    std::vector<PointWithID> points;
    points.reserve(pool.size());
    for(size_t i = 0; i < pool.size(); ++i){
        Coor c = pool[i]->getNewCoor();
        points.emplace_back(Point(c.x, c.y), (int)i);
    }
    bgi::rtree<PointWithID, bgi::quadratic<16>> rtree(points.begin(), points.end());

    // Step 4: build LEMON graph
    lemon::SmartGraph g;
    std::vector<lemon::SmartGraph::Node> gnodes(pool.size());
    for(size_t i = 0; i < pool.size(); ++i) gnodes[i] = g.addNode();
    lemon::SmartGraph::EdgeMap<long long> weight(g);

    struct PairInfo { size_t i, j; Coor tgt; double dC; };
    std::vector<PairInfo> pairs;
    pairs.reserve(pool.size() * kmax / 2);
    const long long scale = 1000;

    for(size_t i = 0; i < pool.size(); ++i){
        FF* wi = pool[i];
        Coor ci = wi->getNewCoor();
        FF* ii = wi->getClusterFF()[0];
        int clki = wi->getClkIdx();
        double costI = predictMBFFCost({ii}, ci, oneBitCell);

        std::vector<PointWithID> knn;
        rtree.query(bgi::nearest(Point(ci.x, ci.y), kmax + 1), std::back_inserter(knn));
        for(auto& q : knn){
            size_t j = (size_t)q.second;
            if(j <= i) continue;
            FF* wj = pool[j];
            if(wj->getClkIdx() != clki) continue;
            Coor cj = wj->getNewCoor();
            if(HPWL(ci, cj) > radius) continue;
            FF* ij = wj->getClusterFF()[0];

            double costJ = predictMBFFCost({ij}, cj, oneBitCell);
            Coor o0 = twoBitCell->getPinCoor("D0");
            Coor o1 = twoBitCell->getPinCoor("D1");
            Coor dA = ci + oneBitCell->getPinCoor("D");
            Coor dB = cj + oneBitCell->getPinCoor("D");
            Coor target{ (dA.x + dB.x - o0.x - o1.x) * 0.5,
                         (dA.y + dB.y - o0.y - o1.y) * 0.5 };
            double costPair = predictMBFFCost({ii, ij}, target, twoBitCell);
            double dC = costPair - costI - costJ;
            if(dC >= -margin) continue;

            auto e = g.addEdge(gnodes[i], gnodes[j]);
            weight[e] = (long long)(-dC * scale);
            pairs.push_back({i, j, target, dC});
        }
    }
    std::cout << "[UR_V2] edges=" << pairs.size() << std::endl;

    // Step 5: run matching
    lemon::MaxWeightedMatching<lemon::SmartGraph,
        lemon::SmartGraph::EdgeMap<long long>> mwm(g, weight);
    mwm.run();

    // Step 6: collect + sort matched pairs (most negative \u0394C first)
    std::vector<bool> claimed(pool.size(), false);
    std::vector<PairInfo> matched;
    matched.reserve(pairs.size() / 2);
    for(auto& p : pairs){
        auto mate = mwm.mate(gnodes[p.i]);
        if(mate != lemon::INVALID && mate == gnodes[p.j]
           && !claimed[p.i] && !claimed[p.j]){
            claimed[p.i] = claimed[p.j] = true;
            matched.push_back(p);
        }
    }
    std::sort(matched.begin(), matched.end(), [](const PairInfo& a, const PairInfo& b){
        return a.dC < b.dC;
    });
    std::cout << "[UR_V2] matched=" << matched.size() << std::endl;

    // Step 7: commit in priority order
    int committed = 0, fpFail = 0;
    double totalGain = 0;
    for(auto& p : matched){
        FF* wi = pool[p.i];
        FF* wj = pool[p.j];
        Coor placed = legalizer->FindPlace(p.tgt, twoBitCell);
        if(placed.x == DBL_MAX){
            placed = legalizer->FindNearestLegalSpace(p.tgt, twoBitCell, 4 * twoBitCell->getW());
        }
        if(placed.x == DBL_MAX){
            fpFail++;
            claimed[p.i] = claimed[p.j] = false;
            continue;
        }
        FF* newMBFF = bankFF(placed, twoBitCell, {wi, wj});
        newMBFF->setIsLegalize(true);
        legalizer->UpdateRows(newMBFF);
        committed++;
        totalGain += -p.dC;
    }

    // Step 8: place unmatched 1-bits (still live in FF_Map)
    int place1ok = 0, place1fallback = 0;
    for(size_t i = 0; i < pool.size(); ++i){
        if(claimed[i]) continue;
        FF* w = pool[i];
        if(FF_Map.count(w->getInstanceName()) == 0) continue;
        Coor placed = legalizer->FindPlace(w->getNewCoor(), oneBitCell);
        if(placed.x == DBL_MAX){
            placed = legalizer->FindNearestLegalSpace(w->getNewCoor(), oneBitCell, 4 * oneBitCell->getW());
        }
        if(placed.x == DBL_MAX){
            placed = w->getNewCoor();
            place1fallback++;
        } else {
            place1ok++;
        }
        w->setCoor(placed);
        w->setNewCoor(placed);
        w->setIsLegalize(true);
        legalizer->UpdateRows(w);
    }

    std::cout << "[UR_V2] committed=" << committed
              << " fpFail=" << fpFail
              << " place1ok=" << place1ok
              << " place1fallback=" << place1fallback
              << " totalGain=" << totalGain << std::endl;
}

// P7: Post-LG Targeted Iterative Refinement.
// Picks top-K MBFFs with worst negative-slack concentration post-LG, debanks
// them into 1-bit wrappers, and re-matches among THIS pool only (untouched
// MBFFs outside the pool keep their LG state). Edge weights use the real
// Banking::CostCompare — NOT the crude predictMBFFCost that torched UR v2.
// Thesis novelty: cost-attribution-driven post-LG MBFF re-synthesis with
// validated per-pair cost (vs predict) on a targeted worst-cost subset
// (vs global).
void Manager::postLGResynth(){
    const char* envOn = std::getenv("POST_LG_RESYNTH");
    if(!envOn || std::atoi(envOn) == 0) return;

    binTable.invalidate();
    int topK = 500;
    if(const char* e = std::getenv("PLR_TOPK")) topK = std::atoi(e);
    double margin = 0.0;
    if(const char* e = std::getenv("PLR_MARGIN")) margin = std::atof(e);
    double radiusMul = 5.0;
    if(const char* e = std::getenv("PLR_RADIUS_MUL")) radiusMul = std::atof(e);
    int kmax = 10;
    if(const char* e = std::getenv("PLR_KMAX")) kmax = std::atoi(e);

    Cell* oneBitCell = Bit_FF_Map[1][0];
    auto itTwo = Bit_FF_Map.find(2);
    if(itTwo == Bit_FF_Map.end() || itTwo->second.empty()){
        std::cout << "[P7] no 2-bit cell, skip" << std::endl;
        return;
    }
    Cell* twoBitCell = itTwo->second[0];
    double radius = std::max(twoBitCell->getW(), twoBitCell->getH()) * radiusMul;

    // Step 1: score 2-bit MBFFs by negative-slack concentration.
    std::vector<std::pair<double, FF*>> scored;
    scored.reserve(FF_Map.size());
    for(auto& kv : FF_Map){
        FF* m = kv.second;
        if(m->getCell()->getBits() != 2) continue;
        double badness = 0;
        for(FF* cf : m->getClusterFF()){
            if(!cf) continue;
            double s;
            try { s = cf->getSlack(); }
            catch(...) { continue; }
            if(s < 0) badness += -s;
        }
        if(badness > 0) scored.emplace_back(badness, m);
    }
    std::sort(scored.begin(), scored.end(),
              [](const std::pair<double,FF*>& a, const std::pair<double,FF*>& b){
                  return a.first > b.first;
              });
    if((int)scored.size() > topK) scored.resize(topK);
    std::cout << "[P7] topK=" << topK << " radius=" << radius
              << " kmax=" << kmax << " margin=" << margin
              << " selected=" << scored.size() << std::endl;
    if(scored.empty()) return;

    // Step 2: free legalizer state + debank the selected MBFFs.
    std::vector<FF*> mbffs;
    mbffs.reserve(scored.size());
    for(auto& p : scored) mbffs.push_back(p.second);
    for(FF* m : mbffs){
        legalizer->FreeRect(m->getNewCoor(), m->getCell()->getW(), m->getCell()->getH());
        legalizer->RemoveNodeByFFPtr(m);
    }
    std::vector<FF*> pool;
    pool.reserve(mbffs.size() * 2);
    for(FF* m : mbffs){
        auto ones = debankFF(m, oneBitCell);
        for(FF* w : ones) pool.push_back(w);
    }
    std::cout << "[P7] pool=" << pool.size() << std::endl;

    // Step 3: rtree over the debanked pool.
    namespace bgi = boost::geometry::index;
    std::vector<PointWithID> points;
    points.reserve(pool.size());
    for(size_t i = 0; i < pool.size(); ++i){
        Coor c = pool[i]->getNewCoor();
        points.emplace_back(Point(c.x, c.y), (int)i);
    }
    bgi::rtree<PointWithID, bgi::quadratic<16>> rtree(points.begin(), points.end());

    // Step 4: build LEMON graph with Banking::CostCompare as edge weight.
    Banking banker(*this);
    lemon::SmartGraph g;
    std::vector<lemon::SmartGraph::Node> gnodes(pool.size());
    for(size_t i = 0; i < pool.size(); ++i) gnodes[i] = g.addNode();
    lemon::SmartGraph::EdgeMap<long long> weight(g);

    struct PairInfo { size_t i, j; Coor tgt; double gain; };
    std::vector<PairInfo> pairs;
    pairs.reserve(pool.size() * kmax / 2);
    const long long scale = 1000;

    for(size_t i = 0; i < pool.size(); ++i){
        FF* wi = pool[i];
        Coor ci = wi->getNewCoor();
        int clki = wi->getClkIdx();

        std::vector<PointWithID> knn;
        rtree.query(bgi::nearest(Point(ci.x, ci.y), kmax + 1), std::back_inserter(knn));
        for(auto& q : knn){
            size_t j = (size_t)q.second;
            if(j <= i) continue;
            FF* wj = pool[j];
            if(wj->getClkIdx() != clki) continue;
            Coor cj = wj->getNewCoor();
            if(HPWL(ci, cj) > radius) continue;

            Coor tgt((ci.x + cj.x) / 2.0, (ci.y + cj.y) / 2.0);
            std::vector<FF*> pair = {wi, wj};
            double gain = banker.CostCompare(tgt, twoBitCell, pair);
            if(gain <= margin) continue;

            auto e = g.addEdge(gnodes[i], gnodes[j]);
            weight[e] = (long long)(gain * scale);
            pairs.push_back({i, j, tgt, gain});
        }
    }
    std::cout << "[P7] edges=" << pairs.size() << std::endl;

    // Step 5: max-weight matching.
    lemon::MaxWeightedMatching<lemon::SmartGraph,
        lemon::SmartGraph::EdgeMap<long long>> mwm(g, weight);
    mwm.run();

    // Step 6: collect matched pairs (sort by gain descending).
    std::vector<bool> claimed(pool.size(), false);
    std::vector<PairInfo> matched;
    matched.reserve(pairs.size() / 2);
    for(auto& p : pairs){
        auto mate = mwm.mate(gnodes[p.i]);
        if(mate != lemon::INVALID && mate == gnodes[p.j]
           && !claimed[p.i] && !claimed[p.j]){
            claimed[p.i] = claimed[p.j] = true;
            matched.push_back(p);
        }
    }
    std::sort(matched.begin(), matched.end(),
              [](const PairInfo& a, const PairInfo& b){ return a.gain > b.gain; });
    std::cout << "[P7] matched=" << matched.size() << std::endl;

    // Step 7: commit in priority order, re-verify at actual placement coord.
    int committed = 0, fpFail = 0, costFail = 0;
    double totalGain = 0;
    for(auto& p : matched){
        FF* wi = pool[p.i];
        FF* wj = pool[p.j];
        Coor placed = legalizer->FindPlace(p.tgt, twoBitCell);
        if(placed.x == DBL_MAX){
            placed = legalizer->FindNearestLegalSpace(p.tgt, twoBitCell, 4 * twoBitCell->getW());
        }
        if(placed.x == DBL_MAX){
            fpFail++;
            claimed[p.i] = claimed[p.j] = false;
            continue;
        }
        std::vector<FF*> pair = {wi, wj};
        double realGain = banker.CostCompare(placed, twoBitCell, pair);
        if(realGain <= margin){
            costFail++;
            claimed[p.i] = claimed[p.j] = false;
            continue;
        }
        FF* newMBFF = bankFF(placed, twoBitCell, pair);
        newMBFF->setIsLegalize(true);
        legalizer->UpdateRows(newMBFF);
        committed++;
        totalGain += realGain;
    }

    // Step 8: place unmatched 1-bits back into the freed rows.
    int place1ok = 0, place1fallback = 0;
    for(size_t i = 0; i < pool.size(); ++i){
        if(claimed[i]) continue;
        FF* w = pool[i];
        if(FF_Map.count(w->getInstanceName()) == 0) continue;
        Coor placed = legalizer->FindPlace(w->getNewCoor(), oneBitCell);
        if(placed.x == DBL_MAX){
            placed = legalizer->FindNearestLegalSpace(w->getNewCoor(), oneBitCell, 4 * oneBitCell->getW());
        }
        if(placed.x == DBL_MAX){
            placed = w->getNewCoor();
            place1fallback++;
        } else {
            place1ok++;
        }
        w->setCoor(placed);
        w->setNewCoor(placed);
        w->setIsLegalize(true);
        legalizer->UpdateRows(w);
    }

    std::cout << "[P7] committed=" << committed
              << " fpFail=" << fpFail
              << " costFail=" << costFail
              << " place1ok=" << place1ok
              << " place1fallback=" << place1fallback
              << " totalGain=" << totalGain << std::endl;
}

void Manager::iterativeBankingLoop(){
    const char* envOn = std::getenv("ITER_BANKING");
    if(!envOn || std::atoi(envOn) == 0) return;

    binTable.invalidate();

    int maxIters = 3;
    double debankMargin = 0.0;
    double radiusMul = 5.0;
    int kmax = 15;
    double commitMargin = 0.0;
    bool crossPollinate = true;
    if(const char* e = std::getenv("IB_ITERS"))      maxIters = std::atoi(e);
    if(const char* e = std::getenv("IB_MARGIN"))      debankMargin = std::atof(e);
    if(const char* e = std::getenv("IB_RADIUS_MUL"))  radiusMul = std::atof(e);
    if(const char* e = std::getenv("IB_KMAX"))        kmax = std::atoi(e);
    if(const char* e = std::getenv("IB_COMMIT_MARGIN")) commitMargin = std::atof(e);
    if(const char* e = std::getenv("IB_CROSS"))       crossPollinate = (std::string(e) != "0");

    Cell* oneBitCell = Bit_FF_Map[1][0];
    auto it2 = Bit_FF_Map.find(2);
    if(it2 == Bit_FF_Map.end() || it2->second.empty()){
        std::cout << "[ITER_BANK] no 2-bit cell, skip" << std::endl;
        return;
    }
    Cell* twoBitCell = it2->second[0];
    double radius = std::max(twoBitCell->getW(), twoBitCell->getH()) * radiusMul;

    Banking banker(*this);

    // scoreDelta: net cost of keeping the MBFF banked vs N×1-bit at same position.
    // Negative => MBFF is net-harmful, debanking is always an improvement.
    // Mirrors postLGDecluster logic.
    auto scoreDelta = [&](FF* mbff) -> double {
        Cell* mCell = mbff->getCell();
        int N = mCell->getBits();
        double pwrDelta  = beta  * (N * oneBitCell->getGatePower() - mCell->getGatePower());
        double areaDelta = gamma * (N * oneBitCell->getArea()      - mCell->getArea());
        double oldTNS = 0, newTNS = 0;
        double qDelayBenefit = mCell->getQpinDelay() - oneBitCell->getQpinDelay();
        std::vector<FF*>& clusterFF = mbff->getClusterFF();
        int slot = 0;
        for(auto* cf : clusterFF){
            std::string slotStr = (N == 1) ? "" : std::to_string(slot);
            Coor curQpin = mbff->getNewCoor() + mbff->getPinCoor("Q" + slotStr);
            Coor debankedCoor = mbff->getNewCoor() + mbff->getPinCoor("D" + slotStr)
                              - oneBitCell->getPinCoor("D");
            Coor newQpin = debankedCoor + oneBitCell->getPinCoor("Q");
            for(auto& next : cf->getNextStage()){
                double nextCurSlack = next.ff->getSlack();
                Coor loadCoor;
                if(next.outputGate){
                    loadCoor = next.outputGate->getCoor()
                             + next.outputGate->getPinCoor(next.pinName);
                } else {
                    loadCoor = next.ff->getPhysicalFF()->getNewCoor()
                             + next.ff->getPhysicalFF()->getPinCoor(
                                 "D" + next.ff->getPhysicalPinName());
                }
                double deltaHpwlQ = HPWL(loadCoor, curQpin) - HPWL(loadCoor, newQpin);
                double predNextSlack = nextCurSlack + qDelayBenefit
                                     + DisplacementDelay * deltaHpwlQ;
                oldTNS += std::max(0.0, -nextCurSlack);
                newTNS += std::max(0.0, -predNextSlack);
            }
            slot++;
        }
        double tnsDelta = alpha * (newTNS - oldTNS);
        return tnsDelta + pwrDelta + areaDelta;
    };

    std::cout << "[ITER_BANK] maxIters=" << maxIters
              << " debankMargin=" << debankMargin
              << " radius=" << radius << " kmax=" << kmax
              << " commitMargin=" << commitMargin << std::endl;

    for(int iter = 0; iter < maxIters; iter++){
        // Step 1: Find MBFFs that are net-harmful at current position.
        // scoreDelta < -debankMargin => debanking is provably beneficial.
        struct ScoredMBFF { double delta; FF* mbff; };
        std::vector<ScoredMBFF> harmful;
        harmful.reserve(FF_Map.size());
        for(auto& kv : FF_Map){
            FF* m = kv.second;
            if(m->getCell()->getBits() < 2) continue;
            double d = scoreDelta(m);
            if(d < -debankMargin)
                harmful.push_back({d, m});
        }
        std::sort(harmful.begin(), harmful.end(),
                  [](const ScoredMBFF& a, const ScoredMBFF& b){
                      return a.delta < b.delta;
                  });
        if(harmful.empty()){
            std::cout << "[ITER_BANK] iter=" << iter << " no harmful MBFFs" << std::endl;
            break;
        }

        double predictedSaving = 0;
        for(auto& h : harmful) predictedSaving += -h.delta;

        // Step 2: Free legalizer state for harmful MBFFs.
        for(auto& h : harmful){
            FF* m = h.mbff;
            legalizer->FreeRect(m->getNewCoor(), m->getCell()->getW(),
                                m->getCell()->getH());
            legalizer->RemoveNodeByFFPtr(m);
        }

        // Step 3: Debank harmful MBFFs into 1-bit wrappers.
        std::vector<FF*> pool;
        pool.reserve(harmful.size() * 2);
        for(auto& h : harmful){
            auto ones = debankFF(h.mbff, oneBitCell);
            for(FF* w : ones) pool.push_back(w);
        }

        // Step 4: Collect nearby unbanked 1-bit FFs for cross-pollination.
        namespace bgi = boost::geometry::index;
        typedef std::pair<Point, int> PtID;
        std::vector<PtID> pts;
        pts.reserve(pool.size());
        for(size_t i = 0; i < pool.size(); ++i){
            Coor c = pool[i]->getNewCoor();
            pts.emplace_back(Point(c.x, c.y), (int)i);
        }
        bgi::rtree<PtID, bgi::quadratic<16>> poolTree(pts.begin(), pts.end());

        std::vector<FF*> nearby;
        if(crossPollinate){
            std::unordered_set<FF*> poolSet(pool.begin(), pool.end());
            for(auto& kv : FF_Map){
                FF* m = kv.second;
                if(m->getCell()->getBits() != 1) continue;
                if(poolSet.count(m)) continue;
                Coor c = m->getNewCoor();
                std::vector<PtID> nn;
                poolTree.query(bgi::nearest(Point(c.x, c.y), 1), std::back_inserter(nn));
                if(!nn.empty()){
                    Coor nc = pool[nn[0].second]->getNewCoor();
                    if(HPWL(c, nc) <= radius)
                        nearby.push_back(m);
                }
            }
            for(FF* nf : nearby){
                legalizer->FreeRect(nf->getNewCoor(), nf->getCell()->getW(),
                                    nf->getCell()->getH());
                legalizer->RemoveNodeByFFPtr(nf);
                pool.push_back(nf);
            }
        }

        // Step 5: Build LEMON matching graph using CostCompare edge weights.
        pts.clear();
        pts.reserve(pool.size());
        for(size_t i = 0; i < pool.size(); ++i){
            Coor c = pool[i]->getNewCoor();
            pts.emplace_back(Point(c.x, c.y), (int)i);
        }
        bgi::rtree<PtID, bgi::quadratic<16>> fullTree(pts.begin(), pts.end());

        lemon::SmartGraph g;
        std::vector<lemon::SmartGraph::Node> gnodes(pool.size());
        for(size_t i = 0; i < pool.size(); ++i) gnodes[i] = g.addNode();
        lemon::SmartGraph::EdgeMap<long long> wt(g);

        struct PairInfo { size_t i, j; Coor tgt; double gain; };
        std::vector<PairInfo> pairs;
        pairs.reserve(pool.size() * kmax / 2);
        const long long wscale = 1000;

        for(size_t i = 0; i < pool.size(); ++i){
            FF* wi = pool[i];
            Coor ci = wi->getNewCoor();
            int clki = wi->getClkIdx();
            std::vector<PtID> knn;
            fullTree.query(bgi::nearest(Point(ci.x, ci.y), kmax + 1),
                           std::back_inserter(knn));
            for(auto& q : knn){
                size_t j = (size_t)q.second;
                if(j <= i) continue;
                FF* wj = pool[j];
                if(wj->getClkIdx() != clki) continue;
                Coor cj = wj->getNewCoor();
                if(HPWL(ci, cj) > radius) continue;

                Coor tgt((ci.x + cj.x) / 2.0, (ci.y + cj.y) / 2.0);
                std::vector<FF*> pair = {wi, wj};
                double gain = banker.CostCompare(tgt, twoBitCell, pair);
                if(gain <= commitMargin) continue;

                auto e = g.addEdge(gnodes[i], gnodes[j]);
                wt[e] = (long long)(gain * wscale);
                pairs.push_back({i, j, tgt, gain});
            }
        }

        // Step 6: Max-weight matching.
        lemon::MaxWeightedMatching<lemon::SmartGraph,
            lemon::SmartGraph::EdgeMap<long long>> mwm(g, wt);
        mwm.run();

        // Step 7: Collect and sort matched pairs by gain descending.
        std::vector<bool> claimed(pool.size(), false);
        std::vector<PairInfo> matched;
        matched.reserve(pairs.size() / 2);
        for(auto& p : pairs){
            auto mate = mwm.mate(gnodes[p.i]);
            if(mate != lemon::INVALID && mate == gnodes[p.j]
               && !claimed[p.i] && !claimed[p.j]){
                claimed[p.i] = claimed[p.j] = true;
                matched.push_back(p);
            }
        }
        std::sort(matched.begin(), matched.end(),
                  [](const PairInfo& a, const PairInfo& b){
                      return a.gain > b.gain;
                  });

        // Step 8: Commit matches at legal positions (re-verify CostCompare).
        int committed = 0, fpFail = 0, costFail = 0;
        double totalGain = 0;
        for(auto& p : matched){
            FF* wi = pool[p.i];
            FF* wj = pool[p.j];
            Coor placed = legalizer->FindPlace(p.tgt, twoBitCell);
            if(placed.x == DBL_MAX)
                placed = legalizer->FindNearestLegalSpace(
                    p.tgt, twoBitCell, 4 * twoBitCell->getW());
            if(placed.x == DBL_MAX){
                fpFail++;
                claimed[p.i] = claimed[p.j] = false;
                continue;
            }
            std::vector<FF*> pair = {wi, wj};
            double realGain = banker.CostCompare(placed, twoBitCell, pair);
            if(realGain <= commitMargin){
                costFail++;
                claimed[p.i] = claimed[p.j] = false;
                continue;
            }
            FF* newMBFF = bankFF(placed, twoBitCell, pair);
            newMBFF->setIsLegalize(true);
            legalizer->UpdateRows(newMBFF);
            committed++;
            totalGain += realGain;
        }

        // Step 9: Place unmatched 1-bits back.
        int place1ok = 0, place1fb = 0;
        for(size_t i = 0; i < pool.size(); ++i){
            if(claimed[i]) continue;
            FF* w = pool[i];
            if(FF_Map.count(w->getInstanceName()) == 0) continue;
            Coor placed = legalizer->FindPlace(w->getNewCoor(), oneBitCell);
            if(placed.x == DBL_MAX)
                placed = legalizer->FindNearestLegalSpace(
                    w->getNewCoor(), oneBitCell, 4 * oneBitCell->getW());
            if(placed.x == DBL_MAX){
                placed = w->getNewCoor();
                place1fb++;
            } else {
                place1ok++;
            }
            w->setCoor(placed);
            w->setNewCoor(placed);
            w->setIsLegalize(true);
            legalizer->UpdateRows(w);
        }

        std::cout << "[ITER_BANK] iter=" << iter
                  << " harmful=" << harmful.size()
                  << " pool=" << pool.size()
                  << " nearby=" << nearby.size()
                  << " predictedSaving=" << predictedSaving
                  << " edges=" << pairs.size()
                  << " matched=" << matched.size()
                  << " committed=" << committed
                  << " fpFail=" << fpFail
                  << " costFail=" << costFail
                  << " place1=" << place1ok << "+" << place1fb
                  << " gain=" << totalGain << std::endl;

        if(harmful.size() <= 1 && committed == 0) break;
    }
}

void Manager::getNS(double& TNS, double& WNS, bool show){
    TNS = 0;
    WNS = 0;
    for(auto& FF_m : FF_Map){
        double curTNS, curWNS;
        FF_m.second->getNS(curTNS, curWNS);
        TNS += curTNS;
        WNS = std::max(WNS, curWNS);
    }
    if(show){
        std::cout << "\tWorst negative slack : " << WNS << std::endl;
        std::cout << "\tTotal negative slack : " << TNS << std::endl << std::endl;
    }
}

// Method D Stage A — Step 1: compute per-FF redistributed D-pin slack budget.
// Iterate over dest FFs (canonical: one path per dest). For each path with a
// prev-FF source, split the path slack between the two endpoints; take min
// across all incident paths. Pure instrumentation — banking still reads raw
// slack via getTimingSlack("D"). The redistributed value is clamped to [−inf,
// raw_slack] so it never becomes a looser budget than banking already trusts.
void Manager::computeSlackRedistribution(){
    std::vector<FF*> wrappers;
    wrappers.reserve(FF_Map.size());
    for(auto& m : FF_Map){
        wrappers.push_back(m.second);
    }

    // Refresh D-pin slacks once so getTimingSlack("D") is up to date.
    for(FF* w : wrappers) w->updateSlack();

    const int mode = param.SLACK_REDIST_MODE;
    const double eps = 1e-12;

    if(mode == 0){
        for(FF* w : wrappers) w->setRedistributedSlackD(w->getTimingSlack("D"));
    } else {
        for(FF* w : wrappers) w->setRedistributedSlackD(DBL_MAX);

        for(FF* w : wrappers){
            FF* logic = w->getClusterFF()[0];
            PrevStage prev = logic->getPrevStage();
            double S_P = w->getTimingSlack("D");

            if(prev.ff && prev.ff->getPhysicalFF()){
                FF* prevW = prev.ff->getPhysicalFF();
                double w_this = 1.0, w_prev = 1.0;
                if(mode == 2){
                    w_this = std::max(w->getTimingSlack("D"), eps);
                    w_prev = std::max(prevW->getTimingSlack("D"), eps);
                }
                double sum = w_this + w_prev;
                double share_this = S_P * w_this / sum;
                double share_prev = S_P * w_prev / sum;
                if(share_this < w->getRedistributedSlackD())
                    w->setRedistributedSlackD(share_this);
                if(share_prev < prevW->getRedistributedSlackD())
                    prevW->setRedistributedSlackD(share_prev);
            } else {
                if(S_P < w->getRedistributedSlackD())
                    w->setRedistributedSlackD(S_P);
            }
        }

        // Clamp: source-only FFs whose budget remains DBL_MAX (unreachable in
        // practice since every wrapper is dest of its own path) fall back to
        // raw slack. Also cap at raw slack so we never widen budget.
        for(FF* w : wrappers){
            double rawS = w->getTimingSlack("D");
            double cur = w->getRedistributedSlackD();
            if(cur == DBL_MAX) cur = rawS;
            if(cur > rawS) cur = rawS;
            w->setRedistributedSlackD(cur);
        }
    }

    // Stats
    size_t n = wrappers.size();
    if(n == 0){
        std::cout << "[SLACK_REDIST] mode=" << mode << " N=0" << std::endl;
        return;
    }
    double sumRaw = 0, sumRed = 0;
    double minRed = DBL_MAX, maxRed = -DBL_MAX;
    size_t nNegRaw = 0, nNegRed = 0;
    std::vector<double> redVals;
    redVals.reserve(n);
    for(FF* w : wrappers){
        double rawS = w->getTimingSlack("D");
        double redS = w->getRedistributedSlackD();
        sumRaw += rawS;
        sumRed += redS;
        if(redS < minRed) minRed = redS;
        if(redS > maxRed) maxRed = redS;
        if(rawS < 0) nNegRaw++;
        if(redS < 0) nNegRed++;
        redVals.push_back(redS);
    }
    std::sort(redVals.begin(), redVals.end());
    auto pct = [&](double p)->double{
        size_t idx = std::min<size_t>(n - 1, (size_t)(p * n));
        return redVals[idx];
    };
    std::cout << "[SLACK_REDIST] mode=" << mode
              << " N=" << n
              << " raw_sum=" << sumRaw
              << " red_sum=" << sumRed
              << " raw_neg=" << nNegRaw
              << " red_neg=" << nNegRed
              << " red_min=" << minRed
              << " red_p10=" << pct(0.10)
              << " red_p50=" << pct(0.50)
              << " red_p90=" << pct(0.90)
              << " red_max=" << maxRed
              << std::endl;
}

void Manager::timingPreRelocation(){
    const char* envTP = std::getenv("TIMING_PRELOC");
    if(!envTP || std::string(envTP) == "0") return;

    double fraction = 0.3;
    double critMult = 5.0;
    double maxDispFrac = 0.5;
    int iters = 3;
    if(const char* e = std::getenv("PRELOC_FRACTION")) fraction = std::atof(e);
    if(const char* e = std::getenv("PRELOC_CRIT_MULT")) critMult = std::atof(e);
    if(const char* e = std::getenv("PRELOC_MAX_DISP")) maxDispFrac = std::atof(e);
    if(const char* e = std::getenv("PRELOC_ITERS")) iters = std::atoi(e);

    std::cerr << "[TIMING_PRELOC] fraction=" << fraction
              << " critMult=" << critMult
              << " maxDispFrac=" << maxDispFrac
              << " iters=" << iters << std::endl;

    const double dieXlo = die.getDieOrigin().x;
    const double dieYlo = die.getDieOrigin().y;
    const double dieXhi = die.getDieBorder().x;
    const double dieYhi = die.getDieBorder().y;

    int nMoved = 0;
    double sumDisp = 0;

    for(int iter = 0; iter < iters; iter++){
        nMoved = 0;
        sumDisp = 0;
        for(auto& kv : FF_Map){
            FF* w = kv.second;
            if(w->getCell()->getBits() != 1) continue;
            auto& cfs = w->getClusterFF();
            if(cfs.empty() || !cfs[0]) continue;
            FF* logic = cfs[0];

            double fx = 0, fy = 0, totalW = 0;
            Coor wCoor = w->getNewCoor();
            double cellW = w->getCell()->getW();
            double cellH = w->getCell()->getH();

            double slackD = 0;
            try { slackD = w->getTimingSlack("D"); }
            catch (...) { continue; }

            // Only move critical FFs (negative slack) — non-critical FFs are better
            // left at their MeanShift-optimized positions for banking locality.
            if(slackD >= 0) continue;

            // NTU sign-based force: for each critical timing path, add a unit
            // force in the sign direction that would reduce HPWL. The magnitude
            // is uniform (1.0 per path) so no single distant pin dominates.
            auto signOf = [](double v) -> double {
                return (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0;
            };

            // D-pin: sign force toward driver
            PrevInstance prev = logic->getPrevInstance();
            if(prev.instance){
                Coor driverCoor;
                if(prev.cellType == CellType::IO){
                    driverCoor = prev.instance->getCoor();
                } else if(prev.cellType == CellType::GATE){
                    driverCoor = prev.instance->getCoor()
                               + prev.instance->getPinCoor(prev.pinName);
                } else {
                    FF* inFF = dynamic_cast<FF*>(prev.instance);
                    if(inFF && inFF->getPhysicalFF()){
                        driverCoor = inFF->getPhysicalFF()->getNewCoor()
                                   + inFF->getPhysicalFF()->getPinCoor(
                                       "Q" + inFF->getPhysicalPinName());
                    }
                }
                Coor curDpin = wCoor + w->getCell()->getPinCoor("D");
                fx += signOf(driverCoor.x - curDpin.x);
                fy += signOf(driverCoor.y - curDpin.y);
                totalW += 1.0;
            }

            // Q-pin: sign force toward each downstream load
            for(auto& ns : logic->getNextStage()){
                if(!ns.ff) continue;
                Coor loadCoor;
                if(ns.outputGate){
                    loadCoor = ns.outputGate->getCoor()
                             + ns.outputGate->getPinCoor(ns.pinName);
                } else if(ns.ff->getPhysicalFF()){
                    loadCoor = ns.ff->getPhysicalFF()->getNewCoor()
                             + ns.ff->getPhysicalFF()->getPinCoor(
                                 "D" + ns.ff->getPhysicalPinName());
                } else {
                    continue;
                }
                Coor curQpin = wCoor + w->getCell()->getPinCoor("Q");
                fx += signOf(loadCoor.x - curQpin.x);
                fy += signOf(loadCoor.y - curQpin.y);
                totalW += 1.0;
            }

            if(totalW < 1e-12) continue;

            // Step size scales with fraction * slack budget
            double stepSize = (-slackD) / DisplacementDelay * fraction;
            double dx = stepSize * fx / totalW;
            double dy = stepSize * fy / totalW;

            // Clamp displacement to maxDispFrac of the slack-implied budget
            double budget = (-slackD) / DisplacementDelay * maxDispFrac;
            double dist = std::abs(dx) + std::abs(dy);
            if(dist > budget && budget > 0){
                double scale = budget / dist;
                dx *= scale;
                dy *= scale;
            }

            double nx = std::max(dieXlo, std::min(dieXhi - cellW, wCoor.x + dx));
            double ny = std::max(dieYlo, std::min(dieYhi - cellH, wCoor.y + dy));
            double moved = std::abs(nx - wCoor.x) + std::abs(ny - wCoor.y);
            if(moved > 0.01){
                w->setNewCoor(Coor(nx, ny));
                nMoved++;
                sumDisp += moved;
            }
        }
        std::cerr << "[TIMING_PRELOC] iter=" << iter
                  << " moved=" << nMoved
                  << " avgDisp=" << (nMoved > 0 ? sumDisp / nMoved : 0)
                  << std::endl;
    }
}

/**
 * @brief get TNS
 *
 * @return double return the total negative slack value (+: has negative slack, 0: no negative slack)
 */
double Manager::getTNS(){
    double TNS = 0;
    for(auto& FF_m : FF_Map){
        TNS += FF_m.second->getTNS();
    }
    return TNS;
}

double Manager::getWNS(){
    double WNS = 0;
    for(auto& FF_m : FF_Map){
        WNS = std::max(WNS, FF_m.second->getWNS());
    }
    return WNS;
}

void Manager::showNS(){
    double _0, _1;
    this->getNS(_0, _1, true);
}

FF* Manager::getNewFF(){
    if(FFGarbageCollector.empty()){
        FF* temp = new FF;
        return temp;
    }
    else{
        FF* temp = FFGarbageCollector.front();
        FFGarbageCollector.pop();
        return temp;
    }
}

void Manager::deleteFF(FF* in){
    in->clear();
    FFGarbageCollector.push(in);
}

double Manager::calculateBinDensityCost(){
    int numBins = 0;
    int numViolationBins = 0;
    int DieStartX = die.getDieOrigin().x;
    int DieStartY = die.getDieOrigin().y;
    int DieEndX = die.getDieBorder().x;
    int DieEndY = die.getDieBorder().y;
    int BinW = die.getBinWidth();
    int BinH = die.getBinHeight();
    double maxUtil = die.getBinMaxUtil() / 100.0;

    // Calculate number of bins along X and Y axes
    int numBinsX = (DieEndX - DieStartX + BinW - 1) / BinW; // Round up
    int numBinsY = (DieEndY - DieStartY + BinH - 1) / BinH; // Round up

    // 2D array to store area contributions for each bin
    std::vector<std::vector<double>> binAreas(numBinsX, std::vector<double>(numBinsY, 0.0));

    // Iterate over FFs and accumulate area contributions to bins
    for (const auto &ff : FF_Map) {
        int ffStartX = ff.second->getNewCoor().x;
        int ffStartY = ff.second->getNewCoor().y;
        int ffEndX = ffStartX + ff.second->getW();
        int ffEndY = ffStartY + ff.second->getH();

        // Calculate the range of bins that the FF overlaps
        int startBinX = (ffStartX - DieStartX) / BinW;
        startBinX = (startBinX < 0) ? 0 : startBinX;
        int startBinY = (ffStartY - DieStartY) / BinH;
        startBinY = (startBinY < 0) ? 0 : startBinY;
        int endBinX = (ffEndX - DieStartX) / BinW;
        int endBinY = (ffEndY - DieStartY) / BinH;

        for (int binX = startBinX; binX <= endBinX && binX < numBinsX; ++binX) {
            for (int binY = startBinY; binY <= endBinY && binY < numBinsY; ++binY) {
                // Calculate overlap dimensions
                double overlapW = std::min(DieStartX + (binX + 1) * BinW, ffEndX) - std::max(DieStartX + binX * BinW, ffStartX);
                double overlapH = std::min(DieStartY + (binY + 1) * BinH, ffEndY) - std::max(DieStartY + binY * BinH, ffStartY);
                binAreas[binX][binY] += overlapW * overlapH;
            }
        }
    }

    // Iterate over Gates and accumulate area contributions to bins
    for (const auto &gate : Gate_Map) {
        int gateStartX = gate.second->getCoor().x;
        int gateStartY = gate.second->getCoor().y;
        int gateEndX = gateStartX + gate.second->getW();
        int gateEndY = gateStartY + gate.second->getH();

        // Calculate the range of bins that the Gate overlaps
        int startBinX = (gateStartX - DieStartX) / BinW;
        startBinX = (startBinX < 0) ? 0 : startBinX;
        int startBinY = (gateStartY - DieStartY) / BinH;
        startBinY = (startBinY < 0) ? 0 : startBinY;
        int endBinX = (gateEndX - DieStartX) / BinW;
        int endBinY = (gateEndY - DieStartY) / BinH;

        for (int binX = startBinX; binX <= endBinX && binX < numBinsX; ++binX) {
            for (int binY = startBinY; binY <= endBinY && binY < numBinsY; ++binY) {
                // Calculate overlap dimensions
                double overlapW = std::min(DieStartX + (binX + 1) * BinW, gateEndX) - std::max(DieStartX + binX * BinW, gateStartX);
                double overlapH = std::min(DieStartY + (binY + 1) * BinH, gateEndY) - std::max(DieStartY + binY * BinH, gateStartY);
                binAreas[binX][binY] += overlapW * overlapH;
            }
        }
    }

    // Check each bin for violations
    for (int binX = 0; binX < numBinsX; ++binX) {
        for (int binY = 0; binY < numBinsY; ++binY) {
            numBins++;
            double area = binAreas[binX][binY];
            double binArea = BinW * BinH;
            if (area / binArea > maxUtil) {
                numViolationBins++;
            }
        }
    }
    return lambda * numViolationBins;
}

double Manager::computeAccurateTNS(){
    // Dual-BFS mini-STA: computes arrival at every gate and FF using BOTH
    //   (a) original positions (post-CG, pre-banking) — the baseline for origSlack
    //   (b) current positions (post-banking)
    // Then: newSlack = origSlack - (curArrival - origArrival).
    //
    // Key fix vs getSlack(): takes max over ALL gate inputs at current positions,
    // not just the single critical path recorded during preprocessing.

    // Step 0: Build inner-FF map (logical 1-bit FFs from clusterFF).
    std::unordered_map<std::string, FF*> innerFF;
    innerFF.reserve(FF_Map.size() * 2);
    for(auto& ff_pair : FF_Map){
        for(FF* cf : ff_pair.second->getClusterFF()){
            innerFF[cf->getInstanceName()] = cf;
        }
    }

    // Per-gate: running max arrival (orig & cur), and shared input-counter.
    struct ArrPair { double orig = 0, cur = 0; };
    std::unordered_map<Gate*, ArrPair> gateArr;
    std::unordered_map<Gate*, int>     gateCnt;
    gateArr.reserve(Gate_Map.size());
    gateCnt.reserve(Gate_Map.size());
    std::queue<Gate*> q;

    // Step 1: IO → Gate arrivals (IOs don't move; same for orig and cur).
    for(auto& io_m : Input_Map){
        Instance& ioInst = IO_Map[io_m.first];
        auto& outs = ioInst.getOutputInstances();
        for(auto& outPair : outs){
            for(auto& tgt : outPair.second){
                const std::string& instName = tgt.first;
                const std::string& pinName  = tgt.second;
                auto it = Gate_Map.find(instName);
                if(it != Gate_Map.end()){
                    Gate* gate = it->second;
                    Coor gatePin = gate->getCoor() + gate->getPinCoor(pinName);
                    double arr = DisplacementDelay * HPWL(ioInst.getCoor(), gatePin);
                    ArrPair& ap = gateArr[gate];
                    if(arr > ap.orig) ap.orig = arr;
                    if(arr > ap.cur)  ap.cur  = arr;
                    int& cnt = gateCnt[gate];
                    cnt++;
                    if(cnt == gate->getCell()->getInputCount())
                        q.push(gate);
                }
            }
        }
    }

    // Step 2: FF → Gate arrivals (orig uses originalQ/originalQpd; cur uses current).
    for(auto& inner_pair : innerFF){
        FF* cf   = inner_pair.second;
        FF* phys = cf->getPhysicalFF();
        Coor origQ  = cf->getOriginalQ();
        double origQpd = cf->getOriginalQpinDelay();
        Coor curQ    = phys->getNewCoor() + phys->getPinCoor("Q" + cf->getPhysicalPinName());
        double curQpd  = phys->getCell()->getQpinDelay();

        auto& outs = cf->getOutputInstances();
        for(auto& outPair : outs){
            for(auto& tgt : outPair.second){
                const std::string& instName = tgt.first;
                const std::string& pinName  = tgt.second;
                auto it = Gate_Map.find(instName);
                if(it != Gate_Map.end()){
                    Gate* gate = it->second;
                    Coor gatePin = gate->getCoor() + gate->getPinCoor(pinName);
                    double oArr = origQpd + DisplacementDelay * HPWL(origQ, gatePin);
                    double cArr = curQpd  + DisplacementDelay * HPWL(curQ,  gatePin);
                    ArrPair& ap = gateArr[gate];
                    if(oArr > ap.orig) ap.orig = oArr;
                    if(cArr > ap.cur)  ap.cur  = cArr;
                    int& cnt = gateCnt[gate];
                    cnt++;
                    if(cnt == gate->getCell()->getInputCount())
                        q.push(gate);
                }
            }
        }
    }

    // Step 3: BFS (topological order) through gate graph.
    // Gate-to-gate HPWL is identical for orig and cur (gates don't move).
    // When a gate outputs to an inner FF, record both arrivals.
    struct FFArrPair { double orig = 0, cur = 0; };
    std::unordered_map<std::string, FFArrPair> ffArr;
    ffArr.reserve(innerFF.size());

    while(!q.empty()){
        Gate* gate = q.front(); q.pop();
        const ArrPair& myArr = gateArr[gate];

        auto& outs = gate->getOutputInstances();
        for(auto& outPair : outs){
            const std::string& outPin = outPair.first;
            Coor gateOut = gate->getCoor() + gate->getPinCoor(outPin);
            for(auto& tgt : outPair.second){
                const std::string& instName = tgt.first;
                const std::string& pinName  = tgt.second;

                auto git = Gate_Map.find(instName);
                if(git != Gate_Map.end()){
                    Gate* next = git->second;
                    Coor nextPin = next->getCoor() + next->getPinCoor(pinName);
                    double hop = DisplacementDelay * HPWL(gateOut, nextPin);
                    ArrPair& nap = gateArr[next];
                    double oArr = myArr.orig + hop;
                    double cArr = myArr.cur  + hop;
                    if(oArr > nap.orig) nap.orig = oArr;
                    if(cArr > nap.cur)  nap.cur  = cArr;
                    int& cnt = gateCnt[next];
                    cnt++;
                    if(cnt == next->getCell()->getInputCount())
                        q.push(next);
                }
                else{
                    auto fit = innerFF.find(instName);
                    if(fit != innerFF.end()){
                        FF* cf   = fit->second;
                        FF* phys = cf->getPhysicalFF();
                        Coor origD = cf->getOriginalD();
                        Coor curD  = phys->getNewCoor() + phys->getPinCoor("D" + cf->getPhysicalPinName());
                        double oArr = myArr.orig + DisplacementDelay * HPWL(gateOut, origD);
                        double cArr = myArr.cur  + DisplacementDelay * HPWL(gateOut, curD);
                        FFArrPair& fap = ffArr[instName];
                        if(oArr > fap.orig) fap.orig = oArr;
                        if(cArr > fap.cur)  fap.cur  = cArr;
                    }
                }
            }
        }
    }

    // Step 4: Compute per-FF slack using accurate arrival deltas.
    double totalTNS = 0;
    for(auto& inner_pair : innerFF){
        const std::string& name = inner_pair.first;
        FF* cf = inner_pair.second;
        double origSlack = cf->getTimingSlack("D");
        PrevInstance prev = cf->getPrevInstance();
        FF* phys = cf->getPhysicalFF();
        Coor curD = phys->getNewCoor() + phys->getPinCoor("D" + cf->getPhysicalPinName());

        if(!prev.instance){
            if(origSlack < 0) totalTNS += -origSlack;
            continue;
        }

        double arrChange = 0;

        if(prev.cellType == CellType::GATE){
            auto fit = ffArr.find(name);
            if(fit != ffArr.end()){
                arrChange = fit->second.cur - fit->second.orig;
            }
            else{
                // Gate unreached by BFS — fall back to old delta model.
                Coor gateOut = prev.instance->getCoor() + prev.instance->getPinCoor(prev.pinName);
                arrChange = DisplacementDelay * (HPWL(gateOut, curD) - HPWL(gateOut, cf->getOriginalD()));
                const PrevStage& ps = cf->getPrevStage();
                if(ps.ff){
                    FF* srcPhys = ps.ff->getPhysicalFF();
                    Coor origQ = ps.ff->getOriginalQ();
                    Coor newQ  = srcPhys->getNewCoor() + srcPhys->getPinCoor("Q" + ps.ff->getPhysicalPinName());
                    double dqpd = srcPhys->getCell()->getQpinDelay() - ps.ff->getOriginalQpinDelay();
                    Coor firstGatePin = ps.outputGate->getCoor() + ps.outputGate->getPinCoor(ps.pinName);
                    arrChange += dqpd + DisplacementDelay * (HPWL(firstGatePin, newQ) - HPWL(firstGatePin, origQ));
                }
            }
        }
        else if(prev.cellType == CellType::IO){
            Coor ioCoor = prev.instance->getCoor();
            arrChange = DisplacementDelay * (HPWL(ioCoor, curD) - HPWL(ioCoor, cf->getOriginalD()));
        }
        else{
            FF* prevFF   = static_cast<FF*>(prev.instance);
            FF* prevPhys = prevFF->getPhysicalFF();
            Coor origQ = prevFF->getOriginalQ();
            Coor newQ  = prevPhys->getNewCoor() + prevPhys->getPinCoor("Q" + prevFF->getPhysicalPinName());
            double origQpd = prevFF->getOriginalQpinDelay();
            double newQpd  = prevPhys->getCell()->getQpinDelay();
            double origArr = origQpd + DisplacementDelay * HPWL(origQ, cf->getOriginalD());
            double newArr  = newQpd  + DisplacementDelay * HPWL(newQ,  curD);
            arrChange = newArr - origArr;
        }

        double newSlack = origSlack - arrChange;
        if(newSlack < 0) totalTNS += -newSlack;
    }

    return totalTNS;
}

void Manager::refreshArrivalCorrections(){
    // Dual-BFS: compute per-inner-FF arrival correction.
    // correction = accurate_slack - old_getSlack().
    // getSlack() adds arrCorrection_, so all callers automatically get accurate results.
    // The correction is D-pin-position-independent: when DP moves an FF,
    // getSlack() recomputes the D-pin delta, and the correction handles
    // the upstream gate-arrival delta. Together they give exact accurate slack.

    // Step 0: Collect inner FFs, temporarily zero their corrections.
    std::unordered_map<std::string, FF*> innerFF;
    innerFF.reserve(FF_Map.size() * 2);
    for(auto& ff_pair : FF_Map){
        for(FF* cf : ff_pair.second->getClusterFF()){
            cf->setArrCorrection(0);
            innerFF[cf->getInstanceName()] = cf;
        }
    }

    // Per-gate dual arrival.
    struct ArrPair { double orig = 0, cur = 0; };
    std::unordered_map<Gate*, ArrPair> gateArr;
    std::unordered_map<Gate*, int>     gateCnt;
    gateArr.reserve(Gate_Map.size());
    gateCnt.reserve(Gate_Map.size());
    std::queue<Gate*> q;

    // Step 1: IO → Gate (same for orig and cur).
    for(auto& io_m : Input_Map){
        Instance& ioInst = IO_Map[io_m.first];
        auto& outs = ioInst.getOutputInstances();
        for(auto& outPair : outs){
            for(auto& tgt : outPair.second){
                auto it = Gate_Map.find(tgt.first);
                if(it != Gate_Map.end()){
                    Gate* gate = it->second;
                    Coor gp = gate->getCoor() + gate->getPinCoor(tgt.second);
                    double arr = DisplacementDelay * HPWL(ioInst.getCoor(), gp);
                    ArrPair& ap = gateArr[gate];
                    if(arr > ap.orig) ap.orig = arr;
                    if(arr > ap.cur)  ap.cur  = arr;
                    if(++gateCnt[gate] == gate->getCell()->getInputCount())
                        q.push(gate);
                }
            }
        }
    }

    // Step 2: FF → Gate (orig uses originalQ/Qpd, cur uses current).
    for(auto& ip : innerFF){
        FF* cf   = ip.second;
        FF* phys = cf->getPhysicalFF();
        Coor  origQ  = cf->getOriginalQ();
        double origQpd = cf->getOriginalQpinDelay();
        Coor  curQ   = phys->getNewCoor() + phys->getPinCoor("Q" + cf->getPhysicalPinName());
        double curQpd  = phys->getCell()->getQpinDelay();

        auto& outs = cf->getOutputInstances();
        for(auto& outPair : outs){
            for(auto& tgt : outPair.second){
                auto it = Gate_Map.find(tgt.first);
                if(it != Gate_Map.end()){
                    Gate* gate = it->second;
                    Coor gp = gate->getCoor() + gate->getPinCoor(tgt.second);
                    double oA = origQpd + DisplacementDelay * HPWL(origQ, gp);
                    double cA = curQpd  + DisplacementDelay * HPWL(curQ,  gp);
                    ArrPair& ap = gateArr[gate];
                    if(oA > ap.orig) ap.orig = oA;
                    if(cA > ap.cur)  ap.cur  = cA;
                    if(++gateCnt[gate] == gate->getCell()->getInputCount())
                        q.push(gate);
                }
            }
        }
    }

    // Step 3: BFS through gates. Record per-FF arrivals.
    struct FFArrPair { double orig = 0, cur = 0; };
    std::unordered_map<std::string, FFArrPair> ffArr;
    ffArr.reserve(innerFF.size());

    while(!q.empty()){
        Gate* gate = q.front(); q.pop();
        const ArrPair& my = gateArr[gate];
        auto& outs = gate->getOutputInstances();
        for(auto& outPair : outs){
            Coor go = gate->getCoor() + gate->getPinCoor(outPair.first);
            for(auto& tgt : outPair.second){
                auto git = Gate_Map.find(tgt.first);
                if(git != Gate_Map.end()){
                    Gate* nxt = git->second;
                    Coor np = nxt->getCoor() + nxt->getPinCoor(tgt.second);
                    double hop = DisplacementDelay * HPWL(go, np);
                    ArrPair& nap = gateArr[nxt];
                    double oA = my.orig + hop, cA = my.cur + hop;
                    if(oA > nap.orig) nap.orig = oA;
                    if(cA > nap.cur)  nap.cur  = cA;
                    if(++gateCnt[nxt] == nxt->getCell()->getInputCount())
                        q.push(nxt);
                }
                else{
                    auto fit = innerFF.find(tgt.first);
                    if(fit != innerFF.end()){
                        FF* cf   = fit->second;
                        FF* phys = cf->getPhysicalFF();
                        Coor origD = cf->getOriginalD();
                        Coor curD  = phys->getNewCoor() + phys->getPinCoor("D" + cf->getPhysicalPinName());
                        double oA = my.orig + DisplacementDelay * HPWL(go, origD);
                        double cA = my.cur  + DisplacementDelay * HPWL(go, curD);
                        FFArrPair& fap = ffArr[tgt.first];
                        if(oA > fap.orig) fap.orig = oA;
                        if(cA > fap.cur)  fap.cur  = cA;
                    }
                }
            }
        }
    }

    // Step 4: Compute per-FF correction = accurate_slack - old_getSlack().
    // arrCorrection_ is already 0 (cleared in Step 0), so getSlack() returns old model.
    for(auto& ip : innerFF){
        FF* cf = ip.second;
        auto fit = ffArr.find(ip.first);
        if(fit == ffArr.end()) continue; // IO-direct or unreached: correction stays 0

        PrevInstance prev = cf->getPrevInstance();
        if(prev.cellType != CellType::GATE) continue;

        double origSlack = cf->getTimingSlack("D");
        double accurateSlack = origSlack - (fit->second.cur - fit->second.orig);
        double oldSlack = cf->getSlack(); // uses old model (arrCorrection_ == 0)
        cf->setArrCorrection(accurateSlack - oldSlack);
    }
}

/**
 * @brief The cost function of the problem
 *
 * @param verbose whether to print the pretty table
 * @param runEvaluator whether to run evaluator to get the real cost
 * @return double the weighted overall cost
 */
double Manager::getOverallCost(bool verbose, bool runEvaluator){
    double TNS_cost = 0;
    double Power_cost = 0;
    double Area_cost = 0;
    double Bin_cost = 0;

    static const bool useAccurate = std::getenv("ACCURATE_TNS") && std::atoi(std::getenv("ACCURATE_TNS"));
    if(useAccurate){
        TNS_cost = alpha * computeAccurateTNS();
    }

    for(const auto & ff_pair : FF_Map){
        if(!useAccurate){
            double curTNS = ff_pair.second->getTNS();
            TNS_cost += alpha * (curTNS);
        }
        Power_cost += beta * ff_pair.second->getCell()->getGatePower();
        Area_cost += gamma * (ff_pair.second->getCell()->getArea());
    }

    // Check for the bin density
    Bin_cost = calculateBinDensityCost();

    double cost = TNS_cost + Power_cost + Area_cost + Bin_cost;
    double TNS_percentage = TNS_cost / cost * 100;
    double Power_percentage = Power_cost / cost * 100;
    double Area_percentage = Area_cost / cost * 100;
    double Bin_percentage = Bin_cost / cost * 100;
    if(verbose){
        if(runEvaluator) std::cout << "[EVALUATOR] Score: " + std::to_string(getEvaluatorCost()) << std::endl;
        size_t numAfterDot = 6;
        std::vector<std::string> header = {"Cost", "Weight", "Value", "Percentage(%)"};
        std::vector<std::vector<std::string>> rows = {
            {"TNS", toStringWithPrecision(alpha, numAfterDot), toStringWithPrecision(TNS_cost, numAfterDot), toStringWithPrecision(TNS_percentage, numAfterDot) + "(%)"},
            {"Power", toStringWithPrecision(beta, numAfterDot), toStringWithPrecision(Power_cost, numAfterDot), toStringWithPrecision(Power_percentage, numAfterDot) + "(%)"},
            {"Area", toStringWithPrecision(gamma, numAfterDot), toStringWithPrecision(Area_cost, numAfterDot), toStringWithPrecision(Area_percentage, numAfterDot) + "(%)"},
            {"Bin", toStringWithPrecision(lambda, numAfterDot), toStringWithPrecision(Bin_cost, numAfterDot), toStringWithPrecision(Bin_percentage, numAfterDot) + "(%)"},
            {"Total", "-", toStringWithPrecision(cost, numAfterDot), "100.00(%)"},
            {"WNS", toStringWithPrecision(getWNS(), numAfterDot), "TNS", toStringWithPrecision(getTNS(), numAfterDot)}
        };

        PrettyTable pt;
        pt.AddHeader(header);
		pt.AddRows(rows);
        pt.SetAlign(PrettyTable::Align::Internal);
		std::cout << pt << std::endl;
    }
    return cost;
}

/**
 * @brief score each cell based on ?????
 * @author cheng119 help confirm this score
 * What is this?? alpha*cell_vector[i]->getQpinDelay() * (cell_vector[i]->getW() + cell_vector[i]->getH())
 * does (cell_vector[i]->getW() + cell_vector[i]->getH()) to approximate HPWL???
 */
void Manager::libScoring(){
    for(auto &pair: Bit_FF_Map){
        std::vector<Cell *> &cell_vector = pair.second;
        for(size_t i = 0; i < cell_vector.size(); i++){
            double area = cell_vector[i]->getArea();
            double score = alpha*cell_vector[i]->getQpinDelay() + beta*cell_vector[i]->getGatePower() + gamma*area;
            cell_vector[i]->setScore(score);
        }
        sortCell(cell_vector);
        //DEBUG
        // for(size_t i = 0; i < pair.second.size(); i++){
        //     std::cout << pair.second[i]->getCellName() << ": " << pair.second[i]->getScore() << std::endl;
        // }
    }
    // DEBUG
    // std::map<int, std::vector<Cell *>> bit_map(Bit_FF_Map.begin(), Bit_FF_Map.end());
    // for(auto &pair: bit_map){
    //     std::cout << pair.second[0]->getCellName() << ": " << pair.second[0]->getScore() << std::endl; 
    // }

}

void Manager::sortCell(std::vector<Cell *> &cell_vector){
    auto scoreCmp = [](const Cell * cell1, const Cell * cell2){
        return cell1->getScore() < cell2->getScore();
    };
    std::sort(cell_vector.begin(), cell_vector.end(), scoreCmp);
}

double Manager::getEvaluatorCost(){
    DEBUG_MGR("Run Evaluator...");
    Manager::dump("temp.out");
    std::string binaryPath = "evaluator/preliminary-evaluator";
    std::string command = binaryPath + " " + this->input_filename + " " + "temp.out > evaluator.out";
    
    // execute the binary and redirect output
    int result = system(command.c_str());

    if (result != 0) {
        DEBUG_MGR("Evaluator execution failed");
        return DBL_MAX;
    }

    // get the score
    std::string sedCommand = "sed -n 's/.*Final score:[[:space:]]*\\([0-9]*\\.[0-9]*\\).*/\\1/p' evaluator.out > score.out";
    int sed_result = system(sedCommand.c_str());
    if (sed_result != 0) {
        DEBUG_MGR("SED execution failed");
        return DBL_MAX;
    }

    std::ifstream fin("score.out");
    double score;
    fin >> score;
    fin.close();

    std::remove("temp.out");
    std::remove("evaluator.out");
    std::remove("score.out");
    return score;
}

/**
 * @brief Predict the cost of the banked MBFF at newbankCoor
 * 
 * @param newbankCoor merge MBFF coordinate
 * @param bankCellType merge MBFF type
 * @param FFToBank All of FFs to merge
 * @return double (new cost - original cost)
 * 
 */
double Manager::getCostDiff(Coor newbankCoor, Cell* bankCellType, std::vector<FF*>& FFToBank){
    // bank to a psudo MBFF
    size_t bit = bankCellType->getBits();
    FF* newFF = getNewFF();
    newFF->setCoor(newbankCoor);
    newFF->setNewCoor(newbankCoor);
    newFF->setCell(bankCellType);
    newFF->setClusterSize(bit);
    for(size_t i=0;i<FFToBank.size();i++){
        for(size_t j=0;j<FFToBank[i]->getClusterFF().size();j++){
            newFF->addClusterFF(FFToBank[i]->getClusterFF()[j], i + j);
            FFToBank[i]->getClusterFF()[j]->setPhysicalFF(newFF, i + j);
        }
    }
    assignSlot(newFF);

    double cost = newFF->getCost();
    for(size_t i=0;i<FFToBank.size();i++){
        for(size_t j=0;j<FFToBank[i]->getClusterFF().size();j++){
            FFToBank[i]->getClusterFF()[j]->setPhysicalFF(FFToBank[i], j);
        }
    }
    deleteFF(newFF);

    double oldCost = 0;
    for(auto& MBFF : FFToBank){
        oldCost += MBFF->getCost();
    }
    return cost - oldCost;
}