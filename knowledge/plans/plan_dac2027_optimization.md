# Plan: DAC 2027 極限優化作戰(score + runtime 雙軸)

**建立**: 2026-07-08(Fable)| **狀態**: ACTIVE — Phase 0 進行中
**前提**: ASP-DAC 2027 放棄投稿;目標改為 DAC 2027。paper 資產全數沿用(6pp ACM 格式相同)。
**死線(2026-07-11 校準)**: DAC 2027 = 64th,2027-07-10~16 San Jose(官方 placeholder 已確認);CFP 預計 2026 年 8–9 月出(DAC 2026 是 7/26–29 還沒開)。模板推估:abstract ~11 月上中旬、manuscript 一週後、通知 ~2027 年 3 月上旬;6 頁+refs、雙盲、ACM 模板。**技術凍結目標 = 10 月底(不變)**。

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

## Phase 1(7/09–7/20)Exactness 戰役 [2026-07-11 修訂:原 T4 假說已被實測取代]

**實際根因鏈(全部實測)**:① Preprocess 用 1-hop 模型重錨 → 誤差烤進錨點(P1a:`EVAL_ANCHOR=1` parse 真錨,已 commit 30eef42,tc2 全配置 −2,714);② evaluator 語義黑盒解碼(16 微實驗零誤差:max-max STA、float32 算術、不可達 D-pin 踢除、OUT1-only 死弧);③ 離線參考 STA(`tools/refsta.py`,22 微案逐位重現)per-FF 取證 → 剩餘 404 TNS = **tie-cell 凍結連鎖 −398.7(139 顆零輸入 gate,427 bits 凍在 parse slack)+ OUT2 死弧 −5.7**。

1. [x] P1a EVAL_ANCHOR(commit 30eef42;byte-exact off 驗證)
2. [~] P1b 三規則修復(tie-cell 解凍 / 不可達踢除 / OUT1-only)——workflow 執行中;驗收 = base 態 accurateTNS ≈ 7,850.49(float32 容差 0.01)+ byte-exact off + tc2 全配置分數
3. [x] P1c 七案 sweep 完成:**composite 0.9518 → 0.9500**(表:reports/2026-07-11_p1_sweep_results.md;tc2 −0.80%、hc02 −0.61%;6 勝 1 微負過 gate)
4. [~] P1d 新基線儀式(repeat#2/3 跑動中):P1b 過關後 7 案 × 3 重跑(EVAL_ANCHOR+語義修復併入 unified config v2)→ 更新 README/bundle/knowledge;確定性重驗;舊 config 為回退分支
- **tc1 期待值(新情報)**:tc1 有 1,530 顆零輸入 gate(tc2 的 11 倍)——tie-cell 修復對我們最弱的 TNS 案可能是大額紅利
- 已知限制(記錄、不行動):evaluator 為 float32、我們 double,邊際噪音 ~1e-8 相對,不做模擬

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
- **2g Slack-headroom harvesting(新 move class,語義解碼直接催生)**:解碼證實「非臨界路徑加長免費、直到成為新 max」——headroom = max − 該 fanin arrival,現在可精確計算。做「用 headroom 換 power」operator:找 sink 非臨界 fanin 有大 headroom 的 FF 群合併省 power,加長被 headroom 吸收 → evaluator 零 timing 費。hc02 型金礦,與 LNS 正交
- R3 註記升級:三規則同樣證明前端 1-hop 模型與 Preprocess::DelayPropagation 錯在同處——前端 oracle 化價值上修
- Gate:7 案嚴格不回歸;任一案 >0.5% 回歸就停下分析

## Phase 3(8/15–9/30,與 Phase 2 重疊)Runtime 攻擊

- **3a-2 false-sharing 修復(survey 途中抓到的現行 bug)**:dyna2 rescore 迴圈的 `dirty` vector<char> 在 schedule(dynamic,8) 下跨執行緒共享 cache line——打包成 per-i 32B POD 陣列即修
- 設計文件就緒:`reports/v1/2026-07-11_dehashing_design_survey.md`(CSR 化快取、topo-index ID、per-thread SoA scratch、單 socket pinning、dynamic,1;**FP 累加順序已是決定性 → 驗證標準升級為 byte-exact .out**)
- **3a-1 scratch de-hashing(profiling 實證最大餅)**:oracle 報價 scratch 從 per-call unordered_map 改為稠密邏輯 ID + per-thread epoch-stamped 平坦陣列(hash+malloc 佔 55% 取樣!)。預期 bitRepair 2–3×;驗收 = 7 案分數中性(容 1e-13 FP 漂移)
- **3a-0 rate-based early exit(預算探測直接證據)**:bitRepair 輪內改善率 < ε/min 即退場,後續階段自癒——預期 capped 案再 1.5–2×、零分數代價;一個下午可驗證
- **3a bitRepair 增量化**(最大餅):dirty-set 重篩(只重篩被上輪 commit 弄髒的鄰域,而非每輪全量)、候選清單快取、鄰域剪枝
- **3b 執行緒擴展研究**:8 → 16/32(機器有 40 邏輯核;8 是歷史值,scaling 曲線沒量過)
- **3c 收斂偵測**接管排程,時間上限降級為保險絲
- **3d 微優化**:trial-apply 向量化、K-nearest 預計算重用
- 交付:**score-vs-time Pareto 曲線**(DAC 的招牌圖;「每個預算點都是 SOTA」)

## Phase 4(9/15–10/15)泛化 + 論文

- **ICCAD'25 Problem B 移植 [2026-07-11 重新定價:比想像貴]**:偵察發現 2025 版不只換輸入格式——**評分改用 ICC2 `update_timing -full`(商用 STA),無 density 項,I/O 是 Verilog+DEF+SDC+Liberty**(intel 報告有完整對照表與測資 GitHub 鏡像)。「evaluator-exact」不能字面遷移;精確本地評分需要 ICC2 license(**問使用者:NYCU 實驗室有 Synopsys ICC2 嗎?**)。無 license 的退路:僅定性移植(operator 層 + 自建近似 STA)或放棄雙 suite、把泛化論述改為方法論可遷移性。NTU 冠軍系 DAC'26 LBR 已宣稱制霸 2025 全部隊伍——2025 suite 的敘事權在他們手上,我們的主場是 2024 suite 深度 + 方法論
- **TIMBER 反駁素材(已備好)**:其「13× 勝 2024 冠軍」實為 bin-violation 罰金套利(他們的 checker 設定下冠軍 binary 出現 1–14 個 BDV;官方計分為零),PPA 幾何平均僅「comparable」且 case2 輸冠軍一倍——DAC 稿引用+反駁一段即可
- DAC 稿:新頭牌 = 端到端 exact(audit→根因→修復)+ LNS + runtime Pareto;沿用 ASP-DAC 圖文資產;模擬審稿 ×2 輪
- 黑盒解碼 16 實驗 + refsta + per-FF 取證 = 完整新章素材;**預備答辯**「逆向 evaluator 算不算 overfit?」:(a) contest evaluator 就是宣告的目標函數;(b) decode-your-signoff 方法論可遷移(黑盒微測資→參考實作→per-sink 取證);(c) ICCAD'25 雙 suite 泛化佐證
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
