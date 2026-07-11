# Survey + Design: De-hashing the Timing Oracle (dense IDs + epoch-stamped scratch)

**Date**: 2026-07-11 | **Type**: engineering survey → implementation-ready design
**Feeds**: DAC 2027 Phase 3a-1 (`plans/active/plan_dac2027_optimization.md`), profile evidence in
`reports/v1/2026-07-11_dac_phase0_analysis.md` §3.
**Target code**: `2024-ICCAD-Problem-B_V3` (branch main), `src/Manager.cpp` + `inc/Manager.h`.

---

## 0. Problem statement (grounded in our profile + code)

gdb-sampling on tc1 @ 8T shows the bitRepair oracle stack burns:

| Bucket | ~% samples | Code source |
|---|---|---|
| `unordered_map<Gate*/FF*>` hash/equal/node-alloc | 37% | per-call scratch maps **and** lookups into 10 persistent pointer-keyed caches |
| malloc/free/consolidate | 18% | node-based `std::unordered_map/set` → one `new` per insert; plus per-call `std::vector` churn |
| GOMP barrier spin | 17% | load imbalance in `schedule(dynamic,8/16)` screening loops + serial apply sections |
| Domain logic (`evalBitSwapDelta` arithmetic) | 6% | the actual work |

Concrete code anchors (all `src/Manager.cpp`):

- **Per-call scratch** in `evalBitSwapDelta` (line 3519): `std::unordered_set<Gate*> coneSet` (3538), `std::unordered_map<Gate*,double> gateOv` (3552), `std::unordered_set<FF*> affSet` (3633), plus `std::vector` stack/cone/aff allocated fresh per candidate. `evalRemapDelta` (3655) mirrors all of this.
- **Persistent pointer-keyed caches** built once in `incrAccurateBuild` (2434): `incrTopoIdx_`, `incrFanin_`, `incrFanoutG_`, `incrSinkFF_`, `incrFFQGates_`, `incrFFDirectSinks_`, `incrFFDrivers_`, `incrGateCur_`, `incrFFArrOrig_`, `incrFFNeg_` (`inc/Manager.h` 204–213). Every fanin visit does 1–3 hash probes (`topoIdx`, `gateCur`, `gateOv.find` lambdas at 3529–3530, 3560, 3582).
- **Screening loops**: dynasearch dyna2 rescore `#pragma omp parallel for schedule(dynamic,8)` (3881), sub-batch repricing `schedule(dynamic,16)` (3960).

libstdc++ `unordered_map` is node-based (separate chaining): every insert is a malloc, every probe is a pointer chase to a heap node, and pointer keys hash via `std::hash<T*>` (identity on libstdc++) which clusters on allocator alignment. The 37% + 18% buckets are two views of the same disease. The universe of keys is **immutable and dense** (gates fixed at parse; logical FFs fixed after `debankAll`), so hashing buys nothing.

---

## 1. Prior art

### 1.1 ABC (Berkeley): `TravId` — the canonical epoch-stamped visited array in EDA

ABC stamps every `Abc_Obj_t` with a per-object `TravId` and keeps one network-wide counter `nTravIds`. A traversal calls `Abc_NtkIncrementTravId()` once, then marks with `Abc_NodeSetTravIdCurrent(p)` and tests with `Abc_NodeIsTravIdCurrent(p)` — no clearing between traversals, O(1) "reset" by counter bump. Every windowing/cone routine in ABC (e.g. `resWin.c` resubstitution windows — structurally the same job as our fanout-cone BFS) is built on this. This is exactly our `coneSet`/`affSet` replacement, proven at ABC scale (multi-million-node AIGs, called billions of times). ABC guards wraparound by periodically resetting all stored ids when the counter approaches its limit — see §2.1.

### 1.2 OpenTimer v2: incremental timing with indexed storage + task parallelism

OpenTimer v2 (Huang et al., TCAD 2021) rearchitected v1's loop-based incremental STA into a task-dependency-graph engine, achieving up to 5.33× on incremental timing. Two lessons transfer:

1. **Frontier-limited propagation over indexed structures** — its incremental repropagation touches only the affected cone in topological order, with pin/node data in contiguous storage rather than string/pointer-keyed maps. Our `evalBitSwapDelta` already does cone-limited repricing; what we're missing is the contiguous storage half.
2. **Task-based decomposition beats loop-based when work per unit is irregular** — directly relevant to our 17% barrier bucket (§3). OpenTimer's speedup came largely from *not* bulk-synchronizing between levels.

