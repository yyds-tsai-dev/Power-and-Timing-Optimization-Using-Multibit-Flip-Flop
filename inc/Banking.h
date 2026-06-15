#ifndef _BANKING_H_
#define _BANKING_H_

#ifdef ENABLE_DEBUG_BAN
#define DEBUG_BAN(message) std::cout << "[BANKING] " << message << std::endl
#else
#define DEBUG_BAN(message)
#endif

#include <vector>
#include "Cluster.h"
#include "Manager.h"
#include "Util.h"
#include "FF.h"
#include "Legalizer.h"

namespace bg = boost::geometry;

// 2-D point with coordinate type of double in cartesian
typedef bg::model::point<double, 2, bg::cs::cartesian> Point;

// Define a Point with an ID:
typedef std::pair<Point, int> PointWithID;

class Manager;
class Cluster;
class FF;
class Legalizer;

class Banking{
private:
    Manager& mgr;
    std::vector<FF *> FFs;
    std::unordered_map<int, int> clusterNum;
    std::vector<int> bitOrder;

public:
    explicit Banking(Manager& mgr);
    ~Banking();

    void run();

    void bitOrdering();
    bool chooseCandidateFF(FF* nowFF, const std::vector<FF*> &localFFs, std::vector<PointWithID>& resultFFs, std::vector<PointWithID>& toRemoveFFs, std::vector<FF*> &FFToBank, const int &targetBit);
    // Cell* chooseCellLib(int bitNum);
    static Coor getMedian(const std::vector<FF*> &localFFs, std::vector<PointWithID>& toRemoveFFs);
    static void sortFFs(std::vector<std::pair<int, double>> &nearFFs);
    void doClustering();
    void doMatchingClustering();
    int  doTopDown4Bit(Cell* cell4bit, Cell* cell2bit);
    void restoreUnclusterFFCoor();
    void ClusterResult();
    void computePinTNS(const std::vector<FF*>& FFToBank, Cell* targetCell,
                       const Coor& placeCoor, double& oldTNS, double& newTNS,
                       double* downstreamMargin = nullptr,
                       bool forceNetHPWL = false);
    Coor findWindowOptimal(const std::vector<FF*>& FFToBank, Cell* targetCell,
                           double xlo, double xhi, double ylo, double yhi);
    double CostCompare(const Coor clusterCoor, Cell* chooseCell, std::vector<FF*> FFToBank);
    double CostCompareNetHPWL(const Coor clusterCoor, Cell* chooseCell, std::vector<FF*> FFToBank);
    Coor ComputeOptimalPosition(Cell* chooseCell, const std::vector<FF*>& FFToBank);
    static double weightedMedian(std::vector<std::pair<double,double>>& coordWeights);

    // RAII opt-in guard: while alive, ENABLES bin-density Δλ in CostCompare.
    // Default behavior is OFF, since most CostCompare callers (graph-build,
    // greedy 4-bit fallback, post-LG resynth) are over-pessimistic against the
    // binTable — penalizing pairs whose violations the legalizer would resolve
    // anyway. Wrap the matching commit Pass-3 realGain check (the only point
    // where the binTable is incrementally accurate) with CommitBinAware.
    static thread_local int commitBinAwareDepth;
    struct CommitBinAware {
        CommitBinAware()  { ++commitBinAwareDepth; }
        ~CommitBinAware() { --commitBinAwareDepth; }
    };
};

#endif