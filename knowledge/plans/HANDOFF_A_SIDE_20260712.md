# A 側施工交接(2026-07-12,Fable 到期防護)

**接手者:先讀 BOOTSTRAP_DAC2027.md(B 已刷新),再讀本檔(A 側在途狀態),再讀
2026-07-12_four_rulings.md(現行裁決)。**

## 1|conv-term:程式碼已寫、驗收跑到後段、**尚未 commit**

- **在途編輯在 A 機 worktree 的 src/Manager.cpp(未 commit!)**。快照雙保險:
  scratchpad `conv/Manager.cpp.convterm_wip` + 專案根 `Manager.cpp.convterm_wip_20260712`。
- 實作:CONV_TERM=1(定點終止,時間上限降保險絲)、CONV_RATE=ε(速率提前退場)、
  `[CONV] round=… improved=… exit=fixpoint|rate|fuse` 日誌。default-off byte-exact(驗過)。
- 驗收已見結果(A 機 scratchpad conv/*.log/.done/.evaluator):
  hc02 純定點腿 3600s 保險絲觸發時仍 +0.5–0.9/round 改善(→ **v3 的 fuse 要拉高
  或靠 rate-exit**);tc3 rate 腿 1432s 收官;兩條 fixpoint 腿 2130s/1164s;
  D 階段(CONV_RATE 1e-6/1e-5)寫此檔時仍在跑(workflow wf_2955eaf0-5ec)。
- **接手動作**:等 workflow 完/讀其 output → 核對六關驗收 → commit(訊息含驗收數字)
  → byte-exact off 複驗。

## 2|A 側未清償義務(four_rulings §2):tns_share 軸實作

規格:首次 incrAccurateBuild() 完成時 tns_share = α·incrTNS_/oracleCostSnapshot();
θ 走 ParamMgr(env ADAPT_STACK_TH,default 0.05;B 實測窗口 (4.13%,14.55%),3.5×);
ON 時掛 B 的配方:COSTCOMPARE_DOWNSTREAM=1 DP_SLOT_INTRA_ONLY=0 DP_SLOT_ORACLE=1;
log `[ADAPT] stack=<on|off> tns_share=<x> th=<θ>`。與 conv-term 同批 commit。

## 3|v3 整合批(一次儀式)檢查單

(1) commit conv-term;(2) 實作 §2 軸;(3) v3 config = v2 + CONV_TERM=1 +
CONV_RATE=<依 D 階段挑> + 單軸疊加;(4) R1(c2e2490)已快審通過(結構檢查:
無自創 commit 機制、無裸變更、HR floor 在位)——default-off 直到 B 的 1b 豐富案
gate 波次全綠再併;(5) **雙機 3× 互驗儀式**(A/B 各三跑,分數應機器無關——
conv-term 的意義所在);(6) 24h 解綁閘 = **7/13**:conv-term 未全綠則走 B 案
(v3-knobs 先行,conv-term 進 v4)。

## 4|其他在途/資料位置

- P1d(v2-interim)三重跑:scratchpad bis/p1d_repeats.csv——tc1 byte 一致、
  tc2 4e-5 浮點翻轉、**tc3 rep3 在負載下 −182k 擺動 = conv-term 動機展品**。
- composite 現況:v2-interim 0.9500;B 預估 v3(+單軸疊加)~0.941;
  conv-term 的 tc3/hc04 定點紅利再往下。
- 兩機協作:段落式 push 協議 + 圍欄制(本檔所在 repo 即作戰室);
  **agent 在 worktree 施工時嚴禁 rebase --autostash**(教訓 ×2)。