### 1.3 VPR / tatum: strong dense IDs + "modified-list" reset

VPR converted its routing-resource graph to strongly-typed dense integer IDs (`RRNodeId`, via `vtr::StrongId`) with all per-node lookaside data in flat `vector`s indexed by ID. Its router keeps `rr_node_route_inf` as a flat array and resets between net routings via an **explicit modified-node list** (only touched entries are cleaned) — the undo-list alternative to epoch stamping, O(touched) instead of O(1) reset but no epoch array at all. tatum (VPR's STA engine, FPT 2018) advertises "cache-optimized data structures" — levelized dense-ID arrays — plus parallel level-by-level analysis as its two performance pillars. Take: strong-typed IDs (`struct GateId { uint32_t v; }`) are cheap insurance against index-universe mixups (gate vs FF id), and the modified-list reset is our fallback if epoch-array memory ever matters.

### 1.4 General literature: Briggs–Torczon sparse sets & timestamped visited arrays

Briggs & Torczon, *An Efficient Representation for Sparse Sets* (ACM LOPLAS, 1993): dense/sparse twin arrays give O(1) `clear`, O(1) membership/insert, O(|S|) iteration, tolerating uninitialized memory (see also research.swtch.com "Using Uninitialized Memory for Fun and Profit"). The timestamp/epoch variant — store a generation number per slot instead of the two-array validity check — is the folklore standard in compilers (liveness bitsets), game ECS engines, and graph search (visited arrays across repeated BFS/DFS from different sources). Our case is the textbook fit: repeated small traversals (cone ≪ G) over a fixed universe, where per-traversal O(G) clearing or per-traversal allocation dominates.

**Survey verdict**: the plan (dense immutable IDs + per-thread epoch-stamped flat arrays) is exactly the pattern all four bodies of prior art converged on. No exotic alternative (concurrent hash maps, arena-allocated maps, robin-hood/`boost::unordered_flat_map`) is competitive here: flat maps still pay hash+probe per access and don't fix iteration-order nondeterminism; arenas fix only the malloc bucket. Direct indexing costs 1 load (+1 for epoch check) with hardware-prefetchable CSR neighbors.

---

## 2. Pitfalls and how to handle them

### 2.1 Epoch overflow

`uint32_t` epoch wraps after 2³²−1 evaluations *per thread scratch*. Our scale: bitRepair reprices up to ~10⁴–10⁵ candidates/round × 10³ rounds ≈ 10⁸ per run — two orders below wrap, but do not rely on it. Protocol (ABC-style):

```cpp
void ThreadScratch::newEpoch() {
    if (++epoch == 0) {                       // wrapped
        std::fill(gateEpoch.begin(), gateEpoch.end(), 0u);
        std::fill(ffEpoch.begin(),   ffEpoch.end(),   0u);
        epoch = 1;
    }
}
```

Cost: one O(G+F) fill per 4.29G evaluations — statistically free. **Never** skip the wrap check to save the branch (it predicts perfectly). Do not "fix" by using `uint64_t` epochs unless you adopt the AoS layout (§4.3) where the padding is free anyway; in SoA it doubles epoch-array footprint for nothing.

Subtle bug class: if one logical operation stamps in two phases (e.g. cone marking, then aff marking) using **the same epoch array**, one `newEpoch()` per candidate suffices only if the two membership meanings never collide. We keep them in separate arrays (`gateEpoch` vs `ffEpoch`), so one bump per candidate is safe. If you later add a second gate-universe marker in the same candidate (e.g. "cone" vs "boundary"), use `epoch` and `epoch|0x80000000`-style tags or bump twice — decide explicitly, don't improvise.

### 2.2 False sharing between threads' scratch

Two distinct risks:

1. **Scratch arrays themselves**: each thread's arrays are multi-MB separate heap allocations; interior false sharing is impossible, only the first/last cache line can abut another thread's data. Cheap insurance: `alignas(64)` the `ThreadScratch` struct and let each thread **allocate its own scratch inside the parallel region** (also gives correct NUMA first-touch, §2.3). Do *not* put scratch structs contiguously in one `std::vector<ThreadScratch>` unless the struct is padded to 64B multiples — the classic histogram-array mistake.
2. **Already-present hazard in the shared result arrays** (found during this survey): the dyna2 rescore loop writes `bDelta[i]` (`double`), `bIb/bSa/bSb[i]` (`int`) and `dirty[i]` (**`vector<char>`**, Manager.cpp 3864/3910) from concurrent threads. With `schedule(dynamic,8)`, one 64-byte line of `dirty` spans 64 iterations = up to 8 chunks owned by different threads → line ping-pong on every `dirty[i]=0` store. Same for `bSa/bSb` (16 ints/line = 2 chunks). This is part of the barrier/imbalance bucket in disguise. Fix options, cheapest first: (a) chunk size ≥ 64 — but that worsens imbalance; (b) keep `dynamic,1..8` for work distribution but write results to a per-thread local and flush once per chunk — overkill; (c) **widen `dirty` to `uint8_t` is irrelevant — instead pack per-`i` results into one 32-byte POD struct (`{double delta; int32_t ib, sa, sb; int32_t pad}`) in a single array**: 2 structs/line halves sharing vs 4 separate arrays, and one store stream instead of four. Recommended: (c) + chunk 8. Measure; if still visible, (a) with cost-sorted iteration (§3) recovers balance.

### 2.3 NUMA on 2× E5-2630 v4 (Broadwell-EP: 10C/20T per socket, 25 MB L3 each, 2 NUMA nodes, QPI)

- **Per-thread scratch**: must be first-touched by its owning thread. Allocate + `newEpoch()`-init inside `#pragma omp parallel` (not by the master thread pre-loop). With Linux default first-touch policy this lands pages on the thread's local node. Pin threads: `OMP_PROC_BIND=close OMP_PLACES=cores`, and keep thread→scratch binding stable (index by `omp_get_thread_num()`; the team size is fixed across our parallel regions).
- **≤10 threads: pin to one socket** (`numactl --cpunodebind=0 --membind=0` or OMP_PLACES) — the Phase-0 profile ran 8T, which fits one socket entirely; NUMA effects then vanish and you also share one 25 MB L3 for the read-only CSR. Given bitRepair's observed scaling, a single-socket 10T run may beat a naive 20T run; benchmark both before chasing 20T.
- **Shared read-only CSR caches** (built serially in `incrAccurateBuild` → all pages on the build thread's node): at 20T, socket-1 threads take QPI latency (~1.5–2× local) on every cone walk. Mitigations in order of effort: (i) run single-socket (above); (ii) interleave just the big CSR arrays (`numactl --interleave=all` for the whole process is a blunt but acceptable first cut — scratch stays local via first-touch regardless of the policy? **No** — `--interleave=all` overrides first-touch for *all* allocations, including scratch. Use targeted interleaving instead: allocate CSR arrays with `mbind(MPOL_INTERLEAVE)` or first-touch them from a parallel loop); (iii) full per-socket replication of the CSR — only if perf counters (`perf stat -e node-load-misses` or `numastat`) prove remote-read dominance. Expectation: cone walks are latency-bound random reads into ~100 MB of CSR; interleaving averages latency, replication eliminates it. Start with (i).

### 2.4 Determinism of float accumulation when map iteration → index order

Current state (important — better than feared):

- The per-candidate `delta` sum (3639–3644) iterates `aff` — a `std::vector` filled in **deterministic order**: cfa, cfb, direct-sink vectors (build order), then cone gates **after `std::sort` by `topoIdx`** (a total order: topo indices are unique), each gate's `incrSinkFF_` vector in build order. `unordered_set` is only used for dedup; iteration order never reaches arithmetic. So per-candidate deltas are already run-to-run deterministic *given identical inputs*, and pointer-valued hash iteration (which varies with ASLR/allocation history) never feeds an FP sum in this path.
- The only OMP reduction in the loop is `reduction(+:rescored)` on an integer — immune.
- Cross-thread FP never accumulates: each `i` writes its own `bDelta[i]`; the apply pass is serial and sorted by `(delta, ia, ib, sa, sb)` (3914–3917) — deterministic tie-breaks already in place.

Consequence for this refactor: **replacing hash containers with epoch arrays + explicit insertion-order lists can and should be arithmetic-order-identical** — dedup via epoch check, append to a plain vector in the same BFS/visit order as today, keep the topo sort. Then results are not "within 1e-13", they are **bit-identical**, and validation reduces to byte-comparing outputs. The 1e-13 tolerance (plan 3a-1) remains the fallback acceptance if some ordering does change inadvertently — but treat any non-byte-exact diff as a bug to root-cause first, because trajectory divergence amplifies through accept ordering (phase-0 §2: cross-machine score forks come from accept-order divergence, not budget).

General literature note (for the report's completeness): OpenMP does not specify reduction combination order; bitwise reproducibility requires either fixed-order manual reduction, `KMP_DETERMINISTIC_REDUCTION` (same-thread-count consistency only, LLVM/Intel runtime), or reproducible-summation algorithms (Demmel–Nguyen). We need none of these as long as we keep FP sums serial-per-candidate — preserve that invariant in any future parallelization of the apply loop.

---

## 3. OpenMP scheduling for the 17% barrier idle

Diagnosis first: barrier samples = (a) tail imbalance inside `parallel for` (cone sizes are heavy-tailed → per-`i` cost varies by 10–100×; `dirty`-skips make many iterations ~free), (b) all-threads-idle during the serial sections between regions (sort cands, serial apply, tree maintenance), (c) spin-wait policy making idle visible as CPU samples (libgomp default `OMP_WAIT_POLICY` spins ~300ms before sleeping — the 17% is partly *accounting*, not all recoverable work).

Fixes, ranked by (impact ÷ risk):

1. **`schedule(dynamic,1)` on both oracle loops** (3881, 3960). Per-iteration work is ~10⁴–10⁶ ns (K-nearest query + up to nA·nB oracle calls), so per-chunk dispatch overhead (~100 ns lock-free atomic fetch-add in libgomp) is noise even at chunk 1. Chunk 8 exists today only to amortize dispatch; after de-hashing, iterations get faster, but still ≫ dispatch cost. Combine with the §2.2 result-struct packing so chunk-1 doesn't ping-pong result lines (one 32 B struct = half a line; two threads can still share — acceptable, stores are once-per-i, not per-inner-iteration). If measurable, use `nonmonotonic:dynamic` explicitly (OpenMP ≥4.5; it is the default meaning of `dynamic` from OpenMP 5.0) to permit work-stealing-style reordering.
2. **Compact-before-parallel**: both loops iterate the full range and `continue` on `!dirty[i]` / `!alive[ci]` / `usedMB`. Build a compacted index vector of live work items serially (O(N) memcpy-cheap), then `parallel for` over it. This makes chunk sizes meaningful again, removes skip-iteration jitter from the scheduler's cost model, and shrinks the dispatch count. Serial compaction cost is trivial vs. the imbalance it removes.
3. **Cost-ordered LPT dispatch**: sort the compacted work list by descending estimated cost (estimate = bits(A)·(bits(A)+K·avg bits)·last-seen cone size for that MBFF, or simply cached cone size from the previous rescore, stored per MBFF). Longest-Processing-Time-first + `dynamic,1` is the classic near-optimal makespan recipe for irregular loops. Costs ~one extra sort per round; do it only if (1)+(2) leave >5% barrier residue.
4. **`guided` is the wrong tool here**: guided front-loads large chunks; with heavy-tailed *unsorted* cost, a big early chunk can trap one thread with several giant cones. Prefer dynamic-with-small-chunks or LPT+dynamic. (Literature: "OpenMP Loop Scheduling Revisited" (Ciorba et al.) — no single schedule wins on irregular loads; dynamic,1 is the robust default when iteration cost ≫ dispatch cost.)
5. **Taskloop / task-based restructuring** (`#pragma omp taskloop grainsize(1)` or explicit tasks with dependency-driven apply): would also let the serial apply overlap screening (OpenTimer v2's core lesson). This is a Phase-2 architecture change (V2-worktree material), not part of 3a-1 — the serial apply must stay serial for determinism unless redesigned with the same care as batchMode 2's aff-disjoint logic.
6. **`OMP_WAIT_POLICY=passive`** while re-profiling, to separate real imbalance from spin accounting. For production keep default (active spin) — passive adds wakeup latency per region and we enter regions thousands of times.

Expected recovery: (1)+(2) should convert most of the 17% tail-imbalance share; the serial-section share only shrinks via (5) or by making serial sections faster (de-hashing helps: apply-path `evalBitSwapDelta` calls speed up 2–3× too).

---

## 4. Recommended design

### 4.1 ID assignment (one-time, at debank/build)

Universes are immutable at oracle time:

- **`GateId` = topological index.** In `incrAccurateBuild` (2434), the Kahn loop (2509–2514) already assigns `incrTopoIdx_[g] = k`. Make this the gate's dense id: add `uint32_t gid_ = UINT32_MAX;` to `Gate` (`inc/Gate.h`), set `g->setGid(k)` where `incrTopoIdx_` is filled today. Gates that never pop from the Kahn queue (event-deadlocked; they exist in `Gate_Map`) get ids `k = topoCount .. G-1` in a deterministic second pass (iterate `incrTopo_`-absent gates in **name-sorted or parse order — not `Gate_Map` unordered order** — to keep ids reproducible run-to-run). Bonus: `gid` order = topo order = cone-walk visit order → CSR reads become near-sequential, prefetcher-friendly. `topoIdx(g)` becomes `g->gid()` — the map disappears entirely.
- **`FFId` (logical/cluster FF) assigned at debank completion.** Logical FFs are created in `debankFF` (572) / `debankAll` (614) and are stable thereafter (later `debankFF` calls during declustering re-wrap the *same* logical constituents in new physical FFs — logical identity, and thus the id, survives; this is exactly why the incr caches are keyed by logical FF, Manager.h 204 comment). Add `uint32_t lid_` to `FF`, assign in `incrAccurateBuild`'s `innerFF` pass (2444–2445) **in a deterministic order** (`FF_Map` is `unordered_map<string,FF*>` — sort names once, or iterate the physical-FF creation-order vector if one exists). Also build `std::vector<FF*> lidToFF_` and `std::vector<Gate*> gidToGate_` reverse maps. Guard: `assert(cf->lid()!=UINT32_MAX)` in debug on every oracle entry — any FF created after build without an id is a logic error (must not happen while incr engine is live; `incrBuilt_` already enforces build-once).

### 4.2 Persistent caches → SoA / CSR (indexed by gid / lid)

Replace Manager.h 204–213 with:

```cpp
// gate-side, indexed by gid (size G)
std::vector<uint32_t> faninOff_;        // CSR offsets, size G+1
std::vector<IncrFaninF> faninArr_;      // flattened; see below
std::vector<uint32_t> fanoutOff_, fanoutArr_;     // gate -> gate gids
std::vector<uint32_t> sinkFFOff_, sinkFFArr_;     // gate -> sink lids
std::vector<double>   gateCur_;         // committed arrival, size G
// FF-side, indexed by lid (size F)
std::vector<uint32_t> ffQGateOff_, ffQGateArr_;   // lid -> driven gids
std::vector<uint32_t> ffDirSinkOff_, ffDirSinkArr_;
std::vector<uint32_t> ffDrvOff_;  std::vector<FFDriver> ffDrvArr_; // {uint32_t gid; Coor gateOut;}
std::vector<double>   ffArrOrig_;       // kEvalNegInf-init
std::vector<double>   ffNeg_;           // 0-init

struct IncrFaninF {                     // 32 B, was {int,Gate*,FF*,double,Coor}
    uint32_t kind;                      // 0=IO-const 1=FF.Q 2=gate
    uint32_t idx;                       // gid (kind2) or lid (kind1); unused kind0
    double   cnst;                      // precomputed delay contribution
    Coor     pin;                       // sink-side pin coor (2 doubles → 32 B total w/ packing; measure)
};
```

Build path: keep `incrAccurateBuild`'s current two-phase structure — phase 1 counts degrees per gid/lid (the existing loops, writing counts instead of `push_back`), prefix-sum into `*Off_`, phase 2 fills `*Arr_`. This also removes the build-time rehash/malloc storm. The "presence" semantics of `find()==end()` map to sentinel values: `gateCur_` init to `g_evalAnchor ? kEvalNegInf : 0.0` (matches the `gateCur` lambda default, 3530), `ffArrOrig_` to `kEvalNegInf`, `ffNeg_` to 0.0 — audit each of the 10 lambdas' miss-defaults one by one; they are not uniform (e.g. 3529 defaults topoIdx to 0, 3587 defaults orig to `kEvalNegInf`).

`origDSlack_` (Manager.h 71), `netPinCache_`, `origNetHPWL_` are outside the oracle hot loop — leave them; convert later only if a fresh profile says so.

### 4.3 Per-thread scratch: epoch-stamped flat arrays

```cpp
struct alignas(64) OracleScratch {
    // gate universe
    std::vector<double>   gateOv;      // override arrival; valid iff gateEpoch[g]==epoch
    std::vector<uint32_t> gateEpoch;   // 0-init
    std::vector<uint32_t> coneList;    // gids in BFS insertion order (reused; clear() keeps capacity)
    // FF universe
    std::vector<uint32_t> ffEpoch;
    std::vector<FF*>      affList;     // insertion order == today's aff order
    uint32_t epoch = 0;
    void init(size_t G, size_t F) { gateOv.resize(G); gateEpoch.assign(G,0); ffEpoch.assign(F,0); }
    void newEpoch() { if (++epoch == 0) { std::fill(gateEpoch.begin(),gateEpoch.end(),0u);
                                          std::fill(ffEpoch.begin(),ffEpoch.end(),0u); epoch = 1; } }
    inline bool  gateSeen(uint32_t g) const { return gateEpoch[g] == epoch; }
    inline void  gateMark(uint32_t g)       { gateEpoch[g] = epoch; }
    inline bool  ffSeen(uint32_t f)  const { return ffEpoch[f] == epoch; }
    inline void  ffMark(uint32_t f)        { ffEpoch[f] = epoch; }
};
```

Layout choice — **SoA (separate `gateOv` / `gateEpoch`), as planned**, because the two dominant access patterns split cleanly: (a) membership-test-only on gates *outside* the cone (`gateCur` fallback path, 3560/3582/3600 — the common case) touches only the 4-byte epoch array → 16 epochs per cache line, and the epoch array for G=1M is 4 MB — a large fraction stays L2/L3-resident across candidates; (b) value reads for in-cone gates touch `gateOv`, whose footprint per candidate is only the cone. AoS (`{double val; uint32_t ep; uint32_t pad}` 16 B) would save one potential miss on the in-cone hit path but halve epoch-line density and double the epoch-scan footprint — for our cone ≪ G ratio, SoA wins. (If a later profile shows in-cone double-miss dominating, flipping to AoS is a 20-line change.)

Ownership & lifetime: `std::vector<std::unique_ptr<OracleScratch>> scratchPool_` sized `omp_get_max_threads()`, **lazily constructed by each thread on first use inside a parallel region** (correct NUMA first-touch, §2.3), indexed by `omp_get_thread_num()`. `evalBitSwapDelta`/`evalRemapDelta` fetch their scratch at entry; serial callers (apply loop, 3923/3941) use thread 0's scratch — same code path, no branching. The existing `coneOut`/`affOut` out-params keep their signatures: copy from `coneList`(mapped via `gidToGate_`)/`affList` — or better, change callers to consume `const std::vector<...>&` views into scratch (they already `std::move` today, 3645–3646; a view is safe because each caller finishes consuming before its next oracle call — verify the batchMode 0/1 loops don't hold `aff` across a second `evalBitSwapDelta` call... **they do** (3923 then loop-continue uses `aff` at 3925–3933). So: keep the copy-out for `affOut != nullptr` callers; the screening path passes nullptr and pays nothing).

### 4.4 Rewritten kernel skeleton (`evalBitSwapDelta`)

```cpp
OracleScratch& S = scratch();          // per-thread
S.newEpoch(); S.coneList.clear(); S.affList.clear();
// cone BFS — same visit order as today
for (FF* cf : {cfa, cfb})
    for (uint32_t g : ffQGates(cf->lid()))                 // CSR span
        if (!S.gateSeen(g)) { S.gateMark(g); S.coneList.push_back(g); }
for (size_t i = 0; i < S.coneList.size(); ++i)
    for (uint32_t n : fanoutG(S.coneList[i]))
        if (!S.gateSeen(n)) { S.gateMark(n); S.coneList.push_back(n); }
std::sort(S.coneList.begin(), S.coneList.end());           // gid == topo order: plain uint32 sort
for (uint32_t g : S.coneList) {                            // arrival recompute
    double mc = -1e300;
    for (const IncrFaninF& f : fanin(g)) {
        double vc = (f.kind==0) ? f.cnst
                  : (f.kind==1) ? qArrival(f.idx /*lid*/, f.pin)          // as today
                  : (S.gateSeen(f.idx) ? S.gateOv[f.idx] : gateCur_[f.idx]) + f.cnst;
        if (vc > mc) mc = vc;
    }
    S.gateOv[g] = (mc==-1e300 && !g_evalAnchor) ? 0 : mc;   // epoch already stamped
}
```

Notes: (i) `std::sort` on raw `uint32_t` gids replaces today's comparator-through-hash-map sort (3549) — both faster and provably the same order (gid *is* topoIdx); consider in-BFS ordered insertion later, not now — keep transformations order-preserving for the byte-exact goal. (ii) One subtlety: today `coneSet.insert` marks membership and `gateOv.find` separately marks "recomputed"; in the new code `gateSeen` conflates them. That is safe **because** the arrival loop runs in topo order over the full cone before any consumer reads `gateOv` (consumers are the fanin loop itself — reads only lower-topo gids, already written — and `slackOv` — runs after the loop). Assert in debug: in the fanin loop, `f.kind==2 && S.gateSeen(f.idx)` implies `f.idx < g` in gid order. (iii) The aff-collection and delta loop transliterate the same way with `ffSeen/ffMark` + `affList`; the delta sum iterates `affList` in the identical order as today's `aff` → bit-identical accumulation.

### 4.5 Memory budget

Let G = gates, F = logical FFs (measure at build; large cases ~10⁵–10⁶ each).
Per-thread scratch: 8G (gateOv) + 4G (gateEpoch) + 4F (ffEpoch) + lists ≈ **12G + 4F bytes ≈ 16 MB @ G=F=1M**. × 20 threads = 320 MB — fine for this machine; × 10 threads (recommended single-socket) = 160 MB. Shared CSR replaces the current node-based maps and will *shrink* resident memory substantially (each unordered_map node today carries ≥32 B overhead + malloc metadata per entry). If a future case explodes G, fallback: drop `gateEpoch` for a Briggs–Torczon/VPR-style modified-list (reset only `coneList` entries; O(cone) reset, zero epoch memory) — semantics identical, keep behind the same interface.

### 4.6 Rollout (V1 safety rules compliant)

1. Land ID assignment + CSR build alongside the old maps (both populated; asserts compare lookups under `ORACLE_FLAT_CHECK=1` on tc1 for one round).
2. `ORACLE_FLAT=1` switches `evalBitSwapDelta`/`evalRemapDelta`/`incrFFSlack`/`incrAccurateRecomputeFF` to the flat path. Default off ⇒ byte-exact by construction (old code untouched).
3. After §5 validation passes, flip default on, delete the map path in a follow-up commit (V1 has no code-beauty budget, but dead dual paths are a determinism hazard).

Order of conversion within the gate: do the **persistent caches first** (pure lookups, zero ordering risk), measure; then the per-call scratch (ordering-sensitive part). Two commits, two measurements — if the win is lopsided we learn where the 37% actually lived.

---

## 5. Score-neutrality validation plan

Because §2.4 shows every FP accumulation already runs in deterministic order and the refactor is order-preserving, the primary acceptance bar is **byte-exact**, with 1e-13 as documented fallback:

1. **Paired-oracle unit check** (pre-flip): `ORACLE_FLAT_CHECK=1` runs both engines per candidate on identical committed state, asserts `fabs(dOld - dNew) == 0.0` (exact, not epsilon — same operations, same order) and identical `aff` sequences. Run one full bitRepair round on tc1 + hc02 (hc02 stresses the g_evalAnchor path). Any mismatch is a transliteration bug — fix, don't tolerate. The existing one-shot apply/revert self-check (3900/4017 comment) stays as second witness.
2. **7-case end-to-end** (`BANKING_MODE=matching PRODUCTION=1`, official time budgets): `cmp` old-vs-new `.out` files. Expected: identical. If identical → ship. Outputs to `/tmp/v1_${case}_dehash.{out,log,evaluator}`.
3. **Fallback gate (only if 2 diverges)**: locate the first diverging accepted move via the checkpoint/oc_rep machinery; if the per-delta difference at that point is ≤1e-13·|delta| (true FP-drift, e.g. from an unavoidable ordering change), accept trajectory divergence and gate on: per-case **evaluator-binary** score (never `getEvaluatorCost()`, per `feedback_verify_legality`) within the established cross-machine noise band (phase-0 §2 note 3), and no case regresses vs. V1 HEAD beyond that band; plus `checker/sanity` + `checker/placement_checker` green on all 7.
4. **Determinism regression**: run tc1 3× same-machine same-thread-count → identical `.out`; then 8T vs 10T → identical (nothing in the design accumulates FP across threads; if this fails, a hidden cross-thread order dependence was introduced — bug).
5. **Perf acceptance**: bitRepair stage wall time on tc1 / tc3 / hc04 from the official logs; target ≥2× stage speedup (plan 3a-1 predicts 2–3× from the 55% hash+malloc bucket alone; Amdahl: eliminating 55% ⇒ 2.2×, plus L3-friendlier CSR). Re-profile with the gdb-sampling recipe (phase-0 §5) to confirm the barrier bucket for the follow-up 3b scheduling pass (§3 items 1–2 can ship with this change; 3+ separately).
6. On time-capped cases (tc3/hc04/hc02), a 2× faster oracle means **more rounds within cap → scores will legitimately improve, breaking byte-exactness at the run level**. Therefore run step 2 with `BIT_REPAIR_TIME` effectively uncapped *and* fixed `maxRounds` (equal-work comparison) for the neutrality check, then separately run official budgets to measure the actual score gain — report both numbers in the exp report.

---

## 6. Sources

- [OpenTimer v2: A New Parallel Incremental Timing Analysis Engine (TCAD 2021, PDF)](https://tsung-wei-huang.github.io/papers/tcad21-ot2.pdf) · [Semantic Scholar entry](https://www.semanticscholar.org/paper/OpenTimer-v2:-A-Parallel-Incremental-Timing-Engine-Huang-Lin/31b242f709ae35dd4cade135665b8b87038ba80b) · [Taskflow STA case study](https://taskflow.github.io/taskflow/opentimer.html)
- ABC traversal-ID machinery: [berkeley-abc core architecture (DeepWiki)](https://deepwiki.com/berkeley-abc/abc/2-core-architecture) · [resWin.c windowing example](http://eddiehung.github.io/dox-abc/d1/db0/resWin_8c_source.html) · [abc.h TravId API](https://people.ece.ubc.ca/eddieh/abc_dox/dd/dad/abc_8h.html)
- [Tatum: Parallel Timing Analysis for Faster Design Cycles (FPT 2018, PDF)](https://www.eecg.utoronto.ca/~kmurray/tatum/fpt2018_tatum.pdf) · [tatum GitHub](https://github.com/verilog-to-routing/tatum) · [VPR timing docs](https://docs.verilogtorouting.org/en/latest/tutorials/timing_analysis/)
- [Briggs & Torczon, An Efficient Representation for Sparse Sets, LOPLAS 1993](https://dl.acm.org/doi/10.1145/176454.176484) · [research.swtch.com: Using Uninitialized Memory for Fun and Profit](https://research.swtch.com/sparse)
- Scheduling: [Intel VTune cookbook: OpenMP Imbalance and Scheduling Overhead](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2023-2/openmp-imbalance-and-scheduling-overhead.html) · [OpenMP Loop Scheduling Revisited (arXiv 1809.03188)](https://arxiv.org/pdf/1809.03188) · [Worksharing Tasks (arXiv 2004.03258)](https://arxiv.org/pdf/2004.03258) · [nonmonotonic:dynamic discussion](https://fortran-lang.discourse.group/t/how-does-work-openmp-nonmonotonic-dynamic-schedule/6506)
- False sharing / NUMA: [alic.dev: Measuring the impact of false sharing](https://alic.dev/blog/false-sharing) · [Oracle OpenMP guide: False Sharing](https://docs.oracle.com/cd/E19205-01/819-5270/6n7c71veg/index.html) · [NUMA locality guide (Towards Dev)](https://medium.com/@sagar.necindia/cpp-numa-performance-memory-locality-guide-430c6e6d2664)
- FP reproducibility: [Impacts of floating-point non-associativity on reproducibility (arXiv 2408.05148)](https://arxiv.org/pdf/2408.05148) · [Numerical reproducibility for parallel reduction (ScienceDirect)](https://www.sciencedirect.com/science/article/abs/pii/S0167819115001155) · [Jim Cownie: Serial and Parallel Sum Reductions](https://cpufun.substack.com/p/fun-with-serial-and-parallel-sum)
