# LNS v1.5 — bit 拆分 destroy(2026-07-13,server B)

**Type**: exp(含一次 postmortem 級的 bug 修復)
**Commit**: `ac71ff3`(`LNS_SPLIT` gate,default off)
**施工圖**: `plan_lns_destroy_repair.md`(v1 = 整顆 FF 重組,cf7675f;
v1.5 = 跨 FF 邊界的 bit 級重組——「價值主體」,revival finale 預告成真)

## 0. 一句話結果

**hc02 −0.252%(9,653,644.30,合法、K=6)——v1 同案 −0.0096% 的 26 倍**;
bit 拆分的 move class 是真的,且與 rebank/R1 的收割空間正交。

## 1. 設計(EJECT 機具組合)

- **篩選**:整區一次 `evalRemapDelta`(overridden bits only;群組 site =
  wish 質心,wish = EJECT 的 idealOf 驅動腳位置)+ planPA,無副作用,
  screen 不過 → 零物理接觸跳下一區。
- **exact phase**:multi-bit 成員 `debankWithUndo` → 計劃群組跨界 `bankFF`
  (FindPlace @ wish 質心)→ 剩餘 bit 單體 FindPlace;逐步定價
  `incrAccurateRecomputeFF` 摺疊,accept 判準 α·dT + dPA + λ·dV < −margin。
- **萬用還原**:解散新 bank(debankWithUndo)→ 已放置 leftover 釋放 →
  按快照逐成員 `bankFF` 回原位 → `incrTNS_` 釘死(EJECT :5227 樣板)。

## 2. Postmortem:第一版的 row-discipline bug(06:03 smoke 全滅的原因)

**症狀**:hc02 K=10 與 tc3 gate evaluator **零分(非法)**;tc3 是
**0 accepts 也非法** → 兇手必在 reject 路徑。K=16 等 0-accept 案分數漂移。
**根因**:還原路徑對「從未進 legalizer rows 的 piece」呼叫 FreeRect。
debank/解散出來的 1b piece 座標 = 原 FF origin + pin 偏移 − 1b D 偏移,
**rect 可溢出原 footprint 到鄰居的佔用區**;FreeRect 把鄰居的空間標成
free → 後續 FindPlace 疊上去 → overlap。EJECT 的 revertAll 同型危險
(failure path)但觸發面窄;LNS reject 是高頻路徑,必炸。
**修復**(`ac71ff3`):顯式 row 追蹤——只有真正 rowed 的實體
(banked 群組 via debankWithUndo 內部、placedLeft 清單、尚未讓位的 1b
成員)才釋放;unrowed piece 永不碰 rows。
**教訓(給 EJECT 也適用)**:`FreeRect` 的合法輸入 = 「這個 rect 是我
rowed 的」,不是「這個 FF 存在」。EJECT revertAll 的 failure path 有同款
潛伏雷,v4 順手補。

## 3. 修復後驗證(fix/,binary b78d5473)

| 案 | 修前 | 修後 | 判定 |
|---|---|---|---|
| hc02 K=10 | **非法(0 分)** | 9,672,778.78(−0.054%)accepted=2, screen 115, rej 113 | ✅ 113 次 revert 無污染 |
| tc3 T=120 | **非法(0 accepts!)** | 725,805,800.84 合法;28 進 28 拒 | ✅ 指標案痊癒 |
| hc02 K=6 | 9,658,023(幸運合法)| **9,653,644.30(−0.252%)** accepted=5, screen 121, rej 116 | ✅ 最佳 hc02 |

參數訊號:**K=6 > K=10 > K=16**(小區域 = 細粒度 accept;K=16 篩過即全拒)。

## 4. 已知殘餘與 knob

- **reject 改名攪動**:exact-phase reject 會 debank/rebank 改名 → 下游名序
  漂移(tc3 +0.0003%)。非法性已除,漂移是固有成本;緩解 = `LNS_MARGIN`
  收緊篩選(tc3 放行 28 全空手 = 篩選過鬆的證據;篩選未計 FindPlace 位移,
  與 EJECT 同款樂觀偏差)。
- INCR_VALIDATE 的 diff 行出現在 RELOC/BIT 階段(LNS 前)——是否為
  EVAL_ANCHOR 既有基線【對照組 chain4 執行中,待填】。

## 5. 驗收狀態(house 標準)

1. byte-exact off:tc2 ✅(06:03 批次);**全七案 ✅(build 不變性套件,
   HEAD binary b78d5473 gate-off 七案全部逐位同儀式 rep1)**
2. determinism:buggy 版 ×3 逐位同(病也病得決定性);**修復版 K=6 ×2
   【待填】**
3. 七案 gate(0.5% 規則):修前那輪作廢;**修復版 gate 待排**(建議 K=6,
   在 K6 determinism 綠後)
4. v4 疊加測試:R1(`REBANK_MODES=15`)+ LNS_SPLIT 同開,排 v4 驗證輪。

## 6. Novelty 記事(DAC 稿)

exact-priced destroy-repair 無先例(lit recon 2026-07-08);RESYNTH 屍檢 =
天然負對照;v1(整顆)→ v1.5(bit 拆分)= 內建 ablation 階梯:
−0.0096% → −0.252%(26×)就是「跨 FF 邊界自由度」單獨貢獻的直接量化。
