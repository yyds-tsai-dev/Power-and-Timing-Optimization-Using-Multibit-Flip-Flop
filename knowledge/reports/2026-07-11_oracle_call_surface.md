# Oracle 呼叫面契約(P1b 後,給翻案階段切換定價用)(2026-07-11,server A)

**回覆 B 的工作單請求。** 本檔是把任一階段的定價切到 evaluator-exact oracle 的 API 契約。
全部入口在 `src/Manager.cpp`;語義 = 黑盒解碼 + 反組譯確證版(EVAL_ANCHOR=1 時)。

## 入口(全部要求 `incrAccurateBuild()` 已跑、caches 有效)

| 入口 | 用途 | 副作用 | 執行緒安全 |
|---|---|---|---|
| `evalBitSwapDelta(physA, ia, physB, ib)` | 兩 bit 交換(A==B 安全 → intra-slot)的 αΔTNS | 無(thread-local 草稿)| ✅ 可平行,凍結態下任意併發 |
| `evalRemapDelta(bits, nD, nQ, nQpd)` | 任意 per-bit (D,Q,Qpd) 覆寫:merge/split/resynthesis 通用 | 無 | ✅ 同上 |
| `evalGroupMoveDelta(...)` | remap 的 group-move 便利包裝 | 無 | ✅ |
| `incrFFSlack(cf)` | 單一邏輯 bit 的**絕對** slack(evaluator 語義)| 無 | ✅ 讀取 |
| `incrAccurateRecomputeFF(phys)` | **commit 後**把受影響 cone 摺回 committed caches | 寫 incrGateCur_/incrFFNeg_/incrTNS_ | ❌ 序列;每個 accept 後必呼(A、B 各一次)|
| `computeAccurateTNS()` | 全量 ground truth(驗證用)| 無 | 單執行緒呼叫 |
| `oracleCostSnapshot()` | 四項完整成本快照 | 無 | 序列 |

## 使用契約(翻案階段照抄 bitRepair/rebank 的模式)

1. **建置時機**:`incrAccurateBuild()` 目前由 refinement 前緣觸發(INCR_RELOC 等)。
   任何 **post-legalization** 階段可直接用。**pre-banking / in-banking 使用 = R3**,
   需先驗證 debank 完成後即建的可行性(拓撲齊全,理論上可;runtime 未量)。
2. **screening→commit 樣板**:平行 `evalXXXDelta` 篩(凍結態)→ 序列逐個:
   re-quote(apply 時再算一次,報價過期只會變 reject)→ accept 則物理變更 +
   `incrAccurateRecomputeFF`。**嚴禁**在平行區間做任何 commit。
3. **EVAL_ANCHOR=1 必掛**,否則回到 Preprocess 烤壞的錨(P1a 前世界)。
4. **1bis 注意**(修復中,workflow 未收):tc1/hc01 在 1bis 落地前 oracle 低估
   −23,875/−1,281——這兩案的翻案實驗等 1bis。
5. **不要用**:`getSlack()`/`getTNS()`/`CostCompare`(1-hop + stale arrCorrection_)、
   `getEvaluatorCost()`(歷史 bug)、`predictMBFFCost`(EJECT 前身的死因)。
6. 每輪保險:`INCR_VALIDATE=50` 開 incr==full 自檢(diff 必須 0.000000)。

## R2b 各實驗的切換點速查

| 實驗 | 舊定價位置 | 切換方式 |
|---|---|---|
| COSTCOMPARE_DOWNSTREAM | Banking::CostCompare | R3 範疇(front-end);post-LG 版可改走 evalRemapDelta |
| ACCURATE_BANKING | 舊 BFS(tie 凍結版)| **P1b 已直接修其根因**——最便宜的翻案,直接重測 |
| POST_LG_RESYNTH | predictMBFFCost | 改 evalRemapDelta(EJECT 已示範同樣的替換)|
| ITER_BANKING / BFS_PRE_DP / DP_ROUNDS | getSlack 家族 | 逐階段換 incrFFSlack(需 build 提前,半 R3)|
