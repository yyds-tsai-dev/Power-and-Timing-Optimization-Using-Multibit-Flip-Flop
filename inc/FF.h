#ifndef _FF_H_
#define _FF_H_

#include <iostream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include "Instance.h"
#include "Manager.h"
#include "Util.h"

class Net;

class Manager;
class FF;
class Gate;
struct PrevStage
{
    FF* ff; // start point of critical path (FF)
    Gate* outputGate; // start ff's output gate
    std::string pinName; // input pin of outputGate
};

typedef PrevStage NextStage; // ff -> the end of critical path
                             // outputGate -> your output gate for this critical path
                             // outputGate's pinName

enum class CellType{
    IO = 0,
    FF = 1,
    GATE = 2
};

struct PrevInstance{
    Instance* instance;
    CellType cellType;
    std::string pinName;
};

class FF : public Instance{
private:
    std::unordered_map<std::string, double> TimingSlack;
    std::vector<FF*> clusterFF;
    // ######################################### used in cluster ########################################################
    int ffIdx;
    int clusterIdx;
    Coor newCoor;
    double bandwidth;   // used in gaussian kernel function
    bool isShifting;
    // pair<other ffId, euclidean distance>, store the neighbor ff with their Id and the distance to this FF
    std::vector<std::pair<int, double>> NeighborFFs;
    int clkIdx;
    bool isLegalize;

    // ######################################### used in Preprocessing ########################################################
    PrevStage prevStage; // {prev stage FF/INPUT, {prevFF's output cell on critical path, output cell pin}}
                                                                    // if prev stage FF is nullptr, cur(this) FF is directly connect with prev stage or is IO
                                                                    // use prevInstance
    PrevInstance prevInstance; // prev instance on critical path and its output pin
    std::vector<NextStage> nextStage;
    Coor originalD, originalQ; // initial location for FF list, only can be set in mgr.Debank
    double originalQpinDelay;
    FF* physicalFF;
    int slot;

    // ######################################### Fixed flag ########################################################
    bool fixed;

    // ######################################### Method D Stage A ##################################################
    // Redistributed D-pin slack budget. When SLACK_REDIST_MODE == 0 this stays
    // equal to the raw D-pin slack (or 0 until computeSlackRedistribution runs).
    // Step 1 populates + logs only; banking still reads raw slack.
    double redistributedSlackD;

    // Method D Step 5: inter-batch slack release credit. Accumulated during
    // banking commits when an MBFF lands closer to an upstream driver / closer
    // to a downstream load than the original FF was. Added on top of getSlack()
    // via getEffectiveSlack() and consumed only by matching edge-weight logic
    // under SLACK_RELEASE=1. Not part of getSlack() itself.
    double bankingReleasedSlackD;

    // BFS-based arrival correction. Captures the difference between accurate
    // gate-arrival (max over ALL inputs at current positions) and the stale
    // single-path model (prevStage). Position-independent: D-pin terms cancel.
    // Set by Manager::refreshArrivalCorrections(), consumed by getSlack().
    double arrCorrection_;

    // Net HPWL infrastructure (set by Manager::buildNetHPWLInfra).
    // dNet_: the net connected to this inner FF's D pin in the original netlist.
    // qNet_: the net connected to this inner FF's Q pin in the original netlist.
    // Used by the NET_HPWL=1 timing model to compute per-net bounding-box HPWL
    // instead of per-sink two-point HPWL.
    Net* dNet_;
    Net* qNet_;

public:
    FF();
    explicit FF(int size);
    ~FF();

    // Setters
    void setTimingSlack(const std::string &pinName, double slack);
    void addClusterFF(FF* inputFF, int slot);
    void setFFIdx(int ffIdx);
    void setClusterIdx(int clusterIdx);
    void setClkIdx(int clkIdx);
    void setNewCoor(const Coor &coor);
    void setBandwidth(const Manager &mgr);
    void addNeighbor(int ffIdx, double euclidean_distance);
    void setIsShifting(bool shift);
    void setPrevStage(const PrevStage&);
    void setPrevInstance(const PrevInstance&);
    void addNextStage(const NextStage&);
    void setOriginalCoor(const Coor& coorD, const Coor& coorQ);
    void setOriginalQpinDelay(double);
    void setPhysicalFF(FF* targetFF, int slot);
    void setClusterSize(int);
    void setFixed(bool fixed);
    void setIsLegalize(bool isLegalize);
    void setRedistributedSlackD(double s);
    void addBankingReleasedSlackD(double delta);
    void clearBankingReleasedSlackD();
    void setArrCorrection(double c);
    double getArrCorrection()const;
    void setDNet(Net* n);
    void setQNet(Net* n);
    Net* getDNet()const;
    Net* getQNet()const;
    // Getter
    double getTimingSlack(const std::string &pinName)const;
    std::vector<FF*>& getClusterFF();
    int getFFIdx()const;
    bool getIsCluster()const;
    int getClusterIdx()const;
    int getClkIdx()const;
    Coor getNewCoor()const;
    double getBandwidth()const;
    std::pair<int, double> getNeighbor(int idx)const;
    int getNeighborSize()const;
    bool getIsShifting()const;
    PrevStage getPrevStage()const;
    PrevInstance getPrevInstance()const;
    std::vector<NextStage> getNextStage()const;
    Coor getOriginalD()const;
    Coor getOriginalQ()const;
    double getOriginalQpinDelay()const;
    FF* getPhysicalFF()const;
    int getSlot()const;
    bool getFixed()const;
    bool getIsLegalize()const;
    double getRedistributedSlackD()const;
    double getBankingReleasedSlackD()const;
    double getEffectiveSlack();   // getSlack() + bankingReleasedSlackD; for matching edge weights only
    std::string getPhysicalPinName();
    std::vector<std::pair<Coor, double>> getCriticalCoor(); // return the relative coor on critical path
    size_t getCriticalSize(); // return the size of all critical path both Q and D pin
    double getAllSlack(); // return the slack of all critical path both Q and D pin (can be positive)
    double getCost(); // return overall cost of MBFF(can be 1 bit), include TNS of Q pin (next stage FFs)
    // ######################################### used in cluster ########################################################
    void sortNeighbors();
    double shift(const std::vector<FF *> &FFs);     // shift the ff and return the euclidean distance from origin coordinate
    // ######################################### used in cluster ########################################################

    void getNS(double& TNS, double& WNS); // getNS, getTNS, getWNS will call updateSlack
    double getTNS();
    double getWNS();
    void updateSlack();
    
    void clear(); // clear all the data

    friend std::ostream &operator<<(std::ostream &os, const FF &ff);
    friend class postBankingObjFunction;
    static double DisplacementDelay;
    static double alpha;
    static double beta;
    static double gamma;

    double getSlack(); // don't touch is only for FF in FF_list
};


#endif
