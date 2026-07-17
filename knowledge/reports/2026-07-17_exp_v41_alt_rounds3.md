# v4.1 — ALT_ROUNDS=3(2026-07-17,server B)

**Type**: exp(凍結升級)
**一句話**:從沒人動過的 `ALT_ROUNDS`(2→3)在 v4 配方上**七案全勝**,
composite **0.9428 → 0.942405**;rep2 七案逐位同、hc02 det×2 逐位同,
儀式級收官。v4.1 = v4 env 僅改 `ALT_ROUNDS=3`,仍是單一配方零 per-case 超參數。

## 七案表(vs v4/W1)

| Case | v4.1 | Δ vs v4 | rep2 |
|---|---:|---:|---|
| tc1 | 734,281,585.055865 | **−85,899** | 逐位 ✓ |
| tc2 | 703,219.651524365 | −224 | 逐位 ✓ |
| tc3 | 725,796,409.964884 | −4,143 | 逐位 ✓ |
| hc01 | 29,952,245.2189526 | **−25,623** | 逐位 ✓ |
| hc02 | 9,621,648.9672128 | **−19,067** | 逐位 ✓(另 det×2 ✓)|
| hc03 | 55,769,935.7138071 | −462 | 逐位 ✓ |
| hc04 | 725,936,994.014967 | **−10,274** | 逐位 ✓ |

sanity 7/7;CONV_RATE=1e-7 對照(tc3/hc04/hc02)全面遜於 AR3,不採。

## 為什麼第三輪有肉(v3 時代沒有)

v3 的 ALT_ROUNDS=2 收斂後,round 3 只會空轉(conv-term 之前的實驗史)。
v4 引入 R1(modes 4/8)與 LNS(bit 拆分)兩個**新 move class**——每輪
RELOC/CRIT_SWAP/BIT_REPAIR 重整時序面後,R1/LNS 的可行域被刷新;
第三輪就是在收這兩個新算子的二階紅利(tc1 −86k 最顯著:R1+LNS 在
第三輪 tc1 找到第一二輪沒有的組合)。**推論:v5 若再加新算子,
ALT_ROUNDS=4 值得一 probe(邊際遞減預期)。**

## 代價(wall,含當時負載失真)

hc02 763→1058s、tc3 1350→1424s、hc04 1460→1711s;整輪 sweep
~67→~85 分鐘(T128 名單適用下)。House 規矩 score at all costs:採。

## tc1 副效應

v4.1 tc1 = 734,281,585,距不可重現的 v2-era 數字(734,266,279,見
`2026-07-16_diagnosis_tc1_v2_record_provenance.md`)只剩 **+15,306
(+0.002%)**——懸案的實際重要性已趨零。

## 附:Phase C2(pa 鏡頭)第一輪陰性結果

`LNS_LENS=pa` v1(driver-wish 質心分組):四案 0 accepts
(lens-off byte-exact PASS、無回歸)。診斷:健康 bits 被拉離現位,
α·dTNS 吃掉 dPA 節省。**v2 迭代已落庫(ccbf57cd):pa 鏡頭 wish 改現位
(最小位移收割)+ PROBE real-site 定價**,re-smoke 執行中,結果補記於此:
【PA2 待填】
