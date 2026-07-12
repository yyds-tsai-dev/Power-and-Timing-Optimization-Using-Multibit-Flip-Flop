# BOOTSTRAP — DAC 2027 極限優化(給接手的 Claude session)

**你是誰、在做什麼**:接續 DAC 2027 優化戰役(score + runtime 雙軸)。
主計畫:`knowledge/plans/plan_dac2027_optimization.md`。使用者授權「照計畫走」。
**雙機分工(2026-07-12 現行)**:server A 擁有 Manager.cpp 主幹(conv-term/
ParamMgr/oracle 家族);server B(NVL4 msedalab)擁有翻案實驗/分析 + 圍欄內實作
(oracleRebankRefine、postLGResynth、DetailPlacement)。圍欄協議見 four_rulings。

## 讀取順序(2026-07-13 刷新)

0. **`knowledge/HANDOFF_B_20260713.md`** — **此刻在飛什麼**(v3 儀式執行中、GPU 實驗鏈、背景任務續看方式)。新 session 先讀這份。
1. `knowledge/reports/2026-07-12_revival_finale.md` — **當前狀態總表**(墳場 8/8
   終審、v3 頭牌配置、四實作驗收、待辦交接清單)
2. `knowledge/reports/2026-07-12_four_rulings.md` — 現行裁決(單軸疊加規則、
   分工圍欄、conv-term 綁定與 24h 解綁閘)
3. `knowledge/plans/plan_lns_destroy_repair.md` — LNS 施工圖(implementation-ready)
4. `knowledge/reports/2026-07-11_evaluator_disassembly_semantics.md` +
   `2026-07-11_refsta_perff_diff.md` — evaluator 語義權威紀錄(反組譯+黑盒雙證)
5. `knowledge/memory/MEMORY.md` — 長期記憶索引(42+ 記憶檔同目錄,按需讀)

## 當前狀態(2026-07-12,server B session 收官)

- **定價器 evaluator-exact 已達成**:P1a 錨(30eef42)+ P1b 三規則(968c2b3)+
  Rule 1bis(e7df424)。四案 base 殘差 = f32 噪音級(tc1 −3.78、hc01 −0.09、
  tc2/hc02 ±0.009),雙機雙軌跡驗證。
- **unified v2** = v1 + `EVAL_ANCHOR=1`(composite 0.9518→0.9500,A 的 P1c 七案表)。
- **v3 候選配方已定**:v2 + 單軸疊加規則——`tns_share ≥ 0.05` 時
  `COSTCOMPARE_DOWNSTREAM=1 DP_SLOT_INTRA_ONLY=0 DP_SLOT_ORACLE=1` 全開。
  實測 tc2 −4.23% / hc02 −4.29%(七案矩陣見 finale §3),**composite 預估 ~0.941**。
  θ 窗口 (4.13%, 14.55%)(`2026-07-12_tns_share_axis.md`)。ParamMgr 收口歸 A,
  與 conv-term 同批;**24h 解綁閘 2026-07-13**(未全綠則 v3-knobs 先行)。
- **3b 執行緒結論**:分數對執行緒數/環境負載雙重不變(不撞牆案逐位驗證);
  撞牆案 T64 = 分數更好且 wall 1/3(tc3 −81k、hc04 −200k)。conv-term 後
  執行緒 = 純速度參數。runtime 現況 T64 全套 ~1h10m;+conv-term+de-hashing
  (A,設計圖就緒)預估 ~30-40 分。
- **實作齊備**(全部 default-off byte-exact,cmp 驗證):R2c'
  `DP_SLOT_ORACLE`(七案 gate 全過)、R1 `REBANK_MODES` 4/8 + `REBANK_HR_FLOOR`、
  PLR `PLR_ORACLE`(判決:設計死,存檔為 LNS 負對照)、LNS v1 `LNS_KICK`
  (冒煙結果補記於 finale §4)。
- **DAC 素材線**:audit→根因→修復→跨機重現;墳場 8/8 終審;斷崖=定價 artifact;
  slack-wallet double-spend(已定名)+ oracle gate 根治;單軸 adaptive(θ 窗口
  robustness);Pareto 曲線(待 conv-term)。

## 環境開機

```bash
git clone git@github.com:Coffeeturtle7/2024-ICCAD-Problem-B.git && git checkout v3_experimental
# boost:jfrog 已死 → https://archives.boost.io/release/1.84.0/source/boost_1_84_0.tar.gz
#        header-only 即可(不用 b2);g++12 需 Legalizer.h 的 <list>(已 commit)
make release -j24 CXXFLAGS="-I ./inc -I ./lib -I ./boost_1_84_0 -std=c++14 -fopenmp -pipe" WARNINGS="-g -Wall"
# 標準跑法見 README;EVAL_ANCHOR=1 = v2;v3 候選 env 見上
```

## 環境陷阱(雙機血淚合集)

- 長跑一律 cp binary 到暫存目錄再跑(rebuild 換 inode 殺 run)
- 背景 shell cwd 會重置——腳本第一行明確 `cd`,一切絕對路徑
- server B:`/`(含 /tmp)100% 滿 → 全走 NAS `~/scratch/`;gcc `-pipe`+`TMPDIR`
- 共用機 benchmark 紀律:每輪記 loadavg;撞牆案(tc3/hc04/hc02-r1)A/B
  必成對同窗口;不撞牆案分數決定性,窗口無關
- `grep -c` 零匹配 exit 1 會斷 `&&` 鏈;tcsh 登入殼(script 用 bash)
- 評分永遠 evaluator 本尊 + 兩 checker;`BANKING_MODE=matching PRODUCTION=1` 必掛
- **邏輯 vs 物理 FF**:oracle API(evalRemapDelta 等)吃邏輯 bits(clusterFF
  元素);pool/FF_Map 是物理——混用 = segv(R2c' 首版血淚)
- 數字凍結紀律:文字改動用 numeric-token 多重集前後比對自驗

## 下一步佇列(接力點,細節在 finale §6)

1. LNS 冒煙判讀 → 參數掃 → 七案 gate(施工圖 §5/§8;v1.5 bit 拆分規格 §2)
2. R1 modes 4/8 於 1b 豐富案 gate
3. v3 儀式:等 A conv-term → 雙機各 3× 互驗
4. 2g 實作(A 規格含 headroom floor;過濾器與 R1 共用 `rbBitHeadroom`)
5. Phase 4:ICCAD'25 移植(先問使用者 2025 測資位置)、DAC 稿

## 與 server A/B 的同步協議

新結果以「更新 knowledge/ + push」送出;開工前先 `git pull`。報告寫進
`knowledge/reports/` 並 commit push(私有 repo)。墳場歷史上下文
(memory postmortem + graveyard/ 詳報)完整在包內,翻案類工作先讀
`plan_phase2f_graveyard_revival.md` 與 finale。
