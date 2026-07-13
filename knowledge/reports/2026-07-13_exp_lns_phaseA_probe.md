# LNS v2 計畫 Phase A(v1.5 調參)+ Phase B(PROBE)結果(2026-07-13 晚)

**Type**: exp
**歸檔口徑**:Phase A 的全部數據 = **v1.5 引擎調參**(`LNS_SPLIT`,無新代碼);
Phase B = **v2 首批代碼數據**(`LNS_PROBE`,real-site 定價)。
工件:`~/scratch/lns15/fix/`(A1/A2)、chain5 log(A3/A4)、
`~/scratch/lns_probe/`(chain6)。基線 = 儀式 rep1。

## Phase A1 — margin 掃(hc02,K=6):**M=0 定案,單調變差**

| M | 0 | 50 | 200 | 1000 |
|---|---|---|---|---|
| score | **9,653,644** | 9,659,541 | 9,666,491 | 9,677,940 |

擋 accept 的損失 >> 省下的攪動 → 治命中率靠 real-site 定價,不是 margin。

## Phase A2 — K 掃(hc02,M=0):**甜點 K=4,非單調**

| K | 3 | **4** | 6 | 10 | 16 |
|---|---|---|---|---|---|
| score | 9,646,418 | **9,640,716(−0.385%)** | 9,653,644 | 9,672,779 | ~9,685,411 |
| accepts | 20 | 7 | 5 | 2 | 0 |

## Phase A4 — LNS-only 七案 gate(K=6):形式 PASS,實質「特定案武器」

hc02 −0.252% / tc1 −0.009% 賺;**hc01 +0.063%**(13 accepts 局部全 δ<0
但擾動下游淨負——LNS 不該在 hc01 開);其餘 ±0.02% 攪動噪音。

## Phase A3 — R1+LNS 七案(K=6):**最佳單一配置,composite 0.9430**

6 案改善 + hc01 中性。亮點 = **hc04 正協同**:R1 開路後 LNS 得 4 accepts
(−0.0119%;單開各只 −0.003%/0)。tc2 被 LNS 微稀釋(−0.205% vs R1-only
−0.242%)→ tc2 建議 R1-only。

## Phase B — PROBE 驗證(chain6,K=6):工程 ✅、分數槓桿混合

- byte-exact off ✅(tc1/tc2)、det×2 逐位 ✅。
- **攪動降 ~60%**(hc02:probeRej 142 零改名擋掉,exactRej 116→47)。
- **解鎖新案**:tc3 0→**10 真 accepts**(−2,730,LNS 首次在 tc3 獲利)、
  hc04 0→2。real-site 定價讓「wish 樂觀」下不可見的真機會浮現。
- 代價:hc02 退到 −0.150%(probe 擋掉了部分事後有利的樂觀 accept)。
- **誠實註記**:probe reject 是 name-clean 但**非 row-state-neutral**
  (tc2:probeRej=16、exactRej=0、0 accepts,分數仍動 −236——
  FreeRect+UpdateRows 還原後 row 簿記與原狀有微差,影響下游決策;
  v1 同款模式固有性質;合法且決定性,但不是基線中立)。

## v4 配方藍圖(chain7 K=4 矩陣補完後定案)

per-case:tc2/hc01/tc3※/hc03 → R1-only;hc02 → LNS K=4 無 probe;
hc04/tc1 → R1+LNS;tc3※ 若 chain7 W2/W3 確認 probe 增益 → R1+LNS+probe。
估 composite ~0.9428(vs v3 0.9437、v2 0.9500)。
chain7(過夜):W1 R1+LNS K4 / W2 R1+LNS K4+probe / W3 LNS K4+probe /
W4 LNS K4,+ W1 det×2(hc02/tc3)。
