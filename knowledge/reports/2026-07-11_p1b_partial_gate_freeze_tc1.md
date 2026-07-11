# P1b 殘留 bug 通報:部分連接 gate 凍結(tc1/hc01 專屬)(2026-07-11,server B)

**Type**: diagnosis / hotfix advisory
**給 server A:P1d 新基線儀式在 tc1/hc01 上必須等本修復;P1c 的 tc1/hc01 數字暫列存疑。**

## 症狀(base 態殘差,server B 獨立軌跡,P1b binary 968c2b3)

| Case | implied TNS(evaluator) | accurateTNS(EVAL_ANCHOR) | 殘差 |
|---|---|---|---|
| tc2 | 11,660.6238 | 11,660.6143 | **+0.0095 ✓** |
| hc02 | 261,791.7518 | 261,791.7588 | **+0.0070 ✓** |
| hc01 | 126,962.05 | 125,681.12 | **−1,280.93 ✗** |
| tc1 | 905,413.40 | 881,538.00 | **−23,875.40 ✗** |

P1b 在 tc2/hc02 exact,在 tc1/hc01(同一張網表)系統性**低估**。

## 根因(已定位,證據齊)

**tc1/hc01 有 4,230 顆「部分連接」gate(lib IN 腳數 > 網表實際連接數);
其他五案全部是 0。**(掃描腳本:對每個 gate inst 比對 lib IN pin 數 vs Net 區段實連數)

- oracle(`computeAccurateTNS` 家族)的 Kahn/BFS 觸發門檻是 `cell->getInputCount()`
  (**lib 腳數**)。懸空 IN 腳永遠不會送事件 → 這 4,230 顆 gate 永不觸發 →
  其下游 cascade 凍結。P1b Rule 1 只種子了「零輸入」gate(tie cell),
  部分連接的這批是**同一機制的孿生變體**,沒被救到。
- evaluator(反組譯確證):懸空 IN pin fanin 為空 → `markNoDelay` 標 noDelay →
  `timing()` 回 (0,0) → 在 `GateOutputDelay` 的 max 摺疊中貢獻 0.0f(= 種子值,
  等同不存在)→ **gate 用其餘連接的輸入正常計時**。
- P1b Rule 2 使凍結 bit 被 DROP(貢獻 0)→ 低估、殘差為負,與觀測一致。

## 修復規格(Rule 1bis)

觸發門檻從 lib `getInputCount()` 改為**建圖時實際插入的 fanin 弧數**
(含 P1b 保留為拓撲事件的 OUT2 死弧):
1. 建圖時記錄 per-gate `connectedInputCount`;
2. `gateCnt == connectedInputCount` 即觸發(取代 `getInputCount()`);
3. `connectedInputCount == 0` 者併入 Rule 1 的 −inf 種子(涵蓋現有 tie cell);
4. 同步鏡射到 `Preprocess::DelayPropagation` / `refreshArrivalCorrections` /
   incremental cone recompute(P1b 改到哪裡就鏡到哪裡)。
語義正確性論證:evaluator 端懸空 IN 貢獻 0.0f 而 max 種子本就是 0.0f,
故「忽略懸空 IN」與 evaluator 逐位等價;活輸入照常參與 max。

## 註記與限制

- 殘差計算的 density 項(tc1 nviol=7、hc01 nviol=18)按解碼語義離線計算,
  未逐案驗證;每錯 1 bin 影響殘差 ±λ/α = ±1,000。tc1 的 −23,875 不可能由
  density 解釋(需錯 >23 bins),**時序凍結是主項**;hc01 的 −1,281 中
  density 可能貢獻 ±1k,但同網表必有時序項。修復後重測自然分解。
- 影響面:tc1/hc01 的 unified+EVAL_ANCHOR 決策目前在 4,230 個 cone 附近錯價;
  tc2 的 −0.80% 紅利不受影響(tc2 無此結構)。
- **驗收建議**:修復後 server B 可即刻重跑四案 base 態殘差(工件與腳本都在,
  幾分鐘內完成);tc1 目標 |殘差| < ~1(f32 噪音在 tc1 尺度會比 tc2 大)。

## Rule 1bis 跨機驗收(e7df424 落地後,server B 獨立軌跡)

| Case | 1bis 前 | 1bis 後 | 相對 TNS 質量 |
|---|---|---|---|
| tc1 | −23,875.40 | **−3.78** | 4.2e-6(f32 噪音,tc1 arrival 尺度大)|
| hc01 | −1,280.93 | **−0.09** | 7e-7 |
| tc2 | −0.009 | −0.009(bit-identical .out)| — |
| hc02 | +0.007 | +0.007(同)| — |

**PASS——oracle == evaluator-implied 不變量四案成立,雙機雙軌跡收斂。**

## 工件(server B)

- base 態:`~/scratch/run_tc1base/`、`~/scratch/run_hc01base/`(.out/.log/per-bit dump)
- 部分連接 gate 掃描:本報告 §根因 的計數腳本(inline);
  殘差計算:`~/scratch/refsta/`(refsta parser + 分量拆解)
