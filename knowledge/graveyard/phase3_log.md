# Phase 3 Implementation Log

## Step 1 — Phase 3B: Global rtree + `bgi::satisfies` (2026-04-13)

### Change
`src/Banking.cpp::doClustering()`：每個 bitLib 只 build **一棵 global rtree** 涵蓋所有 clk domain，clkIDX loop 內用 `bgi::satisfies` 謂詞過濾。`FFs` 與 `isClustered` 改為 per-bitLib 全域。

### Benchmark（serial run，baseline = Phase 1.5 提交的版本）

| Testcase | Baseline score | 3B score | Δ score | Baseline banking | 3B banking | Δ banking |
|---|---:|---:|---:|---:|---:|---:|
| testcase1_0812 | 743,005,833.39 | 742,595,178.48 | **-0.055%** ✅ | 6 701 ms | 6 425 ms | -4.1% |
| testcase2_0812 | 830,272.50 | **833,096.78** | **+0.340%** ❌ | 18 144 ms | 17 433 ms | -3.9% |
| testcase3 | 728,870,766.19 | 729,266,476.34 | +0.054% ✅ | 5 590 ms | 5 470 ms | -2.1% |
| testcase1_MBFF | 752,479,215.02 | 752,274,944.00 | **-0.027%** ✅ | 6 937 ms | 6 598 ms | -4.9% |
| testcase2_MBFF | 865,419.19 | 866,124.35 | +0.081% ✅ | 18 069 ms | 17 374 ms | -3.8% |

Placement checker：**全 pass**（site / overlap）。

### 觀察

- Banking 時間 **-2~5%**，遠低於設計文件預估的 15-25%。說明 rtree build 不是 banking 的主瓶頸；real hotspot 是 `FindPlace` / `CostCompare` / nearest query 本身。
- **testcase2_0812 超 gate**（+0.34% > 0.1%），其他四個 testcase 內噪 ±0.08%（含 2 個變好）。
- 推測原因：global rtree 與 per-clk rtree 在「距離相同的候選」上 tie-breaking 不同，由於 boost::geometry rtree 結構不同而給出不同 result set → `chooseCandidateFF` 選到不同 FF 配對。不是 bug，是結構性差異。

### 決策

**建議 revert 3B**，理由：
1. Banking 時間收益有限（-3%ish），且 run3 超 score gate。
2. 3B 對 3A（per-clk 平行化）**不是前置條件**——3A 讓每 thread 建自己的 per-clkIDX rtree（和原版行為一致、不改 tie-break），可單獨跑且保 score。
3. 把 Banking parallelization ROI 留給 3A（預期 3-4× 銀行時間降幅）。

> 等 user 核可後：revert Banking.cpp 的 3B 改動，進 Step 2 Phase 3A。

## Step 2 — Phase 3A: Per-clkIDX parallelism (2026-04-13)

### Design evolution

1. **3A v1**：per-clkIDX 平行 + merge 階段 canonical legalizer 重跑 FindPlace。
   → banking **+20%**（merge 等於重跑 serial 版）。
2. **3A v2**：引入 `UpdateRowsFootprint` / `canPlaceFootprint` fast-path，衝突時 fallback FindPlace。
   → banking **+8%**（仍比 baseline 慢；merge 3966ms + 雙倍 init 2225ms/bitLib 吃光平行收益，fallback rate 55%）。
3. **3A v3 (option a, 採用)**：**把 thread-0 的 thread-local legalizer 升格成 canonical**。省掉一次 `initial()`（~1s/bitLib）；thread-0 的 footprint stub 在 merge 階段 in-place 升級為 real FF（無須重新 slice rows），其他 thread 的 pending 走原本 `canPlaceFootprint` fast-path / fallback FindPlace 流程。

### Change
- `inc/Legalizer.h` + `src/Legalizer.cpp`：
  - `UpdateRowsFootprint` 改為回傳 stub 的 index（merge 階段要用）；slice 時順便填 `PlaceRowIdx` for DP
  - 新增 `PromoteFootprintToFF(stubIdx, newFF)`：把 stub Node 就地補上 name / TNS / FFPtr（不重 slice）
- `src/Banking.cpp::doClustering()`：
  - `PendingBank` 加上 `int tid` 和 `size_t stubIdx`
  - `perClkPending` → `perThreadPending(nthreads)`；parallel region 內以 tid 寫入自己的桶（天然無鎖）
  - Merge 階段：`mgr.legalizer = tlegalizers[0].release();` 當 canonical
    - thread-0 pending：直接 `PromoteFootprintToFF`（100% fast path）
    - thread ≥1 pending：`canPlaceFootprint` → fast path；否則 `FindPlace` fallback

### Benchmark（baseline = Phase 1.5）

| Testcase | Baseline score | 3A-(a) score | Δ score | Baseline banking | 3A-(a) banking | Δ banking |
|---|---:|---:|---:|---:|---:|---:|
| testcase1_0812 | 743,005,833.39 | 742,623,859.31 | **-0.051%** ✅ | 6 701 ms | 5 482 ms | **-18.2%** |
| testcase2_0812 | 830,272.50 | 830,272.50 | 0.000% ✅ | 18 144 ms | 17 924 ms | -1.2% |
| testcase3 | 728,870,766.19 | 728,870,766.19 | 0.000% ✅ | 5 590 ms | 5 616 ms | +0.5% |
| testcase1_MBFF | 752,479,215.02 | 751,914,594.58 | **-0.075%** ✅ | 6 937 ms | 5 870 ms | **-15.4%** |
| testcase2_MBFF | 865,419.19 | 865,419.19 | 0.000% ✅ | 18 069 ms | 18 015 ms | -0.3% |
| sampleCase | 593.36 | 593.36 | 0.000% ✅ | 0.02 ms | 0.01 ms | — |

**Placement checker**：6 個全 pass（site / overlap）。

### `[BAN_PROFILE]` 摘錄（per bitLib 合計 ms）

| Testcase | init | par | canonical | merge | pending | fast% | fallbackOK | dropped |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| testcase1_0812 | 1179 | 866 | **253** | 3121 | 14 403 | 51% | 6 360 | 701 |
| testcase2_0812 | 2669 | 15 127 | 0 | 115 | 12 622 | 100% | 0 | 0 |
| testcase3 | 964 | 4 488 | 0 | 154 | 14 833 | 100% | 0 | 0 |
| testcase1_MBFF | 1249 | 892 | **256** | 3321 | 14 398 | 48% | 6 697 | 727 |
| testcase2_MBFF | 2705 | 15 184 | 0 | 114 | 12 520 | 100% | 0 | 0 |

對照：**3A v2 的 canonical=999ms**（單獨 `initial()`），v3 變成 **253ms**（僅 pointer swap + vector reset）。

### 觀察

- **clkCount=1 的 testcase（2_0812 / 3 / 2_MBFF）拿不到平行收益**：`nthreads = min(MAX_THREADS, clkCount) = 1`，退化成 serial + 一次 footprint 升級，merge 幾乎零成本（100% fast-path）。
- **多 clk domain testcase（1_0812 / 1_MBFF）拿到 -15~-18% banking**：par phase ~870ms、canonical ~250ms、merge 3.1s（約一半 fallback，另一半 fast-path）。
- 所有 score 不是持平就是進步（最多 -0.075%），全部在 gate 內（<0.1%）。

### 決策

**收進 main**。Step 2 完成。下一步 Step 3 Phase 3D：重啟 `mgr.meanshift()`（P10 weighted k-means）做 pre-banking 重心。
