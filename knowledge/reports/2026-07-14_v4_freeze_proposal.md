# v4 凍結提案(2026-07-14 03:40,server B;數據獨立重算驗證)

**Type**: proposal(chain7 K=4 矩陣收全;所有數字經第二計算通道從原始 log
重算核對,零出入)

## 主推:v4 = v3 + `REBANK_MODES=15` + `LNS_KICK=1 LNS_SPLIT=1 LNS_REGION_K=4`

(= 矩陣的 W1 配置;LNS_PROBE **不進** v4,理由見 §3)

| Case | v4(W1) | Δ vs v3 rep1 | 案內最佳者 |
|---|---:|---:|---|
| tc2 | 703,444.13 | −0.241% | W2(−0.298%)|
| tc1 | 734,367,484.19 | −0.045% | W2(−0.051%)|
| hc01 | 29,977,868.53 | **−0.051%** | **W1** |
| hc03 | 55,770,398.14 | −0.0017% | W2(−0.0031%)|
| hc02 | 9,640,715.82 | **−0.385%** | **W1**(=W4)|
| tc3 | 725,800,552.78 | −0.0004% | W2(−0.0048%)|
| hc04 | 725,947,268.20 | −0.0046% | K6-R1+LNS(−0.0119%)|

**七案全勝 v3(0.5% gate 滿分);composite 0.9437 → 0.942788。**
per-case 混配上限 0.942684——只比單配置好 1bp,不值得放棄 README
「單一配方、零 per-case 超參數」的故事主張。**建議凍結單配置。**

## 2. 完整矩陣 composite(獨立重算)

| 配置 | composite |
|---|---|
| **W1 = R1+LNS K4** | **0.942788** |
| W2 = R1+LNS K4+probe | 0.942934 |
| rl = R1+LNS K6 | 0.943031 |
| r1v3 = R1-only | 0.943220 |
| W4 = LNS-only K4 | 0.943245 |
| W3 = LNS-only K4+probe | 0.943345 |
| v3 rep1 | 0.943653 |

歸因:R1 貢獻 ~0.43bp、LNS-K4 貢獻 ~0.41bp、正協同 ~0.03bp(W1 超過
兩者單開之和);probe 在 hc02 的逆風(−0.385%→−0.175%)抵銷其在
tc1/tc2/tc3/hc03 的增益,單配置下淨負。

## 3. PROBE 的定位(不進 v4,進 v5 菜單)

工程主張全驗證(byte-exact off、det、攪動 −60%、tc3 真 TNS 改善 ~3k)。
但 score 面是 per-case 分化:W2 贏 4 案卻在 hc02(composite 最大槓桿)
損失 0.21%。若未來走 per-case gates 或 probe 的 accept 門檻再調
(probe-accept 但保留 wish-accept 候補),可再擠 ~1bp。v5 素材。

## 4. 驗收狀態

- 全 49 run(7 配置 × 7 案)sanity 綠、無零分(獨立重算旁證)。
- W1 determinism ×2(hc02/tc3):hc02 det1 ✓【det 其餘進行中,
  CHAIN7_DONE 後補記】
- byte-exact off:全七案 BI 套件 ✅(binary b78d5473;0bc0365a 抽查 tc1/tc2 ✅)
- 環境:placement_checker 限 A 機(GLIBCXX);B 側以 sanity + evaluator
  placeViol 證合法(儀式同款)。

## 5. 建議流程

1. det×2 全綠 → 本提案轉 v4 凍結清單(env set 如上,執行緒自由,
   T128 名單 tc3/hc04/hc03 適用)。
2. v4 儀式:B 側 2×(rep2 逐位同即免 rep3,v3 先例)→ README v4 欄。
3. A 側有空時:v3+v4 加冕包一起跑(`knowledge/coronation/` 已備 v3;
   v4 kit 屆時同款生成)。
4. 隊列不阻塞項:EJECT rowfix(gated)、K=2 探底、LNS Phase C/D、
   de-hashing(A)。
