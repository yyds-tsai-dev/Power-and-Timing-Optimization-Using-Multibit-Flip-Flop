#ifndef _CONFLICT_PARTITION_H_
#define _CONFLICT_PARTITION_H_

#include <vector>

class FF;

// Phase 3Z Step 4: conflict-graph partitioning for batched matching.
// Two FFs are in conflict if they share a prev/next-stage path within `hops`
// AND at least one of them has D-pin slack < slackThresh (i.e. banking one
// materially perturbs the other's timing).
// partitionByConflict produces batches; DSatur colors the graph then merges
// tiny / excess batches to satisfy maxK and minSize.
namespace ConflictPartition {

std::vector<std::vector<int>> buildConflictGraph(
    const std::vector<FF*>& ffs, int hops, double slackThresh);

std::vector<std::vector<FF*>> partitionByConflict(
    const std::vector<FF*>& ffs, int hops, double slackThresh,
    int maxK, int minSize);

} // namespace ConflictPartition

#endif
