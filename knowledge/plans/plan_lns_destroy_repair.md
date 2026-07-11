# LNS(exact-priced destroy-and-repair)設計規格(2026-07-12,server B)

**Type**: implementation-ready design(仿 de-hashing survey 標準;任何接手者可直接施工)
**Novelty 地位**: DAC 2027 頭牌之一——lit recon(2026-07-08)確認 placement/banking
無任何 exact-priced destroy-repair 先例;Synopsys US 11,328,109 自述 de-bank/re-bank
「expensive and unpredictable」= 現成動機引言。**今晚 RESYNTH 屍檢(2026-07-12)
就是「為什麼 naive destroy 必死」的 ablation——負對照組已經自帶。**

---

## 0. 一句話設計

在 `oracleRebankRefine` 圍欄內新增 kick 模式:選 TNS 熱點區域 → 整批 debank 成
bit 池 → 重建計劃(配對/四合)→ **整包一筆交易用 `evalRemapDelta` 無副作用定價**
→ 淨改善才物理落地,否則整包還原。**絕不無償 commit destroy——這是 RESYNTH
死因的直接否定式。**

## 1. 掛載點與圍欄

- **位置**: `Manager::oracleRebankRefine()` 內(four-rulings 第 4 條圍欄已授權 B;
  helper 放函式上方)。**不碰 main.cpp**(A 的 conv-term 禁區)——以
  `LNS_KICK=1` env 在 rebank 常規 rounds 之後進入 kick 迴圈。
- Gate 全家:`LNS_KICK`(default off,byte-exact)、`LNS_TIME`(default 120s)、
  `LNS_REGION_K`(區域 MBFF 數,default 10)、`LNS_ROUNDS`(default 4)、
  `LNS_PATIENCE`(default 50)、`LNS_MARGIN`(default 0)。
  headroom floor 沿用 `REBANK_HR_FLOOR`(four-rulings 要求)。

## 2. 交易語義(核心;RESYNTH 教訓的形式化)

RESYNTH 死因鏈:無條件 debank 500 顆 → 重配只救回 65% → 遺孤 P/A 白虧;
還原也救不了(散開後搬回的 αΔTNS ~O(100) >> P/A +64)。**結論:destroy 的
每一分代價必須在 accept 判定內,且 reject 時物理狀態逐位還原。**

LNS 的交易協議(關鍵洞察:`evalRemapDelta` 讓定價天然無副作用,
只有 accept 才動真格):

```
1. 選區域(§3)→ members = k 顆 MBFF(混 bits 可)
2. 【探測,不 commit】legalizer 試探:
   for m in members: FreeRect(m) + RemoveNodeByFFPtr(m)     // 騰空區域
   重建計劃 P = 貪婪組裝(§4):每組 FindPlace 得真實落點
   leftover bits 也 FindPlace 單體落點 → 全部 bits 都有 (newD,newQ,newQpd)
   若任何 FindPlace 失敗 → abort:for m in members: UpdateRows(m) 還原,下一區域
3. 【定價,無副作用】一次呼叫:
   dT   = evalRemapDelta(all_bits, nD, nQ, nQpd)             // 整包時序
   dPA  = Σ wcost(新 cells) − Σ wcost(members 的 cells)      // 庫表算術
   dV   = binTable.estimateViolationDelta(...)               // 密度
   Δ    = α·dT + dPA + λ·dV
4. Δ ≥ −LNS_MARGIN → 【整包還原】for m in members: UpdateRows(m);continue
   (定價無副作用,物理未動,還原 = O(k) 個 UpdateRows)
5. Δ < −LNS_MARGIN → 【落地】對每組:bankFF_deferred → commitFinalizeBank;
   debank 側用 debankFF(不是 WithUndo——已決定 commit);leftover 單體
   setNewCoor + UpdateRows;每顆新物理 FF `incrAccurateRecomputeFF` 摺快取;
   binTable.applyMutation;curTNS 更新(壓 incrTNS_ 釘死,照 rebank :4649 模式)
```

注意與 rebank `tryMerge` 的差異:tryMerge 是「先 bank 再看再回滾」
(單組小交易);LNS 是「先全定價再決定」(區域大交易)——因為
bankFF_deferred+rollbackBank 的 k 組嵌套回滾易錯,而 evalRemapDelta
本來就支援任意 per-bit 假設,無需真的 bank 就能精確定價。
**物理探測(FindPlace)與定價(evalRemapDelta)分離是本設計的支點。**

## 3. 區域選擇(deterministic,不用 rand)

- 主鏡頭(TNS kick):對 `incrFFNeg_` 取 top 壞 bit(排序:負量降冪、
  同值按 instanceName——house 決定性紀律),以其物理 FF 為種子,
  rtree 取同 clk 最近 `LNS_REGION_K` 顆 MBFF(含種子;跳過 getFixed)。
