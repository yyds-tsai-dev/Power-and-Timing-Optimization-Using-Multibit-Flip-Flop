#ifndef _MEAN_SHIFT_H_
#define _MEAN_SHIFT_H_

#ifdef ENABLE_DEBUG_MS
#define DEBUG_MS(message) std::cout << "[MEANSHIFT] " << message << std::endl
#else
#define DEBUG_MS(message)
#endif

#define BOOST_ALLOW_DEPRECATED_HEADERS


#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/foreach.hpp>
#include <vector>
#include <omp.h>
#include "Util.h"
#include "FF.h"
#include "Manager.h"

namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;
namespace bgm = boost::geometry::model;

// 2-D point with coordinate type of double in cartesian
typedef bg::model::point<double, 2, bg::cs::cartesian> Point;

// Define a Point with an ID:
typedef std::pair<Point, int> PointWithID;

// The R-tree stores PointWithID elements and uses a quadratic split algorithm for node splitting
typedef bgi::rtree<PointWithID, bgi::quadratic<P_PER_NODE>> RTree;

class FF;
// Phase 3D-(C): weighted K-means based FF pre-banking relocation
// (P10 DAC'16 "Flip-flop Clustering by Weighted K-means Algorithm").
// Class name kept as MeanShift for wiring compatibility with mgr.meanshift().
class MeanShift{
public:
    MeanShift();
    ~MeanShift();

    void run(Manager &mgr);

private:
    void runKMeansOnClkGroup(std::vector<FF*> &ffs, int sizeLimit, double maxSqDisp);
    void initCentersBipartition(std::vector<FF*> group, int K, bool splitByX,
                                std::vector<Coor> &out);
};

#endif
