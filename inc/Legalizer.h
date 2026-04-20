#ifndef _LEGALIZER_H_
#define _LEGALIZER_H_

#include "Cell.h"
#include "Node.h"
#include "Row.h"
#include "Subrow.h"
#include "Manager.h"
#include "Timer.h"
#include "Util.h"
#include "XTour.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cfloat>

#ifdef ENABLE_DEBUG_LGZ
#define DEBUG_LGZ(message) std::cout << "[LEGALIZER] " << message << std::endl
#else
#define DEBUG_LGZ(message)
#endif

class Node;
class Row;

// P4: top-K nearest legal slot probe result (read-only).
// Used by Banking's defer-and-batch path (ALG1_2B) to compute NTU S_space
// priority score D_{U,2} - D_{U,1} before committing any placement.
struct LegalCandidate {
    Coor coor;
    double disp;
};

// Hybrid Route A / Stage 1 — per-row subrow snapshot used to roll back a
// UpdateRows() mutation. SnapshotRowsForRect() deep-copies the subrow vector
// of every row UpdateRows would iterate for a given rect; RestoreRowSubrows
// swaps the copies back in and frees the post-UpdateRows pointers. Snap
// owns the Subrow* copies until one of Restore/Discard is called.
struct RowSubrowSnap {
    size_t row_idx;
    std::vector<Subrow*> subrows;   // deep copies; snap owns
};

class Legalizer{
private:
    Manager& mgr;
    std::vector<Node *> ffs;
    std::vector<Node *> gates;
    std::vector<Row *> rows;
    Timer timer;

public:
    explicit Legalizer(Manager& mgr);
    ~Legalizer();
    void initial();
    void run();
    Coor FindPlace(const Coor &coor, Cell * cell);
    Coor FindNearestLegalSpace(const Coor &coor, Cell* cell, double maxDist);
    // P4: read-only top-2 legal-slot probe used for NTU Algorithm 1 priority.
    // Returns up to 2 nearest legal coords sorted ascending by displacement.
    // Does NOT mutate subrow reject caches (unlike FindPlace).
    std::vector<LegalCandidate> FindTop2LegalCoors(const Coor &coor, Cell *cell);
    void UpdateRows(FF* newFF);
    // Phase 5: free a rect from the row/subrow state (inverse of UpdateRows' slicing).
    // Used by postLGDecluster so the newly-debanked 1-bit FFs can reclaim the
    // MBFF's footprint instead of searching around a phantom-occupied rect.
    void FreeRect(const Coor &lgCoor, double width, double height);
    // Phase 5: drop the Node tracking `ff` from legalizer->ffs so DP's GlobalSwap
    // stops iterating over a stale entry whose FFPtr has been recycled by debankFF.
    void RemoveNodeByFFPtr(FF* ff);
    // Hybrid Route A / Stage 1 — snapshot / restore the subrow vectors that
    // UpdateRows(ffRect) would mutate. Pair with UpdateRows to roll back row
    // state. DiscardRowSnap frees the snap without installing it.
    std::vector<RowSubrowSnap> SnapshotRowsForRect(const Coor& lgCoor, double height);
    void RestoreRowSubrowsFromSnap(std::vector<RowSubrowSnap>& snap);
    void DiscardRowSnap(std::vector<RowSubrowSnap>& snap);
    // Phase 3A: slice rows for a synthetic placement footprint (coord + cell),
    // without binding to a real FF in mgr.FF_Map. Returns the index of the
    // stub Node appended to ffs so the merge phase can upgrade it in place.
    size_t UpdateRowsFootprint(const Coor &coor, Cell *cell);
    // Phase 3A: cheap check whether a given footprint (coord+cell) still fits
    // on the canonical rows. Used to detect cross-thread placement conflicts
    // during the merge phase without paying for a full FindPlace local search.
    bool canPlaceFootprint(const Coor &coor, Cell *cell);
    // Phase 3A: upgrade a footprint stub (placed via UpdateRowsFootprint) into
    // a real FF Node. Rows are already sliced, so this just rebinds name/FFPtr
    // and sets the DP row index — no re-slicing.
    void PromoteFootprintToFF(size_t stubIdx, FF* newFF);

private:
    // void CheckIfMBFFMove();
    void LoadFF();
    void LoadGate();
    void LoadPlacementRow();
    void SliceRowsByRows();
    void SliceRowsByGate();
    void Tetris();
    void LegalizeWriteBack();
    

