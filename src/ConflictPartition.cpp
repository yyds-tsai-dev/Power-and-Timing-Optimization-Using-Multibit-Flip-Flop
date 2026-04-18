#include "ConflictPartition.h"
#include "FF.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cfloat>

namespace ConflictPartition {

std::vector<std::vector<int>> buildConflictGraph(
    const std::vector<FF*>& ffs, int hops, double slackThresh)
{
    std::vector<std::vector<int>> adj(ffs.size());
    if(hops <= 0 || ffs.size() < 2) return adj;

    // ffs[i] is a cluster FF (wrapper). Prev/next stage lives on the original
    // FFs inside getClusterFF(). Map original FF* -> cluster index.
    std::unordered_map<FF*, int> idxOf;
    idxOf.reserve(ffs.size() * 4);
    for(int i = 0; i < (int)ffs.size(); i++){
        for(FF* o : ffs[i]->getClusterFF()){
            if(o) idxOf[o] = i;
        }
    }

    std::vector<double> slack(ffs.size(), DBL_MAX);
    int ffsWithPrev = 0, ffsWithNext = 0, totalNextEdges = 0;
    for(size_t i = 0; i < ffs.size(); i++){
        // Each cluster FF wraps one or more original 1-bit FFs; min D-pin
        // slack across constituents summarizes the wrapper's criticality.
        for(FF* o : ffs[i]->getClusterFF()){
            if(!o) continue;
            double s = o->getTimingSlack("D");
            if(s < slack[i]) slack[i] = s;
            if(o->getPrevStage().ff) ffsWithPrev++;
            auto ns = o->getNextStage();
            if(!ns.empty()){
                ffsWithNext++;
                totalNextEdges += (int)ns.size();
            }
        }
        if(slack[i] == DBL_MAX) slack[i] = 0.0;
    }
    std::cerr << "[PARTITION] diag ffs=" << ffs.size()
              << " ffsWithPrev=" << ffsWithPrev
              << " ffsWithNext=" << ffsWithNext
              << " totalNextEdges=" << totalNextEdges << std::endl;

    // Deduped via per-source set to avoid double edges, then flatten symmetrically.
    std::vector<std::unordered_set<int>> outEdges(ffs.size());

    for(int i = 0; i < (int)ffs.size(); i++){
        std::unordered_set<FF*> visited;
        std::queue<std::pair<FF*, int>> q;
        for(FF* o : ffs[i]->getClusterFF()){
            if(o && visited.insert(o).second) q.push({o, 0});
        }

        while(!q.empty()){
            auto item = q.front(); q.pop();
            FF* f = item.first;
            int d = item.second;
            if(d >= hops) continue;

            // Follow single prev-stage link
            PrevStage p = f->getPrevStage();
            if(p.ff && visited.insert(p.ff).second){
                auto it = idxOf.find(p.ff);
                if(it != idxOf.end() && it->second != i){
                    int j = it->second;
                    if(slack[i] < slackThresh || slack[j] < slackThresh){
                        if(j > i) outEdges[i].insert(j);
                        else      outEdges[j].insert(i);
                    }
                }
                q.push({p.ff, d + 1});
            }

            // Follow next-stage fanout
            std::vector<NextStage> ns = f->getNextStage();
            for(const NextStage& s : ns){
                if(s.ff && visited.insert(s.ff).second){
                    auto it = idxOf.find(s.ff);
                    if(it != idxOf.end() && it->second != i){
                        int j = it->second;
                        if(slack[i] < slackThresh || slack[j] < slackThresh){
                            if(j > i) outEdges[i].insert(j);
                            else      outEdges[j].insert(i);
                        }
                    }
                    q.push({s.ff, d + 1});
                }
            }
        }
    }

    for(int i = 0; i < (int)ffs.size(); i++){
        for(int j : outEdges[i]){
            adj[i].push_back(j);
            adj[j].push_back(i);
        }
    }
    return adj;
}

std::vector<std::vector<FF*>> partitionByConflict(
    const std::vector<FF*>& ffs, int hops, double slackThresh,
    int maxK, int minSize)
{
    std::vector<std::vector<FF*>> result;
    if(ffs.empty()) return result;
    if(maxK < 1) maxK = 1;

    auto adj = buildConflictGraph(ffs, hops, slackThresh);
    {
        size_t totalEdges = 0;
        for(auto& a : adj) totalEdges += a.size();
        std::cerr << "[PARTITION] ffs=" << ffs.size()
                  << " conflictEdges=" << (totalEdges / 2)
                  << " hops=" << hops << " slackThresh=" << slackThresh << std::endl;
    }

    int n = (int)ffs.size();
    std::vector<int> color(n, -1);
    std::vector<int> degree(n);
    for(int i = 0; i < n; i++) degree[i] = (int)adj[i].size();
    std::vector<std::unordered_set<int>> satur(n);

    // DSatur: pick uncolored vertex with max saturation (break ties by degree).
    for(int step = 0; step < n; step++){
        int best = -1, bestSat = -1, bestDeg = -1;
        for(int i = 0; i < n; i++){
            if(color[i] >= 0) continue;
            int s = (int)satur[i].size();
            if(s > bestSat || (s == bestSat && degree[i] > bestDeg)){
                best = i;
                bestSat = s;
                bestDeg = degree[i];
            }
        }
        if(best < 0) break;

        int c = 0;
        while(satur[best].count(c)) c++;
        color[best] = c;

        for(int nb : adj[best]){
            if(color[nb] < 0) satur[nb].insert(c);
        }
    }

    int K = 0;
    for(int c : color) K = std::max(K, c + 1);
    std::vector<std::vector<FF*>> clusters(K);
    for(int i = 0; i < n; i++) clusters[color[i]].push_back(ffs[i]);

    // Cap K and merge clusters below minSize. Merge smallest into second-smallest.
    auto needMerge = [&](){
        if((int)clusters.size() > maxK) return true;
        if(clusters.size() > 1){
            size_t mn = clusters[0].size();
            for(auto& c : clusters) if(c.size() < mn) mn = c.size();
            if((int)mn < minSize) return true;
        }
        return false;
    };
    while(clusters.size() > 1 && needMerge()){
        std::sort(clusters.begin(), clusters.end(),
            [](const std::vector<FF*>& a, const std::vector<FF*>& b){
                return a.size() < b.size();
            });
        clusters[1].insert(clusters[1].end(),
            clusters[0].begin(), clusters[0].end());
        clusters.erase(clusters.begin());
    }

    result = std::move(clusters);
    return result;
}

} // namespace ConflictPartition
