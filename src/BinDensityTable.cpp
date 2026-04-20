#include "BinDensityTable.h"
#include "Manager.h"
#include "FF.h"
#include "Gate.h"
#include "Cell.h"
#include "Die.h"
#include <unordered_map>
#include <iostream>

void BinDensityTable::build(const Manager& mgr) {
    DieStartX = mgr.die.getDieOrigin().x;
    DieStartY = mgr.die.getDieOrigin().y;
    int DieEndX = mgr.die.getDieBorder().x;
    int DieEndY = mgr.die.getDieBorder().y;
    BinW = mgr.die.getBinWidth();
    BinH = mgr.die.getBinHeight();
    double maxUtil = mgr.die.getBinMaxUtil() / 100.0;
    numBinsX = (int)((DieEndX - DieStartX + BinW - 1) / BinW);
    numBinsY = (int)((DieEndY - DieStartY + BinH - 1) / BinH);
    maxArea = BinW * BinH * maxUtil;

    binAreas.assign(numBinsX, std::vector<double>(numBinsY, 0.0));

    for (const auto& kv : mgr.FF_Map) {
        FF* ff = kv.second;
        Coor c = ff->getNewCoor();
        forEachBin(c.x, c.y, ff->getW(), ff->getH(),
                   [&](int bx, int by, double a) { binAreas[bx][by] += a; });
    }
    for (const auto& kv : mgr.Gate_Map) {
        Gate* g = kv.second;
        Coor c = g->getCoor();
        forEachBin(c.x, c.y, g->getW(), g->getH(),
                   [&](int bx, int by, double a) { binAreas[bx][by] += a; });
    }

    curViolations = 0;
    int nearViol = 0;            // bins ≥ 80% of cap (headroom proxy)
    double maxA = 0;
    for (int bx = 0; bx < numBinsX; ++bx)
        for (int by = 0; by < numBinsY; ++by){
            double a = binAreas[bx][by];
            if (a > maxArea) ++curViolations;
            if (a > 0.8 * maxArea) ++nearViol;
            if (a > maxA) maxA = a;
        }

    built = true;
    std::cerr << "[BIN_DENSITY] build: bins=" << numBinsX << "x" << numBinsY
              << " maxArea=" << maxArea
              << " curViolations=" << curViolations
              << " nearViol(>=80%)=" << nearViol
              << " maxBinArea=" << maxA
              << " (ratio=" << (maxA / maxArea) << ")\n";
}

int BinDensityTable::estimateViolationDelta(const std::vector<FF*>& removedFFs,
                                            const Coor& newCoor,
                                            const Cell* newCell) const {
    if (!built || numBinsX == 0 || numBinsY == 0) return 0;

    // Collect Δarea per touched bin without mutating binAreas.
    std::unordered_map<long long, double> delta;
    delta.reserve(removedFFs.size() * 4 + 4);
    auto key = [this](int bx, int by) { return (long long)bx * numBinsY + by; };

    for (FF* f : removedFFs) {
        Coor c = f->getNewCoor();
        forEachBin(c.x, c.y, f->getW(), f->getH(),
                   [&](int bx, int by, double a) { delta[key(bx, by)] -= a; });
    }
    forEachBin(newCoor.x, newCoor.y, newCell->getW(), newCell->getH(),
               [&](int bx, int by, double a) { delta[key(bx, by)] += a; });

    int dViol = 0;
    for (const auto& kv : delta) {
        long long k = kv.first;
        double da = kv.second;
        int bx = (int)(k / numBinsY);
        int by = (int)(k % numBinsY);
        double oldA = binAreas[bx][by];
        double newA = oldA + da;
        bool oldV = oldA > maxArea;
        bool newV = newA > maxArea;
        if (oldV != newV) dViol += newV ? 1 : -1;
    }
    return dViol;
}

void BinDensityTable::applyMutation(const std::vector<Rect>& removed,
                                    const std::vector<Rect>& added) {
    if (!built) return;

    auto applyRect = [&](double sx, double sy, double w, double h, double sign) {
        forEachBin(sx, sy, w, h, [&](int bx, int by, double a) {
            double oldA = binAreas[bx][by];
            double newA = oldA + sign * a;
            binAreas[bx][by] = newA;
            bool oldV = oldA > maxArea;
            bool newV = newA > maxArea;
            if (oldV != newV) curViolations += newV ? 1 : -1;
        });
    };

    for (const Rect& r : removed) applyRect(r.x, r.y, r.w, r.h, -1.0);
    for (const Rect& r : added)   applyRect(r.x, r.y, r.w, r.h, +1.0);
}
