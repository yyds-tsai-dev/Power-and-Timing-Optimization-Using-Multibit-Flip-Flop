# Plan: DAC 2027 極限優化作戰(score + runtime 雙軸)

**建立**: 2026-07-08(Fable)| **狀態**: ACTIVE — Phase 0 進行中
**前提**: ASP-DAC 2027 放棄投稿;目標改為 DAC 2027。paper 資產全數沿用(6pp ACM 格式相同)。
**死線(依 DAC 2026 模式推估,CFP 公布後校準)**: abstract ~2026-11-19、manuscript 隨後(DAC'26 為 abstract 11/19/2025);**技術凍結目標 = 10 月底**。

## 現況基線(2026-07-06 官方紀錄)

- Unified config(含 EJECT):composite **0.952**,7/7 勝 top-3;已發表最佳 0.979(DATE'26)/0.991(LBR)
- Runtime:204s(tc1)~ 2,582s(hc04);全套 ~3h @ 2.2GHz×8T
- **Stage-time 解剖(本日實測)**:bitRepair 佔 87–96%;**tc3/hc04 兩輪各撞滿 1200s、hc02 第一輪撞滿——三案根本沒收斂,是被時間切斷的**
- 未兌現的大票:**T4 re-anchor 修復**(根因已閉合:commit 的有界深度 slack 更新腐蝕狀態;bitRepair 單 pass 高報 42% 改善)

## 兩軸目標

| 軸 | 現況 | 目標 |
|---|---|---|
| Score | 0.952 | **0.94x**(每案打到真收斂定點 + 新 move class)|
| Runtime | 43min 最壞案 | **全品質 ≤10min/案**;2-min tier ≤0.960;Pareto 曲線當賣點 |

---

## Phase 0(7/08–7/11)分析與基礎 [✅ 完成,總報告 reports/v1/2026-07-11_dac_phase0_analysis.md]

- [x] Stage-time 解剖(bitRepair 87–96%,三案雙撞牆)
- [x] **預算解放探測**(tc3/hc04 完成;hc02 補跑中——首輪因測資路徑分居兩處而失敗:hc01/hc02 在 `2024-ICCAD-Problem-B/testcase/`,hc03/hc04 在根目錄 `testcase/`,注意!):
  - tc3:726,041,482 → **725,987,691(−53,791,−0.0074%)**,wall 3080s,**未撞新牆=已達真收斂定點**
  - hc04:726,015,652 → **725,976,226(−39,426,−0.0054%)**,wall 3083s,同上
  - hc02(補跑):**10,089,886.42 = 官方分數一分不差**;round1 到 1393s 才收斂但多做的工被後續階段自癒吸收
  - **結論 1:現行配置已在架構定點附近——score 躍進只能靠 T4 修復 + LNS,加時間白給。Phase 1/2b 優先序拍板**
  - **結論 2(runtime 金礦)**:hc02 證明 bitRepair 尾段純浪費(pipeline 自癒)→ **rate-based early exit(改善率 < ε 提早退場)可能砍半 tc3/hc04 的 40min bitRepair 且零分數代價——升為 Phase 3 首發項,比 dirty-set 增量化便宜**
  - 結論 3:跨機器 hc02 差異的機制修正 = 軌跡分岔(accept 順序不同),非同軌跡多做工;單調性主張仍安全
- [x] Profiling(gdb 取樣法,tc1/8T;報告:`reports/v1/2026-07-11_dac_phase0_analysis.md`):**hash map 操作 ~37% + malloc ~18% + barrier 空等 ~17%,領域邏輯僅 ~6%** → Phase 3 新首發 = scratch de-hashing(稠密 ID + epoch-stamped 平坦陣列,預期 bitRepair 2–3×)
- [x] 文獻掃描(2026-07-08,報告:`reports/v1/2026-07-08_dac2027_lit_recon.md`):
  - **ICCAD 2025 Problem B 又是 MBFF power/timing(新 benchmark suite!)** → 泛化目標升級為核心項;同時警訊:2025 冠軍論文潮會搶 DAC 2027 同一批版面
  - 新增 must-cite:ICCD'25 MBFF DTCO(10.1109/ICCD65941.2025.00066)、MLCAD'25 ML clustering(10.1109/MLCAD65511.2025.11189132);watch:DAC'26 "B-Flex"(UNIST+KAIST,內容未公開,7 月後再查)
  - **LNS novelty 成立**:無任何 exact-priced destroy-repair 在 placement/banking 的先例;canon 引用已驗(Shaw'98、Schrimpf'00、Ropke&Pisinger'06);Synopsys 專利 US 11,328,109 自己說 de-bank/re-bank「expensive and unpredictable」= 現成動機引言
  - ICCAD 2026 contest 已轉向,MBFF 賽道到 2025 為止
- [ ] DAC 2027 CFP 盯梢(每月查一次;姊妹死線 TCAD 不衝突——期刊可後投)

## Phase 1(7/09–7/20)T4 re-anchor 修復 = 最高確定性槓桿

### P1a 進度(2026-07-11,Fable)— EVAL_ANCHOR 已實作並部分驗收
- **實作**:`EVAL_ANCHOR=1` env gate(default off)。debank 時捕捉 parse 真錨(FF.h 四欄位 + Preprocess.cpp 一行),oracle 家族(incrFFSlack/incrAccurateBuild/computeAccurateTNS/兩個 slackOv)orig 側 38 個錨點讀取切到 eval 錨(Manager.cpp 檔內 static helpers aSlack/aD/aQ/aQpd,g_evalAnchor 於建構子讀 env)。前端不動。**未 commit——測完再說**
- **驗收 A:flag off byte-exact PASS**(ea_off.out == t4/pr_ref.out)
- **驗收 B:oc_rep 缺口 6,063(0.81%)→ 4,503(0.60%)**——錨點理論解釋 ~25%,剩 450 TNS 單位殘差
- **意外收穫:flag on 最終分數 745,542.65 vs off 746,646.96(好 1,104!)**——truer pricing 已直接改善決策
- **下一診斷(未跑成,權限流中斷)**:base 配置 + EVAL_ANCHOR=1 + TNS_ORACLE_VALIDATE=1 於 tc2 → 看 accurateTNS(eval 錨版)vs eval-implied TNS **在前端狀態(零 commits)** 的殘差:若 ≠0 → 模型語義仍有缺口(查:max tie、FF 無 prev 的常數 slack、多 driver、CLK 排除);若 =0 → commit 累積路徑還有洞(查 BATCH 重定價、INTRA、affected set 完整性)
- 工作檔:scratchpad/dac0/(cadb_p1 = 已編譯的 patch 版 binary;accept_p1.sh;diag_base 腳本意圖如上)
- 源碼改動(未 commit):inc/FF.h、src/Preprocess.cpp、src/Manager.cpp

規格已在 `HANDOFF_to_opus_20260707.md` T4 + `reports/v1/2026-07-07_diagnosis_oracle_gap_ROOTCAUSE.md`:
1. 便宜版:commit batch 後全域 re-anchor(`origDSlack_` 錨點基礎設施已存在,Manager.cpp:80-115 有現成模式)
2. 驗收:oc_rep 實驗的 6,063 缺口 → ~0;63-checkpoint 殘差 1.12% → <0.1%
3. 通過後增量版(只重算受影響 cone)拿回速度
4. 官方 3× 重跑 → **新基線**(margin 上被高報 42% 的假 accepts 會被正確拒絕;軌跡會變,備好舊 config 回退分支)

**為什麼它是 DAC 故事的頭牌**:修完 = 端到端 evaluator-exact(不只 delta 語義,連 committed state 都 exact)——「audit 方法論 + 根因 + 修復 + 收益」是完整的敘事弧,比 ASP-DAC 版強一級。

## Phase 2(7/20–8/31)Score 極限戰

- **2a 預算解放**:T4 修復後「無改善 move」的判斷可信 → 用收斂偵測取代時間上限;先跑 run-to-convergence 量化三個被切斷案的真定點
- **2b LNS / destroy-and-repair(新 move class,DAC novelty)**:現在是純下降、卡 local optimum。有 exact oracle 就能安全做 kick:選一個區域(k 顆 cell)整批 debank → 用 exact-priced rebank 重建 → 淨改善才收。目標:tc1 這種「其他 operator 全收斂還剩 TNS」的案子
- **2c Higher-bit 重試**:MATCH_HIGHER_BIT 的 +254% 災難是 **proxy 時代的結論**——oracle + staged commit 下重新評估 4b 之上/3-bit 組合(先查 lib 有什麼)
- **2d EGR 退役確認**:T4 後 internal==eval,evaluator-in-loop 不再必要,砍掉包袱
- **2e Per-case 深潛**:tc2(merge-count 型)、tc1(TNS 型)各自的殘餘結構
- **2f 墳場翻案(定價修正的複利;使用者 2026-07-11 拍板)**——只翻「死於定價」的:
  - R1 MATCH_HIGHER_BIT 級 higher-bit 結構 move:走 rebank MODES 擴充 + 真價(+254% 是 proxy 時代判決)
  - R2 便宜重測:DP_SLOT_ASSIGN cross-MBFF(hc02 −3.32% 在桌上,cascade≈定價複利)、tc2 十四連敗實驗的子集(當年 postmortem 明言「只有 evaluator-oracle 能救」)
  - R3 前端 oracle 定價可行性:oracle 只需 debank 後邏輯網表 → banking 階段就能用真價(Approach C/LP banking 當年缺的正是這個)
  - 永久死亡確認(黑盒解碼釘棺):Steiner/RSMT、net-HPWL(evaluator 就是兩點 HPWL);EGR 光榮退役
  - 前置條件:refsta 修完殘差 + 7 案 gate 通過(滿血定價器再翻案)
- Gate:7 案嚴格不回歸;任一案 >0.5% 回歸就停下分析

## Phase 3(8/15–9/30,與 Phase 2 重疊)Runtime 攻擊

- **3a-1 scratch de-hashing(profiling 實證最大餅)**:oracle 報價 scratch 從 per-call unordered_map 改為稠密邏輯 ID + per-thread epoch-stamped 平坦陣列(hash+malloc 佔 55% 取樣!)。預期 bitRepair 2–3×;驗收 = 7 案分數中性(容 1e-13 FP 漂移)
- **3a-0 rate-based early exit(預算探測直接證據)**:bitRepair 輪內改善率 < ε/min 即退場,後續階段自癒——預期 capped 案再 1.5–2×、零分數代價;一個下午可驗證
- **3a bitRepair 增量化**(最大餅):dirty-set 重篩(只重篩被上輪 commit 弄髒的鄰域,而非每輪全量)、候選清單快取、鄰域剪枝
- **3b 執行緒擴展研究**:8 → 16/32(機器有 40 邏輯核;8 是歷史值,scaling 曲線沒量過)
- **3c 收斂偵測**接管排程,時間上限降級為保險絲
- **3d 微優化**:trial-apply 向量化、K-nearest 預計算重用
- 交付:**score-vs-time Pareto 曲線**(DAC 的招牌圖;「每個預算點都是 SOTA」)

## Phase 4(9/15–10/15)泛化 + 論文

- **ICCAD'25 Problem B benchmark 移植(升級為核心,成本下修)**:使用者親自比過 2025——**只改了 input format,內容高度相似**;當年因 bug 中止。移植 = 格式轉接層 + 修掉那隻舊 bug。使用者可能還留有 2025 測資/當時的 parser 改動(屆時先問位置,免重下載)。**現階段先專注 2024 suite(使用者指示)**,2025 排 Phase 4。
- DAC 稿:新頭牌 = 端到端 exact(audit→根因→修復)+ LNS + runtime Pareto;沿用 ASP-DAC 圖文資產;模擬審稿 ×2 輪
- **TCAD 關係決策**:DAC(會議)與 TCAD(期刊)重疊政策確認;預設 TCAD 延後至 DAC 投稿後改寫為其延伸版

## Phase 5(10/15–死線)凍結與投稿

- 3× 官方重跑、數字凍結、合規、abstract + manuscript

## 風險登記

| 風險 | 緩解 |
|---|---|
| T4 修復改變軌跡、個案回歸 | 舊 config 留 fallback 分支;per-case gate |
| LNS 無界 runtime | 嚴格預算 + patience;先在小案驗證 |
| DAC 競爭強度(比 ASP-DAC 高)| 頭牌敘事升級(端到端 exact + 方法論);Pareto 曲線補 runtime 短板 |
| ICCAD'25 移植成本未知 | Phase 4 才做,可裁撤 |
| CFP 日期偏移 | 每月盯梢;凍結日提前一個月當緩衝 |

## 決策記錄

- 2026-07-08:放棄 ASP-DAC(使用者決策);paper 資產保留,GitHub 維持 private 到有發表為止
