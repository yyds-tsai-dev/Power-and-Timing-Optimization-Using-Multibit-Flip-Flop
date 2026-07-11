# Phase 2f 墳場翻案工作單(2026-07-11,server B)

**狀態**: READY——前置條件(P1b 三規則修復 + P1d 新基線)落地即可逐項開跑。
**原則**: 只翻「死於定價」的;每項給死因證據、翻案機制、實驗規格、經濟學上限。
**經濟學數據來源**: `tools/merge_economics.py`(靜態庫分析,本檔表格皆出自它)。

---

## 0. 靜態經濟學總表(每個 merge 的原始收益,未計位移 TNS/density)

| Case | 2×1b→2b 收益/merge | 2×2b→4b 收益/merge | 4b dQpd | 4b TNS 打平點 | 讀法 |
|---|---|---|---|---|---|
| tc1 | +4,341 | +8,649 | +0.87 | 865 | 收益大但 Qpd 貴(2b +4.49!),TNS 型案 |
| tc2 | +64 | +30 | +0.0002 | 3.0 | 4b 微利,Qpd 免費;量大(21k bits) |
| tc3 | +4,567 | +8,975 | +0.87 | 898 | 同 tc1 型 |
| hc01 | **−315** | **+5,012** | +1.81 | 501 | **2b 原始虧損、4b 大賺 → 4-bit-first 結構優勢案** |
| hc02 | +6,004 | **+3,000** | **+0.0002** | 300 | **金礦:4b 每 merge 3,000 分且 Qpd 趨近零** |
| hc03 | +498 | +30 | +0.0002 | 3.0 | 2b 為主,4b 微利 |
| hc04 | +4,567 | +8,975 | +0.87 | 898 | 同 tc3 |

關鍵解讀:
- **hc02 +254% 慘案的真相**:原始經濟學極度有利(每 merge +3,000、Qpd 免費),
  純粹是 cascade 定價(每個 merge 獨立估價、忽略鄰居 slack 變化)殺死的。
  exact oracle 修好後,這是全套件最大的單案紅利候選。
- **hc01 是 NTU 式 top-down(4-bit-first)的結構性論據**:2b 合併原始虧損
  (−315/merge)而 4b 大賺——我們「2-bit 先鎖對、greedy 4-bit 撿剩」的順序
  在這案先天吃虧。1+1+1+1→4 直達 = +4,381/quad。
- tc1/tc3/hc04:收益大但 Qpd 代價高(2b +4.49、4b 再 +0.87),翻案要靠
  oracle 挑「slack 餘裕 ≥ 打平點」的群——正好是 2g headroom 的資料結構。

## R2a. DP_SLOT_ASSIGN cross-MBFF —— 零代碼便宜重測(第一優先)

- **死因證據**: `memory/project_dp_slot_assign.md`——`DP_SLOT_INTRA_ONLY=1` 為
  default,cross-MBFF 分支「hc02 −3.32% on table but cascades」。
- **翻案機制**: 純重測。P1d 新基線(修好的定價器)下 `DP_SLOT_INTRA_ONLY=0`。
- **實驗規格**: unified config v2 + `DP_SLOT_INTRA_ONLY=0`,先 hc02、tc2,
  正向再全 7 案。驗收:evaluator 本尊 + 兩 checker;任一案 >0.5% 回歸即停。
- **成本**: 2 案 ×1 run;上限 = hc02 −3.32% 級。

## R2b. tc2 十四連敗子集重測 —— 零代碼(第二優先)

死因分類(`memory/project_tc2_exhaustive_analysis.md`,2026-05-08 全表):

| 實驗 | 當年 | 死因分類 | 翻案? |
|---|---|---|---|
| COSTCOMPARE_DOWNSTREAM | +2.35% | 1-hop delta 誤導 | ✅ 重測 |
| ACCURATE_BANKING | +1.88% | BFS 版仍 7% 低估(tie 凍結!) | ✅ 重測(P1b 直接修它) |
| POST_LG_RESYNTH | +0.89% | 1-hop 定價 | ✅ 重測 |
| ITER_BANKING | +0.31% | 1-hop 定價 | ✅ 重測 |
| BFS_PRE_DP | +0.16% | BFS 凍結版 | ✅ 重測 |
| DP_ROUNDS=1 | +0.07% | 同上 | ✅ 重測 |
| TNS_SCALE 全掃 | +0.9~2.5% | 組合斷崖(定價相鄰) | 挑 0.95/1.05 兩點驗證斷崖是否還在 |
| MATCH_HIGHER_BIT | +3.6% | cascade 定價 | → 併入 R1 |
| SKIP_DP | +2.81% | 結構性(非定價) | ❌ 維持死亡 |

- **注意**: 當年 postmortem 明言「post-LG 全滅因為用 1-hop 模型做 banking 決策」——
  P1b 修的是 oracle 家族;這些階段若仍走 getSlack()/CostCompare 舊路徑,重測
  只能驗證「壞定價的階段在好基線上還是壞的」。真正翻案要把這些階段的定價
  切到 oracle(掛 EVAL_ANCHOR 家族)——需 server A 確認 P1b 後的 oracle 呼叫面。

