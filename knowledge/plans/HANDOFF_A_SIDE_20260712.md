# A 側施工交接(2026-07-12,Fable 到期防護)

**接手者:先讀 BOOTSTRAP_DAC2027.md(B 已刷新),再讀本檔(A 側在途狀態),再讀
2026-07-12_four_rulings.md(現行裁決)。**

## 0|✅ 已結案(2026-07-12):HEAD 漂移 = 基線方法失誤,v3 解凍

結案雙證:(1) B 六點 bisect e7df424..HEAD **bit-identical,無 code leak**;
(2) A 對比兩腳本 env——conv-term agent 的基線 **漏掛 `ORACLE_EJECT=1 EJECT_TIME=180`**。
數字對帳:tc1 +434k ≈ EJECT 的 tc1 歷史貢獻 −447k;hc02 +8.1k ≈ EJECT 的 −7.2k ✓。
conv-term 的相對結論(CONV vs no-CONV 同缺 EJECT,自洽)不受影響:tc3 −25,325、
tc1 中性、RATE=1e-6 生產值皆有效。**v3 凍結解除。**
教訓入檔:任何「同 binary 基線」宣稱必附 env diff(兩腳本 grep env sort diff,10 秒)。

## 1|conv-term:已 commit(驗收全綠;CONV_RATE=1e-6 = 生產值)

- **在途編輯在 A 機 worktree 的 src/Manager.cpp(未 commit!)**。快照雙保險:
  scratchpad `conv/Manager.cpp.convterm_wip` + 專案根 `Manager.cpp.convterm_wip_20260712`。
- 實作:CONV_TERM=1(定點終止,時間上限降保險絲)、CONV_RATE=ε(速率提前退場)、
  `[CONV] round=… improved=… exit=fixpoint|rate|fuse` 日誌。default-off byte-exact(驗過)。
- 驗收已見結果(A 機 scratchpad conv/*.log/.done/.evaluator):
  hc02 純定點腿 3600s 保險絲觸發時仍 +0.5–0.9/round 改善(→ **v3 的 fuse 要拉高
  或靠 rate-exit**);tc3 rate 腿 1432s 收官;兩條 fixpoint 腿 2130s/1164s;
  D 階段(CONV_RATE 1e-6/1e-5)寫此檔時仍在跑(workflow wf_2955eaf0-5ec)。
- 驗收(13/13 過三檢):byte-exact off ✓×2;tc1 精確中性(byte-identical);
  tc3 同基線 −25,325;**CONV_RATE=1e-6 = 建議生產值**(107% 定點紅利、−16% 牆鐘);
  1e-5 = 快檔(65%、−45~49%)。
- **fuse 政策**:v2 config 下 tc3/hc04 到 3600s 仍 −150~−250/round(move space 被
  EVAL_ANCHOR+INTRA+ALLFF 放大,7/11 的 3080s 收斂證據過時)→ **v3 用 CONV_RATE=1e-6,
  fuse 維持 3× 保險**。
- 缺的三條同 binary 基線(tc2/hc04/hc02 base)腳本備妥 scratchpad conv/,跑掉補全。

## 2|A 側未清償義務:tns_share 軸實作【規格修訂 2026-07-12】

**原規格有時序 bug**:CCDOWN 是 banking knob,跑在 oracle build 之前——「首次 build 量測」
餵不到它。parse 態份額已實測否決(全 ≤0.71% 且 hc01 排序反)。
**新規格 = 雙點量測、同一 θ**:點 1 = banking 入口(1-hop getSlack 求 share,儀器
ADAPT_PROBE=1 已 commit,七案分離度驗證中)→ 餵 CCDOWN;點 2 = refinement 入口
(oracle build,原規格)→ 餵 DP_SLOT 對。**圍欄分工**:A 出量測+決策+
Manager::adaptStackOn() accessor;B 把三個消費點(Banking CCDOWN、DP_SLOT×2)改讀
`env 覆寫 || mgr.adaptStackOn()`。
原文存查:首次 incrAccurateBuild() 完成時 tns_share = α·incrTNS_/oracleCostSnapshot();
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
