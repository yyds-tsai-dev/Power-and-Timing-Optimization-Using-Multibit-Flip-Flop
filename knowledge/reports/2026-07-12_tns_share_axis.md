# tns_share 軸:七案實測與 θ robustness 窗口(2026-07-12,server B)

**Type**: analysis(four-rulings 第 1 條義務 (a))
**量測**: base 態(frontend-only,EVAL_ANCHOR=1+1bis binary)`accurateTNS`(=首次
oracle build 的 incrTNS_ 等價值)與 evaluator score;tns_share = α·TNS / score。
與裁決第 2 條的 run-time 量測點(首次 `incrAccurateBuild()` 時
α·incrTNS_/oracleCostSnapshot())同語義。

## 七案表

| Case | base α·TNS | base score | tns_share | 疊加(ccdown+crossOracle)實測 |
|---|---|---|---|---|
| hc02 | 2,617,918 | 11,543,103 | **22.68%** | **−4.29%** |
| tc2 | 116,606 | 801,542 | **14.55%** | **−4.23%** |
| hc01 | 1,269,620 | 30,774,842 | 4.13% | +0.555%(爆門檻) |
| tc1 | 8,815,380 | 739,607,682 | 1.19% | +0.064% |
| tc3 | 2,767,608 | 728,334,257 | 0.38% | (oracle 疊加對待補;單 knob 皆噪音級) |
| hc04 | 2,788,423 | 728,359,314 | 0.38% | (同上) |
| hc03 | 154,063 | 55,862,759 | 0.28% | −0.039%(棄之,零頭) |

## θ robustness 窗口

ON 群最小值 14.55%(tc2)、OFF 群最大值 4.13%(hc01)→
**θ ∈ (4.13%, 14.55%),任何值決策相同——3.5× 無差別窗口**。
default θ=0.05(裁決)舒適居中。論文陳述格式同 dQpd 軸
(「θ 在 [0.05, 0.14] 內決策不變」= 非調參的機制軸)。

## 附註

- share 排名與疊加響應完全單調(22.7/14.6 → 大贏;≤4.1 → 回歸或零頭),
  無反例——軸的預測力在 7/7 案成立。
- tc1 的 share(1.19%)遠低於其 TNS 絕對量直覺(α·TNS=8.8M 是七案最大),
  分母(佈局面積主導的 score)才是對的歸一化——這正是「β 不是好軸」的
  另一面證據。
- 單 knob 全數據保留於 `2026-07-11_knob_case_feature_table.md` +
  `2026-07-11_graveyard_revival_probes.md`(ablation 素材,義務 (b));
  dQpd 軸退役紀錄同前者(義務 (c))。
