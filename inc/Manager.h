#ifndef _MANAGER_H_
#define _MANAGER_H_

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>
#include "Coor.h"
#include "Instance.h"
#include "Die.h"
#include "Cell_Library.h"
#include "FF.h"
#include "Gate.h"
#include "Net.h"
#include "Parser.h"
#include "Dumper.h"
#include "MeanShift.h"
#include "Preprocess.h"
#include "Cluster.h"
#include "Banking.h"
#include "Legalizer.h"
#include "DetailPlacement.h"
#include "Util.h"
#include "PrettyTable.h"
#include "PostBankingOptimizer.h"
#include "Checker.h"
#include "BinDensityTable.h"

#ifdef ENABLE_DEBUG_MGR
#define DEBUG_MGR(message) std::cout << "[MANAGER] " << message << std::endl
#else
#define DEBUG_MGR(message)
#endif

class FF;
class Preprocess;
class Cluster;
class Banking;
class Param;
class Legalizer;
class Manager{
public:
    // cost function weight
    double alpha;
    double beta;
    double gamma;
    double lambda;
    double DisplacementDelay;
    
    // die info
    Die die;

    // I/O pin coordinate
    int NumInput;
    int NumOutput;
    std::unordered_map<std::string, Coor> Input_Map;
    std::unordered_map<std::string, Coor> Output_Map;
    std::unordered_map<std::string, Instance> IO_Map;

    // Cell library
    Cell_Library cell_library;
    std::unordered_map<int, std::vector<Cell *>> Bit_FF_Map;
    int MaxBit;

    // Instance
    int NumInstances;
    std::unordered_map<std::string, FF *> FF_Map;
    std::unordered_map<std::string, FF *> originalFF_Map;
    std::unordered_map<std::string, Gate *> Gate_Map;
    

    // Netlist
    int NumNets;
    std::unordered_map<std::string, Net> Net_Map;

    // for naming
    std::unordered_map<std::string, int> name_record;

    // preprocess
    Preprocess* preprocessor;

    // Legalize
    Legalizer* legalizer;

    // parameter
    Param param;

    // pointer recycle
    std::queue<FF*> FFGarbageCollector;

    // IO filename
    std::string input_filename;

    // Bin-density table for incremental Δλ in CostCompare. Lazily built on
    // first query when BIN_DENSITY_AWARE=1; maintained by bankFF / debankFF.
    BinDensityTable binTable;

public:
    Manager();
    ~Manager();

    void parse(const std::string &filename);
    void preprocess();
    void meanshift();
    void preLegalize();
    void computeSlackRedistribution();
    void timingPreRelocation();
    void banking();
    void postBankingOptimize();
    void legalize();
    void detailplacement();
    void checker();

    void dump(const std::string &filename);
    void dumpVisual(const std::string &filename);
    void print();
    
    std::string getNewFFName(const std::string&); // using a prefix string to get new unique FF name
                                                  // suggest prefix for N-bit MBFF -> FF_N_

    FF* bankFF(Coor newbankCoor, Cell* bankCellType, std::vector<FF*> FFToBank);
    // given newbankCoor (left down) and target celltype
    // it will bank all the FF in vector (can be MBFF in FFToBank)
    // and it will delete old and insert new FF to FF_Map

    // Hybrid Route A / Stage 1 — rollback-capable banking.
    // Mirrors bankFF() EXCEPT all FF_Map mutations and wrapper deleteFF() are
    // DEFERRED. On rollback: nothing is inserted into or erased from FF_Map —
    // only per-FF physicalFF state and the newMBFF pointer are restored. This
    // is load-bearing for bit-exactness: a rollback must leave the FF_Map's
    // internal bucket/chain state byte-identical to pre-call, and std::
    // unordered_map does not guarantee hash-state rollback across insert/erase.
    //
    // Callers must pair each bankFF_deferred with exactly one of:
    //   rollbackBank(undo)          → restores physicalFF, recycles new FF
    //   commitFinalizeBank(undo)    → inserts/erases FF_Map, fires deferred deletes
    struct BankUndo {
        FF* newMBFF = nullptr;                                // returned MBFF (brand-new, GC-sourced)
        std::string newName;                                  // its instance name (to be inserted on commit)
        std::vector<FF*> pendingInsertWrappers;               // pending deletion after finalize
        std::vector<std::string> pendingEraseNames;           // wrapper names to erase from FF_Map on commit
        struct InnerState {
            FF* innerFF;
            FF* oldPhysical;
            int oldSlot;
        };
        std::vector<InnerState> innerStates;                  // per-bit old physicalFF / slot
    };
    FF* bankFF_deferred(Coor newbankCoor, Cell* bankCellType,
                        const std::vector<FF*>& FFToBank,
                        BankUndo& undo);
    void rollbackBank(BankUndo& undo);
    void commitFinalizeBank(BankUndo& undo);

    void assignSlot(FF* newFF);
    std::vector<FF*> debankFF(FF* MBFF, Cell* debankCellType);
    void debankAll();
    void postLGDecluster(); // Phase 5: undo bad banking decisions using LG-accurate positions
    int perLevelDecluster(int targetBit, double threshold = 0.0);
    void unbankRebank();    // v1: debank 4-bit MBFFs and re-bank constituents as 2x 2-bit if \u0394C < 0
    void unbankRebankGlobal(); // v2: debank ALL MBFFs and run LEMON max-weight matching on constituents at post-LG coords
    void postLGResynth(); // P7: pick top-K worst MBFFs by neg-slack concentration, debank + LEMON rematch with real Banking::CostCompare weights
    void iterativeBankingLoop(); // Iterative post-LG: debank worst MBFFs + nearby 1-bits, re-match at actual positions, repeat

    // --- Evaluator-Guided Refinement (EGR) ---
    struct EGRUndoEntry {
        std::string originalName;
        Cell* originalCell;
        Coor originalPos;
        int clkIdx;
        std::vector<FF*> freedFFs;
    };
    void evaluatorRefinement(const std::string& testcasePath);
    double runEvaluator(const std::string& testcasePath, const std::string& outputPath);
    std::vector<FF*> rankMBFFByDisplacement();
    EGRUndoEntry debankWithUndo(FF* mbff);
    void reLegalizeFreedFFs(EGRUndoEntry& entry);
    void revertDebank(EGRUndoEntry& entry);

    // the FF after debank will be assign to debankCellType (maybe this can be a vector)
    void getNS(double& TNS, double& WNS, bool show); // this retunr TNS and WNS of whole design (all FF in FF_Map)
    double getTNS();
    double getWNS();
    void showNS();
    // for lib cell scoring
    void libScoring();
    static void sortCell(std::vector<Cell *> &cell_vector);
    
    // pointer recycle
    FF* getNewFF();
    void deleteFF(FF*);
    
    double getCostDiff(Coor newbankCoor, Cell* bankCellType, std::vector<FF*>& FFToBank); // > 0 -> after bank cost will be larger
    double getEvaluatorCost();
    double calculateBinDensityCost();
    double computeAccurateTNS();
    void   refreshArrivalCorrections();
    double getOverallCost(bool verbose, bool runEvaluator);
    friend class Parser;
    friend class Dumper;
    friend class MeanShift;
    friend class Preprocess;
    friend class Banking;
    friend class Legalizer;
    friend class DetailPlacement;
    friend class Checker;

private:
    bool isIOPin(const std::string &pinName);
};

#endif