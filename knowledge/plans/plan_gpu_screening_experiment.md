# GPU 篩選加速實驗 — 執行手冊(2026-07-13,server B 備妥,待跑)

**Type**: implementation-ready experiment(代碼已寫完編過,**尚未執行**)
**優先序(使用者 2026-07-13 指示)**:① GPU 實驗 → ② T128 探測 → ③ rep2 補完
(rep2 若逐位同 rep1 則 **不跑 rep3**)。

---

## 0. 假說與動機

bitRepair 佔牆鐘 87–96%;其中**篩選段** = 數千個候選對「凍結狀態」做獨立
cone 定價(read-only、無交互、批次天然)——教科書級資料平行。
架構本來就把「平行篩 / 序列 commit」分離,所以 **GPU 篩、CPU commit** 是
零語義風險的對映。Profiling(Phase 0)另證:hash+malloc 佔 55% 取樣,
稠密化本身就是大餅(de-hashing 是 A 的 v4 項;本實驗用一次性快照繞過它)。

## 1. 已備好的東西

| 物件 | 位置 |
|---|---|
| 原型分支(獨立 worktree,**不碰主線**) | `gpu_screening_proto` @ `~/scratch/gpu_proto`(基於 d182ba1) |
| 快照 dump 鉤子 | 該 worktree 的 `src/Manager.cpp`:`gpuDumpScreenBench()` + `bitRepairRefine` 內 `GPU_DUMP` 觸發點 |
| 原型 binary(已建) | `~/scratch/gpu_proto/cadb_0015_final` |
| CUDA benchmark(已用 nvcc sm_89 編過) | `~/scratch/gpu_proto/tools/gpu_screen_bench.cu` → `./gpu_screen_bench` |

**dump 格式**:`<prefix>.snap`(稠密快照:gate id = topo rank,CSR fanin/fanout/sink;
bit 陣列含 slack/arrOrig/neg/Q/D/Qpd + Q-gates/direct-sinks/drivers CSR + prev fallback)
與 `<prefix>.cand`(候選批 + **CPU 真值**,由 `evalBitSwapDelta` 算,64T 計時一併記錄)。

**GPU 端語義** = `evalBitSwapDelta` 的 `EVAL_ANCHOR` 分支逐行對映:
cone BFS(從兩 bit 的 Q-gates,經 gate fanout)→ topo 序重算 cone arrival
→ drivers-max 求 slack(含 unreachable drop)→ ΔTNS = Σ(newNeg − cachedNeg)。
`priceSwap<T>` 是 host/device 共用模板(FP64 驗證正確性;FP32 改
`kScreen<float>` 即可)。

## 2. 執行步驟

```bash
cd ~/scratch/gpu_proto
cp cadb_0015_final cadb_gpudump          # 長跑紀律:binary 先 cp
nvidia-smi                                # L4 是共用的,先看有沒有被佔滿

# (a) dump:跑 tc2 到 bitRepair 的 oracle build 後即 dump 並 exit(~3-5 分)
env OMP_NUM_THREADS=64 BANKING_MODE=matching PRODUCTION=1 INCR_RELOC=1 RELOC=1 \
    CRIT_SWAP=1 BIT_REPAIR=1 BIT_REPAIR_DYNA=2 BIT_REPAIR_RESCAN=4 \
    BIT_REPAIR_BATCH=1 REFINE_ALLFF=1 BIT_REPAIR_INTRA=1 ALT_ROUNDS=2 \
    BIT_REPAIR_TIME=1200 ORACLE_REBANK=1 REBANK_TIME=600 DENSITY_REPAIR=1 \
    ORACLE_EJECT=1 EJECT_TIME=180 EVAL_ANCHOR=1 \
    GPU_DUMP=$PWD/tc2gpu \
    ./cadb_gpudump ../../2024-ICCAD-Problem-B/testcase/testcase2_0812.txt /dev/null \
    > gpudump.log 2>&1
grep GPUDUMP gpudump.log     # 期望:gates=132293 bits=21164 cands=~數萬 cpuScreen64T=<秒>

# (b) benchmark:CPU-flat 1T / CPU-flat 64T / GPU FP64 三方 + 逐候選誤差
./gpu_screen_bench $PWD/tc2gpu | tee gpubench_result.txt
```

若要重編 benchmark:
`/usr/local/cuda-12.2/bin/nvcc -O3 -std=c++17 -arch=sm_89 -Xcompiler -fopenmp tools/gpu_screen_bench.cu -o gpu_screen_bench`

## 3. 判讀標準(**先讀這節再看數字,免得誤判**)

| 輸出 | 意義 |
|---|---|
| `CPU flat 1T ... maxErr=` | **正確性關卡**:平坦化是否忠實。maxErr 應 ≈1e-9 以下(與 dump 的 `evalBitSwapDelta` 真值比)。若大於此 → 平坦化漏了語義分支,先修這個,其餘數字都不算數 |
| `spill=` | cone/aff 超過 CONE_CAP(2048)/AFF_CAP(4096)的候選數。應為 0 或極小;若多 → 調大 cap 或改動態配置 |
| `CPU flat 64T` | **公平 CPU 基準**(同樣平坦資料結構)。與 dump 的 `cpuScreen64T`(原始 hash 版)相比即 **de-hashing 的單獨收益** |
| `GPU FP64 ... [vs CPU64T: N×]` | GPU 收益。**L4 的 FP64 是 1/64 速率(Ada 消費級架構)——FP64 版輸給 64T CPU 完全正常,不是失敗** |

**生產路線(第二步,benchmark 跑通後)**:**FP32 篩選 + CPU exact 重報價**。
架構天然容錯:apply 時本來就用 `evalBitSwapDelta` 重報價(呼叫面契約的
screening→commit 樣板),所以 FP32 只需「排序候選、找出有希望的」,精度不影響
正確性,只影響篩選命中率。FP32 在 L4 上是全速(~30 TFLOPS)——**這才是真正的
加速槓桿**。模板已泛型,改 `kScreen<float>` + 對應 host 端 ArrT 即可。

## 4. 預期與風險

- **樂觀**:FP32 kernel + 大批次 → 篩選段 5–10×;bitRepair 整體 2–3×(篩選佔其大宗)。
- **風險 1**:cone 大小不齊(10–500 gates)→ warp divergence。緩解:按 cone 大小
  分桶排序候選再送(同 warp 內長度接近)。
- **風險 2**:L4 是**共用**的(現在常被別人佔滿),benchmark 數字要記 GPU 佔用率。
- **風險 3**:PCIe 傳輸——本設計已把快照**一次上傳、多批次重用**(候選才是每批傳的),
  傳輸不是瓶頸。
- **工程量**:FP32 版 + 分桶 ~半天;真正接回 bitRepair(GPU 篩 + CPU commit 迴圈)
  再一天。屬 V2/DAC follow-up 範疇(CLAUDE.md:GPU 實驗 = V2),**不擋 v3/v4**。

## 5. Novelty 定位(若成立)

「GPU-accelerated **exact-oracle** refinement」查無先例:DREAMPlace 那族是
解析式全域佈局的 GPU 化(連續、梯度),我們是**離散精確定價**的 GPU 化
(cone-STA 批次)。DAC 稿可作 runtime 章的 follow-up 亮點,或獨立 short paper。
