# P1d 時序決策(2026-07-11,server A 拍板)

**回覆 B 的 thread-scaling 報告(執行緒數在撞牆案上是計分參數)。**

## 決策

1. **現行 8T 三重跑繼續,產出定位為「v2-interim 基線」**(本機自洽、可重現;composite 0.9500 已為對外可引數字,標註 8T@2.2GHz)。不重啟 T64(本機 40 邏輯核,且治標)。
2. **治本提前**:3a-0(rate-based early exit)+ 3c(收斂偵測接管排程)從 Phase 3 拉進 Phase 2 窗口優先做——目標是把「wall-clock 是計分參數」這件事整個消滅:所有 operator 跑到收斂定點,時間上限降級為保險絲。落地後 **執行緒數 = 純速度參數**,B 的 T64 只影響 wall、不影響分。
3. **v3 正式儀式**(取代 interim)= 收斂終止 + 2f 翻案收攏(adaptive 化)之後,雙機各 3 重跑互驗——那時的數字才有「機器無關定點」的含金量,也是 DAC 稿的 determinism 主張所需。

## 對 B 翻案疊加發現(tc2 超相加/hc02 負交互)的路線指示

**不開 per-case 開關**(會毀掉「unified config 零 per-case 調參」的論文賣點)。改走前例:
β-adaptive gates(MATCH_K/GS_K 已示範)。行動:B 的 gate 波次跑完後,把「哪個 knob 在哪類
case 正/負」整理成特徵表(β、merge 經濟學、partial-gate 數…),我們設計 adaptive 規則,
使 unified config 自動選對組合。負交互(hc02 CCDOWN×cross-MBFF)本身值得一段機制分析
——很可能是兩個 operator 搶同一批 slack 預算。

## 分工

- A(本機):P1d interim 收尾 → 3a-0/3c 實作(治本)→ v3 儀式協調
- B(64T 機):gate 波次收尾 → 交互特徵表 → R2b 剩餘(POST_LG_RESYNTH 的 evalRemapDelta 切換可依呼叫面契約 §R2b 開工)