## R1. Higher-bit 結構 move(MATCH_HIGHER_BIT 級)—— 小代碼,走 rebank 通道

- **死因證據**: `memory/project_match_higher_bit.md`(hc02 +254%、tc2 +24.4%;
  root cause = cascade,非 per-merge 精度);`graveyard/2026-04-26` §4
  (2-bit 已鎖死、位移複合、無 per-merge 安全網)。
- **翻案機制**: **不走 banking 期 matching**(那是 R3),走 `ORACLE_REBANK`
  通道加 2+2→4 MODES——rebank 已是 oracle 定價 + 逐 move 驗收,天然免疫 cascade
  (每個 move 用當下真實狀態報價,accept 後狀態即時更新)。
- **實驗規格**: 順序 hc02(打平點 300,Qpd 免費,量最大)→ tc2/hc03(微利驗證
  operator 不虧)→ hc01(4b 大賺但 Qpd +1.81,驗 oracle 挑群能力)→ tc1/tc3/hc04
  (打平點 ~900,預期少量高價值 merge)。驗收同 R2a。
- **實作面**: rebank operator 在 Manager.cpp——**與 server A 協調**(oracle 區
  P1b 進行中,勿並行改)。規格先行:MODES 擴充 + 候選生成(R-tree 近鄰 2b 對)
  + oracle 報價 + 逐 move commit,預估 <200 行。
- **上限量級**: hc02 若 ~2,000 個 2b MBFF 中 1/4 可合併 → 500×3,000 = 150 萬分
  級(hc02 官方 10.09M,即 ~15%)——上限誇張,實際受位移/密度/佈局約束,
  但即使 1/10 兌現也是全套件最大單項。

## R3. 前端 oracle 定價 / Approach C 復活 —— 大代碼,P1d 後

- **死因證據**: `memory/project_approach_c_deadend.md`(NTU top-down + force
  model 全回歸;root cause = 1-hop 模型);`graveyard/plan_approach_c_hybrid_restructure.md`
  (完整架構已設計,`NTU_FLOW=1`/`TIMING_PRELOC=1` 基礎設施已在)。
- **翻案機制**: oracle 只需 debank 後邏輯網表(refsta 證明語義可從 parse 態算)
  → banking 期即可真價。per-level ΔC 與 S_cluster 的 slack 項全部換 oracle 值。
  計畫 2026-07-11 修訂版已註記「前端 1-hop 與 Preprocess::DelayPropagation 錯在
  同處(tie 凍結)→ 前端 oracle 化價值上修」。
- **與 R1 的關係**: R1 是 rebank 端的低風險增量版;R3 是 banking 端的全結構版。
  R1 先行探明各案 4b 兌現率,R3 依結果決定投入(hc01/hc02 訊號最強)。
- **排程**: P1d 新基線後,與 2b LNS 並行評估(同為 Phase 2 大票)。

## 2g 前置 sizing(server B,tc2 base 態實測,2026-07-11)

headroom 探測(scratch `refsta/headroom_probe.py`;每個 FF Q pin 對其全部 fanout 弧取
min(該分支到所屬 gate max 的距離);直連 D 弧取 max(0, slack);OUT1-only 與 tie-cell
未計時 launch 均按 evaluator 語義處理;21,164 Q pins、132,293 gates 全覆蓋):

| 門檻(TNS 單位) | FF Q pins | 佔比 | 等效自由距離 |
|---|---|---|---|
| ≥ 0.5 | 11,483 | 54.3% | 5,000 units |
| ≥ 2.0 | 10,438 | 49.3% | 20,000 units |
| ≥ 10.0 | **8,843** | **41.8%** | **100,000 units(~4% 晶片寬)** |

分佈:p50 = 1.73、p75 = 44.4、p90 = 84.8;p25 = 0(四分之一的 FF 在臨界分支上,動不得)。

**解讀**:tc2 有四成 FF 的 Q 側弧可以「免費」長 10 萬單位——2g(用 headroom 換 power
的合併)與 R1(4-bit rebank 挑群)共用同一個候選過濾器:`Q 側 headroom 大 + 自身 D
slack 正`。tc2/hc02 同網表,此表直接適用 hc02(權重不同、佈局會不同,落地後重測)。
注意本表是 LOCAL 下界(沒把「越過 max 之後吃 endpoint slack」的第二層預算算進去),
實際可行域只會更大。per-FF 明細:server B `~/scratch/refsta/tc2_headroom.tsv`。

## 永久死亡確認(反組譯釘棺,不再翻案)

- Steiner/RSMT、net-HPWL:evaluator 逐指令確認為兩點 HPWL(int 曼哈頓 × f32 dd)。
- EGR(evaluator-in-loop):P1b 後 internal==eval,光榮退役。
- SKIP_DP:結構性死亡,與定價無關。

## 執行順序建議(落地即跑)

1. **R2a**(零代碼,2 runs)→ 2. **R2b 子集**(零代碼,~8 runs)→
3. **R1 hc02 探路**(小代碼,協調 server A)→ 4. R1 全案 → 5. R3 決策點。

Server B 可先跑 1/2(等 P1d 基線);R1 規格細化與 2g headroom sizing
(挑群資料結構共用)在 server B 的 3b 階梯空檔進行。
