# EJECT_ROWFIX — EJECT revertAll 潛伏雷修復與驗證(2026-07-16,server B)

**Type**: exp(含一次 byte-exact bug 的攔截)
**Commit**: `3160643b`(gate `EJECT_ROWFIX`,default off)
**動機**:LNS v1.5 postmortem(`2026-07-13_exp_lns_v15_bit_split.md` §2)點名
EJECT 的 `revertAll` 在 FindPlace-failure 路徑上有同款
「FreeRect 未 rowed 的 piece」潛伏雷。本項修 + 驗。

## 1. 修法

row 追蹤集 `rowedNew`:只有本候選真正 UpdateRows 過的 piece(mode-1 每片、
mode-2 每 2b bank)才進集合;`revertAll` 的 FreeRect 迴圈在 gate on 時
`if(!rowedNew.count(p)) continue` 跳過未 rowed 的 piece(其 rect 從未佔 row,
釋放會誤donate 鄰居空間)。gate off = 原路徑逐位不變。

## 2. 攔截到的 byte-exact bug(重要教訓)

**首版(4cb86161)gate-off 破 byte-exact**:`rowedNew.insert()` 忘了用 gate
包住 → gate off 時仍執行插入。後果不是語義改變,而是
**`std::unordered_set<FF*>` 的堆配置擾動了指標鍵 unordered 容器的迭代序**
(FF* 的 hash = 指標值;插入 rowedNew 的節點改變後續 heap 佈局 → 部分
FF 物件位址位移 → 某處未排序的 pointer-keyed 迭代序漂移)→ **tc1 輸出位元
變、但分數逐位同 rep1(734,700,023.359553)**——「同分不同位元」正是
命名/順序漂移的指紋,不是真 placement 變化。
**修法**:兩處 insert 改 `if(rowFix)` 守衛。驗證:tc1 gate-off byte-exact 復原
(3160643b,P0 PASS)。
**教訓入 memory**:見 [[server-b-byte-exact-pointer-hash-trap]]。

## 3. 驗證(chain10,binary 3160643b)

| 檢查 | 結果 |
|---|---|
| P0 gate-off byte-exact(tc1/tc2) | **PASS**(修復確認)|
| P1 gate-on 七案 vs rep1 | 6/7 逐位同;tc1 當次 DIFFERS(見 §3b 翻案)|
| tc1 gate-on determinism ×2(獨跑) | **d1≡d2,且 d1≡rep1(gate-off 基線)三方逐位同** |

## 3b. tc1 異常的完整解剖(重要翻案)

P1 的 tc1 漂移**與 rowfix 無關**,證據鏈:
1. 分岔點在 **round-0 RELOC build 之前**(incrTNS Δ=2.38)——早於任何
   EJECT 呼叫;三個 run 的 EJECT 統計(63 tried/28 accepted ×2 invocations)
   完全相同。
2. gate-on 獨跑 ×2 **逐位同,且逐位同 gate-off 基線** —— gate-on 在 tc1
   常態軌跡零影響。
3. env-block-size 理論被對照駁回(gate-off + 同長 dummy var ≡ rep1)。
4. banking [MATCHING] 語義全等(matched/committed/dropped)→ 分岔夾在
   **postBankingOpt/legalize/DP → RELOC** 區間。
**定讞:先天存在的罕見負載觸發 race**(該次 P1 tc1 與另一車道共跑;
全計畫 ~50+ 次跑動僅此一咬;同分 734,700,023.359553、sanity 合法)。
歷史上 DP 有過 ChangeCell OMP race(已修)——疑似殘餘同族。
**追獵排隊**:helgrind/callgrind 對 tc1 postBanking→RELOC 段在人工負載下
重現(score-neutral,純衛生項,不擋 v4)。
**對 determinism 主張的影響**:v4/v3 的 40+ 逐位證據(含共跑)不受影響;
誠實註記改為「負載不變性 = 高置信但非絕對(觀測 1/50+ 例外,同分合法)」。

## 4. 判讀:潛伏雷真實但在賽題集良性

- 路徑**可達**(tc1 gate-on 有變化即證 EJECT 在 tc1 有 FindPlace 失敗的
  reject)。
- 但**未惡化成非法**:tc1 舊 revert(freeing unrowed)與新 row-disciplined
  revert **都合法、同分**。與 LNS v1.5 相反——LNS reject 是高頻路徑,一次
  overlap 就污染輸出;EJECT reject 稀疏,且 revert 後立即 bankFF+UpdateRows
  重佔 oldPos,誤釋放的鄰居空間在被 FindPlace 搶用前多半已被鄰居的
  UpdateRows 重新宣告。
- **定位**:防禦性硬化,非 score fix。**保持 gated、default off、不併入
  凍結 v4**(gate-on 改 tc1 位元、零分數收益,併入需重跑儀式不值)。
  價值 = 消除一個「換 input/config 就可能咬人」的潛伏非法風險 + 證明
  該路徑可達(風險是真的,只是此賽題集未觸發成非法)。

## 5. 後續

- 若未來 EJECT 在別的 config 出現非法輸出,先開 `EJECT_ROWFIX=1` 排除此雷。
- LNS v1.5 的 row-discipline 修法(`ac71ff3`)與本項同源,已落庫;兩處是
  「FreeRect 合法輸入 = 我 rowed 的 rect,不是 FF 存在」教訓的完整覆蓋。
