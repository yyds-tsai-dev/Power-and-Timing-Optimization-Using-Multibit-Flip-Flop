# Oracle-vs-Evaluator Gap — ROOT CAUSED (2026-07-07)

**Supersedes** `2026-07-06_diagnosis_oracle_vs_evaluator_gap.md` (which planned these probes).
**Verdict: 定價語義精確;commit 後的 slack 狀態更新深度不足 → 狀態腐蝕隨 op 數累積。**

## 證據鏈(全部 evaluator 實測,tc2,base = front-end-only 態 761,159.492822677)

1. **基態位移探針**(PROBE_MOVE,9 支,含穿 gate/重收斂):有訊號的 3 支,d1hop = dmultihop = dEval 到 1e-5(如 1.298930/1.298930/1.298935)。tc2 結構事實:**沒有任何 FF→FF 直連 net**,所有 launch path 都穿組合邏輯。
2. **基態結構探針**(PROBE_CELL,5 支,FF7↔FF10 同足跡同 power、ΔQpd=±0.008080 → dP=dA=dV=0,dScore=α·dTNS 純訊號):有訊號的 2 支三方一致到 1e-5(0.188000 vs 0.187988,穿重收斂)。
3. **Final-state 位移探針**(full unified config,ref 分數 723,165.3266 與官方 byte 一致):未被 refinement 碰過的 FF_1_6871,delta 仍三方 1e-5 一致。
4. **Op-class 歸因**(單獨開 20s):rebank/eject 在裸前端態無動作(dump byte 同 base);**bitRepair 一個 pass:內部宣稱 −20,576 vs evaluator 實測 −14,513 → 單 pass 製造 6,063(0.81% score)單向低估**。bitRepair 也正是 production 66–96% 預算的 operator → 解釋 checkpoint 漂移隨結構工作單調成長。
5. **判別實驗**(post-repair 態、被換過 bit 的 FF_4_* 位移探針):未觸碰鄰域(FF_4_992/989)仍 1e-5;**被觸碰者失準達 delta 的 15%**(FF_4_985: 模型 1.0195 vs eval 0.8705;FF_4_996: 1hop 11.995 ≠ multihop 12.332 ≠ eval 10.973——兩個內部模型連彼此都不合)。

## 機制

swap commit 時 stored per-bit slack 用有界深度傳播更新(淺於 delta 評估的全深度 cone walk)→ 深層 sink 留下 stale slack → rectifier Σmax(0,−s) 在錯的邊界剪裁後續 delta → 誤差跨千次 accepts 複利;單向因為未記錄的下游劣化讓 stored 狀態系統性偏樂觀。**incr==full 0.000000 永遠抓不到:兩邊讀同一份腐蝕狀態(自洽檢查的結構性盲點)。**

## 修復規格(留給實作;7/13 gate)

- Commit 後 slack **re-anchor**:從 parsed original per-sink slack + 當前(位置、Qpd)全深度前向傳播無狀態重算,替代在 stored 值上疊增量。兩個實作選項:(a) 每個 commit batch 後對受影響 cone 全深度重算;(b) 週期性全域 re-anchor pass(便宜、實作快,先驗證機制)。
- 驗收:63-checkpoint 殘差中位 0.048%→~0、max 1.12%→<0.1%;tc2 內部 TNS 4,708.6 → ≈5,333.7(eval-implied)。
- 修好後:官方 unified config 重跑 3×,更新兩篇 paper + thesis 全部數字(fidelity-attributable 增益可期:margin 附近被高報 42% 的 accepts 會被正確拒絕)。
- **注意**:re-anchor 改變 refinement 軌跡,分數可能±;但定價變真,長期只會更好(evaluator-monotone accepts)。

## Artifacts

- 探針腳本+全部 log/dump/evaluator:scratchpad `t4/`(session-local;重要數字都在本報告)
- harness:`main.cpp` PROBE_MOVE(c03dece)+ PROBE_CELL(本日新增,未 commit)
- 已寫入:TCAD §V(sec:gap-probes/gap-attrib/gap-fix 真數據)、ASP-DAC §3.4 residual 子句更新(頁面合規複驗過)