- 已 kick 過的種子進 visited 集合(per round),避免重複轟同一區。
- patience:連續 `LNS_PATIENCE` 個區域 reject → 提前結束 round。
- 未來鏡頭(v2,先不做):headroom 貧困區修復 kick、power-harvest kick
  (與 2g 銜接;2g 規格的 floor 約束同源)。

## 4. 重建計劃(區域內)

池 = members 的全部邏輯 bits(debank 假設下每 bit 自由)。貪婪組裝:
1. 候選:同 clk 內 4 合(最近鄰四顆,economics 表:4b 在多數案最省)與
   2 合(best2),照 rebank 枚舉模式(CSRTree per clk)。
2. 每候選估價 `evalGroupMoveDelta(gbits, centroid, tgtCell)` + wcost 差
   (平行篩,read-only ✓)→ 按估值升冪貪婪取 disjoint 組。
3. 覆蓋不到的 bits = leftover,單體 1-bit(oneBitCell)——**其 P/A 與位移
   代價自動進 §2 的整包定價**(RESYNTH 的無償遺孤在此結構性不可能)。
4. HR floor:候選組任一 bit `rbBitHeadroom < REBANK_HR_FLOOR` → 剔除
  (helper 已在 R1 落地,直接呼叫)。

## 5. 驗收協議(house 標準)

1. **byte-exact off**:`LNS_KICK` 未設 → cmp 與前置 binary 的 tc2 unified
   .out 逐位相同(不撞牆案,窗口不敏感)。
2. 開發 A/B 順序:**hc02 先**(final 態 α·TNS≈1.57M = score 的 15.5%,
   LNS 最肥的地);tc1 次(計畫指名的「全收斂仍剩 TNS」案;final α·TNS≈4.2M
   但 share 僅 0.6%,期望值 −0.05% 級);tc2;然後 7 案 gate(≤0.5% 規則)。
3. 決定性重跑 ×2;`INCR_VALIDATE=50` 自檢 incr==full diff=0。
4. 與 v3 的關係:開發期同機 A/B 自洽即可;**正式數字等 conv-term+v3 基線**
  (A 的 24h 解綁閘後自然對齊)。

## 6. 上限量級(sizing,base 態工件實測外推)

| Case | final 態 α·TNS(≈score−βP−γA) | share | LNS 回收 10% = |
|---|---|---|---|
| hc02 | ~1.57M | 15.5% | **−1.6%** |
| tc2 | ~52k(P1b 後) | 7.1% | −0.7% |
| tc1 | ~4.2M | 0.6% | −0.06% |

(oracle 疊加已收 −4.2%×2 之後 LNS 疊乘其上;兩者 move class 正交——
疊加動 slot/定價,LNS 動 banking 結構。)

## 7. 陷阱清單(逐條血淚,接手者必讀)

1. **邏輯 vs 物理 FF**:`evalRemapDelta` 的 bits = 邏輯(clusterFF 元素);
   pool/members 是物理。R2c' 首版就是這樣 segv 的(dbg.log 存證)。
2. legalizer 還原紀律:每個 FreeRect+RemoveNode 的 reject 路徑必配
   `UpdateRows(原 FF)`(rebank tryMerge :4610 模式)。
3. `incrAccurateRecomputeFF` 只在 accept 後呼(摺快取);reject 路徑
   什麼都不呼(定價無副作用!)。incrTNS_ 釘死照 :4649。
4. binTable:估價用 estimateViolationDelta(read-only),落地才 applyMutation。
5. 迭代中不得動 FF_Map(先收集後變異——house 模式)。
6. slot 順序 = group 順序 = bankFF 順序(D0/D1/…);evalRemapDelta 的
   nD/nQ/nQpd 按同序構造。
7. 決定性:嚴禁 rand;一切排序帶 name tiebreak;OMP 只用於 read-only 篩。
8. `getIsLegalize()` 不是 liveness(memory `project_refine_allff_discovery`)
   ——不要拿它過濾。
9. EVAL_ANCHOR=1 必掛(oracle 錨);缺 8-bit lib cell,別寫 8 合模式。

## 8. 施工順序建議

1. 骨架:LNS 迴圈 + 區域選擇 + 探測/還原(先空定價,量 FindPlace 成功率)
2. 定價接線:evalRemapDelta 整包 + dPA + dV
3. hc02 smoke → 參數掃(REGION_K ∈ {6,10,16};TIME 120→600)
4. 7 案 gate + 決定性 + INCR_VALIDATE
5. 報告 + ablation(vs RESYNTH 負對照、vs rebank-only)

工件索引:undo 原語簽名 Manager.h:151-170;rebank 樣板 Manager.cpp
oracleRebankRefine(tryMerge/枚舉/篩選全可抄);RESYNTH 負對照
`2026-07-11_graveyard_revival_probes.md` + plr.log;headroom helper
`rbBitHeadroom`(R1);經濟學 `tools/merge_economics.py`。
