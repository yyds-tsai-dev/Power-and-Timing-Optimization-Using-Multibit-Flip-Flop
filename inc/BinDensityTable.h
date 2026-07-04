#ifndef _BIN_DENSITY_TABLE_H_
#define _BIN_DENSITY_TABLE_H_

#include <vector>
#include <cmath>
#include "Coor.h"

class Manager;
class FF;
class Cell;

// Persistent bin-occupancy table that mirrors Manager::calculateBinDensityCost's
// bin grid. Lets Banking::CostCompare estimate the Δλ contribution of a
// hypothetical bank/debank decision without rebuilding the full grid.
//
// Lifecycle:
//   build(mgr)        — full O(numFF + numGate) sweep from current FF_Map / Gate_Map.
//   estimateViolationDelta(...) — read-only hypothetical (no mutation).
//   applyMutation(removed, added) — incremental update after a real bankFF/debankFF.
//   invalidate() — drop state when external mutations (legalize, DP swap) change positions.
//
// build() is called lazily on the first estimateViolationDelta after invalidate().
class BinDensityTable {
public:
    void build(const Manager& mgr);
    void invalidate() { built = false; binAreas.clear(); curViolations = 0; }
    bool ready() const { return built; }
    int  totalViolations() const { return curViolations; }

    // Read-only geometry / state accessors (density repair pass)
    double binW() const { return BinW; }
    double binH() const { return BinH; }
    double dieX() const { return DieStartX; }
    double dieY() const { return DieStartY; }
    int    nx()   const { return numBinsX; }
    int    ny()   const { return numBinsY; }
    bool   violating(int bx, int by) const { return built && binAreas[bx][by] > maxArea; }
    double areaOf(int bx, int by)    const { return binAreas[bx][by]; }
    double capArea() const { return maxArea; }

    // Hypothetical Δviolations if we removed all `removedFFs` from FF_Map and
    // added a single new MBFF of `newCell` at `newCoor`. Pure read-only.
    // Returns positive if MORE violations would result.
    int estimateViolationDelta(const std::vector<FF*>& removedFFs,
                               const Coor& newCoor, const Cell* newCell) const;

    struct Rect { double x; double y; double w; double h; };

    // Apply a real mutation atomically. Rects are pre-snapshotted from FFs so the
    // caller can preserve original deleteFF/getNewFF ordering (FFGarbageCollector
    // recycles pointers; touching FFs after deleteFF can mutate coor mid-bank).
    void applyMutation(const std::vector<Rect>& removed,
                       const std::vector<Rect>& added);

private:
    int numBinsX = 0;
    int numBinsY = 0;
    double BinW = 0;
    double BinH = 0;
    double DieStartX = 0;
    double DieStartY = 0;
    double maxArea = 0;       // BinW * BinH * (maxUtil/100)
    std::vector<std::vector<double>> binAreas;
    int curViolations = 0;
    bool built = false;

    // Iterate bins covered by rect [sx,sx+w] x [sy,sy+h]; invoke f(bx, by, overlap).
    template <typename F>
    inline void forEachBin(double sx, double sy, double w, double h, F&& f) const {
        double ex = sx + w, ey = sy + h;
        int bxLo = (int)std::floor((sx - DieStartX) / BinW);
        int byLo = (int)std::floor((sy - DieStartY) / BinH);
        int bxHi = (int)std::floor((ex - DieStartX) / BinW);
        int byHi = (int)std::floor((ey - DieStartY) / BinH);
        if (bxLo < 0) bxLo = 0;
        if (byLo < 0) byLo = 0;
        if (bxHi >= numBinsX) bxHi = numBinsX - 1;
        if (byHi >= numBinsY) byHi = numBinsY - 1;
        for (int bx = bxLo; bx <= bxHi; ++bx) {
            double xl = std::max(sx, DieStartX + bx * BinW);
            double xh = std::min(ex, DieStartX + (bx + 1) * BinW);
            double ow = xh - xl;
            if (ow <= 0) continue;
            for (int by = byLo; by <= byHi; ++by) {
                double yl = std::max(sy, DieStartY + by * BinH);
                double yh = std::min(ey, DieStartY + (by + 1) * BinH);
                double oh = yh - yl;
                if (oh <= 0) continue;
                f(bx, by, ow * oh);
            }
        }
    }
};

#endif