    // Helper Function
    static void UpdateXList(double start, double end, std::list<XTour> & xList);
    size_t FindClosestRow(const Coor &coor);
    static int FindClosestSubrow(Node *ff, Row *row);
    void PredictFFLGPlace(const Coor &coor, Cell* cell, size_t row_idx, bool &placeable, double &minDisplacement, Coor &newCoor);
    // P4: top-2 variant of PredictFFLGPlace. Read-only: does not touch addRejectCell.
    void PredictFFLGPlaceTop2(const Coor &coor, Cell* cell, size_t row_idx, std::vector<LegalCandidate> &top2);
    double PlaceFF(Node *ff, size_t row_idx, bool& placeable);
    bool ContinousAndEmpty(double startX, double startY, double w, double h, int row_idx);
    static double getDisplacement(const Coor &Coor1, const Coor &Coor2);
    
    friend class DetailPlacement;
};

#endif


// Backup
// #include <iostream>
// #include <boost/icl/interval_map.hpp>
// #include <boost/icl/interval.hpp>

// using namespace boost::icl;

// class Legalizer {
// public:
//     void UpdateXInterval(int start, int end, interval_map<int, int>& imap) {
//         // Define the new interval to be inserted, using closed intervals
//         interval<int>::type newInterval = interval<int>::closed(start, end);

//         // Add or merge the interval into the interval_map
//         imap += std::make_pair(newInterval, 1);

//         // Normalize intervals to merge adjacent ones
//         NormalizeIntervals(imap);

//         // Debug output to show the current state of imap after each update
//         for (const auto& elem : imap) {
//             std::cout << "[" << elem.first.lower() << "," << elem.first.upper() << "] -> " << elem.second << std::endl;
//         }
//         std::cout << "\n\n";
//     }

// private:
//     void NormalizeIntervals(interval_map<int, int>& imap) {
//         interval_map<int, int> mergedMap;
//         for (const auto& elem : imap) {
//             if (!mergedMap.empty()) {
//                 auto lastElem = mergedMap.end();
//                 --lastElem;

//                 if (lastElem->first.upper() >= elem.first.lower() - 1) {
//                     // Merge intervals
//                     interval<int>::type mergedInterval = interval<int>::closed(
//                         lastElem->first.lower(),
//                         std::max(lastElem->first.upper(), elem.first.upper())
//                     );
//                     mergedMap.erase(lastElem);
//                     mergedMap += std::make_pair(mergedInterval, 1);
//                 } else {
//                     mergedMap += elem;
//                 }
//             } else {
//                 mergedMap += elem;
//             }
//         }
//         imap = mergedMap;
//     }
// };

// int main() {
//     interval_map<int, int> imap;
//     Legalizer legalizer;

//     // Applying updates with potential overlapping and adjacent intervals
//     legalizer.UpdateXInterval(2, 4, imap);
//     legalizer.UpdateXInterval(6, 12, imap);
//     legalizer.UpdateXInterval(13, 16, imap);
//     legalizer.UpdateXInterval(2, 19, imap);
//     legalizer.UpdateXInterval(5, 8, imap);

//     // Print the final result
//     std::cout << "Final Interval Map:\n";
//     for (const auto& elem : imap) {
//         std::cout << "[" << elem.first.lower() << "," << elem.first.upper() << "] -> " << elem.second << std::endl;
//     }

//     return 0;
// }