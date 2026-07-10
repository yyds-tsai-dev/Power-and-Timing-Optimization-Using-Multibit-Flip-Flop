# DAC 2027 Phase 0 分析總報告(2026-07-11 收官)

**計畫**: `plans/active/plan_dac2027_optimization.md` | **狀態**: Phase 0 完成,Phase 1 可開工

## 1. Stage-time 解剖(官方 log)

bitRepair 佔牆鐘 87–96%(hc04: 1244s+1231s / 2582s)。tc3/hc04 兩輪各撞滿 1200s 上限、hc02 第一輪撞滿。其餘階段(rebank ≤108s、eject ≤堆疊內、density <0.2s)都不是餅。

## 2. 預算解放探測(BIT_REPAIR_TIME=3600)

| Case | 官方(1200 cap)| 3600 放開 | Δ | 收斂? |
|---|---|---|---|---|
| tc3 | 726,041,482 | 725,987,691 | −0.0074% | ✓(3080s,未撞新牆)|
| hc04 | 726,015,652 | 725,976,226 | −0.0054% | ✓(3083s)|
| hc02 | 10,089,886 | **10,089,886(一分不差)** | ±0 | ✓(1393s+976s)|

**結論**:
1. 現行配置已在架構定點附近;加時間近乎白給 → **score 躍進只能靠 T4 修復 + 新 move class(LNS)**
2. hc02 證明 bitRepair 尾段純浪費(pipeline 自癒)→ **rate-based early exit 可砍時間、零分數代價**
3. 跨機器分數差 = 軌跡分岔(accept 順序),非預算效應;單調性主張安全

## 3. 取樣式 profiling(tc1、8T、gdb 60 快照全執行緒)

| 類別 | 佔比 | 備註 |
|---|---|---|
| unordered_map<Gate/FF> 操作(hash/equal/node alloc)| ~37% | oracle 報價 scratch 是每呼叫新建的 hash map |
| malloc/new/free/consolidate | ~18% | 同上,節點配置風暴 |
| gomp barrier 空等 | ~17% | 負載不均;screening chunk 排程可改 |
| 領域邏輯(evalBitSwapDelta、bitRepairRefine 本體)| ~6% | 演算法本身便宜 |

**Phase 3 首發項(依效益/成本排序)**:
- **3a-1 scratch de-hashing**:邏輯物件發稠密 ID(不可變,天然適合)→ per-thread 預配置 epoch-stamped 平坦陣列。預期 bitRepair 2–3×。風險:map 迭代順序改變 → FP 加總順序改變 → 需 7 案分數中性驗證(允許 1e-13 級漂移,參照 hc04 既有噪音)
- **3a-0 rate-based early exit**:輪內改善率 < ε 退場。預期 capped 案再 1.5–2×
- **3b barrier 空等**:screening 迴圈 dynamic scheduling / chunk 調整(17% 上限)
- 合併預估:hc04 43min → 10–15min,全套 3h → ~1h;2-min tier 品質同步提升(單位時間做更多 exact 報價)

## 4. 文獻偵察(詳見 2026-07-08_dac2027_lit_recon.md)

- ICCAD 2025 Problem B 同題新測資(使用者親自比過:僅改 input format;移植=格式層+修舊 bug;Phase 4)
- LNS/exact-priced destroy-repair 無先例,novelty 成立;Synopsys 專利 US 11,328,109 是現成動機引言
- must-cite 新增:ICCD'25 DTCO、MLCAD'25 ML clustering;watch:DAC'26 B-Flex

## 5. 環境陷阱(執行紀錄)

- 測資分居:hc01/hc02 在 `2024-ICCAD-Problem-B/testcase/`,hc03/hc04+tc* 在根目錄 `testcase/`
- gprof + OpenMP 不出 gmon.out(-pg 已入連結仍然無效);**用 gdb 取樣法**(`gdb -p PID -batch -ex "thread apply all bt 4"` 每 3s)
- obj/ 曾被 -pg 汙染後已重編乾淨(repo binary 為正常版)

## 下一步(Phase 1 開工單)

T4 re-anchor 修復:規格在 handoff T4 + ROOTCAUSE 報告;起點 Manager.cpp:80-115 的 origDSlack_ 錨點;驗收 = oc_rep 6,063 缺口歸零 + checkpoint 殘差 <0.1%;過門後官方 3× 重跑立新基線。
